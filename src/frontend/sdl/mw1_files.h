// Ours: the launcher's MECHWARRIOR 1 line taking the player's MechWarrior
// (1989) the way MW3's and Pirate's Moon's are taken (docs/launcher.md,
// MechWarrior 1's files): its folder or any file in it, a folder holding it a
// level or two down, a download (.zip and the other archives SETUP reads,
// unpacked into the write dir's mw1/game), and the usual places searched
// when nothing is picked yet.
#pragma once

#include <cstdint>
#include <functional>
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

// Whether Elthial's port can play this folder's combat: its BTECH.EXE is the one revision the port unpacks
// (its CombatInstallation.c pins the whole file's CRC-32 at 0x36A13E1A; 174,267 bytes). Any other revision fails
// only at the first battle, so the launcher says so first (2026-11-20, the mw1 lead and dev 1).
bool original_supported(const std::string& dir);
inline constexpr uint32_t kPortBtechCrc = 0x36A13E1Au;
inline constexpr uint64_t kPortBtechSize = 174267;

// The folder our unpacking makes: <write dir>/mw1/game.
std::string unpacked_dir(const std::string& write_dir);
// A download of the game unpacked into unpacked_dir: the archive's folder
// that holds BTECH.EXE, with everything under it. A folder there that we did
// not make (no marker) is never replaced. "" and `error` (plain words) on failure.
std::string unpack(const std::string& archive, const std::string& write_dir, std::string* error);

// Whether `dir` is unpacked_dir or inside it (links followed): our own copy, used where it is.
bool in_unpacked(const std::string& dir, const std::string& write_dir);

// A MechWarrior (1989) folder copied into unpacked_dir, so the player's own may be deleted afterwards (a project
// decision, 2026-11-21): the kit's copy_folder into <unpacked_dir>.copying (room checked, links followed, a failure
// or a cancel leaving nothing), marked as ours, then swapped in (rekit::swap_in_folder: a folder there that we did
// not make is never replaced; the old copy kept until the new one is in). A folder already in_unpacked is used where
// it is, never copied onto itself. The folder now holding the game, or "" with `error` (plain words) and
// `*cancelled` set when `progress` said stop.
using CopyProgress = std::function<bool(uint64_t done, uint64_t total, const std::string& file)>;
std::string copy_in(const std::string& game, const std::string& write_dir, std::string* error,
                    const CopyProgress& progress = nullptr, bool* cancelled = nullptr);

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
