// Ours: the upgrade hooks' network game type (src/upgrades/kits/kit_hooks.h) is pilot.h's. kit_hooks.h can't include
// pilot.h (its mech3::Handle and weapon.h's differ), so it keeps the number itself; should pilot.h renumber, this file
// stops the build instead of the hooks quietly answering in a network game.
#include "check.h"
#include "mech3/pilot.h"
#include "upgrades/kits/kit_hooks.h"

static_assert(upgrades::kMultiplayerGame == mech3::kMultiplayer, "kit_hooks.h's kMultiplayerGame drifted from pilot.h");

TEST(kit_hooks_network_game_type_is_pilot_s) { EXPECT(upgrades::kMultiplayerGame == mech3::kMultiplayer); }
