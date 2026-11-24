#pragma once
// Ours: the launcher's games on demand, the doing part (docs/launcher.md, Games on demand; the logic is
// upgrades/update/games.h). Each game lives in a folder of the launcher's own under the games folder, unpacked
// from that game's package for this platform: Windows' zip, the Mac's disk image (its app copied out), Linux's
// AppImage (unpacked with its own --appimage-extract). A folder is replaced or removed only when it holds the
// launcher's record (rebuild-game.txt), and the games folder only holds what the launcher put there.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "rekit/status.h"
#include "upgrades/update/games.h"

namespace sdlfe::games {

namespace u = upgrades::update;

// The games folder: MW3_GAMES_DIR (checks), else per user: %LOCALAPPDATA%\BattleTech\games,
// ~/Library/Application Support/BattleTech/games, $XDG_DATA_HOME (~/.local/share)/BattleTech/games (
// 2026-11-22: the family's folder, no longer MicroProse/BattleTech). Empty for a hidden run (checks) without
// MW3_GAMES_DIR, so a check never sees the player's games.
std::string root(bool hidden);

// The games folder of 1.3.x, <base>/MicroProse/BattleTech/games for `root`'s <base>/BattleTech/games: renamed to
// `root` once in a package (rekit::move_once), the saved paths in `cfg` that named it following, and the empty
// MicroProse/BattleTech left behind removed. Returns the folder to use: `root`, or the old one while it cannot be
// moved (a game running from it) or, in a development build, while it is the one with the games.
std::string root_moved(const std::string& root, const std::string& cfg);

// A game's program inside an unpacked package or an installed folder, wherever its package keeps it (Windows'
// <dir>/mw3.exe, the Mac's <dir>/<name>.app/Contents/MacOS/mw3, Linux's <dir>/squashfs-root/usr/bin/mw3); empty
// when it is not there.
std::string program_in(const std::string& dir, u::Game g);

// The game installed under `root`: its record and program; false when there is none (or no program in it).
bool installed(const std::string& root, u::Game g, u::InstalledGame* record, std::string* program);

// A program's version as its file holds it ("MW3-PROGRAM-VERSION:pm-v1.2.3"), read, never run. Empty when none.
std::string program_version(const std::string& bin);

// The game's repo's releases (MW3_GAMES_API: a URL with {repo} in it, file:// allowed, for checks), judged against
// the installed tag.
u::GameCheck check(u::Game g, const std::string& installed_tag);

using Progress = std::function<bool(double, const std::string&)>;  // the fraction, the words; false cancels
struct Done {
  bool ok = false, cancelled = false;
  std::string words, program;  // program: the installed game's, when ok
  u::Game game = u::Game::kMw3;
};

// INSTALL and UPDATE: the release's file for this platform and its SHA256SUMS downloaded, the file checked
// against its line (nothing installed without it), unpacked, and put in place of the game's folder.
Done install_release(const std::string& root, u::Game g, const u::Release& r, const Progress& progress);

// INSTALL FROM FILE: what a SHA256SUMS beside the file says of it.
struct FileCheck {
  enum Kind { kMatches, kNoSums, kMismatch } kind = kNoSums;
  std::string sha256, words;
};
FileCheck check_file(const std::string& file);
// The file unpacked; which game it is found from the program inside (one game's package, not the full one), then
// put in place of that game's folder. `sha256` is the file's (check_file's).
Done install_file(const std::string& root, const std::string& file, const std::string& sha256, const Progress& progress);

// The player's own folder for a game (2026-11-22: "everything can be installed in the same location the user
// picks"): its program (installed under it as under the games folder) and its game files (install/); pilots, saves,
// settings and mods stay in the user folder. A picked folder is the game's when it is empty or already carries this
// game's mark (rebuild-game-folder.txt), else a folder named for the game inside it ("MechWarrior 3", "Pirate's Moon",
// "MechWarrior 1 Remake"), so a drive's top or someone's folder is never filled loose.
std::string game_folder_in(const std::string& picked, u::Game g);
// The folder made the game's before anything goes in: created when missing, marked as the launcher's games folder
// (its program can go in) and as this game's. One holding anything else without the marks is never written to.
bool claim_game_folder(const std::string& dir, u::Game g, std::string* error);

// A kit game's answer to `bin --status` (re-setup-kit's launcher contract: one JSON object, exit 0, nothing on
// screen; MechWarrior 1 Rebuild first): false when it did not answer in time or its answer is not one.
bool kit_status(const std::string& bin, rekit::Status* out);

// Whether `bin` is inside the AppImage this program runs from ($APPDIR), so goes away with it.
bool in_own_mount(const std::string& bin);

// Ours (2026-11-24, docs/launcher.md, Copies installed without the launcher): a game of ours installed by its own
// Setup, disk image or AppImage is found and linked, and a player can link one by hand (a panel's gear).
// Each of our programs keeps this file in its user folder: where it is and its version (rebuild-program.txt).
constexpr const char* kProgramRecord = "rebuild-program.txt";
// This program's file as a player starts it ($APPIMAGE when it runs from that AppImage's mount, not one it inherited
// from the launcher's); empty when it runs from somewhere that goes
// away (the Mac's App Translocation, a disk image).
std::string self_path();
// Whether `prog` is in a game's folder the launcher made (its rebuild-game.txt in a folder above): never a copy
// installed without it.
bool launchers_own(const std::string& prog);
// The record written (only when it changed) for a released build not the launcher's own copy: a development one never
// links itself, and the launcher's copy never takes the place of one installed without it.
void record_self(const std::string& user_folder, u::Game g, const std::string& version);
// What `prog` is as game g's program: its version from its stamp, else (when `may_run`) from its --status (an
// AppImage's stamp is compressed); empty when it is not g's.
std::string identify(const std::string& prog, u::Game g, bool may_run);
// The program a player picked for g (the program, its folder, a .app, an AppImage); empty when none is there.
std::string program_of_pick(const std::string& pick, u::Game g);
struct Found {
  std::string program, version, how;  // how: "its record", "its Setup", "Applications"
};
// g's released copies installed without the launcher, newest first: the record in `user_folder`, Windows' uninstall
// entry for its Setup, the Mac's Applications folders (those two only when `system`: a check run reads its own folder
// alone). Each checked (identify): present and g's.
std::vector<Found> found_elsewhere(u::Game g, const std::string& user_folder, bool system);
// The launcher's own copy of g under `root` removed: only a folder carrying the launcher's record. False with why.
bool uninstall(const std::string& root, u::Game g, std::string* error);

}  // namespace sdlfe::games
