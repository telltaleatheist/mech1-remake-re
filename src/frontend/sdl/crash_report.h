#pragma once
// Ours, not original code (docs/program.md, The log): a crash says why, in the
// log and in a message box, rather than the window just going away.
#include <string>

namespace sdlfe {

// Ours (opt-in, MW1's FIXES 123): the name of a program other than Pirate's Moon built on PM's game main
// (mw3_game_main; MechWarrior 1: Critical Hit's mw1_main.cpp), set before it runs. It names the log's first line
// ("<id> (<title>) <version>, log ...", or "...: started" on Windows), the crash report's lines (as crash_report_install's
// `program`) and the crash box's and fatal_box's title and words. Unset (id empty: MW3, PM, the launcher), every one
// is Pirate's Moon's, as it was.
struct ProgramName {
  std::string id;       // "mw1"
  std::string title;    // "MechWarrior 1: Critical Hit"
  std::string version;  // its own (mw1-v...), in place of the engine's
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
