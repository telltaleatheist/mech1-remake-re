#pragma once
// MechWarrior 3's and Pirate's Moon's setup calls (mech3/game_setup.h) in the launcher's own Work types (launcher
// split b1): the one place the launcher still names mech3::setup, until b2 and b3 put these behind the games' served
// verbs and this file goes. Each passes straight through.

#include <string>
#include <vector>

#include "frontend/sdl/launcher_own.h"
#include "mech3/source_map.h"

namespace sdlfe::mw3setup {

own::Result check_install(const std::string& dir, const std::string& write_dir, bool pirates_moon = false);
own::Result start_check(const std::string& dir, const std::string& write_dir, bool pirates_moon);
own::Result install_from_source(const mech3::source::Found& found, const std::string& out_dir, const std::string& patch,
                                const std::string& music_dir, const std::string& write_dir, const own::ProgressFn& progress);
own::Result add_from_disc(const std::string& source, const std::string& video_dir, const std::string& music_dir,
                          const own::ProgressFn& progress);
own::Result add_pm_from_disc(const std::string& source, const std::string& music_dir, const std::string& write_dir,
                             const own::ProgressFn& progress);
own::Result apply_patch(const std::string& source, const std::string& write_dir, const own::ProgressFn& progress);
// The drives holding an MW3 disc: their roots.
std::vector<std::string> find_discs();

}  // namespace sdlfe::mw3setup
