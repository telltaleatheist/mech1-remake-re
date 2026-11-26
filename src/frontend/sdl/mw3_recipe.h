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

// What the player should know before playing with `install` (the status's notes). pm: check_install's warnings (the
// full sounds), then the music's; `once` (the status's notes_once) counts them all: the launcher shows Pirate's Moon's
// notes once until they change, the music's included, as its pm_notes= hash always has.
std::vector<std::string> mw3_notes(const std::string& install, const std::string& version, const std::string& write_dir,
                                   int* once = nullptr);
// What `install` lacks and plays without (the status's lacks), in the words of its --copies row's missing, from the
// same code. pm: "music", "full-quality sounds".
std::vector<std::string> mw3_lacks(const std::string& install, const std::string& version, const std::string& write_dir);

// The copy the status reports (docs/launcher-split.md, b2), in the order the launcher looked before. pm: the card's
// folder's install/, this program's own marked game folder's and the user folder's install/, mw3.cfg's install=, the
// installer's places, `more`. The first whole one (check_install) is taken; `offered` when it came from the
// installer's places: the launcher offers to copy it in.
struct StatusFind {
  bool ok = false, offered = false;
  bool own = false;  // from the game's own folders (the card INSTALL's, this program's): never saved as mw3.cfg's
  std::string dir, version, summary;  // dir and summary also when only an incomplete copy was found
};
StatusFind mw3_status_find(const RecipePlaces& places);

}  // namespace sdlfe
