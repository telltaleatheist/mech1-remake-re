#pragma once
// Ours, not original code (docs/program.md, The data folder): the player's
// folder (settings, pilots, saves, converted movies) elsewhere than the usual
// place, on another drive say. A pointer file in the usual place names it, and a
// marker in the folder says it is ours. Moving copies, verifies, switches the
// pointer, and only then may remove the old copy, file by file: nothing here
// removes a folder's contents it did not itself copy or make. The setup kit's
// since 2026-11-25 (rekit/data_folder.h); these give it MechWarrior 3's name.
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "rekit/data_folder.h"

namespace mech3::data_folder {

using rekit::data_folder::kCopying;
using rekit::data_folder::kMarker;
using rekit::data_folder::kPointer;

// The folder to use. No pointer: `usual`, as if none of this existed. A pointer
// whose folder cannot be used (drive not connected, read-only, full): `error`
// says so and `dir` is that folder, never a fresh one in its place.
using Resolved = rekit::data_folder::Resolved;
Resolved resolve(const std::string& usual);

// "" if the folder can be used: it is there, it has the marker, a small write works.
std::string unusable(const std::string& dir);

// Where a picked folder means to move to: itself if it is empty or holds no
// data of ours, else a `name` folder inside it (a drive's root): "MechWarrior
// 3 Data", Pirate's Moon's "Pirate's Moon Data".
std::string destination(const std::string& picked, const std::string& usual, const char* name = "MechWarrior 3 Data");

// A move's check before starting: `error` if it cannot be done.
using Plan = rekit::data_folder::Plan;
Plan plan(const std::string& from, const std::string& to, const std::string& usual);

using Moved = rekit::data_folder::Moved;
// The copy, each file read back and compared; mw3.cfg's paths under `from` now
// under `to`; the pointer switched last. On failure or cancel what this made
// is taken away again and the pointer is as it was. The log files stay behind:
// they are in use, and each start makes new ones.
Moved move_data(const std::string& from, const std::string& to, const std::string& usual,
                const std::function<bool(double, const std::string&)>& progress);

using Removed = rekit::data_folder::Removed;
// The player's yes after a move: the files move_data copied, only when the
// pointer names `to` and `to` still has every one. Folders only when empty.
Removed remove_old(const std::string& from, const std::string& to, const std::vector<std::string>& copied,
                   const std::string& usual);

// Only plain ASCII in the path: what plan asks of a destination on Windows.
bool plain_name(const std::string& path);

// Tests: a drive's free bytes, when set; a call right after the pointer switches.
const uint64_t*& free_for_test();
std::function<void()>& after_switch_for_test();

// `path` under `from` as the same place under `to`; else as it was.
std::string moved_path(const std::string& path, const std::string& from, const std::string& to);

// The family folder (2026-11-22; docs/program.md, The family folder): `usual` is
// <base>/BattleTech/<Game>; a player's <base>/MicroProse/<Game> from before is renamed to it once
// (rekit::move_once), a pointer left in the old place so an earlier version of any of the games still finds
// the same data, and the saved paths in every family folder's mw3.cfg that named a moved folder made to name
// it where it is now. Returns the folder to use: `usual`, or the old one while it cannot be moved (a file in
// use: tried again at the next start). `move` false (a development build, on the developer's own folders):
// nothing moved, the old folder used while it is the one with the data.
std::string family_move(const std::string& usual, bool move = true);

// Pirate's Moon's folder as Windows launchers up to 1.3.1 had it, inside MechWarrior 3's (its path's end read wrong):
// what `usual` (<base>/BattleTech/Pirate's Moon) lacks of <base>/BattleTech/MechWarrior 3/Pirate's Moon, and of the
// same under MicroProse, moved to it, renamed on the same drive (2026-11-24). Never over a file: one in both stays in
// both, the player's own kept in use (those of the folder with pilots, when the other has none: the two change
// places). Once, a note left there; a folder whose data lives elsewhere (a pointer) is left alone. The files moved.
int adopt_nested(const std::string& usual);

// `cfg`'s saved paths under each moved pair's first folder made to name the same place under its second.
void retarget_config(const std::string& cfg, const std::vector<std::pair<std::string, std::string>>& moved);

}  // namespace mech3::data_folder
