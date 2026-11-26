#pragma once
// A test of the mods code that checks MechWarrior 3's names (for=mw3, "MechWarrior 3 Rebuild") runs it under
// MW3's name, mods_game(), and puts the name back. A no-op here; on the branch pm, where the program's own name is
// Pirate's Moon's and the code is the same, it tests that code (PM's names: tests/pm/test_pm_mods.cpp).
#include <string>

#include "mech3/mods.h"

struct ModsGameMw3 {
  std::string was = mech3::mods_game();
  ModsGameMw3() { mech3::mods_game() = "mw3"; }
  ~ModsGameMw3() { mech3::mods_game() = was; }
};
