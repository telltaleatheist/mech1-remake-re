#pragma once
// The MW1 remake's start (VISION.md, The code base): which installs it
// uses and where it keeps its settings, before PM's game main takes over.
// No SDL here, so the tests read it directly.
#include <cstdint>
#include <string>
#include <vector>

#include "rekit/status.h"

namespace mw1 {

// The program's folder under the platform's app data, apart from MW3's and Pirate's Moon's: the family's
// "BattleTech/MechWarrior 1 Remake" (FIXES 147: every game of the family under BattleTech, as PM's since the
// launcher 1.3.2). Before 1.1.0 it was "MicroProse/MechWarrior 1 Remake", and before FIXES 139
// "MicroProse/MechWarrior 1 Critical Hit": both moved to it at the start (settle_user_folder).
inline constexpr const char* kOrg = "BattleTech";
inline constexpr const char* kOldOrg = "MicroProse";  // before 1.1.0
inline constexpr const char* kApp = "MechWarrior 1 Remake";
inline constexpr const char* kOldApp = "MechWarrior 1 Critical Hit";  // before FIXES 139: under kOldOrg
inline constexpr const char* kTitle = "MechWarrior 1 Remake";        // the window's title, the log's and the boxes'
inline constexpr const char* kPmApp = "Pirate's Moon";  // PM's own write dir, beside ours (mw3pm's default)
inline constexpr const char* kCfg = "mw1.cfg";

// The three installs the player brings (VISION.md, pillar 4).
// FIXES.md item 100: the sound settings MW1's OPTIONS keeps (mw1_music.h), in mw1.cfg beside the installs. -1: never
// set (the game's own, PM's settings, stand).
struct SoundCfg {
  int32_t music = -1;           // music=1 on, 0 off
  int32_t music_volume = -1;    // music_volume=0..100
  int32_t effects_volume = -1;  // effects_volume=0..100
  // Written out: the project is C++17 (a defaulted == is C++20: GCC refuses it, FIXES.md item 105's Linux build).
  bool operator==(const SoundCfg& o) const {
    return music == o.music && music_volume == o.music_volume && effects_volume == o.effects_volume;
  }
  bool operator!=(const SoundCfg& o) const { return !(*this == o); }
};

struct Installs {
  std::string pm;   // Pirate's Moon: the game's own install (mw3pm's --install)
  std::string mw3;  // MechWarrior 3: its worlds c2 and c4b (research/maps.md)
  std::string mw1;  // MechWarrior (1989), the DOS game: its BTECH.EXE folder
  SoundCfg sound;   // not an install: kept with them (merge() keeps the saved one)
  int32_t save_slot = -1;  // FIXES 125: the save slot in play, 1..kSaveSlots (-1: none written; slot 1)
};

// What mw1 takes before handing the rest to the game main.
struct Args {
  Installs given;                 // --install, --mw3-install, --mw1-game; the launcher's --with <id>=<where>
  std::string write_dir;          // --write-dir, else the usual folder
  bool new_company = false;       // --new-company: start over (mw1's own)
  std::string worlds;             // --mw1-worlds: MW3's converted worlds' folder, else <write dir>/worlds
  std::vector<std::string> rest;  // everything else, for the game main, in order
};

// argv[1..] split into mw1's own and the game's. --install and --write-dir
// are read and also left in `rest` (the game main takes them too).
Args parse(int argc, char** argv);
// Whether this start is recorded in the user folder for the launcher (its rebuild-program.txt, sdl_games.h): a
// player's, so not a --hidden run or a --status answer.
bool records_program(const std::vector<std::string>& rest);

// mw1.cfg in the write dir: install=, mw3_install=, mw1_game=, the sound's music=, music_volume=, effects_volume=, and
// save_slot= (FIXES 125) (a line each,
// paths as zutl::path_from_config reads them). A missing file reads empty.
Installs read_cfg(const std::string& write_dir);
bool write_cfg(const std::string& write_dir, const Installs& in);

// `given` over `saved`, field by field: a path on the command line wins and
// is what gets saved.
Installs merge(const Installs& given, const Installs& saved);

// What a folder is, for the MW3 install: by its zbd/reader.zbd's zArchive
// version (MW3 1, PM 2; pm::install_kind). True only for MW3's.
bool is_mw3_install(const std::string& dir);
// MW1's DOS folder: BTECH.EXE in it, any case (as the launcher checks).
bool is_mw1_install(const std::string& dir);

// FIXES 139: the write dir under its old name, moved to the new one at the start (the usual folder only, not a
// --write-dir). The old folder is moved when it exists and the new one is missing or empty: renamed in place, else
// copied beside the new one, every file compared, the copy renamed into place and only then the old one removed.
// Nothing is merged or overwritten: with files in both, the new one is used and the old one left alone. A move that
// fails leaves the old folder as it was (a half copy removed) and the run uses the old folder.
enum class Moved {
  kNothing,   // no old folder: the new one, as it is
  kRenamed,   // the old folder renamed to the new name
  kCopied,    // copied and checked; `why` says when the old one could not then be removed (left, the new one used)
  kBothKept,  // files in both: the new one used, the old one left
  kFailed,    // not moved (`why`): the old one used for this run
};
struct WriteDirMove {
  Moved how = Moved::kNothing;
  std::string use;  // the write dir for this run
  std::string why;  // what failed, if anything
};
// `try_rename` false goes straight to the copy (the tests' way to reach it).
WriteDirMove move_write_dir(const std::string& old_dir, const std::string& new_dir, bool try_rename = true);
const char* moved_words(Moved how);

// FIXES 147: the user folder settled at the start, `usual` being <base>/BattleTech/MechWarrior 1 Remake (the usual
// folder only, never a --write-dir). In order:
//  1. FIXES 139's move: <base>/MicroProse/MechWarrior 1 Critical Hit to <base>/MicroProse/MechWarrior 1 Remake
//     (move_write_dir), unless `usual` holds something already. A failure uses the Critical Hit folder this run.
//  2. <base>/MicroProse/MechWarrior 1 Remake renamed to `usual` once, as PM's own folder moved (rekit::move_once:
//     never over a folder with something in it, never a copy), and the same pointer file PM leaves put in the old
//     place (data-folder.txt, naming `usual`). A rename that fails (a file in use, a read-only parent) uses the
//     old folder this run and is tried again at the next start.
// A fresh install has neither old folder and starts in `usual`. `move` false (a development build, on the
// developer's own folders): nothing is moved, and an old folder with the data is used while `usual` holds nothing.
struct UserFolder {
  std::string use;                // the write dir for this run
  std::vector<std::string> said;  // the log's lines: what moved, what was kept, what failed
};
UserFolder settle_user_folder(const std::string& usual, bool move = true);

// FIXES 145: where a saved install now is when a family folder it was inside has moved: for a path inside
// <base>/MicroProse/<Game>, the same place inside <base>/BattleTech/<Game>, then inside the folder the old place's
// pointer file names (data-folder.txt). The candidates in that order, unchecked (the caller checks each as an
// install); none for a path inside no MicroProse folder.
std::vector<std::string> moved_install_places(const std::string& saved);

// The first of moved_install_places(`dir`) that `holds` the game; `dir` itself when it holds it or none does.
std::string follow_install(const std::string& dir, bool (*holds)(const std::string&));
bool is_pm_install(const std::string& dir);  // pm::install_kind's Pirate's Moon

// `mw1 --status` (the launcher contract, re-setup-kit's DESIGN.md): what MW1 has. A pure read: `folder` is the user
// folder in use (settle_user_folder with `move` false), where mw1.cfg is read, and `given` the command line's
// installs. ready with all three games found, missing with none, else incomplete. The id is the Remake's own
// ("mech1" is the 1989 game's rebuild); the version the program's tag, as --version and the record say it.
inline constexpr const char* kStatusId = "mech1-remake";
rekit::Status status(const std::string& folder, const Installs& given, const std::string& version);

// The game main's argv: the program's name, --write-dir (unless given), then
// `rest`, with --install added when only mw1.cfg named PM's.
std::vector<std::string> game_args(const std::string& argv0, const Args& a, const Installs& use);

}  // namespace mw1
