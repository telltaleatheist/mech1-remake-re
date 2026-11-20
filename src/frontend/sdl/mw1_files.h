// Ours: the launcher's MECHWARRIOR 1 line taking the player's MechWarrior
// (1989) the way MW3's and Pirate's Moon's are taken (docs/launcher.md,
// MechWarrior 1's files): its folder or any file in it, a folder holding it a
// level or two down, a download (.zip and the other archives SETUP reads,
// unpacked into the write dir's mw1/game), and the usual places searched
// when nothing is picked yet.
#pragma once

#include <string>
#include <vector>

namespace sdlfe::mw1 {

// A MechWarrior (1989) folder: BTECH.EXE in it (any case).
bool game_valid(const std::string& dir);
// MW_MAIN.EXE beside it (any case): without it MW1's Bar keeps ORDER DRINK and NEWS greyed.
bool has_main(const std::string& dir);
// `dir` when it is the game, else the first folder under it, `depth` levels
// down at most, that is (shallowest first, names sorted); "" when none.
std::string game_in(const std::string& dir, int depth = 2);

// The folder our unpacking makes: <write dir>/mw1/game.
std::string unpacked_dir(const std::string& write_dir);
// A download of the game unpacked into unpacked_dir: the archive's folder
// that holds BTECH.EXE, with everything under it. A folder there that we did
// not make (no marker) is never replaced. "" and `error` (plain words) on failure.
std::string unpack(const std::string& archive, const std::string& write_dir, std::string* error);

// What a pick from the MECHWARRIOR 1 line stands for: an archive unpacked,
// a folder searched (game_in), any other file its folder's. "" and `error`.
std::string game_of_pick(const std::string& path, const std::string& write_dir, std::string* error);

// The places MW1 is likely to be, in order (our unpacked copy first, then
// beside this program, then the usual DOS folders), for find_mw1 when none
// was picked. Only named folders are looked into, never a whole home folder
// (on the Mac that would ask for Documents' and Downloads' permission).
std::vector<std::string> search_places(const std::string& program_dir, const std::string& write_dir);
// The first of search_places that holds the game; "" when none.
std::string search(const std::string& program_dir, const std::string& write_dir);

}  // namespace sdlfe::mw1
