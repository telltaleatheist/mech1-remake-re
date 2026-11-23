#include "frontend/sdl/sdl_games.h"

#include "frontend/sdl/sdl_update.h"
#include "mech3/patch_import.h"
#include "rekit/archive.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace sdlfe::games {
namespace {

constexpr const char* kRootMark = "rebuild-games.txt";   // the games folder is the launcher's
constexpr const char* kStagingMark = ".rebuild-staging";  // a folder being filled, the launcher's
constexpr const char* kGameFolderMark = "rebuild-game-folder.txt";  // a game's own folder (game_folder_in)
constexpr const char* kIncoming = ".incoming";             // where a package is unpacked before it goes in place

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool ends_with(const std::string& s, const char* e) {
  const size_t n = std::strlen(e);
  return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}
std::string unique_part() {
  static int n = 0;
  return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++n);
}
std::string read_text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
bool write_text(const fs::path& p, const std::string& text) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << text;
  return static_cast<bool>(f);
}
bool allow_file() { return std::getenv("MW3_GAMES_API") != nullptr; }  // checks: file:// releases and downloads

// The games folder made (or found) as the launcher's: an empty or new folder gets the mark; one holding anything
// else without it is someone's own and is never written to.
bool own_root(const std::string& root, std::string* error) {
  std::error_code ec;
  if (root.empty()) return *error = "there is no games folder", false;
  fs::create_directories(root, ec);
  if (fs::exists(fs::path(root) / kRootMark, ec)) return true;
  if (!fs::is_directory(root, ec)) return *error = "the games folder " + root + " could not be made", false;
  if (!fs::is_empty(root, ec)) return *error = root + " holds files that are not the launcher's: move them and try again", false;
  if (!write_text(fs::path(root) / kRootMark, "# The MechWarrior 3 Rebuild launcher's games. Each folder is one game it installed.\n"))
    return *error = "the games folder " + root + " could not be written to", false;
  return true;
}

// A leftover of an earlier try, removed only when it carries `mark` (else it is in the way and is not ours).
bool clear_ours(const fs::path& p, const char* mark, std::string* error) {
  std::error_code ec;
  if (!fs::exists(fs::symlink_status(p, ec))) return true;
  if (!fs::is_directory(fs::symlink_status(p, ec)) || !fs::exists(p / mark, ec))
    return *error = p.string() + " is in the way and is not the launcher's: move it and try again", false;
  fs::remove_all(p, ec);
  if (ec) return *error = "an earlier try's " + p.string() + " could not be removed (" + ec.message() + ")", false;
  return true;
}

#if !defined(_WIN32) && !defined(__APPLE__)
// A program run to its end in `cwd` (an AppImage's --appimage-extract writes where it runs).
int run_in(const std::vector<std::string>& args, const std::string& cwd, int seconds, std::string* said) {
  std::vector<const char*> argv;
  for (const std::string& a : args) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, cwd.c_str());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return -1;
  int code = -1;
  bool ended = false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000;
  while (!(ended = SDL_WaitProcess(p, false, &code)) && SDL_GetTicks() < until) {
    char buf[4096];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    if (said->size() < 4096) said->append(buf, n);  // its first lines; the rest read and dropped (a full pipe stalls it)
    if (n == 0) SDL_Delay(20);
  }
  if (!ended) SDL_KillProcess(p, true), SDL_WaitProcess(p, true, &code), code = -1;
  SDL_DestroyProcess(p);
  return code;
}
#endif

