// The setup kit's in-game menu in mw1 (mw1_main.cpp's sdlfe::program_name; docs/in-game-menu.md): the Remake's name,
// its report page, its own releases (mech1-remake-re's mw1-v tags and MW1-Remake files) and no CHANGE, and the menu
// seen over the mech bay's first-run pass. Built into mw3pmsdltests (mw1/CMakeLists.txt), on its in-game menu helpers.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "../../tests/check.h"  // the engine's (the kit's tests have a check.h of their own)
#include "rekit/hash.h"
#include "test_keys.h"

namespace ingame {
const std::string kUnset = "\x01unset";  // as test_sdl_frontend.cpp's (its own has internal linkage): unset for the run
std::string read(const std::string& path);
bool in(const std::string& text, const std::string& what);
std::string run(const std::string& log, const std::vector<std::string>& args,
                const std::vector<std::pair<const char*, std::string>>& env, int* code, int seconds, const char* bin);
bool menu_in_shot(const std::string& path);
void keep_shots(const std::string& dir);
}  // namespace ingame

namespace {
namespace fs = std::filesystem;
bool have_fonts() { return fs::exists(std::string(MW3_GAME_DIR) + "/arial.ttf"); }  // as test_sdl_frontend.cpp's

// The stand-in world (as ingame::make_update_world's, for the Remake): a signed catalog and mech1-remake-re's
// mw1-v9.9.0 with the file its packaging names, only DIR/install (marked by mw1) replaceable.
std::string make_mw1_world(const std::string& root) {
  std::error_code ec;
  fs::remove_all(root, ec);
  auto put = [](const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
  };
  auto url = [](const fs::path& p) { return "file://" + p.generic_string(); };
  const fs::path r(root);
  const std::string cat = "{\"catalog\": 1, \"counts\": \"\", \"homes\": [{\"key\": \"github\", \"releases\": \"" +
                          url(r / "github" / "{repo}.json") + "\", \"order\": 1}], \"games\": []}";
  put(r / "catalog.json", cat), put(r / "catalog.json.sig", test_keys::sign(cat, 1));
#if defined(_WIN32)
  const std::string named = "MW1-Remake-mw1-v9.9.0-win64.zip";
#elif defined(__APPLE__)
  const std::string named = "MW1-Remake-mw1-v9.9.0-mac.dmg";
#else
  const std::string named = "MW1-Remake-mw1-v9.9.0-x86_64.AppImage";
#endif
  const std::string payload = "the new Remake",
                    sums = rekit::sha256_hex(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()) + "  " + named + "\n";
  const fs::path d = r / "github" / "mw1-v9.9.0";
  put(d / named, payload), put(d / "SHA256SUMS", sums), put(d / "SHA256SUMS.sig", test_keys::sign(sums, 1));
  std::string assets;
  for (const std::string& f : {named, std::string("SHA256SUMS"), std::string("SHA256SUMS.sig")})
    assets += std::string(assets.empty() ? "" : ",") + "{\"name\": \"" + f + "\", \"size\": " + std::to_string(fs::file_size(d / f)) +
              ", \"browser_download_url\": \"" + url(d / f) + "\"}";
  put(r / "github" / "mech1-remake-re.json", "[{\"tag_name\": \"mw1-v9.9.0\", \"body\": \"## Fixes\\n- The stand-in's one note.\", "
                                             "\"html_url\": \"https://example.org/mw1-v9.9.0\", \"assets\": [" + assets + "]}]");
  put(r / "install" / "mw1", "the installed program");
  return test_keys::public_key(1);
}

std::string fresh(const std::string& name) {
  const std::string dir = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir + "/wd");
  return dir;
}
}  // namespace

