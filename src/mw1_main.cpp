// mw1: MechWarrior 1: Critical Hit (VISION.md, The code base). The third
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
//                      "MechWarrior 1 Critical Hit", apart from MW3's and PM's)
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
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gamez/zutil/zutl_path.h"
#include "mw1_game.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_save.h"
#include "mw1_start.h"
#include "mw1_worlds.h"
#include "pm/pm_game.h"

int mw3_game_main(int argc, char** argv);

namespace {

std::string usual_write_dir() {
  std::string dir;
  if (char* p = SDL_GetPrefPath(mw1::kOrg, mw1::kApp)) dir = p, SDL_free(p);
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir;
}

}  // namespace

namespace {
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

int main(int argc, char** argv) {
  mw1::Args a = mw1::parse(argc, argv);
  if (a.write_dir.empty()) a.write_dir = usual_write_dir();
  if (!a.write_dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(a.write_dir, ec);
  }
  const mw1::Installs saved = mw1::read_cfg(a.write_dir);
  mw1::Installs use = mw1::merge(a.given, saved);
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
  std::printf("mw1: MechWarrior 1: Critical Hit, write dir %s\nmw1:   Pirate's Moon %s\nmw1:   MechWarrior 3 %s\nmw1:   MechWarrior (1989) %s\n",
              a.write_dir.c_str(), use.pm.empty() ? "(not found)" : use.pm.c_str(), use.mw3.empty() ? "(not found)" : use.mw3.c_str(),
              use.mw1.empty() ? "(not found)" : use.mw1.c_str());
  mw1::set_mw1_game_dir(use.mw1);  // the Bar's pitches, from the player's MW_MAIN.EXE (mw1_pilots.h)
  // MW1's loop over PM's game: the hub at the main menu, the contract's mission, the settlement (mw1_game.h).
  // MW1_SEED (tests) fixes the company's and the contracts' rolls; MW1_REACH=<node> (a test override) makes every
  // contract reach that structure on c2's second Instant Action map.
  const char* seed = std::getenv("MW1_SEED");
  mw1::install_game_loop(seed ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 0, std::getenv("MW1_REACH"));
  // The company's save in the write dir (mw1_save.h); --new-company starts over (until the hub has a menu).
  if (!a.write_dir.empty()) mw1::set_game_loop_save((std::filesystem::path(a.write_dir) / mw1::kSaveFile).string(), a.new_company);
  // MW1's own data (the hub's layout), beside the program: mounted after what is (PM's install comes in the game main).
  if (const char* base = SDL_GetBasePath()) {
    const std::filesystem::path data = std::filesystem::path(base) / "mw1data";
    std::error_code ec;
    if (std::filesystem::is_directory(data, ec)) zutl::mount(data.string());
    else std::printf("mw1: no mw1data beside the program (%s): the hub is PM's message boxes\n", data.string().c_str());
  }
  // MW1's map table (mw1_missions.h): the maps and structures the contracts choose from; none, the milestone's map.
  {
    std::string why;
    mw1::set_map_table(mw1::parse_maps(mw1::kMapsFile, &why));
    std::printf("mw1: %zu maps in %s%s%s\n", mw1::map_table().size(), mw1::kMapsFile, why.empty() ? "" : ": ", why.c_str());
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
  const zipkit::Progress said = [](const std::string& world, const std::string& step, int done, int total) {
    std::printf("mw1:   %s: %s (%d of %d)\n", world.c_str(), step.c_str(), done + 1, total);
    std::fflush(stdout);
  };
  mw1::prepare_worlds(use.mw3, worlds, said, hidden ? mw1::Convert{} : mw1::Convert(convert_in_window),
                      [](const std::string& line) { std::printf("mw1: %s\n", line.c_str()), std::fflush(stdout); });
  for (const std::string& w : mw1::register_worlds(worlds)) std::printf("mw1: world %s from %s\n", w.c_str(), worlds.c_str());
  // The rest is PM's game main, on its own write dir and PM install.
  std::vector<std::string> args = mw1::game_args(argv[0], a, use);
  std::vector<char*> raw;
  for (std::string& s : args) raw.push_back(s.data());
  raw.push_back(nullptr);
  return mw3_game_main(static_cast<int>(args.size()), raw.data());
}