// A zip's files written under `into`; the folder every entry shares (the package's own top folder) left out.
bool unzip(const std::string& file, const fs::path& into, const Progress& progress, std::string* error) {
  if (!rekit::archives_supported()) return *error = "this build cannot read zip files", false;
  std::error_code ec;
  const double total = static_cast<double>(std::max<uintmax_t>(1, fs::file_size(file, ec)));
  std::string top;
  bool one_top = true, any = false;
  {  // the shared top folder, from the listing
    rekit::ArchiveReader r;
    if (!r.open_file(file)) return *error = "the zip could not be opened (" + r.error() + ")", false;
    rekit::ArchiveReader::Entry e;
    while (r.next(&e)) {
      const std::string first = e.path.substr(0, e.path.find('/'));
      const bool deeper = e.path.find('/') != std::string::npos && e.path.find('/') + 1 < e.path.size();
      if (!any) top = first;
      any = true;
      if (first != top || (!deeper && !e.dir)) one_top = false;
    }
    if (!r.error().empty()) return *error = "the zip could not be read (" + r.error() + ")", false;
  }
  if (!any) return *error = "the zip is empty", false;
  rekit::ArchiveReader r;
  if (!r.open_file(file)) return *error = "the zip could not be opened (" + r.error() + ")", false;
  rekit::ArchiveReader::Entry e;
  std::vector<uint8_t> buf(1 << 20);
  while (r.next(&e)) {
    std::string rel = e.path;
    if (one_top) rel = rel.size() > top.size() + 1 ? rel.substr(top.size() + 1) : std::string();
    while (!rel.empty() && rel.back() == '/') rel.pop_back();
    if (rel.empty() || e.other) continue;  // the top folder itself; links and devices are never written
    // Every part a plain name: nothing written outside `into`.
    const fs::path relp = fs::path(rel);
    bool safe = !relp.is_absolute() && !relp.has_root_name() && rel.find('\\') == std::string::npos && rel.find(':') == std::string::npos;
    for (const fs::path& part : relp) safe = safe && part != ".." && part != "." && !part.empty();
    if (!safe) return *error = "the zip names a file outside its folder (" + e.path + "): not installed", false;
    const fs::path to = into / relp;
    if (e.dir) {
      fs::create_directories(to, ec);
      continue;
    }
    fs::create_directories(to.parent_path(), ec);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) return *error = "could not write " + to.string(), false;
    for (int64_t n; (n = r.read(buf.data(), buf.size())) > 0;) out.write(reinterpret_cast<const char*>(buf.data()), n);
    if (!out || !r.error().empty()) return *error = "could not unpack " + e.path + (r.error().empty() ? "" : " (" + r.error() + ")"), false;
    if (progress && !progress(static_cast<double>(r.position()) / total, "Unpacking")) return *error = "cancelled", false;
  }
  if (!r.error().empty()) return *error = "the zip could not be read (" + r.error() + ")", false;
  return true;
}

#if defined(__APPLE__)
// The disk image attached read-only where nothing shows it, every app on it copied into `into`, detached.
bool from_dmg(const std::string& file, const fs::path& into, std::string* error) {
  std::error_code ec;
  const fs::path mnt = fs::temp_directory_path(ec) / ("MW3-Rebuild-game-mount-" + unique_part());
  fs::create_directories(mnt, ec);
  std::string said;
  if (run_program({"/usr/bin/hdiutil", "attach", "-nobrowse", "-readonly", "-noautoopen", "-mountpoint", mnt.string(), file}, 120, &said) != 0) {
    fs::remove(mnt, ec);
    return *error = "the disk image could not be opened", false;
  }
  bool any = false;
  std::string why;
  for (const auto& e : fs::directory_iterator(mnt, ec)) {
    if (e.path().extension() != ".app" || !fs::is_directory(e.path(), ec)) continue;
    std::error_code cec;
    fs::copy(e.path(), into / e.path().filename(), fs::copy_options::recursive | fs::copy_options::copy_symlinks, cec);
    if (cec) why = cec.message();
    else any = true;
  }
  std::string detach_said;
  run_program({"/usr/bin/hdiutil", "detach", mnt.string(), "-force"}, 60, &detach_said);
  fs::remove(mnt, ec);
  if (!why.empty()) return *error = "the app could not be copied off the disk image (" + why + ")", false;
  if (!any) return *error = "the disk image holds no app", false;
  return true;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
// The AppImage unpacked by itself (squashfs-root in `into`), no FUSE needed; the copy run is removed after.
bool from_appimage(const std::string& file, const fs::path& into, std::string* error) {
  std::error_code ec;
  const fs::path copy = into / "package.AppImage";
  fs::copy_file(file, copy, fs::copy_options::overwrite_existing, ec);
  if (ec) return *error = "the AppImage could not be copied (" + ec.message() + ")", false;
  fs::permissions(copy, fs::perms::owner_exec | fs::perms::owner_read, fs::perm_options::add, ec);
  std::string said;
  const int code = run_in({copy.string(), "--appimage-extract"}, into.string(), 600, &said);
  fs::remove(copy, ec);
  if (code != 0 || !fs::is_directory(into / "squashfs-root", ec)) {
    while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) said.pop_back();
    std::printf("mw3: launcher: games: --appimage-extract said: %s\n", said.c_str());
    return *error = "the AppImage could not be unpacked", false;
  }
  return true;
}
#endif

