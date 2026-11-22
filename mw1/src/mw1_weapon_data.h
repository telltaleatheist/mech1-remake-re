#pragma once
// 1.1.0 (FIXES 122 and 118): PM's weapon data at the hub. PM reads weapons.zrd into zWeapon's definitions only as a
// world loads (Missions::load_world: a mission's, or the lab's world 6, which PM's lab and the Mech Complex's live
// 'Mech load) and frees them with it, so at the hub mech3::weapon_weight() is 0 for every weapon until one of those
// has loaded (never with the software renderer or headless, where the live 'Mech is off). MW1's dealer prices by the ton and the supply screen limits range steps by a weapon's RANGE at the hub,
// so MW1 reads the same file itself, from the mounted archives (a mod's weapons.zrd overlaid, as the mission reads
// it): each weapon's lab tons by tech, a round's tons and its RANGE. Every read asks the engine first and uses these
// only when it has no definition loaded. Never through mech3::set_weapon_lab: that replaces the engine's own reads
// everywhere, a mod's weapons too.
#include <cstdint>

namespace zrdr {
struct Node;
}

namespace mw1::weapon_data {

// weapons.zrd from the mounted archives (the game, once its hub is up; never by a read below, so a test's prices
// stay its own); again after clear(). False when it isn't there or holds none of PM's table's weapons.
bool load();
int32_t count();
// A weapons.zrd tree already read (tests): its weapons added.
void read(zrdr::Node* root);
void clear();

// The engine's figure when it has the weapon's definition, else MW1's read; 0 when neither has it.
float tons(int32_t weapon, int32_t tech);  // the lab's tons, Clan's or the Inner Sphere's
float round_tons(int32_t weapon);          // a round's tons (ammunition)
float range(int32_t weapon);               // its RANGE, metres

}  // namespace mw1::weapon_data
