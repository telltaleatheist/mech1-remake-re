#pragma once
// MW3's worlds for MW1 (VISION.md: MW3's worlds converted to PM's formats at
// first run). At each start, once MechWarrior 3's install is known, the
// worlds folder (<mw1 write dir>/worlds) is checked against its
// zipkit.stamp; zipkit (tools/zipkit, zipkit::mw3_to_pm) converts them when
// the stamp is missing or zipkit's version, the worlds or the install's
// files (sizes, CRCs) differ. Then the worlds a whole run left there are
// registered by name (mw3c2, mw3c4b), never by number. MW3's and PM's files
// are only read; everything is written under the worlds folder.
#include <functional>
#include <string>
#include <vector>

#include "zipkit.h"

namespace mw1 {

// MW3's worlds as MW1 knows them: MW3's folder, the converted world's folder
// and Instant Action's name ("" when it has no Instant Action maps). A world
// is always named by its folder, never its number (from 9, in the order
// worlds register: the player's mods register after these).
struct World {
  const char* source;
  const char* folder;
  const char* ia_name;
};
inline constexpr World kWorlds[] = {{"c2", "mw3c2", "Grasslands"}, {"c4b", "mw3c4b", ""}};
inline constexpr const char* kWorldsFolder = "worlds";  // under mw1's write dir

// Why the worlds in `dir` need converting from MW3's `install`; empty when
// they are current (the stamp is this zipkit's, for kWorlds, from the
// install's files as they are now, and everything it made is there).
std::string worlds_stale(const std::string& install, const std::string& dir);

// True when `dir` holds a whole zipkit run: its stamp names kWorlds and
// everything it says it made is there. A run that failed leaves the last
// whole one (or nothing) in place, never part of a world.
bool worlds_whole(const std::string& dir);

// The conversion, as the start runs it (the program wraps it in its window).
using Convert = std::function<bool(const zipkit::Options&, std::string* error)>;

// The start's step: nothing without MW3 (one line saying its worlds need
// it); the stamp checked; when stale, `convert` (null: zipkit::mw3_to_pm) run
// with `progress`, and what a failed run left in its temp folder removed.
// Each line to say goes to `say` as it happens. True when a conversion ran
// and succeeded.
bool prepare_worlds(const std::string& install, const std::string& dir, const zipkit::Progress& progress, const Convert& convert,
                    const std::function<void(const std::string&)>& say);

// The one place mw1 registers its worlds: when `dir` holds a whole run, it is
// mounted after what is mounted already and each kWorlds folder is added to
// the game's world list (mech3::mod_worlds) and, when it has readeria1.zbd
// and a name, to Instant Action's (mech3::mod_ia_worlds). No mod and nothing
// in MODS. The folders registered, in order.
std::vector<std::string> register_worlds(const std::string& dir);

}  // namespace mw1