// A package unpacked into `into` (made, marked as the launcher's): a zip anywhere, a .dmg on the Mac, an
// .AppImage on Linux.
bool unpack(const std::string& file, const fs::path& into, const Progress& progress, std::string* error) {
  std::error_code ec;
  fs::create_directories(into, ec);
  if (!write_text(into / kStagingMark, "being installed by the launcher\n")) return *error = "could not write in " + into.string(), false;
  const std::string name = lower(fs::path(file).filename().string());
  if (ends_with(name, ".zip")) return unzip(file, into, progress, error);
#if defined(__APPLE__)
  if (ends_with(name, ".dmg")) return from_dmg(file, into, error);
#elif !defined(_WIN32)
  if (ends_with(name, ".appimage")) return from_appimage(file, into, error);
#endif
#if defined(_WIN32)
  return *error = "the launcher installs a game's .zip on Windows (not its Setup): " + fs::path(file).filename().string(), false;
#elif defined(__APPLE__)
  return *error = "the launcher installs a game's .dmg on the Mac: " + fs::path(file).filename().string(), false;
#else
  return *error = "the launcher installs a game's .AppImage on Linux: " + fs::path(file).filename().string(), false;
#endif
}

// The unpacked folder put in place of the game's: the old one (the launcher's, by its record) moved aside, the new
// one renamed in, the old removed last; a failure puts the old one back.
bool put_in_place(const std::string& root, u::Game g, const fs::path& staged, std::string* error) {
  std::error_code ec;
  const fs::path to = fs::path(root) / u::game_info(g).key, old = fs::path(root) / ("." + std::string(u::game_info(g).key) + ".old");
  if (!clear_ours(old, u::kInstalledName, error)) return false;
  const bool had = fs::exists(fs::symlink_status(to, ec));
  if (had && (!fs::is_directory(fs::symlink_status(to, ec)) || !fs::exists(to / u::kInstalledName, ec)))
    return *error = to.string() + " is not a game the launcher installed: move it and try again", false;
  if (had) {
    fs::rename(to, old, ec);
    if (ec) return *error = "the installed copy could not be moved aside (" + ec.message() + "): close the game and try again", false;
  }
  fs::rename(staged, to, ec);
  if (ec) {
    const std::string why = ec.message();
    std::error_code back;
    if (had) fs::rename(old, to, back);
    return *error = "the new copy could not be put in place (" + why + ")" + (had ? "; the installed one is kept" : ""), false;
  }
  fs::remove(to / kStagingMark, ec);
  if (had) fs::remove_all(old, ec);  // a failure leaves a stale .old, cleared by the next install
  return true;
}

void make_runnable(const std::string& program) {
#if !defined(_WIN32)
  std::error_code ec;
  if (!program.empty())
    fs::permissions(program, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
#else
  (void)program;
#endif
}

}  // namespace

