#pragma once
// Ours, not original code (docs/program.md, The log): a crash says why, in the
// log and in a message box, rather than the window just going away.
#include <functional>
#include <string>
#include <vector>

namespace sdlfe {

// Ours (opt-in, MW1's FIXES 123): the name of a program other than Pirate's Moon built on PM's game main
// (mw3_game_main; MechWarrior 1: Critical Hit's mw1_main.cpp), set before it runs. It names the log's first line
// ("<id> (<title>) <version>, log ...", or "...: started" on Windows), the crash report's lines (as crash_report_install's
// `program`) and the crash box's and fatal_box's title and words. Unset (id empty: MW3, PM, the launcher), every one
// is Pirate's Moon's, as it was.
struct ProgramName {
  std::string id;       // "mw1"
  std::string title;    // "MechWarrior 1 Remake"
  std::string version;  // its own (mw1-v...), in place of the engine's
  // The in-game setup menu's (mw3_main.cpp, menu_hooks): its report page, its own releases (the catalog's entry, the
  // repo, the tags' prefix and the file an install updates from, "{tag}" for the tag; update_file_setup a Windows
  // Setup's, else update_file), and whether GAME FILES offers CHANGE (PM's find screen: PM's files alone).
  std::string report_url;
  std::string update_key, update_repo, update_tag_prefix, update_file, update_file_setup;
  bool change_files = true;
  std::vector<std::string> keep;  // the write dir's files an update backs up first, besides PM's (its saves)
  std::string files_note;         // GAME FILES without CHANGE: where to change them
  std::string badge;              // the menu's medallion, a PNG in program_dir (sdlfe::program_dir); "" none
  // UPGRADES: where it can open and the opening, in place of PM's Upgrades page (in a mission, after the pause's
  // OPTIONS); RESET SETTINGS: the program's own settings to their defaults after PM's, `settings` (in the write dir)
  // backed up first besides PM's. Null: PM's alone.
  std::function<bool()> upgrades_reachable, upgrades_open, reset_own;
  std::vector<std::string> settings;
};
ProgramName& program_name();

// The handlers for a crash and for an uncaught exception. `log` is named in
// the box; with `box` false only the log says it (hidden runs do not install
// this at all; MW3_LAUNCHER_WATCHES: the launcher shows the box, see
// Launcher::watch_game). MW3_CRASH_TEST=1 ends with exit code 70 after the
// log line, for the test.
void crash_report_install(const std::string& program, const std::string& log, bool box);

// The start of main: `--crash-box <text>` shows the box and is all this run
// does. A crash's handler cannot use SDL, so it starts its own program for this.
bool crash_box_main(int argc, char** argv);

// A start that cannot go on: the log line and, unless hidden, the box.
void fatal_box(const std::string& message, bool hidden);

}  // namespace sdlfe
