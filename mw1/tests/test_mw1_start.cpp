// mw1's start (src/mw1_start.h): its own flags apart from the game's,
// mw1.cfg, which installs win, and the argv handed to PM's game main.
#include "check.h"
#include "mw1_start.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
mw1::Args parse(std::vector<std::string> v) {
  v.insert(v.begin(), "mw1");
  std::vector<char*> raw;
  for (std::string& s : v) raw.push_back(s.data());
  return mw1::parse(static_cast<int>(raw.size()), raw.data());
}
fs::path fresh(const char* name) {
  const fs::path d = fs::path(MW3_TEST_TMP) / name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}
}  // namespace

TEST(mw1_parse_takes_its_own_flags_and_passes_the_rest) {
  const mw1::Args a = parse({"--mw3-install", "/m3", "--install", "/pm", "--mw1-game", "/m1", "--hidden", "--write-dir", "/w"});
  EXPECT(a.given.mw3 == "/m3" && a.given.pm == "/pm" && a.given.mw1 == "/m1" && a.write_dir == "/w");
  // --install and --write-dir are the game's too, so they stay; mw1's own two go.
  EXPECT((a.rest == std::vector<std::string>{"--install", "/pm", "--hidden", "--write-dir", "/w"}));
}

TEST(mw1_game_args_add_the_write_dir_and_the_saved_pm_install) {
  mw1::Args a = parse({"--hidden"});
  a.write_dir = "/w";
  mw1::Installs use;
  use.pm = "/pm";
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--write-dir", "/w", "--install", "/pm", "--hidden", "-nofmv"}));
  // Given on the command line (either form): not added again.
  a = parse({"--install", "/other", "--write-dir", "/w", "-nofmv"});
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--install", "/other", "--write-dir", "/w", "-nofmv"}));
  a = parse({"/bare"});
  a.write_dir = "/w";
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--write-dir", "/w", "/bare", "-nofmv"}));
}

TEST(mw1_cfg_round_trip_and_the_command_line_wins) {
  const fs::path d = fresh("mw1cfg");
  EXPECT(mw1::read_cfg(d.string()).pm.empty());  // no file: nothing
  mw1::Installs in;
  in.pm = "/a b/pm", in.mw3 = "/mw3", in.mw1 = "/dos";
  EXPECT(mw1::write_cfg(d.string(), in));
  const mw1::Installs back = mw1::read_cfg(d.string());
  EXPECT(back.pm == in.pm && back.mw3 == in.mw3 && back.mw1 == in.mw1);
  mw1::Installs given;
  given.mw3 = "/new";
  const mw1::Installs m = mw1::merge(given, back);
  EXPECT(m.mw3 == "/new" && m.pm == in.pm && m.mw1 == in.mw1);
}

TEST(mw1_install_checks) {
  const fs::path d = fresh("mw1dos");
  EXPECT(!mw1::is_mw1_install(d.string()));
  std::ofstream(d / "btech.exe") << "x";  // any case, as the launcher's check
  EXPECT(mw1::is_mw1_install(d.string()));
  EXPECT(!mw1::is_mw1_install(""));
  EXPECT(!mw1::is_mw3_install(d.string()));
  if (fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "reader.zbd")) EXPECT(mw1::is_mw3_install(MW1_MW3_GAME_DIR));
  if (fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd")) EXPECT(!mw1::is_mw3_install(MW3PM_GAME_DIR));
}