std::string root(bool hidden) {
  if (const char* d = std::getenv("MW3_GAMES_DIR")) return d;
  if (hidden) return {};
#if defined(_WIN32)
  const char* base = std::getenv("LOCALAPPDATA");
  if (!base || !*base) return {};
  return (fs::path(base) / "MicroProse" / "BattleTech" / "games").string();
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home) return {};
  return (fs::path(home) / "Library" / "Application Support" / "MicroProse" / "BattleTech" / "games").string();
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  const char* home = std::getenv("HOME");
  const fs::path base = xdg && *xdg ? fs::path(xdg) : home && *home ? fs::path(home) / ".local" / "share" : fs::path();
  if (base.empty()) return {};
  return (base / "MicroProse" / "BattleTech" / "games").string();
#endif
}

std::string program_in(const std::string& dir, u::Game g) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return {};
  const std::string prog = u::game_info(g).program;
#if defined(_WIN32)
  const std::string file = prog + ".exe";
#else
  const std::string file = prog;
#endif
  for (const fs::path& p : {fs::path(dir) / file, fs::path(dir) / "usr" / "bin" / file, fs::path(dir) / "squashfs-root" / "usr" / "bin" / file})
    if (fs::is_regular_file(p, ec)) return p.string();
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ".app") {
      const fs::path p = e.path() / "Contents" / "MacOS" / file;
      if (fs::is_regular_file(p, ec)) return p.string();
    }
  return {};
}

bool installed(const std::string& root, u::Game g, u::InstalledGame* record, std::string* program) {
  if (root.empty()) return false;
  const fs::path dir = fs::path(root) / u::game_info(g).key;
  std::error_code ec;
  if (!fs::is_regular_file(dir / u::kInstalledName, ec)) return false;
  u::InstalledGame rec;
  if (!u::parse_installed(read_text(dir / u::kInstalledName), &rec) || rec.game != u::game_info(g).key) return false;
  const std::string prog = program_in(dir.string(), g);
  if (prog.empty()) return false;
  if (record) *record = rec;
  if (program) *program = prog;
  return true;
}

std::string program_version(const std::string& bin) {
  const std::string tag = std::string("MW3-PROGRAM") + "-VERSION:";  // not whole in this file's own strings
  std::FILE* f = std::fopen(bin.c_str(), "rb");
  if (!f) return {};
  std::string buf, out;
  std::vector<char> chunk(1 << 20);
  for (size_t n; out.empty() && (n = std::fread(chunk.data(), 1, chunk.size(), f)) > 0;) {
    buf.append(chunk.data(), n);
    for (size_t at = buf.find(tag); at != std::string::npos; at = buf.find(tag, at + 1)) {
      const size_t from = at + tag.size(), end = buf.find('\0', from);
      if (end == std::string::npos) break;  // its end in the next chunk
      const std::string v = buf.substr(from, end - from);
      if (!v.empty() && v.size() < 64 && std::all_of(v.begin(), v.end(), [](char c) { return c > ' ' && c < 127; })) {
        out = v;
        break;
      }
    }
    if (buf.size() > 256) buf.erase(0, buf.size() - 256);  // a tag across the chunks' edge
  }
  std::fclose(f);
  return out;
}

u::GameCheck check(u::Game g, const std::string& installed_tag) {
  const std::string repo = u::game_info(g).repo;
  std::string url = std::string("https://api.github.com/repos/telltaleatheist/") + repo + "/releases?per_page=20";
  if (const char* api = std::getenv("MW3_GAMES_API")) {
    url = api;
    if (const size_t at = url.find("{repo}"); at != std::string::npos) url.replace(at, 6, repo);
  }
  u::Answer a;
  curl_get(url, 15, &a.json, &a.fetch_error, allow_file());
  std::printf("mw3: launcher: games: %s at %s: %s\n", u::game_info(g).key, url.c_str(), a.json.empty() ? a.fetch_error.c_str() : "answered");
  return u::judge_game(g, installed_tag, a, u::this_platform());
}

