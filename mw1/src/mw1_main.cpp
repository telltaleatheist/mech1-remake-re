// mw1: the MW1 remake, MechWarrior 1 Remake (VISION.md, The code base). The third
// program over the shared engine, built on Pirate's Moon's game code the way
// mw3pm is: PM's game main (mw3_main.cpp) with its own write dir and the
// player's three installs. For now it starts into PM's frontend as it is.
//
//   mw1 [--install PM-DIR] [--mw3-install MW3-DIR] [--mw1-game MW1-DIR]
//       [--write-dir DIR] [anything mw3pm takes]
//
//   --install DIR      Pirate's Moon's folder (the game's own install)
//   --mw3-install DIR  MechWarrior 3's folder (its worlds c2 and c4b)
//   --mw1-game DIR     MechWarrior (1989)'s DOS folder (BTECH.EXE)
//   --new-company      a new company, over the saved one (mw1save.txt)
//   --mw1-worlds DIR   MW3's converted worlds (default: the write dir's worlds/)
//   --write-dir DIR    settings and saves (default: the user's app data,
//                      "MechWarrior 1 Remake", apart from MW3's and PM's;
//                      the old "MechWarrior 1 Critical Hit" moved to it, FIXES 139)
//
// Each folder given is checked and saved to mw1.cfg in the write dir, so the
// next start without them (the launcher's or a plain one) uses them again.
// MW3's worlds are converted into the write dir's worlds/ when they are due
// (mw1_worlds.h), with a progress window unless --hidden, then registered.
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "platform/hires_2d.h"
#include "platform/menu_pointer.h"
#include "platform/pause_hud.h"
#include "platform/mode_labels.h"
#include "platform/list_wheel.h"
#include "platform/pointer_hands_off.h"
#include "gamez/zutil/zutl_path.h"
#include "frontend/sdl/crash_report.h"
#include "frontend/sdl/sdl_games.h"
#include "frontend/sdl/sdl_platform.h"
#include "frontend/sdl/sdl_twod_host.h"
#include "gamez/ztwod/twod_host.h"
#include "platform/hires_lists.h"
#include "mw1_game.h"
#include "mw1_hud_labels.h"
#include "mw1_bounds.h"
#include "mw1_mission_maps.h"
#include "mw1_missions.h"
#include "mw1_movie.h"
#include "mw1_pilots.h"
#include "mw1_save.h"
#include "mw1_music.h"
#include "mw1_start.h"
#include "mw1_worlds.h"
#include "pm/pm_game.h"

int mw3_game_main(int argc, char** argv);

namespace {

// SDL's folder for `org`/MechWarrior 1 Remake, made when missing, without a trailing separator; "" when it can't be.
std::string pref_path(const char* org) {
  std::string dir;
  if (char* p = SDL_GetPrefPath(org, mw1::kApp)) dir = p, SDL_free(p);
  while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
  return dir;
}

// The usual write dir, the family's BattleTech/MechWarrior 1 Remake, with the earlier folders moved to it
// (FIXES 139 and 147, mw1::settle_user_folder): in a package only, so a development build never moves its developer's
// own folders. SDL makes the new folder when asked for it, so an empty one counts as missing. When even that can't be
// made (a read-only app data folder), the earlier place, MicroProse's, as before 1.1.0.
std::string usual_write_dir() {
  const std::string dir = pref_path(mw1::kOrg);
  if (dir.empty()) {
    std::printf("mw1: the family folder %s/%s could not be made: the earlier folder is used\n", mw1::kOrg, mw1::kApp);
    return pref_path(mw1::kOldOrg);
  }
#if defined(MW3_PACKAGE_BUILD)
  const mw1::UserFolder u = mw1::settle_user_folder(dir);
#else
  const mw1::UserFolder u = mw1::settle_user_folder(dir, false);
#endif
  for (const std::string& line : u.said) std::printf("mw1: %s\n", line.c_str());
  return u.use;
}

}  // namespace