// FIXES 139: the write dir's old name moved to the new one (mw1::move_write_dir).
namespace {
void put(const fs::path& f, const std::string& text) {
  fs::create_directories(f.parent_path());
  std::ofstream(f, std::ios::binary) << text;
}
std::string got(const fs::path& f) {
  std::ifstream in(f, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
// A player's folder: settings, a save in a slot folder, mw1.cfg.
void player_folder(const fs::path& d, const std::string& tag) {
  put(d / "mw1.cfg", "install=/pm-" + tag + "\n");
  put(d / "saves" / "slot1" / "mw1save.txt", "company " + tag + "\n");
  put(d / "options.ini", std::string("x\0y", 3) + tag);
}
bool player_folder_is(const fs::path& d, const std::string& tag) {
  return got(d / "mw1.cfg") == "install=/pm-" + tag + "\n" && got(d / "saves" / "slot1" / "mw1save.txt") == "company " + tag + "\n" &&
         got(d / "options.ini") == std::string("x\0y", 3) + tag;
}
}  // namespace

TEST(mw1_write_dir_old_only_moves_whole) {
  for (const bool rename : {true, false}) {  // renamed in place, and the copy that is checked
    const fs::path root = fresh("mw1move_old");
    const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
    player_folder(from, "old");
    fs::create_directories(to);  // as SDL_GetPrefPath leaves it: made, empty
    const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string(), rename);
    EXPECT(m.how == (rename ? mw1::Moved::kRenamed : mw1::Moved::kCopied) && m.why.empty());
    EXPECT(m.use == to.string() && player_folder_is(to, "old"));
    EXPECT(!fs::exists(from) && !fs::exists(to.string() + ".moving"));
    EXPECT(mw1::read_cfg(m.use).pm == "/pm-old");
  }
}

TEST(mw1_write_dir_both_keeps_both) {
  const fs::path root = fresh("mw1move_both");
  const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
  player_folder(from, "old");
  player_folder(to, "new");
  const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string());
  EXPECT(m.how == mw1::Moved::kBothKept && m.use == to.string());
  EXPECT(player_folder_is(to, "new") && player_folder_is(from, "old"));  // nothing merged, nothing overwritten
}

TEST(mw1_write_dir_new_only_stays) {
  const fs::path root = fresh("mw1move_new");
  const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
  player_folder(to, "new");
  const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string());
  EXPECT(m.how == mw1::Moved::kNothing && m.use == to.string() && player_folder_is(to, "new") && !fs::exists(from));
  EXPECT(mw1::move_write_dir("", to.string()).how == mw1::Moved::kNothing);
}

// The copy (no rename) with read-only files, and on POSIX read-only folders too: once the copy is checked and in
// place, the old tree's read-only bits are cleared and the old folder removed whole.
TEST(mw1_write_dir_copy_removes_a_read_only_old) {
  const fs::path root = fresh("mw1move_ro_copy");
  const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
  player_folder(from, "old");
  const auto ro = fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read;
  for (const char* f : {"mw1.cfg", "options.ini", "saves/slot1/mw1save.txt"}) fs::permissions(from / f, ro);
#ifndef _WIN32
  const auto ro_dir = fs::perms::owner_read | fs::perms::owner_exec;
  fs::permissions(from / "saves" / "slot1", ro_dir), fs::permissions(from / "saves", ro_dir), fs::permissions(from, ro_dir);
#endif
  fs::create_directories(to);
  const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string(), false);
  EXPECT(m.how == mw1::Moved::kCopied && m.why.empty() && m.use == to.string());
  EXPECT(!fs::exists(from) && player_folder_is(to, "old") && !fs::exists(to.string() + ".moving"));
}

#ifndef _WIN32
#include <unistd.h>
// A read-only old folder: its files reach the new one whole; the old one, which can't be emptied, is left.
TEST(mw1_write_dir_read_only_old) {
  if (geteuid() == 0) return;  // root reads and removes anything
  for (const bool rename : {true, false}) {
    const fs::path root = fresh("mw1move_ro");
    const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
    player_folder(from, "old");
    const auto ro = fs::perms::owner_read | fs::perms::owner_exec;
    fs::permissions(from / "saves" / "slot1", ro), fs::permissions(from / "saves", ro), fs::permissions(from, ro);
    const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string(), rename);
    EXPECT(m.how == mw1::Moved::kRenamed || m.how == mw1::Moved::kCopied);
    EXPECT(m.use == to.string() && player_folder_is(to, "old"));
    if (fs::exists(from)) EXPECT(player_folder_is(from, "old") && !m.why.empty());  // left whole, and said
    put(to / "saves" / "slot1" / "mw1save.txt", "saved again\n");  // the new one is a write dir the game can write in
    EXPECT(got(to / "saves" / "slot1" / "mw1save.txt") == "saved again\n");
    for (const fs::path& d : {from, from / "saves", from / "saves" / "slot1", to, to / "saves", to / "saves" / "slot1"}) {
      std::error_code ec;
      fs::permissions(d, fs::perms::owner_all, ec);
    }
  }
}

