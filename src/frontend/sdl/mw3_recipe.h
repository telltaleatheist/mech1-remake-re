// Ours, not the exe's: MechWarrior 3's recipe for the setup kit (re-setup-kit's rekit/sdl/setup_screen.h), so mw3
// answers a launcher's `--setup --describe | --search | --look | --install` itself (rekit/sdl/setup_serve.h;
// docs/launcher-split.md, a2). The finds are map_source's, the install install_from_source's, the search the start's.
// Its extras (a3): --copies (GAME FILES' list), --add <disc> (ADD FROM MY DISC), --first-run movies.
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

// What the player should know before playing with `install` (the status's notes): the start check's first, their
// count in `once` (the status's notes_once: a launcher shows those once until they change), then launcher_notes'
// (the data's version, the music, the movies), shown every time.
std::vector<std::string> mw3_notes(const std::string& install, const std::string& version, const std::string& write_dir,
                                   int* once = nullptr);
// What `install` lacks and plays without (the status's lacks), in the words of its --copies row's missing, from the
// same code.
std::vector<std::string> mw3_lacks(const std::string& install, const std::string& version, const std::string& write_dir);

// The copy the status reports (docs/launcher-split.md, b2), in the order the launcher looked before: the card
// INSTALL's folder's install/ and this program's own marked game folder's, mw3.cfg's install=, the user folder's
// install/, the installer's places, `more`. The first whole one (check_install) is taken; `offered` when it came
// from the installer's places, or is any other while the game's own folder is set: the launcher offers to copy it in.
struct StatusFind {
  bool ok = false, offered = false;
  std::string dir, version, summary;  // dir and summary also when only an incomplete copy was found
};
StatusFind mw3_status_find(const RecipePlaces& places);

}  // namespace sdlfe
