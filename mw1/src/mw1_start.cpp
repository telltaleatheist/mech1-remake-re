#include "mw1_start.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "gamez/zutil/zutl_path.h"
#include "mech3/data_folder.h"
#include "pm/pm_game.h"
#include "rekit/family_folder.h"

namespace fs = std::filesystem;

namespace mw1 {

bool records_program(const std::vector<std::string>& rest) {
  return std::none_of(rest.begin(), rest.end(), [](const std::string& s) { return s == "--hidden" || s == "--status"; });
}

Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string s = argv[i];
    const bool more = i + 1 < argc;
    if (s == "--mw3-install" && more) a.given.mw3 = argv[++i];
    else if (s == "--mw1-game" && more) a.given.mw1 = argv[++i];
    else if (s == "--new-company") a.new_company = true;
    else if (s == "--mw1-worlds" && more) a.worlds = argv[++i];
    else if (s == "--with" && more) {  // the launcher's needs (kit contract: <id>=<where>, from each one's status)
      const std::string w = argv[++i];
      const size_t eq = w.find('=');
      const std::string id = w.substr(0, eq), where = eq == std::string::npos ? std::string() : w.substr(eq + 1);
      if (id == "mech3-pm") a.given.pm = where;
      else if (id == "mech3") a.given.mw3 = where;
      else if (id == "mech1") a.given.mw1 = where;
      else std::printf("mw1: --with %s: not a game the Remake needs\n", w.c_str());
    }
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
    number("save_slot", in.save_slot, 3);
    if (in.save_slot == 0) in.save_slot = -1;  // not a slot
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
  if (in.save_slot >= 1) f << "save_slot=" << in.save_slot << "\n";
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

namespace {

// Every file under `a` is under `b` with the same bytes (folders too); false on anything unreadable.
bool same_tree(const fs::path& a, const fs::path& b) {
  std::error_code ec;
  for (fs::recursive_directory_iterator it(a, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path there = b / fs::relative(it->path(), a, ec);
    if (ec) return false;
    if (it->is_directory(ec)) {
      if (!fs::is_directory(there, ec)) return false;
      continue;
    }
    if (fs::file_size(it->path(), ec) != fs::file_size(there, ec) || ec) return false;
    std::ifstream x(it->path(), std::ios::binary), y(there, std::ios::binary);
    if (!x || !y) return false;
    std::vector<char> bx(65536), by(65536);
    while (x && y) {
      x.read(bx.data(), static_cast<std::streamsize>(bx.size())), y.read(by.data(), static_cast<std::streamsize>(by.size()));
      if (x.gcount() != y.gcount() || !std::equal(bx.data(), bx.data() + x.gcount(), by.data())) return false;
    }
    if (x.bad() || y.bad()) return false;
  }
  return !ec;
}

// `from` copied to `to` (made): folders made plain and files owner-writable, so a read-only old folder still gives
// a write dir the game can write in (fs::copy would carry the folders' permissions over and fail inside them).
void copy_tree(const fs::path& from, const fs::path& to, std::error_code& ec) {
  if (!fs::create_directory(to, ec) || ec) return;
  // Each step's error ends the copy at once, before the iterator's increment(ec) could clear it.
  for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path there = to / fs::relative(it->path(), from, ec);
    if (ec) return;
    const bool dir = it->is_directory(ec);
    if (ec) return;
    if (dir) {
      fs::create_directory(there, ec);
      if (ec) return;
      continue;
    }
    fs::copy_file(it->path(), there, ec);
    if (ec) return;
    fs::permissions(there, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add, ec);
    if (ec) return;
  }
}

// After a rename: the folders and files the owner can write (a read-only old folder made a write dir again).
void make_writable(const fs::path& d) {
  std::error_code ec;
  const fs::perms dir = fs::perms::owner_all, file = fs::perms::owner_read | fs::perms::owner_write;
  fs::permissions(d, dir, fs::perm_options::add, ec);
  for (fs::recursive_directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    fs::permissions(it->path(), it->is_directory(e2) ? dir : file, fs::perm_options::add, e2);
  }
}

bool empty_dir(const fs::path& d) {
  std::error_code ec;
  return fs::is_directory(d, ec) && fs::directory_iterator(d, ec) == fs::directory_iterator() && !ec;
}

}  // namespace

WriteDirMove move_write_dir(const std::string& old_dir, const std::string& new_dir, bool try_rename) {
  WriteDirMove r;
  r.use = new_dir;
  std::error_code ec;
  const fs::path from(old_dir), to(new_dir);
  if (old_dir.empty() || new_dir.empty() || !fs::is_directory(from, ec) || fs::equivalent(from, to, ec)) return r;
  if (fs::exists(to, ec) && !empty_dir(to)) return r.how = Moved::kBothKept, r;
  auto failed = [&](const std::string& why) {
    r.how = Moved::kFailed, r.use = old_dir, r.why = why;
    return r;
  };
  if (fs::exists(to, ec) && !fs::remove(to, ec)) return failed("the new folder could not be replaced: " + ec.message());
  if (try_rename) {
    fs::rename(from, to, ec);
    if (!ec) return make_writable(to), r.how = Moved::kRenamed, r;
  }
  // The copy, beside the new one; one left by an earlier failed move is ours and goes first.
  const fs::path part = to.string() + ".moving";
  fs::remove_all(part, ec);
  ec.clear();
  copy_tree(from, part, ec);
  if (ec || !same_tree(from, part)) {
    const std::string why = ec ? "the copy failed: " + ec.message() : "the copy did not match";
    fs::remove_all(part, ec);
    fs::create_directories(to, ec);  // as SDL left it
    return failed(why);
  }
  fs::rename(part, to, ec);
  if (ec) {
    const std::string why = "the copy could not be renamed: " + ec.message();
    fs::remove_all(part, ec);
    fs::create_directories(to, ec);
    return failed(why);
  }
  r.how = Moved::kCopied;
  // Only now, the copy checked and in place: the old tree's read-only bits cleared (Windows' remove_all doesn't, and
  // stops at the first read-only file), then the old folder removed. What can't be removed is left and said.
  make_writable(from);
  fs::remove_all(from, ec);
  if (ec) r.why = "the old folder could not be removed: " + ec.message();
  return r;
}

namespace {
// Something in `d` besides what a move leaves (the pointer back) and the system's .DS_Store.
bool holds_data(const fs::path& d) {
  std::error_code ec;
  if (!fs::is_directory(d, ec)) return false;
  for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name != ".DS_Store" && name != mech3::data_folder::kPointer) return true;
  }
  return false;
}
}  // namespace

