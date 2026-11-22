#pragma once
// FIXES 118 (1.1.0 slice 3, research/plan-1.1.0.md): MW1 answers the engine's upgrade hooks (src/upgrades/kits,
// docs/upgrades-kits.md) for the company's side only. The commander's 'Mech and each lancemate's get the kits on
// them; every weapon on the company's side gets its type's steps; an enemy gets nothing, even with the same weapon.
//
// Armed at the deploy from the company as it is then (the kits on each 'Mech, the steps), before the mission loads:
// a part's armour is set while its 'Mech is built. Disarmed at the mission's end and at an abort; unarmed, every hook
// answers none. The game says whose a mission mech is (`Who`); once the launch is done the sides are cached (settle).
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::kit_answers {

// A mission mech's side: 0 the commander's, a lancemate's pilot id, -1 anything else (an enemy, a vehicle).
using Who = std::function<int32_t(const void* mech)>;
// What identifies a mission mech for its lifetime: its AI pilot's name (what lancemate_of matches). A lancemate's side
// is cached with it and looked up again when it no longer matches: a mech made later (a script's reinforcement) at a
// destroyed lancemate's address is never taken for them. Null: the brain's pilot name.
using Stamp = std::function<std::string(const void* mech)>;

void install();                         // sets the engine's hooks (again: harmless)
void uninstall();                       // the engine's hooks null again, and disarmed
void arm(const Company& c, Who who, Stamp stamp = nullptr);  // the deploy: this company's kits and steps
// The launch done, every company mech made: from now each mech's side is cached. The commander's never is (the
// player's controls say it, cheaply); a lancemate's with its stamp, checked on each answer; anything else's as none
// (no company mech is made after the launch).
void settle();
void disarm();                          // the mission's end, an abort
bool armed();

// What the hooks answer, by mission mech (a mech3::Mech*) and by weapon (its owner, PM's weapon id, its tech).
Kits kits_for(const void* mech);
float damage_for(const void* owner, int32_t weapon, int32_t tech);
float range_for(const void* owner, int32_t weapon, int32_t tech);

// The armour kit's points over a 'Mech's 11 armour slots: in proportion to each slot's own (the configuration's)
// points, the largest remainders first, ties to the lower slot; a 'Mech with none, one a slot from slot 0 round.
std::array<int32_t, 11> spread_armour(int32_t points, const int32_t armour[11]);

// A mission mech as the engine now sees it, for the tests and the log: its side, its sinks' rate with the kits
// (heat_sink_rate) and without (HeatSinks::rate), its torso's armour scale and the configuration's points and the
// kit's share there, and each weapon's: PM's id and tech, the damage scale and the reach the engine applies (through
// its own guard), and its type's RANGE.
struct MechReport {
  struct Gun {
    int32_t weapon = -1, tech = 0;
    float damage_scale = 1, max_range = 0, type_range = 0;
  };
  bool found = false;
  int32_t side = -1;
  float sink_rate = 0, own_sink_rate = 0;
  float torso_armour_scale = 0;
  int32_t torso_points = 0, torso_kit_points = 0;
  std::vector<Gun> guns;
};
MechReport report(void* mech);

// A line for the log at the launch: "kits: commander +S/+A, ...; steps on N weapon types".
std::string summary();

}  // namespace mw1::kit_answers
