#include "mw1_start.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "gamez/zutil/zutl_path.h"
#include "pm/pm_game.h"

namespace fs = std::filesystem;

namespace mw1 {

Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string s = argv[i];
    const bool more = i + 1 < argc;
    if (s == "--mw3-install" && more) a.given.mw3 = argv[++i];
    else if (s == "--mw1-game" && more) a.given.mw1 = argv[++i];
    else if (s == "--new-company") a.new_company = true;
    else if (s == "--mw1-worlds" && more) a.worlds = argv[++i];
    else {
      // The game main's own; --install and --write-dir read here as well.
      if (s == "--install" && more) a.given.pm = argv[i + 1];
      if (s == "--write-dir" && more) a.write_dir = argv[i + 1];
      a.rest.push_back(s);
      if ((s == "--install" || s == "--write-dir") && more) a.rest.emplace_back(argv[++i]);
    }
  }
  return a;
}

Installs read_cfg(const std::string& write_dir) {
  Installs in;
  if (write_dir.empty()) return in;
  std::ifstream f(fs::path(write_dir) / kCfg);
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto take = [&](const char* key, std::string& to) {
      const std::string k = std::string(key) + "=";
      if (line.rfind(k, 0) == 0) to = zutl::path_from_config(line.substr(k.size()));
    };
    take("install", in.pm), take("mw3_install", in.mw3), take("mw1_game", in.mw1);
    auto number = [&](const char* key, int32_t& to, int32_t most) {
      const std::string k = std::string(key) + "=";
      if (line.rfind(k, 0) != 0) return;
      char* end = nullptr;
      const long v = std::strtol(line.c_str() + k.size(), &end, 10);
      if (end != line.c_str() + k.size()) to = static_cast<int32_t>(std::clamp<long>(v, 0, most));
    };
    number("music", in.sound.music, 1), number("music_volume", in.sound.music_volume, 100);
    number("effects_volume", in.sound.effects_volume, 100);
  }
  return in;
}

bool write_cfg(const std::string& write_dir, const Installs& in) {
  if (write_dir.empty()) return false;
  std::ofstream f(fs::path(write_dir) / kCfg, std::ios::trunc);
  if (!in.pm.empty()) f << "install=" << in.pm << "\n";
  if (!in.mw3.empty()) f << "mw3_install=" << in.mw3 << "\n";
  if (!in.mw1.empty()) f << "mw1_game=" << in.mw1 << "\n";
  if (in.sound.music >= 0) f << "music=" << in.sound.music << "\n";
  if (in.sound.music_volume >= 0) f << "music_volume=" << in.sound.music_volume << "\n";
  if (in.sound.effects_volume >= 0) f << "effects_volume=" << in.sound.effects_volume << "\n";
  return static_cast<bool>(f);
}

Installs merge(const Installs& given, const Installs& saved) {
  Installs out = saved;
  if (!given.pm.empty()) out.pm = given.pm;
  if (!given.mw3.empty()) out.mw3 = given.mw3;
  if (!given.mw1.empty()) out.mw1 = given.mw1;
  return out;
}

bool is_mw3_install(const std::string& dir) { return !dir.empty() && pm::install_kind(dir) == pm::Install::kMw3; }

bool is_mw1_install(const std::string& dir) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return false;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    for (char& c : n) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    if (n == "BTECH.EXE") return true;
  }
  return false;
}

std::vector<std::string> game_args(const std::string& argv0, const Args& a, const Installs& use) {
  std::vector<std::string> out{argv0};
  bool write_dir = false, install = false;
  for (const std::string& s : a.rest) write_dir |= s == "--write-dir", install |= s == "--install";
  if (!write_dir && !a.write_dir.empty()) out.push_back("--write-dir"), out.push_back(a.write_dir);
  // A bare install-dir (mw3pm's first form) counts as given: nothing added over it.
  install |= !a.rest.empty() && !a.rest[0].empty() && a.rest[0][0] != '-';
  if (!install && !use.pm.empty()) out.push_back("--install"), out.push_back(use.pm);
  out.insert(out.end(), a.rest.begin(), a.rest.end());
  // PM's start movies (its logos and intro) left out: MW1 starts on its own menu (mw1_menu.h).
  if (std::find(a.rest.begin(), a.rest.end(), "-nofmv") == a.rest.end()) out.push_back("-nofmv");
  return out;
}

}  // namespace mw1