namespace {
// A run no player is at, from the arguments for PM's game main. mw3_main.cpp's program() turns pointer_hands_off on
// for --hidden, --frames N (N > 0), --campaign-chain and --action-chain; a --shot run is one too. Keep these in step
// with mw3_main's (its Args, read only there). The Mac's alone uses it (SDL_MAC_BACKGROUND_APP, below).
#if defined(__APPLE__)
bool no_player_at(const std::vector<std::string>& rest) {
  for (size_t i = 0; i < rest.size(); ++i) {
    const std::string& s = rest[i];
    if (s == "--hidden" || s == "--shot" || s == "--campaign-chain" || s == "--action-chain") return true;
    if (s == "--frames" && i + 1 < rest.size() && std::atoi(rest[i + 1].c_str()) > 0) return true;
  }
  return false;
}
#endif

// The conversion with a small window before PM's frontend: a bar, the step in its title (as mw3's first-run movie
// import shows its own). The run in a thread, the window on this one; no window when SDL's video doesn't start.
bool convert_in_window(const zipkit::Options& o, std::string* error) {
  std::mutex m;
  std::string step = "starting";
  std::atomic<float> part{0};
  std::atomic<bool> finished{false};
  bool ok = false;
  zipkit::Options run = o;
  const int worlds = static_cast<int>(o.worlds.size());
  int index = -1;
  std::string last;
  run.progress = [&](const std::string& world, const std::string& s, int done, int total) {
    if (o.progress) o.progress(world, s, done, total);
    if (world != last) last = world, ++index;
    std::lock_guard<std::mutex> lock(m);
    step = world + ": " + s;
    part = (static_cast<float>(index) + (total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f)) /
           static_cast<float>(worlds > 0 ? worlds : 1);
  };
  std::thread worker([&] {
    ok = zipkit::mw3_to_pm(run, error);
    finished = true;
  });
  SDL_Window* win = nullptr;
  SDL_Renderer* ren = nullptr;
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  if (video) SDL_CreateWindowAndRenderer("MechWarrior 1: converting MechWarrior 3's worlds", 480, 64, 0, &win, &ren);
  std::string shown;
  while (!finished) {
    SDL_Event ev;
    while (video && SDL_PollEvent(&ev)) {
    }
    if (win) {
      std::string now;
      {
        std::lock_guard<std::mutex> lock(m);
        now = step;
      }
      if (now != shown) shown = now, SDL_SetWindowTitle(win, ("MechWarrior 1: converting MechWarrior 3's worlds (" + now + ")").c_str());
    }
    if (ren) {
      SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
      SDL_RenderClear(ren);
      const SDL_FRect frame{16, 20, 448, 24};
      SDL_SetRenderDrawColor(ren, 0, 160, 0, 255);
      SDL_RenderRect(ren, &frame);
      const SDL_FRect bar{18, 22, 444.0f * part.load(), 20};
      SDL_RenderFillRect(ren, &bar);
      SDL_RenderPresent(ren);
    }
    SDL_Delay(30);
  }
  worker.join();
  if (ren) SDL_DestroyRenderer(ren);
  if (win) SDL_DestroyWindow(win);
  if (video) SDL_Quit();  // PM's game main starts SDL afresh
  return ok;
}
}  // namespace

// This program's own folder (FIXES.md item 105): a .app's Contents/MacOS, the AppImage's usr/bin, the Windows
// folder; where the launcher sits beside us. SDL's base path is that too but in a .app, where it is Resources (where
// mw1data goes, as the packages put the launcher's art: tools/macos/make_app.sh).
static std::filesystem::path own_dir() {
#if defined(__APPLE__)
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) == 0) return std::filesystem::weakly_canonical(std::filesystem::path(buf)).parent_path();
#endif
  const char* base = SDL_GetBasePath();
  return base ? std::filesystem::path(base) : std::filesystem::path(".");
}

