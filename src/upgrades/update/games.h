#pragma once
// Ours, not the original's: the launcher downloading each game from its own repo's Releases (Owen 2026-11-22,
// docs/launcher.md, Games on demand). Each card asks its game's repo for the newest release; nothing is installed
// until the player presses INSTALL or UPDATE. This part is the logic only (which repo, tag and file, the record of
// an installed game, a card's verdict); the fetching and unpacking are the front end's (frontend/sdl/sdl_games.h).
#include <string>
#include <vector>

#include "upgrades/update/update.h"

namespace upgrades::update {

// kMw1r: MechWarrior 1 Rebuild (mech1-re), the 1989 game rebuilt, MW1's card's ORIGINAL tab (Owen 2026-11-22).
// kChi, kChirm: Inception Rebuild (inception-re) and Inception Remake (inception-remake-re), the INCEPTION card's
// tabs: COMING SOON until their repos have a release, then INSTALL, with no launcher change (Owen 2026-11-22).
enum class Game { kMw3, kPm, kMw1, kMw1r, kChi, kChirm };
constexpr Game kGames[] = {Game::kMw3, Game::kPm, Game::kMw1, Game::kMw1r, Game::kChi, Game::kChirm};
constexpr int kGameCount = 6;

struct GameInfo {
  const char* key;      // "mw3", "pm", "mw1": its folder under the games folder, its record's game=
  const char* name;     // "Pirate's Moon", for the words
  const char* repo;     // its GitHub repo (telltaleatheist/<repo>)
  const char* prefix;   // its tags: "v", "pm-v", "mw1-v" followed by X.Y.Z
  const char* program;  // its program's file name without .exe: "mw3", "mw3pm", "mw1"
};
const GameInfo& game_info(Game g);

// A release tag of this game: its prefix and X.Y.Z, nothing after ("pm-v1.2.3"; not "pm-v1.2.3-rc1").
bool game_tag(Game g, const std::string& tag);

// The file a game's release has for a platform, as its packaging names it (docs/release.md):
//   MechWarrior 3   MW3-alone-<tag>-win64.zip    MW3-alone-<tag>-mac.dmg    MW3-alone-<tag>-x86_64.AppImage
//   Pirate's Moon   mw3pm-<tag>-win64.zip        PM-Rebuild-<tag>-mac.dmg   PM-Rebuild-<tag>-x86_64.AppImage
//   MW1 Remake      MW1-Remake-<tag>-win64.zip   MW1-Remake-<tag>-mac.dmg   MW1-Remake-<tag>-x86_64.AppImage
//   MW1 Rebuild     MW1-Rebuild-<tag>-win64.zip  MW1-Rebuild-<tag>-mac.dmg  MW1-Rebuild-<tag>-x86_64.AppImage
//   Inception Reb.  CHI-Rebuild-<tag>-win64.zip  CHI-Rebuild-<tag>-mac.dmg  CHI-Rebuild-<tag>-x86_64.AppImage
//   Inception Rem.  CHI-Remake-<tag>-win64.zip   CHI-Remake-<tag>-mac.dmg   CHI-Remake-<tag>-x86_64.AppImage
// Windows takes the zip, never the Setup: the launcher unpacks into a folder of its own.
std::string game_asset_name(Game g, const std::string& tag, Platform p);
const Asset* game_asset(Game g, const Release& r, Platform p);

// The newest release of the game that has this platform's file: not a draft or a prerelease. Null when none.
const Release* newest_game(Game g, const std::vector<Release>& releases, Platform p);

// What the launcher keeps beside an installed game (its folder's rebuild-game.txt): a few key=value lines. The
// file also marks the folder as the launcher's own, the only kind it ever replaces or removes.
struct InstalledGame {
  std::string game;    // its key
  std::string tag;     // "pm-v1.2.3"; for a file of unknown origin, its program's own version
  std::string file;    // the package it came from
  std::string sha256;  // that file's, lower case
  std::string from;    // "github" (INSTALL, UPDATE) or "file" (INSTALL FROM FILE)
};
constexpr const char* kInstalledName = "rebuild-game.txt";
std::string installed_text(const InstalledGame& in);
bool parse_installed(const std::string& text, InstalledGame* out);

// A card's answer from its repo's releases: `installed` is the installed game's tag (empty when none).
struct GameCheck {
  enum Kind { kAsking, kFailed, kNoRelease, kNewer, kUpToDate } kind = kAsking;
  Release release;    // kNewer, kUpToDate: the newest one
  const Asset* asset() const;  // its file for this platform (from `release`)
  Platform platform = this_platform();
  std::string words;  // for the log and the status line
};
GameCheck judge_game(Game g, const std::string& installed, const Answer& answer, Platform p);

// A tag as a player reads it: "1.2.3" from "pm-v1.2.3", "1.0.7" from "mw1-v1.0.7".
std::string game_version(const std::string& tag);
// Two of a game's tags (or versions) by their numbers: <0, 0, >0.
int compare_game(const std::string& a, const std::string& b);

// The size of a download for the words: "48 MB".
std::string size_words(uint64_t bytes);

}  // namespace upgrades::update