// A move that fails (a save it can't read): the old folder is as it was and used, the half copy gone.
TEST(mw1_write_dir_failed_move_keeps_the_old) {
  if (geteuid() == 0) return;
  const fs::path root = fresh("mw1move_fail");
  const fs::path from = root / mw1::kOldApp, to = root / mw1::kApp;
  player_folder(from, "old");
  const fs::path locked = from / "saves" / "slot1" / "mw1save.txt";
  fs::permissions(locked, fs::perms::none);
  fs::create_directories(to);
  const mw1::WriteDirMove m = mw1::move_write_dir(from.string(), to.string(), false);
  fs::permissions(locked, fs::perms::owner_read | fs::perms::owner_write);
  EXPECT(m.how == mw1::Moved::kFailed && m.use == from.string() && !m.why.empty());
  EXPECT(m.why.rfind("the copy failed: ", 0) == 0);  // the copy's own error, not only the byte check's
  EXPECT(player_folder_is(from, "old") && !fs::exists(to.string() + ".moving"));
  EXPECT(fs::is_directory(to) && fs::is_empty(to));  // as SDL left it: the next start tries again
}
#endif

// FIXES 147: the user folder settled at the start (mw1::settle_user_folder), in each platform's form of the app data
// folder: Windows' AppData\Roaming, the Mac's Library/Application Support, Linux's .local/share.
namespace {
const char* const kAppDataForms[] = {"AppData/Roaming", "Library/Application Support", ".local/share"};
struct Family {
  fs::path usual, old_remake, critical;
};
Family family(const char* test, const char* form) {
  const fs::path base = fresh(test) / form;
  return {base / mw1::kOrg / mw1::kApp, base / mw1::kOldOrg / mw1::kApp, base / mw1::kOldOrg / mw1::kOldApp};
}
// The old place holds only the pointer, naming the folder in use.
bool pointer_only(const fs::path& old, const fs::path& to) {
  std::vector<std::string> names;
  for (const auto& e : fs::directory_iterator(old)) names.push_back(e.path().filename().string());
  return names == std::vector<std::string>{"data-folder.txt"} && got(old / "data-folder.txt") == to.string() + "\n";
}
}  // namespace

TEST(mw1_user_folder_fresh_starts_in_battletech) {
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_fresh", form);
    fs::create_directories(f.usual);  // as SDL_GetPrefPath leaves it
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    EXPECT(u.use == f.usual.string() && u.said.empty());
    EXPECT(!fs::exists(f.old_remake.parent_path()));  // nothing made under MicroProse
  }
}

TEST(mw1_user_folder_old_critical_hit_moves_to_battletech) {
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_crit", form);
    player_folder(f.critical, "crit");
    fs::create_directories(f.usual);
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    EXPECT(u.use == f.usual.string() && player_folder_is(f.usual, "crit") && u.said.size() == 2);
    EXPECT(!fs::exists(f.critical) && pointer_only(f.old_remake, f.usual));
    EXPECT(mw1::read_cfg(u.use).pm == "/pm-crit");
  }
}

TEST(mw1_user_folder_old_microprose_moves_to_battletech) {
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_mp", form);
    player_folder(f.old_remake, "remake");
    fs::create_directories(f.usual);
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    EXPECT(u.use == f.usual.string() && player_folder_is(f.usual, "remake") && u.said.size() == 1);
    EXPECT(pointer_only(f.old_remake, f.usual));
    // The next start: already moved, nothing more to do.
    const mw1::UserFolder again = mw1::settle_user_folder(f.usual.string());
    EXPECT(again.use == f.usual.string() && again.said.empty() && player_folder_is(f.usual, "remake"));
    EXPECT(pointer_only(f.old_remake, f.usual));
  }
}

TEST(mw1_user_folder_both_old_folders_the_remake_s_moves) {
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_both", form);
    player_folder(f.critical, "crit");
    player_folder(f.old_remake, "remake");
    fs::create_directories(f.usual);
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    // FIXES 139's rule: with files in both, the newer one is used and the Critical Hit folder left as it is.
    EXPECT(u.use == f.usual.string() && player_folder_is(f.usual, "remake"));
    EXPECT(player_folder_is(f.critical, "crit") && pointer_only(f.old_remake, f.usual));
  }
}