Done install_release(const std::string& root, u::Game g, const u::Release& r, const Progress& progress) {
  Done d;
  d.game = g;
  const std::string name = u::game_info(g).name, version = u::game_version(r.tag);
  std::string error;
  if (!own_root(root, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  const u::Asset* file = u::game_asset(g, r, u::this_platform());
  if (!file) return d.words = "This release of " + name + " has no file for this computer.", d;
  const u::Asset* sums = nullptr;
  for (const u::Asset& a : r.assets)
    if (a.name == "SHA256SUMS") sums = &a;
  if (!sums) return d.words = "This release of " + name + " has no SHA256SUMS to check the download against, so nothing was downloaded.", d;
  std::string list;
  if (!curl_get(sums->url, 60, &list, &error, allow_file())) return d.words = "Could not download the release's SHA256SUMS: " + error + ".", d;
  const std::string want = u::sums_line(list, file->name);
  if (want.empty()) return d.words = "The release's SHA256SUMS has no line for " + file->name + ", so nothing was downloaded.", d;
  std::error_code ec;
  const fs::path dl_dir = fs::temp_directory_path(ec) / "MW3-Rebuild-games";  // ours alone
  fs::create_directories(dl_dir, ec);
  const std::string path = (dl_dir / file->name).string();
  bool cancelled = false;
  if (!curl_download(file->url, path, file->size, [&](double x) { return !progress || progress(x * 0.8, "Downloading " + name + " " + version); },
                     &error, &cancelled, allow_file())) {
    d.cancelled = cancelled;
    return d.words = cancelled ? "The download was cancelled. Nothing was installed." : "The download stopped: " + error + ". Nothing was installed.", d;
  }
  const std::string got = mech3::sha256_file_hex(path);
  if (got != want) {
    fs::remove(path, ec);
    std::printf("mw3: launcher: games: %s is %s, SHA256SUMS says %s\n", file->name.c_str(), got.c_str(), want.c_str());
    return d.words = "The download did not match its checksum, so nothing was installed. Try again; if it happens again, tell us.", d;
  }
  const fs::path staged = fs::path(root) / kIncoming;
  if (!clear_ours(staged, kStagingMark, &error)) return fs::remove(path, ec), d.words = "Nothing was installed: " + error + ".", d;
  if (progress) progress(0.85, "Installing " + name + " " + version);
  if (!unpack(path, staged, nullptr, &error)) {
    fs::remove(path, ec);
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = "Nothing was installed: " + error + ".", d;
  }
  fs::remove(path, ec);
  const std::string inside = program_in(staged.string(), g);
  if (inside.empty()) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = file->name + " does not hold " + name + "'s program, so nothing was installed.", d;
  }
  make_runnable(inside);
  if (!write_text(staged / u::kInstalledName, u::installed_text({u::game_info(g).key, r.tag, file->name, got, "github"})) ||
      !put_in_place(root, g, staged, &error)) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    return d.words = "Nothing was installed: " + error + ".", d;
  }
  d.ok = true;
  d.program = program_in((fs::path(root) / u::game_info(g).key).string(), g);
  d.words = name + " " + version + " is installed.";
  return d;
}

FileCheck check_file(const std::string& file) {
  FileCheck c;
  c.sha256 = mech3::sha256_file_hex(file);
  std::error_code ec;
  const fs::path sums = fs::path(file).parent_path() / "SHA256SUMS";
  const std::string name = fs::path(file).filename().string();
  if (!fs::is_regular_file(sums, ec)) {
    c.words = "There is no SHA256SUMS beside " + name + ", so it can't be checked.";
    return c;
  }
  const std::string want = u::sums_line(read_text(sums), name);
  if (want.empty()) {
    c.words = "The SHA256SUMS beside " + name + " has no line for it, so it can't be checked.";
    return c;
  }
  c.kind = want == c.sha256 ? FileCheck::kMatches : FileCheck::kMismatch;
  c.words = c.kind == FileCheck::kMatches ? name + " matches its SHA256SUMS."
                                          : name + " does not match its SHA256SUMS: it is damaged or not the file it says. Nothing was installed.";
  return c;
}