// Started alone: the start-up check finds mech1-remake-re's mw1-v9.9.0 (not Pirate's Moon's), the corner's notice
// names the Remake, the keys open the prompt, and UPDATE NOW reaches the close question, left unanswered.
TEST(mw1_menu_update_prompt_when_started_alone) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_update_alone"), wd = dir + "/wd";
  const std::string key = make_mw1_world(dir + "/world");
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "1500", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", ingame::kUnset}, {"RE_LAUNCHER", ingame::kUnset}, {"MW3_UPDATE_WORLD", dir + "/world"}, {"MW3_UPDATE_KEY", key},
       {"MW3_INSTALL_KIND", "folder"}, {"MW3_MENU_AT", "notice"}, {"MW3_MENU_KEYS", "e@30"},
       {"MW3_MENU_SHOT", dir + "/prompt.bmp@20;" + dir + "/close.bmp@45"}, {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  const size_t notice = log.find("mw3: menu: the update notice: MechWarrior 1 Remake 9.9.0 ("),
               pressed = log.find("mw3: menu: its keys pressed at the update notice"),
               prompt = log.find("mw3: menu: the update prompt: MechWarrior 1 Remake 9.9.0");
  std::printf("  exit %d, notice %d, keys %d, prompt %d, update now %d, held %d\n", code, notice != std::string::npos,
              pressed != std::string::npos, prompt != std::string::npos, ingame::in(log, "mw3: menu: the update prompt: UPDATE NOW"),
              ingame::in(log, "mw3: menu: the game resumed: 0 game frames"));
  EXPECT(code == 0);
  EXPECT(notice != std::string::npos && pressed != std::string::npos && prompt != std::string::npos);
  EXPECT(notice < pressed && pressed < prompt);
  EXPECT(!ingame::in(log, "Pirate's Moon 9.9.0"));
  EXPECT(ingame::in(log, "mw3: menu: the update prompt: UPDATE NOW") && !ingame::in(log, "mw3: menu: UPDATE NOW:"));
  EXPECT(ingame::read(dir + "/world/install/mw1") == "the installed program" && !fs::exists(wd + "/Backups"));
  std::error_code sz;  // the prompt is a box of its own, not the menu's panel (menu_in_shot's place)
  EXPECT(fs::file_size(dir + "/prompt.bmp", sz) > 1000000 && fs::file_size(dir + "/close.bmp", sz) > 1000000);
  ingame::keep_shots(dir);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// Started by the launcher: no prompt of its own; UPDATES' CHECK NOW finds the Remake's mw1-v9.9.0.
TEST(mw1_menu_update_page_under_the_launcher) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_update_launcher"), wd = dir + "/wd";
  const std::string key = make_mw1_world(dir + "/world");
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "900", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", ingame::kUnset}, {"RE_LAUNCHER", "2.2.0"}, {"MW3_UPDATE_WORLD", dir + "/world"}, {"MW3_UPDATE_KEY", key},
       {"MW3_INSTALL_KIND", "folder"}, {"MW3_MENU_AT", "600:drde"}, {"MW3_MENU_SHOT", dir + "/page.bmp@150"}, {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  const size_t checked = log.find("mw3: menu: checked: ");
  const std::string said = checked == std::string::npos ? "" : log.substr(checked, log.find('\n', checked) - checked);
  std::printf("  exit %d, [%s]\n", code, said.c_str());
  EXPECT(code == 0);
  EXPECT(!ingame::in(log, "the update prompt") && !ingame::in(log, "the update notice"));
  EXPECT(ingame::in(log, "mw3: menu: CHECK NOW") && ingame::in(said, "9.9.0"));
  EXPECT(ingame::menu_in_shot(dir + "/page.bmp"));
  ingame::keep_shots(dir);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// REPORT A PROBLEM opens the Remake's issues (the build's setup-kit-settings.json, from assets/), and its title and
// version are the Remake's (no Pirate's Moon in the menu's words).
TEST(mw1_menu_report_page_is_the_remakes) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_report"), wd = dir + "/wd";
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "800", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW3_MENU_AT", "600:ddrdde"}, {"MW3_MENU_SHOT", dir + "/report.bmp@20"}, {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  const std::string ours = "https://github.com/telltaleatheist/mech1-remake-re/issues";
  std::printf("  exit %d, opened %d\n", code, ingame::in(log, "mw3: menu: open the report page: " + ours + "\n"));
  EXPECT(code == 0);
  EXPECT(ingame::in(log, "mw3: menu: open the report page: " + ours + "\n"));
  EXPECT(!ingame::in(log, "mech3-pm-re"));
  EXPECT(ingame::menu_in_shot(dir + "/report.bmp"));
  ingame::keep_shots(dir);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// The menu over the mech bay's first-run pass (a fresh write dir: its pictures not made yet), whose frames are held
// back from the window (Render3D::hold_display): the menu is seen, not a frozen screen, and the pass goes on after.
TEST(mw1_menu_shows_over_the_mech_bay_pass) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_mech_bay"), wd = dir + "/wd";
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "400", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW3_MENU_AT", "60"}, {"MW3_MENU_SHOT", dir + "/bay.bmp@10"}, {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  std::printf("  exit %d, opened %d\n", code, ingame::in(log, "mw3: menu: opened at game frame 60"));
  EXPECT(code == 0);
  EXPECT(ingame::in(log, "mw3: menu: opened at game frame 60") && ingame::in(log, "mw3: menu: closed"));
  EXPECT(ingame::menu_in_shot(dir + "/bay.bmp"));
  ingame::keep_shots(dir);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

namespace {
// The medallion beside the header in a 1280x960 shot: its colored pixels (the badge's bronze) where it sits, in the
// header title's rows. Without it the title, white on the blue-gray band, moves there and has none (the orange line
// under it is below these rows).
int badge_pixels(const std::string& bmp) {
  const std::string d = ingame::read(bmp);
  if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return -1;
  auto u32 = [&](size_t at) {
    return static_cast<uint32_t>(static_cast<uint8_t>(d[at]) | static_cast<uint8_t>(d[at + 1]) << 8 |
                                 static_cast<uint8_t>(d[at + 2]) << 16 | static_cast<uint8_t>(d[at + 3]) << 24);
  };
  const int32_t w = static_cast<int32_t>(u32(18)), h = static_cast<int32_t>(u32(22));
  int n = 0;
  for (int y = 206; y < 234; ++y)
    for (int x = 204; x < 246; ++x) {
      const size_t at = u32(10) + (static_cast<size_t>(h > 0 ? h - 1 - y : y) * w + x) * 4;
      if (at + 3 > d.size()) return -1;
      const int b = static_cast<uint8_t>(d[at]), g = static_cast<uint8_t>(d[at + 1]), r = static_cast<uint8_t>(d[at + 2]);
      n += std::max({r, g, b}) - std::min({r, g, b}) > 25;
    }
  return n;
}
}  // namespace

// The header is the Remake's with its medallion (badge_mw1.png beside the program, as every package has it), and GAME
// FILES has no CHANGE: the files in use, and where to change them.
TEST(mw1_menu_shows_its_medallion) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_medallion"), wd = dir + "/wd";
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "700", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW3_MENU_AT", "600"}, {"MW3_MENU_SHOT", dir + "/files.bmp@10"}, {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  const int n = badge_pixels(dir + "/files.bmp");
  std::printf("  exit %d, the medallion's colored pixels %d\n", code, n);
  EXPECT(code == 0);
  EXPECT(fs::exists(fs::path(MW1_BIN).parent_path() / "badge_mw1.png"));
  EXPECT(n > 20);
  EXPECT(ingame::menu_in_shot(dir + "/files.bmp"));
  ingame::keep_shots(dir);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// UPGRADES opens MW1's SETTINGS on ENHANCEMENTS (FIXES 126's screen holds the Upgrades page's switches), not Pirate's
// Moon's page: over the title, and in a mission through the pause's OPTIONS (which opens SETTINGS), as a player goes.
TEST(mw1_menu_upgrades_opens_settings_on_enhancements) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_upgrades"), wd = dir + "/wd", wd2 = dir + "/wd2";
  fs::create_directories(wd2);
  int code = 0, code2 = 0;
  const std::string title = ingame::run(
      dir + "/title.log", {"--write-dir", wd, "--frames", "700", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW3_MENU_AT", "600:dddddde"}}, &code, 300, MW1_BIN);
  const std::string mission = ingame::run(
      dir + "/mission.log", {"--write-dir", wd2, "--frames", "400", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW1_SCREEN", "mfd"}, {"MW3_MENU_AT", "40:dddddde"}}, &code2, 400, MW1_BIN);
  std::printf("  title: exit %d, settings %d | mission: exit %d, pause %d, settings %d\n", code,
              ingame::in(title, "mw1: menu's UPGRADES: SETTINGS on ENHANCEMENTS\n"), code2,
              ingame::in(mission, "mw3: menu: UPGRADES: the pause screen, then its Upgrades page"),
              ingame::in(mission, "mw1: menu's UPGRADES: SETTINGS on ENHANCEMENTS (up already)"));
  EXPECT(code == 0 && code2 == 0);
  EXPECT(ingame::in(title, "mw3: menu: UPGRADES: the Upgrades page") && ingame::in(title, "mw1: menu's UPGRADES: SETTINGS on ENHANCEMENTS\n"));
  EXPECT(ingame::in(mission, "mw3: menu: UPGRADES: the pause screen, then its Upgrades page"));
  EXPECT(ingame::in(mission, "mw1: menu's UPGRADES: SETTINGS on ENHANCEMENTS (up already)"));
  std::error_code ec;
  fs::remove_all(dir, ec);
}

// RESET SETTINGS: Pirate's Moon's settings, then MW1's own (every section's DEFAULTS: mw1.cfg's sound among them),
// mw1.cfg backed up first with PM's files.
TEST(mw1_menu_reset_settings_resets_mw1s_own) {
  if (!have_fonts()) return;
  const std::string dir = fresh("mw1_menu_reset"), wd = dir + "/wd";
  std::ofstream(wd + "/mw1.cfg") << "music_volume=30\neffects_volume=20\n";
  int code = 0;
  const std::string log = ingame::run(
      dir + "/run.log", {"--write-dir", wd, "--frames", "700", "--hidden", "--nosound", "--window", "1280x960"},
      {{"RE_KIT_TEST", "1"}, {"MW3_MENU_AT", "600:ddddreue"}, {"MW3_MENU_SHOT", dir + "/menu.bmp"}, {"MW3_MENU_SHOT_AFTER", "20"},
       {"MW3_MENU_CLOSE", "1"}},
      &code, 300, MW1_BIN);
  const std::string cfg = ingame::read(wd + "/mw1.cfg");
  bool backed_up = false;
  std::error_code ec;
  for (const auto& d : fs::directory_iterator(wd + "/Backups", ec))
    backed_up = backed_up || ingame::in(ingame::read((d.path() / "mw1.cfg").string()), "music_volume=30");
  std::printf("  exit %d, pm %d, mw1 %d, backed up %d, mw1.cfg [%s]\n", code, ingame::in(log, "mw3: menu: settings reset to their defaults"),
              ingame::in(log, "mw1: menu's RESET SETTINGS: every section's DEFAULTS"), backed_up, cfg.c_str());
  EXPECT(code == 0);
  EXPECT(ingame::in(log, "mw3: menu: settings reset to their defaults") && ingame::in(log, "mw1: menu's RESET SETTINGS: every section's DEFAULTS"));
  EXPECT(!ingame::in(cfg, "music_volume=30") && !ingame::in(cfg, "effects_volume=20"));
  EXPECT(backed_up);
  fs::remove_all(dir, ec);
}
