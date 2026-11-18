#pragma once
// The story's battles (FIXES.md item 47, Owen 2026-11-18): ours, MW1's steps being text choices but its last, the
// Dark Wing's base. One table, the ladder, each rung a step's battle with its own fixed force and its pilots' aim,
// so the numbers are tuned in one place. A story battle pays nothing; its salvage is kept. A loss is a retreat: the
// step waits, offered again on the next arrival.
//
// MW1's own last battle (research/story.md, the story battles; the port's BTECH_0810_IMPORT as reference) fields three
// BattleMasters and a Warhammer, the most its battles hold, with the best pilots there are (gunnery and piloting
// target numbers 0). Owen's rung 6 is MW1's text's force instead (a Warhammer and four BattleMasters) and three
// escorts: eight, elite.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1 {

inline constexpr int32_t kRungs = 6;
inline constexpr int32_t kDarkWingRung = 6;
inline constexpr int32_t kRungMechs = 8;  // the most a rung fields (every table map's enemy places hold it: kMapPlaces)
// Instant Action's line-up: ten slots beside the player's (ActionGame::lineup, PM's layout), the lancemates first.
// So a rung of 8 is up to 8: 7 with a full lance (3 lancemates), its escorts trimmed first (the lead, 2026-11-18).
inline constexpr int32_t kLineupSlots = 10;

struct Rung {
  int32_t rung;                    // 1..kRungs
  int32_t count;                   // its mechs, at most
  int32_t core;                    // the first `core` never trimmed (the rest escorts, trimmed from the last)
  int32_t kinds[kRungMechs];       // MW1 kinds (kMechKinds, PM stand-ins), the first `count`
  float aim;                       // the enemy pilots' aim error factor (mw1_pilots.h's kAimErrorFactor's scale)
};
// Provisional (Owen's numbers, FIXES.md 47): the forces by MW1's tonnage (kMechKinds), the aim rising with the rung.
// Rung 6's aim is past the best lancemate's (kAimErrorFactor[3], 0.6): MW1's target number 0, below any pilot's.
inline constexpr Rung kLadder[kRungs] = {
    {1, 2, 2, {kShadowHawk, kShadowHawk}, 1.4f},                                           // Grig's men: 2 mediums, 110 t
    {2, 3, 3, {kWolverine, kShadowHawk, kWarhammer}, 1.2f},                               // Brown: 180 t
    {3, 4, 4, {kRifleman, kRifleman, kWarhammer, kMarauder}, 1.0f},                       // the ambush: 265 t
    {4, 4, 4, {kWarhammer, kMarauder, kMarauder, kBattleMaster}, 0.8f},                   // Kearney's address: 305 t
    {5, 6, 6, {kWolverine, kRifleman, kRifleman, kWarhammer, kMarauder, kMarauder}, 0.6f},  // Matabushi: 395 t, veterans
    // The Dark Wing: the Warhammer and four BattleMasters (MW1's text), three Marauder escorts; 635 t, elite.
    {6, 8, 5, {kWarhammer, kBattleMaster, kBattleMaster, kBattleMaster, kBattleMaster, kMarauder, kMarauder, kMarauder}, 0.4f},
};
const Rung& rung(int32_t r);  // 1..kRungs (out of range: the first)
// Its force against a lance of `lancemates` (the commander's not counted): its kinds, the escorts trimmed from the
// last to fit the line-up (never below its core).
std::vector<int32_t> rung_force(int32_t r, int32_t lancemates = 0);
int32_t rung_tons(const std::vector<int32_t>& force);  // by MW1's tonnage

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
Contract story_battle(const Company& c, int32_t r, Rng& rng);

}  // namespace mw1
