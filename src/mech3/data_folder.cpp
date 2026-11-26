#include "mech3/data_folder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "gamez/zutil/zutl_path.h"
#include "rekit/family_folder.h"

namespace fs = std::filesystem;

// The setup kit's since 2026-11-25 (rekit/data_folder.h), told the game's name and settings file; the port's calls
// kept. Pirate's Moon's nested folder (adopt_nested) stays here: it is this family's own history.
namespace mech3::data_folder {

namespace kit = rekit::data_folder;

namespace {

const kit::Game& game() {
  static const kit::Game g = [] {
    kit::Game k;
    k.title = "MechWarrior 3", k.configs = {"mw3.cfg"}, k.log_prefix = "mw3: ";
    k.plain_names_only = !zutl::narrow_paths_utf8();  // before Windows 10 1903 (docs/program.md, The data folder)
    k.path_from_text = [](const std::string& s) { return zutl::path_from_config(s); };
    return k;
  }();
  return g;
}

bool same_bytes(const fs::path& a, const fs::path& b) {
  std::ifstream x(a, std::ios::binary), y(b, std::ios::binary);
  if (!x || !y) return false;
  std::vector<char> p(1 << 20), q(1 << 20);
  for (;;) {
    x.read(p.data(), static_cast<std::streamsize>(p.size()));
    y.read(q.data(), static_cast<std::streamsize>(q.size()));
    if (x.gcount() != y.gcount() || std::memcmp(p.data(), q.data(), static_cast<size_t>(x.gcount())) != 0) return false;
    if (!x || !y) return x.eof() && y.eof();
  }
}

bool write_text(const fs::path& p, const std::string& text) {
  std::ofstream o(p, std::ios::binary | std::ios::trunc);
  o << text;
  o.flush();
  return static_cast<bool>(o);
}

const char kAdopted[] = "moved-to-BattleTech.txt";  // left in a nested folder once its data is moved

// A pilot, or a saved game, in the folder's pilots.
bool has_pilots(const fs::path& dir) {
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir / "pilots", ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && !kit::not_data(it->path().filename().string())) return true;
  return false;
}

struct Adopt {
  bool swap = false;    // `to` is unused: a file in both changes places
  int moved = 0, failed = 0;
  void folder(const fs::path& from, const fs::path& to, bool top, bool whole) {
    std::error_code ec;
    std::vector<fs::path> names;
    for (fs::directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) names.push_back(it->path().filename());
    failed += static_cast<bool>(ec);
    std::sort(names.begin(), names.end(), [](const fs::path& a, const fs::path& b) {  // the pilots first
      return (a == "pilots") != (b == "pilots") ? a == "pilots" : a < b;
    });
    for (const fs::path& n : names) {
      if (top && (kit::not_data(n.string()) || n == kAdopted)) continue;
      const fs::path f = from / n, t = to / n;
      std::error_code e;
      const bool dir = fs::is_directory(f, e) && !fs::is_symlink(f, e);
      if (!fs::exists(t, e) || (dir && fs::is_directory(t, e) && fs::is_empty(t, e))) {
        if (fs::exists(t, e)) fs::remove(t, e);  // an empty folder only
        fs::rename(f, t, e);
        e ? ++failed : ++moved;
        if (e && n == "pilots") return;  // nothing without them: again at the next start
      } else if (dir && fs::is_directory(t, e)) {
        // A game's files, a merged set and a mod each stay whole.
        if (!whole && n != "install" && n != "mod-merged") folder(f, t, false, n == "mods");
        rekit::remove_if_empty(f.string());
      } else if (swap && !dir && fs::is_regular_file(t, e) && !same_bytes(f, t)) {
        const fs::path away = to / (n.string() + ".swap");
        fs::rename(t, away, e);
        if (e) {
          ++failed;
          continue;
        }
        fs::rename(f, t, e);
        if (e) {
          fs::rename(away, t, e), ++failed;
          continue;
        }
        fs::rename(away, f, e);
        ++moved;
      }
    }
  }
};

}  // namespace

int adopt_nested(const std::string& usual) {
  const fs::path to(usual), org = to.parent_path();
  std::error_code ec;
  if (org.filename() != rekit::kFamilyOrg || fs::exists(to / kPointer, ec)) return 0;
  int all = 0;
  std::vector<std::pair<std::string, std::string>> places;
  for (const fs::path& base : {org, org.parent_path() / rekit::kOldOrg}) {
    const fs::path from = base / "MechWarrior 3" / to.filename();
    if (!fs::is_directory(from, ec) || fs::exists(from / kAdopted, ec) || fs::exists(from / kPointer, ec) || !kit::holds_data(from.string())) continue;
    Adopt a;
    a.swap = has_pilots(from) && !has_pilots(to);
    fs::create_directories(to, ec);
    a.folder(from, to, true, false);
    std::printf("mw3: Pirate's Moon's folder: %d of its things moved from %s to %s%s\n", a.moved, from.string().c_str(), usual.c_str(),
                a.failed ? " (some could not be: tried again at the next start)" : "");
    all += a.moved;
    places.emplace_back(from.string(), usual);
    if (!a.failed) {
      write_text(from / kAdopted, "Pirate's Moon's pilots, mods and settings are now in " + usual + ".\n"
                                  "What is still here was in both places: the game does not use it.\n");
      rekit::remove_if_empty(from.string());
    }
  }
  if (all)  // the saved paths of the game's files that moved with them
    for (fs::directory_iterator it(org, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory()) {
        const fs::path dir = resolve(it->path().string()).dir;
        for (const char* cfg : {"mw3.cfg", "mw3pm.cfg", "mw1.cfg"}) retarget_config((dir / cfg).string(), places);
      }
  return all;
}

Resolved resolve(const std::string& usual) { return kit::resolve(usual, game()); }
std::string unusable(const std::string& dir) { return kit::unusable(dir, game()); }
std::string destination(const std::string& picked, const std::string& usual, const char* name) {
  return kit::destination(picked, usual, name);
}
Plan plan(const std::string& from, const std::string& to, const std::string& usual) { return kit::plan(from, to, usual, game()); }
Moved move_data(const std::string& from, const std::string& to, const std::string& usual,
                const std::function<bool(double, const std::string&)>& progress) {
  return kit::move_data(from, to, usual, game(), progress);
}
Removed remove_old(const std::string& from, const std::string& to, const std::vector<std::string>& copied,
                   const std::string& usual) {
  return kit::remove_old(from, to, copied, usual, game());
}
bool plain_name(const std::string& path) { return kit::plain_name(path); }
const uint64_t*& free_for_test() { return kit::free_for_test(); }
std::function<void()>& after_switch_for_test() { return kit::after_switch_for_test(); }
std::string moved_path(const std::string& path, const std::string& from, const std::string& to) {
  return kit::moved_path(path, from, to);
}
void retarget_config(const std::string& cfg, const std::vector<std::pair<std::string, std::string>>& moved) {
  kit::retarget_config(cfg, moved);
}
std::string family_move(const std::string& usual, bool move) { return kit::family_move(usual, game(), move); }

}  // namespace mech3::data_folder
