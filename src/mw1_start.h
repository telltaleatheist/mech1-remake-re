#pragma once
// MW1: Critical Hit's start (VISION.md, The code base): which installs it
// uses and where it keeps its settings, before PM's game main takes over.
// No SDL here, so the tests read it directly.
#include <string>
#include <vector>

namespace mw1 {

// The program's folder name under the platform's app data, apart from MW3's
// and Pirate's Moon's ("MicroProse/MechWarrior 1 Critical Hit").
inline constexpr const char* kOrg = "MicroProse";
inline constexpr const char* kApp = "MechWarrior 1 Critical Hit";
inline constexpr const char* kCfg = "mw1.cfg";

// The three installs the player brings (VISION.md, pillar 4).
struct Installs {
  std::string pm;   // Pirate's Moon: the game's own install (mw3pm's --install)
  std::string mw3;  // MechWarrior 3: its worlds c2 and c4b (research/maps.md)
  std::string mw1;  // MechWarrior (1989), the DOS game: its BTECH.EXE folder
};

// What mw1 takes before handing the rest to the game main.
struct Args {
  Installs given;                 // --install, --mw3-install, --mw1-game
  std::string write_dir;          // --write-dir, else the usual folder
  std::vector<std::string> rest;  // everything else, for the game main, in order
};

// argv[1..] split into mw1's own and the game's. --install and --write-dir
// are read and also left in `rest` (the game main takes them too).
Args parse(int argc, char** argv);

// mw1.cfg in the write dir: install=, mw3_install=, mw1_game= (a line each,
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

// The game main's argv: the program's name, --write-dir (unless given), then
// `rest`, with --install added when only mw1.cfg named PM's.
std::vector<std::string> game_args(const std::string& argv0, const Args& a, const Installs& use);

}  // namespace mw1
