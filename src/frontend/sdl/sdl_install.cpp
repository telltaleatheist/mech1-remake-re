#include "frontend/sdl/sdl_install.h"
#include "frontend/sdl/system_pickers.h"
#include "gamez/zutil/zutl_path.h"
#include "rekit/search.h"

#include <sstream>
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>


namespace sdlfe {

namespace fs = std::filesystem;

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string trimmed(std::string s) {
  while (!s.empty() && (s.back() == '/' || s.back() == '\\' || s.back() == ' ')) s.pop_back();
  return s;
}
std::string parent(const std::string& p) { return fs::path(p).parent_path().string(); }
}  // namespace

bool install_valid(const std::string& dir, std::string* missing) {
  std::error_code ec;
  if (!dir.empty() && fs::is_regular_file(zutl::any_case(dir, "zbd/reader.zbd"), ec)) return true;
  if (missing) {
    *missing = dir.empty() || !fs::is_directory(zutl::any_case(dir, "zbd"), ec) ? "zbd\\" : "zbd\\reader.zbd";
    if (dir.empty() || !fs::is_directory(zutl::any_case(dir, "video"), ec)) *missing += " and video\\";
  }
  return false;
}

std::string install_of_pick(const std::string& file) {
  std::string dir = trimmed(file);
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) dir = parent(dir);  // a file: its folder
  const std::string name = lower(fs::path(dir).filename().string());
  if ((name == "zbd" || name == "video") && !install_valid(dir)) dir = parent(dir);
  return dir;
}

std::string pick_source(const std::string& file, const std::vector<std::string>& kinds) {
  std::error_code ec;
  if (fs::is_directory(file, ec)) return file;
  const std::string ext = lower(fs::path(file).extension().string());
  const std::string bare = ext.empty() ? ext : ext.substr(1);
  if (bare == "bin") {  // a cue sheet's data file: the sheet
    for (const char* c : {".cue", ".CUE"}) {
      const fs::path cue = fs::path(file).replace_extension(c);
      if (fs::is_regular_file(cue, ec)) return cue.string();
    }
  }
  for (const std::string& k : kinds)
    if (bare == k) return file;
  return install_of_pick(file);
}

// MechWarrior 3's and Pirate's Moon's installers (disc SETUP.INS, value.shl):
// SOFTWARE\PUBLISHER_NAME\PRODUCT_NAME\PRODUCT_VERSION gets "Program", and
// InstallationInfo's App Paths entry for PRODUCT_KEY (mech3.exe, both games',
// so the caller checks which). Pirate's Moon's key is "MechWarrior 3 EP1"
// (its install's mw3pm.reg) and it must be installed apart from MechWarrior 3
// (its readme).
std::vector<std::string> install_candidates(bool pirates_moon) {
  rekit::SearchHints h;
  h.registry = {{pirates_moon ? "MicroProse\\MechWarrior 3 EP1\\1.0" : "MicroProse\\MechWarrior 3\\1.0", "Program"}};
  h.app_paths = {"mech3.exe"};
  if (pirates_moon) {
    h.installer_folders = {"MicroProse/MechWarrior 3 Pirate's Moon", "MicroProse/Pirate's Moon"};
    h.other_folders = {"Games/MechWarrior 3 Pirate's Moon", "Games/Pirate's Moon", "Games/PiratesMoon",
                       "MechWarrior 3 Pirate's Moon", "Pirate's Moon", "PiratesMoon"};
  } else {
    h.installer_folders = {"MicroProse/MechWarrior 3", "MicroProse/MechWarrior3"};
    h.other_folders = {"Games/MechWarrior 3", "Games/MechWarrior3", "Games/MW3", "MechWarrior 3", "MechWarrior3", "MW3"};
  }
  // MW3_SEARCH_PROGRAM_DIR (checks): the program's places taken from there, for a drop-in laid out by a test.
  const char* base = SDL_getenv("MW3_SEARCH_PROGRAM_DIR");
  if (!base) base = SDL_GetBasePath();
  return rekit::install_candidates(h, base ? base : "");
}

bool save_install(const std::string& write_dir, const std::string& install) {
  // Absolute: a relative folder (mw3 --install-to game/x from the repo) means
  // nothing to the app, which starts in another directory (PR #1, dj0nes).
  std::error_code ec;
  const fs::path abs = install.empty() ? fs::path() : fs::absolute(install, ec);
  return save_config_value(write_dir, "install", ec || abs.empty() ? install : abs.lexically_normal().string());
}