// The program's version tag (mw3_main.cpp's, MW1's describe here: FIXES.md item 105).
extern "C" const char mw3_version_tag[];

int main(int argc, char** argv) {
  // FIXES 123: the log's first line, the crash report and the crash box name MW1 and MW1's version, not Pirate's
  // Moon's (mw3_game_main's, through sdlfe::program_name: unset, PM's own).
  const char* version = mw3_version_tag + std::strlen("MW3-PROGRAM-VERSION:");
  sdlfe::program_name() = {"mw1", mw1::kTitle, version};
  sdlfe::window_name() = mw1::kTitle;  // FIXES 139: the game window's title, over PM's
  // A crash's box (Mac, Linux): the crash's handler starts this program again as `mw1 --crash-box <text>`, which
  // shows the box and is all it does. Before MW1's start, which would have taken it for a game's start.
  if (sdlfe::crash_box_main(argc, argv)) return 0;
  // --version first, before a write dir, an install or a window (FIXES.md item 105): the tag's version, as the
  // launcher reads it from the file.
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--version") == 0) {
      std::printf("mw1 (%s) %s\n", mw1::kTitle, version);
      return 0;
    }
  mw1::Args a = mw1::parse(argc, argv);
  if (a.write_dir.empty()) a.write_dir = usual_write_dir();
  if (!a.write_dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(a.write_dir, ec);
  }
  // Where this program is, for the launcher (its rebuild-program.txt in the user folder, after the folder has moved):
  // a copy installed without it is found and linked. A released build's only, never the launcher's own copy.
  if (mw1::records_program(a.rest)) sdlfe::games::record_self(a.write_dir, upgrades::update::Game::kMw1, version);
  const mw1::Installs saved = mw1::read_cfg(a.write_dir);
  mw1::Installs use = mw1::merge(a.given, saved);
  // FIXES 145: an install saved inside a family folder that has since moved (MicroProse to BattleTech, the launcher
  // 1.3.2's and ours) is looked for where that folder is now, and saved there when it holds the game.
  auto follow = [](std::string& dir, const char* game, bool (*holds)(const std::string&)) {
    if (dir.empty() || holds(dir)) return;
    for (const std::string& place : mw1::moved_install_places(dir))
      if (holds(place)) {
        std::printf("mw1: %s: %s is now %s\n", game, dir.c_str(), place.c_str());
        dir = place;
        return;
      }
  };
  follow(use.pm, "Pirate's Moon", [](const std::string& d) { return pm::install_kind(d) == pm::Install::kPm; });
  follow(use.mw3, "MechWarrior 3", mw1::is_mw3_install);
  follow(use.mw1, "MechWarrior (1989)", mw1::is_mw1_install);
  // A folder that is not the game it is given for is said and not used (nor saved).
  if (!use.pm.empty() && pm::install_kind(use.pm) != pm::Install::kPm) {
    std::fprintf(stderr, "mw1: %s is %s: not used for Pirate's Moon\n", use.pm.c_str(), pm::install_words(pm::install_kind(use.pm)));
    use.pm.clear();
  }
  if (!use.mw3.empty() && !mw1::is_mw3_install(use.mw3)) {
    std::fprintf(stderr, "mw1: %s is %s: not used for MechWarrior 3\n", use.mw3.c_str(), pm::install_words(pm::install_kind(use.mw3)));
    use.mw3.clear();
  }
  if (!use.mw1.empty() && !mw1::is_mw1_install(use.mw1)) {
    std::fprintf(stderr, "mw1: %s has no BTECH.EXE: not used for MechWarrior (1989)\n", use.mw1.c_str());
    use.mw1.clear();
  }
  const bool changed = use.pm != saved.pm || use.mw3 != saved.mw3 || use.mw1 != saved.mw1;
  if (changed && !a.write_dir.empty() && mw1::write_cfg(a.write_dir, use))
    std::printf("mw1: installs saved to %s/%s\n", a.write_dir.c_str(), mw1::kCfg);
  // Development builds: the repo's test copies when nothing else names one (never saved).
