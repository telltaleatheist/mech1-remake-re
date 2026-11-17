#pragma once
// The tests' MW1 install (CMake's MW1_DOS_GAME_DIR): the player's MechWarrior (1989) folder, whose MW_MAIN.EXE and
// BTECH.EXE the exe checks read. Without it a test that needs it is skipped in one way, said in one wording and
// counted on the run's last line (check::skip): ", 9 skipped (no MW1 install at <dir>)". Never silently passed.
#include <cctype>
#include <filesystem>
#include <string>

#include "check.h"

namespace mw1_test {

// MW_MAIN.EXE and BTECH.EXE (any case) in MW1_DOS_GAME_DIR.
inline bool mw1_there() {
  std::error_code ec;
  bool main = false, btech = false;
  for (const auto& e : std::filesystem::directory_iterator(MW1_DOS_GAME_DIR, ec)) {
    std::string n = e.path().filename().string();
    for (char& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    main |= n == "MW_MAIN.EXE", btech |= n == "BTECH.EXE";
  }
  return main && btech;
}

// True when there's no install: the test (or its MW1 part, the rest still run) skipped and counted. Call it in the
// test's own process, before any forked child.
inline bool no_mw1() {
  if (mw1_there()) return false;
  check::skip(std::string("no MW1 install at ") + MW1_DOS_GAME_DIR);
  return true;
}

}  // namespace mw1_test
