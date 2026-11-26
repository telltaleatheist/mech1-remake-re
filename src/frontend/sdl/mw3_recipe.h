// Ours, not the exe's: MechWarrior 3's recipe for the setup kit (re-setup-kit's rekit/sdl/setup_screen.h), so mw3
// answers a launcher's `--setup --describe | --search | --look | --install` itself (rekit/sdl/setup_serve.h;
// docs/launcher-split.md, a2). The finds are map_source's, the install install_from_source's, the search the start's.
#pragma once

#include <string>
#include <vector>

#include "rekit/sdl/setup_screen.h"

namespace sdlfe {

// The recipe for the user folder `write_dir` (a disc's staging goes there); `cfg_install` is mw3.cfg's install=,
// `more` the start's other places (the development install, <write dir>/install), searched as at a start.
rekit::sdl::Game mw3_recipe(const std::string& write_dir, const std::string& cfg_install, const std::vector<std::string>& more);

}  // namespace sdlfe