#ifdef MW1_PM_GAME_DIR
  if (use.pm.empty() && pm::install_kind(MW1_PM_GAME_DIR) == pm::Install::kPm) use.pm = MW1_PM_GAME_DIR;
#endif
#ifdef MW1_MW3_GAME_DIR
  if (use.mw3.empty() && mw1::is_mw3_install(MW1_MW3_GAME_DIR)) use.mw3 = MW1_MW3_GAME_DIR;
#endif
#ifdef MW1_DOS_GAME_DIR
  if (use.mw1.empty() && mw1::is_mw1_install(MW1_DOS_GAME_DIR)) use.mw1 = MW1_DOS_GAME_DIR;
#endif
  std::printf("mw1: MechWarrior 1 Remake, write dir %s\nmw1:   Pirate's Moon %s\nmw1:   MechWarrior 3 %s\nmw1:   MechWarrior (1989) %s\n",
              a.write_dir.c_str(), use.pm.empty() ? "(not found)" : use.pm.c_str(), use.mw3.empty() ? "(not found)" : use.mw3.c_str(),
              use.mw1.empty() ? "(not found)" : use.mw1.c_str());
  // A start without all three games (mw1.exe opened by itself, before the launcher has given them): say so and
  // offer the launcher beside us, which finds them and starts us with them. Not for --hidden runs (tests, shots).
  if ((use.pm.empty() || use.mw3.empty() || use.mw1.empty()) &&
      std::find(a.rest.begin(), a.rest.end(), "--hidden") == a.rest.end()) {
    std::string missing;
    if (use.pm.empty()) missing += "\n  - Pirate's Moon";
    if (use.mw3.empty()) missing += "\n  - MechWarrior 3";
    if (use.mw1.empty()) missing += "\n  - MechWarrior (1989)";
#ifdef _WIN32
    const std::filesystem::path launcher = own_dir() / "mw3launcher.exe";
#else
    const std::filesystem::path launcher = own_dir() / "mw3launcher";  // a .app's MacOS, not SDL's Resources
#endif
    std::error_code ec;
    const bool have_launcher = !launcher.empty() && std::filesystem::exists(launcher, ec);
    const std::string text = "MechWarrior 1 Remake needs your own copies of:" + missing +
                             (have_launcher ? "\n\nStart it from the launcher, which finds them: click MECHWARRIOR 1 there."
                                            : "\n\nStart it from the MechWarrior 3 rebuild's launcher, which finds them.");
    std::printf("mw1: missing installs:%s\n", missing.c_str());
    const SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, have_launcher ? "Open the launcher" : "OK"},
                                                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}};
    const SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING, nullptr, mw1::kTitle, text.c_str(),
                                 have_launcher ? 2 : 1, buttons, nullptr};
    int chose = 0;
    SDL_ShowMessageBox(&box, &chose);
    if (chose == 1 && have_launcher) {
      const std::string exe = launcher.string();
      const char* run[] = {exe.c_str(), nullptr};
      if (SDL_Process* p = SDL_CreateProcess(run, false)) SDL_DestroyProcess(p);  // detached: the launcher lives on
      else std::fprintf(stderr, "mw1: could not start %s: %s\n", exe.c_str(), SDL_GetError());
    }
    return 0;
  }
  mw1::set_mw1_game_dir(use.mw1);  // the Bar's pitches, from the player's MW_MAIN.EXE (mw1_pilots.h)
  // MW1's loop over PM's game: the hub at the main menu, the contract's mission, the settlement (mw1_game.h).
  // MW1_SEED (tests) fixes the company's and the contracts' rolls; MW1_REACH=<node> (a test override) makes every
  // contract reach that structure on c2's second Instant Action map.
  const char* seed = std::getenv("MW1_SEED");
  mw1::install_game_loop(seed ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 0, std::getenv("MW1_REACH"));
  // The company's saves in the write dir (mw1_save.h, FIXES 125: three slots; the one save before 1.0.2 becomes slot
  // 1), the slot in play mw1.cfg's. MW1's main menu first (mw1_menu.h); --new-company is NEW CAMPAIGN at once in the
  // slot in play (the intro, its old save kept as its .prev).
  if (!a.write_dir.empty()) {
    std::string said;
    if (mw1::migrate_slots(a.write_dir, &said)) std::printf("mw1: saves: %s\n", said.c_str());
    mw1::set_game_loop_slots(a.write_dir, use.save_slot >= 1 ? use.save_slot : 1, a.new_company);
  }
  mw1::set_game_loop_start(a.new_company ? mw1::Start::kNewCampaign : mw1::Start::kMenu);
  // FIXES.md item 100: MW1's menu theme (the CD's extra track, mw1data's music/), its settings from mw1.cfg; a run no
  // player hears (--nosound, --hidden) plays no music aloud.
  mw1::music::install(a.write_dir);
  // FIXES.md item 116: no disc music. The missions play MW1's combat tracks (mw1_music.h); neither the player's ripped
  // tracks nor Pirate's Moon's are read.
  // MW1's own data (the hub's layout), beside the program: mounted after what is (PM's install comes in the game main).
  if (const char* base = SDL_GetBasePath()) {
    const std::filesystem::path data = std::filesystem::path(base) / "mw1data";
    std::error_code ec;
    if (std::filesystem::is_directory(data, ec)) zutl::mount(data.string()), mw1::movie::set_data_folder(data.string());
    else std::printf("mw1: no mw1data beside the program (%s): the hub is PM's message boxes\n", data.string().c_str());
  }
  // MW1's map table (mw1_missions.h): the maps and structures the contracts choose from; none, the milestone's map.
  {
    std::string why;
    mw1::set_map_table(mw1::parse_maps(mw1::kMapsFile, &why));
    mw1::set_map_pools(mw1::parse_pools(mw1::kMapsFile, &why));  // item 46
    std::printf("mw1: %zu maps in %s%s%s\n", mw1::map_table().size(), mw1::kMapsFile, why.empty() ? "" : ": ", why.c_str());
  }
  // FIXES 138 (mw1_bounds.h): the mission areas MW1 gives the maps without their own, and the enemy places it moves.
  {
    std::string why;
    mw1::set_bounds(mw1::parse_bounds(mw1::kBoundsFile, &why));
    std::printf("mw1: %zu mission areas, %zu moved places in %s%s%s\n", mw1::bounds().bounds.size(), mw1::bounds().moves.size(),
                mw1::kBoundsFile, why.empty() ? "" : ": ", why.c_str());
  }
  // MW1_WORLD=<folder>ia<n> (a test override): every contract a battle on that world's Instant Action map, e.g.
  // mw3c2ia1. Without it the contracts choose their map and objective (mw1_missions.h, data/mw1maps.zrd).
  if (const char* w = std::getenv("MW1_WORLD")) {
    const std::string s = w;
    const size_t at = s.rfind("ia");
    if (at != std::string::npos && at > 0) mw1::set_game_loop_world(s.substr(0, at), std::atoi(s.c_str() + at + 2));
  }
  // MW3's worlds (mw1_worlds.h): converted into the write dir's worlds/ when due, then registered by name.
  const std::string worlds =
      !a.worlds.empty() ? a.worlds : a.write_dir.empty() ? std::string() : (std::filesystem::path(a.write_dir) / mw1::kWorldsFolder).string();
  const bool hidden = std::find(a.rest.begin(), a.rest.end(), "--hidden") != a.rest.end();
  // As mw3_main's start (the engine's, 2026-11-23): on the Mac a run no player is at stays a background process, never
  // taking the keyboard from the app in front when it starts or exits. Set here, before mw1's first SDL start (the
  // worlds' converting window), from the same arguments as PM's program() reads later (MW3_POINTER_HANDS_OFF=1 too).
  // As SDL_MAC_BACKGROUND_APP in the environment (tests/main.cpp's way), not SDL_SetHint: the converting window's
  // SDL_Quit drops the hints set, and SDL reads the variable at the game's own start after it too.
