// Ours: the Remake's recipe for the setup kit (re-setup-kit's rekit/sdl/setup_screen.h), so mw1 answers a launcher's
// `--setup --describe | --search | --look | --install` for MechWarrior (1989)'s files itself (rekit/sdl/setup_serve.h;
// mw3-re's docs/launcher-split.md, a5). The finds and the copy are mw1_files' (frontend/sdl/mw1_files.h): the game's
// folder, any file in it, a folder a level or two above it, or a download; copied or unpacked into <write dir>/mw1/game.
#pragma once

#include <string>
#include <vector>

#include "rekit/sdl/setup_screen.h"

namespace mw1 {

// `write_dir`: the Remake's user folder (mw1.cfg's); `program_dir`: this program's, for the search beside it.
rekit::sdl::Game files_recipe(const std::string& write_dir, const std::string& program_dir);

// A launcher's ask after `--setup` in argv (its verb and the verb's argument), as mw3's start reads it; empty when
// there is none (no --setup, or the screen's). A `--folder <dir>` after it is dropped, `dropped` set: the Remake's copy
// of MechWarrior (1989) is always <write dir>/mw1/game (about 1 MB), and no game folder is claimed for it.
std::vector<std::string> setup_ask(int argc, char** argv, bool* dropped);

}  // namespace mw1
