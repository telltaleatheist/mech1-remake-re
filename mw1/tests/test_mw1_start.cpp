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
  EXPECT(player_folder_is(from, "old") && !fs::exists(to.string() + ".moving"));
  EXPECT(fs::is_directory(to) && fs::is_empty(to));  // as SDL left it: the next start tries again
}
#endif