Done install_file(const std::string& root, const std::string& file, const std::string& sha256, const Progress& progress) {
  Done d;
  std::string error;
  std::error_code ec;
  const std::string name = fs::path(file).filename().string();
  if (!fs::is_regular_file(file, ec)) return d.words = name + " is not there.", d;
  if (!own_root(root, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  const fs::path staged = fs::path(root) / kIncoming;
  if (!clear_ours(staged, kStagingMark, &error)) return d.words = "Nothing was installed: " + error + ".", d;
  if (progress) progress(0.1, "Unpacking " + name);
  auto fail = [&](const std::string& words) {
    std::string e2;
    clear_ours(staged, kStagingMark, &e2);
    d.words = words;
    return d;
  };
  if (!unpack(file, staged, [&](double x, const std::string& w) { return !progress || progress(0.1 + x * 0.8, w); }, &error))
    return fail("Nothing was installed: " + error + ".");
  // Which game: the one program of ours in it.
  std::vector<u::Game> found;
  for (u::Game g : u::kGames)
    if (!program_in(staged.string(), g).empty()) found.push_back(g);
  if (found.empty()) return fail(name + " holds none of the games (mw3, mw3pm or mw1), so nothing was installed.");
  if (found.size() > 1) return fail(name + " holds more than one game (the full package): pick one game's own download.");
  const u::Game g = found.front();
  d.game = g;
  const std::string prog = program_in(staged.string(), g);
  make_runnable(prog);
  std::string tag = program_version(prog);
  if (tag.empty()) tag = "unknown";
  if (!write_text(staged / u::kInstalledName, u::installed_text({u::game_info(g).key, tag, name, sha256, "file"})) ||
      !put_in_place(root, g, staged, &error))
    return fail("Nothing was installed: " + error + ".");
  d.ok = true;
  d.program = program_in((fs::path(root) / u::game_info(g).key).string(), g);
  d.words = std::string(u::game_info(g).name) + (tag == "unknown" ? std::string() : " " + u::game_version(tag)) + " is installed from " + name + ".";
  return d;
}

std::string game_folder_in(const std::string& picked, u::Game g) {
  static const char* const kNames[] = {"MechWarrior 3", "Pirate's Moon", "MechWarrior 1 Remake"};
  std::error_code ec;
  const fs::path p(picked), mark = p / kGameFolderMark;
  if (!fs::exists(p, ec) || fs::is_empty(p, ec)) return picked;
  if (fs::is_regular_file(mark, ec) && read_text(mark).find("game=" + std::string(u::game_info(g).key) + "\n") != std::string::npos) return picked;
  return (p / kNames[static_cast<int>(g)]).string();
}

bool claim_game_folder(const std::string& dir, u::Game g, std::string* error) {
  std::error_code ec;
  const fs::path mark = fs::path(dir) / kGameFolderMark;
  const std::string key = u::game_info(g).key;
  if (fs::is_regular_file(mark, ec))
    return read_text(mark).find("game=" + key + "\n") != std::string::npos ||
           (*error = dir + " is another game's folder: pick another", false);
  if (fs::exists(dir, ec) && !(fs::is_directory(dir, ec) && fs::is_empty(dir, ec)))  // the games folder itself included
    return *error = dir + " holds files that are not this game's: pick an empty folder", false;
  if (!own_root(dir, error)) return false;  // new or empty: the games folder's mark first
  if (!write_text(mark, "# A game's folder, made by the MechWarrior 3 Rebuild launcher or the game's setup: its program and its game files.\ngame=" + key + "\n"))
    return *error = dir + " could not be written to", false;
  return true;
}

bool in_own_mount(const std::string& bin) {
  const char* appdir = std::getenv("APPDIR");
  if (!std::getenv("APPIMAGE")) return false;
  if (!appdir || !*appdir) return true;  // an AppImage that does not say where it is mounted: as before, watched
  const std::string base = appdir;
  return bin.rfind(base, 0) == 0;
}

}  // namespace sdlfe::games