namespace {
// mw3.cfg's lines, each with its own ending, byte for byte (a CRLF file stays
// CRLF, a mixed one mixed), and the ending a new line takes: the file's first,
// else the system's.
struct CfgLines {
  std::vector<std::string> lines;  // endings included; the last may have none
#if defined(_WIN32)
  std::string eol = "\r\n";
#else
  std::string eol = "\n";
#endif
};
CfgLines read_cfg_lines(const fs::path& cfg) {
  CfgLines c;
  std::ifstream f(cfg, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string text = ss.str();
  bool seen = false;
  for (size_t at = 0; at < text.size();) {
    const size_t nl = text.find('\n', at);
    const size_t end = nl == std::string::npos ? text.size() : nl + 1;
    c.lines.push_back(text.substr(at, end - at));
    if (!seen && nl != std::string::npos) seen = true, c.eol = nl > at && text[nl - 1] == '\r' ? "\r\n" : "\n";
    at = end;
  }
  return c;
}
bool write_cfg_lines(const fs::path& cfg, const CfgLines& c) {
  std::ofstream o(cfg, std::ios::binary | std::ios::trunc);
  for (const std::string& l : c.lines) o << l;
  return static_cast<bool>(o);
}
bool is_key(const std::string& line, const std::string& key) { return line.rfind(key + "=", 0) == 0; }
}  // namespace

bool save_config_value(const std::string& write_dir, const std::string& key, const std::string& value) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  CfgLines c = read_cfg_lines(cfg);
  c.lines.erase(std::remove_if(c.lines.begin(), c.lines.end(), [&](const std::string& l) { return is_key(l, key); }), c.lines.end());
  // A last line with no ending gets the file's, so the next one starts its own.
  if (!c.lines.empty() && c.lines.back().back() != '\n') c.lines.back() += c.eol;
  c.lines.insert(c.lines.begin(), key + "=" + value + c.eol);
  return write_cfg_lines(cfg, c);
}

void apply_upgrades_config(const std::string& write_dir, const upgrades::Config& c, bool original) {
  upgrades::apply(c);
  if (upgrades::needs_migrating(c) && !write_dir.empty()) {
    for (const upgrades::Switch& sw : upgrades::switches())
      save_config_value(write_dir, std::string("upgrade_") + sw.key, c.bugfixes ? "1" : "0");
    remove_config_value(write_dir, "bugfixes");
  }
  if (original) zfix::set_upgrades(false);
}

bool remove_config_value(const std::string& write_dir, const std::string& key) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  CfgLines c = read_cfg_lines(cfg);
  const size_t before = c.lines.size();
  c.lines.erase(std::remove_if(c.lines.begin(), c.lines.end(), [&](const std::string& l) { return is_key(l, key); }), c.lines.end());
  if (c.lines.size() == before) return true;
  return write_cfg_lines(cfg, c);
}

namespace {
// Two buttons: true for Browse.
bool ask_browse(const std::string& text) {
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Browse..."},
  };
  SDL_MessageBoxData box{};
  box.flags = SDL_MESSAGEBOX_INFORMATION;
  box.title = "Find your MechWarrior 3 installation";
  box.message = text.c_str();
  box.numbuttons = 2;
  box.buttons = buttons;
  int pressed = 0;
  return SDL_ShowMessageBox(&box, &pressed) && pressed == 1;
}

// SDL's open-file dialog, waited on (its answer comes through a callback while
// events are pumped; on Windows from another thread). Empty when cancelled.
std::string pick_file(const std::string& start) {
  struct Wait {
    std::atomic<bool> done{false};
    std::string file;  // set before done
  } w;
  static const SDL_DialogFileFilter filters[] = {{"MechWarrior 3 (Mech3.exe)", "exe"}, {"All files", "*"}};
  auto done = [](void* user, const char* const* list, int) {
    auto* w = static_cast<Wait*>(user);
    if (list && list[0]) w->file = list[0];
    w->done = true;
  };
  sdlfe::open_picker({sdlfe::PickerKind::kOpenFile, done, &w, nullptr, filters, 2, start.empty() ? nullptr : start.c_str()});
  while (!w.done) {
    SDL_Event e;
    SDL_WaitEventTimeout(&e, 50);
  }
  return w.file;
}
}  // namespace

std::string find_install(const std::string& configured, const std::vector<std::string>& more, bool may_ask,
                         bool* found_by_search) {
  if (found_by_search) *found_by_search = false;
  if (install_valid(configured)) return configured;
  if (found_by_search) *found_by_search = true;
  std::vector<std::string> all = install_candidates();
  all.insert(all.end(), more.begin(), more.end());
  for (const std::string& d : all)
    if (install_valid(d)) return d;
  if (!may_ask) return {};
  std::string text =
      "MechWarrior 3 needs the files of an installed MechWarrior 3, any version (its zbd folder).\n\n"
      "Choose Mech3.exe in your MechWarrior 3 folder.";
  if (!configured.empty()) text = "No MechWarrior 3 install at \"" + configured + "\" any more.\n\n" + text;
  std::string start;
  while (ask_browse(text)) {
    const std::string file = pick_file(start);
    if (file.empty()) return {};  // cancelled: quit
    const std::string dir = install_of_pick(file);
    std::string missing;
    if (install_valid(dir, &missing)) return dir;
    start = dir;
    text = "\"" + dir + "\" is not a MechWarrior 3 install: " + missing +
           (missing.find(" and ") != std::string::npos ? " are" : " is") + " missing there.\n\nChoose Mech3.exe in the folder MechWarrior 3 was installed to "
           "(the one that holds zbd and video).";
  }
  return {};
}

}  // namespace sdlfe
