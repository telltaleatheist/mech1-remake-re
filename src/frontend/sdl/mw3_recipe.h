// Ours, not the exe's: MechWarrior 3's recipe for the setup kit (re-setup-kit's rekit/sdl/setup_screen.h), so mw3
// answers a launcher's `--setup --describe | --search | --look | --install` itself (rekit/sdl/setup_serve.h;
// docs/launcher-split.md, a2). The finds are map_source's, the install install_from_source's, the search the start's.
// Its extras (a3): --copies (GAME FILES' list), --add <disc> (ADD FROM MY DISC), --first-run movies.
// pm: Pirate's Moon's recipe here (a4): game "mech3-pm", its words, add_pm_from_disc, the full sounds.
#pragma once

#include <string>
#include <vector>

#include "rekit/sdl/setup_screen.h"

namespace sdlfe {

struct RecipePlaces {
  std::string write_dir;          // the user folder (a disc's staging goes there)
  std::string cfg_install;        // mw3.cfg's install=
  std::vector<std::string> more;  // the start's other places (the development install, <write dir>/install)
  std::string games_place;        // mw3.cfg's games_mw3=: the folder the launcher's INSTALL put the game in
};
rekit::sdl::Game mw3_recipe(const RecipePlaces& places);

// What the player should know before playing with `install` (the status's notes): launcher_notes' (the data's
// version, the music, the movies) and the start check's.
std::vector<std::string> mw3_notes(const std::string& install, const std::string& version, const std::string& write_dir);

}  // namespace sdlfe