UserFolder settle_user_folder(const std::string& usual, bool move) {
  UserFolder u;
  u.use = usual;
  const std::string old_remake = rekit::old_org_folder(usual);
  if (usual.empty() || old_remake.empty()) return u;
  const std::string critical = (fs::path(old_remake).parent_path() / kOldApp).string();
  std::error_code ec;
  if (!move) {
    if (!holds_data(usual)) {
      if (holds_data(old_remake) && !fs::exists(fs::path(old_remake) / mech3::data_folder::kPointer, ec)) u.use = old_remake;
      else if (holds_data(critical)) u.use = critical;
    }
    return u;
  }
  // 1. FIXES 139's move first, inside the old family folder.
  if (fs::is_directory(critical, ec) && !holds_data(usual)) {
    const WriteDirMove m = move_write_dir(critical, old_remake);
    if (m.how != Moved::kNothing)
      u.said.push_back("the write dir's old folder " + critical + ": " + moved_words(m.how) + (m.why.empty() ? "" : ", " + m.why));
    if (m.how == Moved::kBothKept) u.said.push_back(old_remake + " is used; the old folder is left as it is");
    if (m.how == Moved::kFailed) {
      u.said.push_back("the old folder is used for this run");
      u.use = critical;
      return u;
    }
  }
  // 2. The family folder: MicroProse's renamed to BattleTech's once, a pointer left behind.
  const rekit::FolderMove f = rekit::move_once(old_remake, usual, {mech3::data_folder::kPointer});
  if (f.kind == rekit::FolderMove::kFailed) {
    u.said.push_back("the family folder: " + f.error + "; " + old_remake + " is used for this run (tried again at the next start)");
    u.use = old_remake;
    return u;
  }
  if (f.kind == rekit::FolderMove::kMoved) {
    make_writable(usual);
    fs::create_directories(old_remake, ec);
    std::ofstream pointer(fs::path(old_remake) / mech3::data_folder::kPointer, std::ios::binary);
    pointer << usual << "\n";
    pointer.close();
    u.said.push_back("the family folder: " + old_remake + " moved to " + usual +
                     (pointer ? " (a pointer left there)" : " (no pointer could be left there)"));
  }
  return u;
}

