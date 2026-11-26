// The Remake's MechWarrior (1989) files, for its served setup (mw1_recipe.h): its folder or any file in it, a folder
// holding it a level or two down, a download (.zip and the other archives SETUP reads, unpacked into the write dir's
// mw1/game), and the usual places searched when nothing is picked yet. Until launcher split b5 the launcher's
// (mw3-re's src/frontend/sdl/mw1_files, which went with its own MW1 finder); its Elthial-port revision check stayed
// behind with the launcher's PLAY ORIGINAL.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mw1::files {

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

// Whether `dir` is unpacked_dir or inside it (links followed): our own copy, used where it is.
bool in_unpacked(const std::string& dir, const std::string& write_dir);

// A MechWarrior (1989) folder copied into unpacked_dir, so the player's own may be deleted afterwards (a project
// decision, 2026-11-21): the kit's copy_folder into <unpacked_dir>.copying (room checked, links followed, a failure
// or a cancel leaving nothing), marked as ours, then swapped in (rekit::swap_in_folder: a folder there that we did
// not make is never replaced; the old copy kept until the new one is in). A folder already in_unpacked is used where
// it is, never copied onto itself. A folder holding more than `max_bytes` (the game is about 1 MB; a pick of a
// Games folder or a drive's root that happens to hold BTECH.EXE is far more) is refused before anything is copied.
// The folder now holding the game, or "" with `error` (plain words) and `*cancelled` set when `progress` said stop.
inline constexpr uint64_t kMaxCopyBytes = uint64_t(512) << 20;
using CopyProgress = std::function<bool(uint64_t done, uint64_t total, const std::string& file)>;
std::string copy_in(const std::string& game, const std::string& write_dir, std::string* error,
                    const CopyProgress& progress = nullptr, bool* cancelled = nullptr,
                    uint64_t max_bytes = kMaxCopyBytes);

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

}  // namespace mw1::files
