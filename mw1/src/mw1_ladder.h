#pragma once
// The story's battles (FIXES.md item 47, the owner 2026-11-18): ours, MW1's steps being text choices but its last, the
// Dark Wing's base. One table, the ladder, each rung a step's battle with its own fixed force and its pilots' aim,
// so the numbers are tuned in one place. A story battle pays nothing; its salvage is kept. A loss is a retreat: the
// step waits, offered again on the next arrival.
//
// MW1's own last battle (research/story.md, the story battles; the port's BTECH_0810_IMPORT as reference) fields three
// 85 t 'Mechs and a 70 t, the most its battles hold, with the best pilots there are (gunnery and piloting target
// numbers 0). The owner's rung 6 is the force MW1's story text tells of instead (a 70 t and four 85 t) and three 75 t
// escorts: eight, elite.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_text_names.h"

namespace mw1 {

inline constexpr int32_t kRungs = 6;
inline constexpr int32_t kDarkWingRung = 6;
inline constexpr int32_t kRungMechs = 8;  // the most a rung fields (every table map's enemy places hold it: kMapPlaces)
// Instant Action's line-up beside the player's, the lancemates first: the exe's ten raised to 16 for MW1 (FIXES.md
// item 48, mech3::set_action_lineup_capacity; PM's ten slots and our tail after them), the enemies past a map's own
// places made beside them on clear ground (ProgramFlow::extra_enemy_places). A full lance (3 lancemates) leaves 13: a
// rung's whole force. The escorts' trim stays as a guard (a rung past 13).
inline constexpr int32_t kLineupSlots = 16;

struct Rung {
  int32_t rung;                    // 1..kRungs
  int32_t count;                   // its mechs, at most
  int32_t core;                    // the first `core` never trimmed (the rest escorts, trimmed from the last)
  int32_t slots[kRungMechs];       // the force's slots (kForceSlots), the first `count`
  float aim;                       // the enemy pilots' aim error factor (mw1_pilots.h's kAimErrorFactor's scale)
  // FIXES 114: a scripted force's chassis, slot by slot (-1 throughout: rolled as a contract's, kStoryBattlesRoll).
  int32_t chassis[kRungMechs] = {-1, -1, -1, -1, -1, -1, -1, -1};
};
// Provisional (the owner's numbers, FIXES.md 47): the forces by tonnage (kForceSlots), the aim rising with the rung.
// Rung 6's aim is past the best lancemate's (kAimErrorFactor[3], 0.6): MW1's target number 0, below any pilot's.
inline constexpr Rung kLadder[kRungs] = {
    {1, 2, 2, {kSlot55, kSlot55}, 1.4f},                                      // Grig's men: 2 mediums, 110 t
    {2, 3, 3, {kSlot55, kSlot55, kSlot70}, 1.2f},                             // Brown: 180 t
    {3, 4, 4, {kSlot60, kSlot60, kSlot70, kSlot75}, 1.0f},                    // the ambush: 265 t
    {4, 4, 4, {kSlot70, kSlot75, kSlot75, kSlot85}, 0.8f},                    // Kearney's address: 305 t
    {5, 6, 6, {kSlot55, kSlot60, kSlot60, kSlot70, kSlot75, kSlot75}, 0.6f},  // Matabushi: 395 t, veterans
    // The Dark Wing: a 70 t and four 85 t (as MW1's story text tells it), three 75 t escorts; 635 t, elite. Scripted
    // (FIXES 114, the owner): MW1's text's 'Mechs as ours, a Thor and four Atlases, the escorts the 75 t record's
    // (mw1_text_names.h's kOurChassis, kOurs75).
    {6, 8, 5, {kSlot70, kSlot85, kSlot85, kSlot85, kSlot85, kSlot75, kSlot75, kSlot75}, 0.4f,
     {kPmThor, kPmAtlas, kPmAtlas, kPmAtlas, kPmAtlas, kOurs75, kOurs75, kOurs75}},
};
const Rung& rung(int32_t r);  // 1..kRungs (out of range: the first)
// Its force against a lance of `lancemates` (the commander's not counted): its slots, the escorts trimmed from the
// last to fit the line-up (never below its core).
std::vector<int32_t> rung_force(int32_t r, int32_t lancemates = 0);
int32_t rung_tons(const std::vector<int32_t>& force);  // the slots' tons

// How many enemies each Instant Action map makes (its enemy places, by the line-up's rules: mw1_loop's
// the_dark_wing_fits_these_maps survey, with rung 6's force), and whether a map makes `n`.
struct MapPlaces {
  const char* world;
  int32_t ia;
  int32_t enemies;
};
extern const std::vector<MapPlaces> kMapPlaces;
bool map_holds(const std::string& world, int32_t ia, int32_t n);

// A story battle as a contract (Contract::rung): on the company's world (its landscape the climate), no pay, no
// advance, accepted; the employer the world's House, against no House; destroy all on a map of the climate whose
// enemy places hold the force (map_holds; none: any that does), its force the rung's for the company's lance.
// `hardest` (item 47's setbacks, ours: MW1's second FIGHT, a death there): an escort more (the rung's last kind) and
// the next rung's aim (story_aim).
Contract story_battle(const Company& c, int32_t r, Rng& rng, bool hardest = false);
// The enemy pilots' aim error factor in a story battle: its rung's, or the next rung's when at its hardest.
float story_aim(const Contract& k);

}  // namespace mw1
