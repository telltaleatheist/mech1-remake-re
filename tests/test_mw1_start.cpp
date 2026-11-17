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
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--write-dir", "/w", "--install", "/pm", "--hidden"}));
  // Given on the command line (either form): not added again.
  a = parse({"--install", "/other", "--write-dir", "/w"});
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--install", "/other", "--write-dir", "/w"}));
  a = parse({"/bare"});
  a.write_dir = "/w";
  EXPECT((mw1::game_args("mw1", a, use) == std::vector<std::string>{"mw1", "--write-dir", "/w", "/bare"}));
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