#if defined(__APPLE__)
  if (platform::pointer_hands_off() || no_player_at(a.rest)) setenv("SDL_MAC_BACKGROUND_APP", "1", 1);
#endif
  mw1::movie::set_hidden(hidden);  // FIXES.md item 54: no movie under --hidden (MW1_MOVIE=1 plays it anyway)
  const zipkit::Progress said = [](const std::string& world, const std::string& step, int done, int total) {
    std::printf("mw1:   %s: %s (%d of %d)\n", world.c_str(), step.c_str(), done + 1, total);
    std::fflush(stdout);
  };
  mw1::prepare_worlds(use.mw3, worlds, said, hidden ? mw1::Convert{} : mw1::Convert(convert_in_window),
                      [](const std::string& line) { std::printf("mw1: %s\n", line.c_str()), std::fflush(stdout); });
  for (const std::string& w : mw1::register_worlds(worlds)) std::printf("mw1: world %s from %s\n", w.c_str(), worlds.c_str());
  mw1::mission_maps::set_install(use.mw3);  // FIXES 140: MW3's mission maps, read from its install
  // The 2D's text and pictures at the window's resolution (FIXES.md item 12, research/hires-2d.md): MW1's alone.
  // MW1_HIRES=0 (a test override) keeps the canvas only, as PM's and MW3's programs draw.
  const char* hires = std::getenv("MW1_HIRES");
  platform::set_hires_2d(!(hires && std::strcmp(hires, "0") == 0));
  std::printf("mw1: high-resolution 2D %s\n", platform::hires_2d() ? "on" : "off (MW1_HIRES=0)");
  // FIXES.md item 82: the HUD pictures' numbers and letters drawn as text in PM's HUD face above canvas size.
  if (platform::hires_2d()) mw1::use_hud_label_font();
  // FIXES.md item 30 (the owner): the menus leave the mouse free fullscreen too (to a second screen), as MW3's do in a
  // window; a mission holds it.
  platform::set_menus_free_pointer(true);
  // FIXES.md item 83 (the lead): the pause's grab without the HUD's marker line (platform/pause_hud.h).
  platform::set_pause_hides_hud_marker(true);
  // FIXES.md item 83 (the lead): RESOLUTION's modes all "W x H", their own sizes (platform/mode_labels.h).
  platform::set_uniform_mode_labels(true);
  // FIXES 102 (the owner): the wheel scrolls the Mech Lab's lists as the arrows do (platform/list_wheel.h).
  platform::set_list_wheel(true);
  // FIXES.md item 88: the pause test's count of the grab's recorded texts (the SDL host's lists).
  mw1::recorded_lines() = [](const void* owner, const int32_t r[4], uint32_t colour) {
    auto* host = dynamic_cast<sdlfe::SdlTwodHost*>(&ztwod::host());
    if (!host) return -1;
    int32_t n = 0;
    if (const auto* cmds = host->hires_lists().of(owner))
      for (const platform::hires::Command& c : *cmds)
        n += c.kind == platform::hires::Kind::kText && c.colour == colour &&
             !platform::hires::intersect(c.rect, platform::hires::Rect{r[0], r[1], r[2], r[3]}).empty();
    return n;
  };
  // The rest is PM's game main, on its own write dir and PM install.
  std::vector<std::string> args = mw1::game_args(argv[0], a, use);
  std::vector<char*> raw;
  for (std::string& s : args) raw.push_back(s.data());
  raw.push_back(nullptr);
  return mw3_game_main(static_cast<int>(args.size()), raw.data());
}