std::vector<std::string> moved_install_places(const std::string& saved) {
  std::vector<std::string> out;
  if (saved.empty()) return out;
  const fs::path path = fs::path(saved).lexically_normal();
  // The nearest <base>/MicroProse/<Game> holding the path.
  for (fs::path game = path; game.has_relative_path() && game != game.parent_path(); game = game.parent_path()) {
    const fs::path org = game.parent_path();
    if (org.filename() != kOldOrg || org == org.parent_path()) continue;
    const fs::path rest = path.lexically_relative(game);
    auto inside = [&rest](const fs::path& d) { return (rest.empty() || rest == "." ? d : d / rest).string(); };
    out.push_back(inside(org.parent_path() / kOrg / game.filename()));
    std::ifstream f(game / mech3::data_folder::kPointer);
    std::string line;
    if (f && std::getline(f, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
      if (!line.empty()) out.push_back(inside(fs::path(zutl::path_from_config(line))));
    }
    break;
  }
  return out;
}

std::string follow_install(const std::string& dir, bool (*holds)(const std::string&)) {
  if (dir.empty() || holds(dir)) return dir;
  for (const std::string& place : moved_install_places(dir))
    if (holds(place)) return place;
  return dir;
}

bool is_pm_install(const std::string& dir) { return !dir.empty() && pm::install_kind(dir) == pm::Install::kPm; }

rekit::Status status(const std::string& folder, const Installs& given, const std::string& version) {
  rekit::Status s;
  s.game = kStatusId, s.version = version;
  // The other two games are its needs, given by the launcher (--with), which knows their own statuses; MechWarrior
  // (1989)'s files are its own (its served setup, mw1_game=): the state, where and missing are theirs alone (missing
  // means not ready, launcher split b2/b5). A need given that does not hold its game is only logged.
  s.needs = {"mech3-pm", "mech3"};
  s.setup = rekit::kServedSetup;  // MechWarrior (1989)'s files: mw1 --setup <ask> (mw1_recipe.h)
  s.play = rekit::kPlayOptions;   // PM's main takes the shared start options and --ready-file
  s.user = folder;
  const Installs use = merge(given, read_cfg(folder));
  auto held = [](const std::string& dir, bool (*holds)(const std::string&)) {
    const std::string at = follow_install(dir, holds);
    return holds(at) ? at : std::string();
  };
  if (!given.pm.empty() && held(given.pm, is_pm_install).empty())
    std::fprintf(stderr, "mw1: status: --with mech3-pm=%s does not hold Pirate's Moon\n", given.pm.c_str());
  if (!given.mw3.empty() && held(given.mw3, is_mw3_install).empty())
    std::fprintf(stderr, "mw1: status: --with mech3=%s does not hold MechWarrior 3\n", given.mw3.c_str());
  s.where = held(use.mw1, is_mw1_install);
  s.state = s.where.empty() ? rekit::Status::kMissing : rekit::Status::kReady;
  if (s.where.empty()) s.missing.push_back("MechWarrior (1989)");
  return s;
}

const char* moved_words(Moved how) {
  switch (how) {
    case Moved::kNothing: return "nothing to move";
    case Moved::kRenamed: return "renamed";
    case Moved::kCopied: return "copied and checked";
    case Moved::kBothKept: return "both folders kept";
    case Moved::kFailed: return "not moved";
  }
  return "";
}

}  // namespace mw1
