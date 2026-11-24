#pragma once
// PM's own weapon lab blocks (weights, slots, a round's tons), read from PM's weapons.zrd as the engine's weapon types
// read them, for tests that price or weigh items without starting the engine (mw1_dealer's). Its own file: the
// weapon types' header (mech3/weapon.h) and the stores' (mech3/pilot.h) name mech3::Handle differently.
namespace mw1_tests {

// mech3::set_weapon_lab to PM's blocks; false (and nothing set) when PM's reader.zbd isn't there or has none.
bool use_pm_weapon_labs();
int use_pm_weapon_labs_count();  // the weapons read
void stop_pm_weapon_labs();      // mech3::set_weapon_lab(nullptr)

// 1.1.0: the engine as between missions (PM loads weapons.zrd into zWeapon's definitions only in a mission): every
// definition freed and no weapon lab set, so nothing an earlier test left in this process stands in for the hub's
// weapon data (mw1_weapon_data.h). Whether the engine has a weapon's definition now.
void clear_engine_weapons();
bool engine_has_weapon(const char* name);

}  // namespace mw1_tests
