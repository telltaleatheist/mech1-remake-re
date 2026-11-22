#pragma once
// Ours: the launcher's games on demand, the doing part (docs/launcher.md, Games on demand; the logic is
// upgrades/update/games.h). Each game lives in a folder of the launcher's own under the games folder, unpacked
// from that game's package for this platform: Windows' zip, the Mac's disk image (its app copied out), Linux's
// AppImage (unpacked with its own --appimage-extract). A folder is replaced or removed only when it holds the
// launcher's record (rebuild-game.txt), and the games folder only holds what the launcher put there.
#include <cstdint>
#include <functional>
#include <string>

#include "upgrades/update/games.h"

namespace sdlfe::games {

namespace u = upgrades::update;

// The games folder: MW3_GAMES_DIR (checks), else per user: %LOCALAPPDATA%\MicroProse\BattleTech\games,
// ~/Library/Application Support/MicroProse/BattleTech/games, $XDG_DATA_HOME (~/.local/share)/MicroProse/BattleTech/
// games. Empty for a hidden run (checks) without MW3_GAMES_DIR, so a check never sees the player's games.
std::string root(bool hidden);

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

// Whether `bin` is inside the AppImage this program runs from ($APPDIR), so goes away with it.
bool in_own_mount(const std::string& bin);

}  // namespace sdlfe::games
