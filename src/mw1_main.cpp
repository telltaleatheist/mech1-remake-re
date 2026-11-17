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
//   --write-dir DIR    settings and saves (default: the user's app data,
//                      "MechWarrior 1 Critical Hit", apart from MW3's and PM's)
//
// Each folder given is checked and saved to mw1.cfg in the write dir, so the
// next start without them (the launcher's or a plain one) uses them again.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "mw1_game.h"
#include "mw1_start.h"
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
  // MW1's loop over PM's game: the hub at the main menu, the contract's mission, the settlement (mw1_game.h).
  // MW1_SEED (tests) fixes the company's and the contracts' rolls; MW1_REACH=<node> makes the contracts
  // reach-a-structure (the milestone's second objective).
  const char* seed = std::getenv("MW1_SEED");
  mw1::install_game_loop(seed ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 0, std::getenv("MW1_REACH"));
  // The rest is PM's game main, on its own write dir and PM install.
  std::vector<std::string> args = mw1::game_args(argv[0], a, use);
  std::vector<char*> raw;
  for (std::string& s : args) raw.push_back(s.data());
  raw.push_back(nullptr);
  return mw3_game_main(static_cast<int>(args.size()), raw.data());
}