TEST(mw1_user_folder_already_moved_is_left_alone) {
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_moved", form);
    player_folder(f.usual, "new");
    put(f.old_remake / "data-folder.txt", f.usual.string() + "\n");
    player_folder(f.critical, "crit");  // a stray older folder: never merged into one in use
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    EXPECT(u.use == f.usual.string() && u.said.empty() && player_folder_is(f.usual, "new"));
    EXPECT(pointer_only(f.old_remake, f.usual) && player_folder_is(f.critical, "crit"));
  }
}

TEST(mw1_user_folder_development_build_moves_nothing) {
  const Family f = family("mw1family_dev", kAppDataForms[2]);
  player_folder(f.old_remake, "remake");
  fs::create_directories(f.usual);
  const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string(), false);
  EXPECT(u.use == f.old_remake.string() && u.said.empty() && player_folder_is(f.old_remake, "remake"));
  EXPECT(fs::is_empty(f.usual));
}

#ifndef _WIN32
// A read-only parent: BattleTech can't be made beside MicroProse. The old folder is used, whole, and the move is
// tried again at the next start. (POSIX only: a Windows folder has no read-only bit that stops a new folder in it.)
TEST(mw1_user_folder_read_only_parent_keeps_the_old) {
  if (geteuid() == 0) return;
  for (const char* form : kAppDataForms) {
    const Family f = family("mw1family_ro", form);
    player_folder(f.old_remake, "remake");
    const fs::path base = f.usual.parent_path().parent_path();
    fs::permissions(base, fs::perms::owner_read | fs::perms::owner_exec);
    const mw1::UserFolder u = mw1::settle_user_folder(f.usual.string());
    fs::permissions(base, fs::perms::owner_all);
    EXPECT(u.use == f.old_remake.string() && u.said.size() == 1 && player_folder_is(f.old_remake, "remake"));
    EXPECT(!fs::exists(f.old_remake / "data-folder.txt") && !fs::exists(f.usual));
    // Writable again: the next start moves it.
    const mw1::UserFolder later = mw1::settle_user_folder(f.usual.string());
    EXPECT(later.use == f.usual.string() && player_folder_is(f.usual, "remake") && pointer_only(f.old_remake, f.usual));
  }
}
#endif

// FIXES 145: a saved install inside a family folder that has moved, looked for where it is now.
TEST(mw1_moved_install_places_battletech_then_the_pointer) {
  for (const char* form : kAppDataForms) {
    const fs::path base = fresh("mw1family_install") / form;
    const fs::path old_pm = base / mw1::kOldOrg / "Pirate's Moon", new_pm = base / mw1::kOrg / "Pirate's Moon";
    const fs::path elsewhere = fresh("mw1family_install_elsewhere") / "PM data";
    put(old_pm / "data-folder.txt", elsewhere.string() + "\r\n");
    const std::vector<std::string> places = mw1::moved_install_places((old_pm / "install").string());
    EXPECT((places == std::vector<std::string>{(new_pm / "install").string(), (elsewhere / "install").string()}));
    // The family folder itself, and a deeper path.
    EXPECT(mw1::moved_install_places(old_pm.string()).front() == new_pm.string());
    EXPECT(mw1::moved_install_places((old_pm / "install" / "zbd").string()).front() == (new_pm / "install" / "zbd").string());
    // No pointer: BattleTech's place only. Inside no MicroProse folder: none.
    fs::remove(old_pm / "data-folder.txt");
    EXPECT(mw1::moved_install_places((old_pm / "install").string()) == std::vector<std::string>{(new_pm / "install").string()});
    EXPECT(mw1::moved_install_places((new_pm / "install").string()).empty());
    EXPECT(mw1::moved_install_places("").empty());
  }
}

// The launcher's record of where MW1 is (sdl_games.h's record_self): a player's start only.
TEST(mw1_records_its_program_for_a_players_start) {
  auto rest = [](std::vector<const char*> argv) {
    argv.insert(argv.begin(), "mw1");
    return mw1::parse(static_cast<int>(argv.size()), const_cast<char**>(argv.data())).rest;
  };
  EXPECT(mw1::records_program(rest({})));
  EXPECT(mw1::records_program(rest({"--windowed", "--write-dir", "/w"})));
  EXPECT(!mw1::records_program(rest({"--hidden"})));
  EXPECT(!mw1::records_program(rest({"--write-dir", "/w", "--status"})));
}
