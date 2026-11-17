#pragma once
// MW1's repair prices on Pirate's Moon's damage (VISION.md, "Damage and repairs"; research/repairs.md). Every mission
// starts fresh (MW3 repairs every mech); only the money is ours: one bill per counted mission for the company's
// mechs' damage, field-base repairs included, and the same rule deducts the wrecks' repairs from their pay. No SDL
// and no screens here.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::repairs {

// MW1's body repair (MwQuoteRepairComponent, REPAIR 081B:3E28; MwQuoteMechRepairTotal 3763): the kind's price
// (bodyRepairBasePricesByKind, DS:3CAB, thousands) for each of the 9 body locations damaged, plus the world's
// premium. A full repair is all 9.
inline constexpr int32_t kBodyRepairPrices[kKindCount] = {4, 3, 4, 8, 10, 10, 12, 10, 12, 15};
inline constexpr int32_t kBodyLocations = 9;  // CAMPAIGN_BODY_LOCATION_COUNT
// Pirate's Moon's mechs aren't MW1's kinds: a location's price is the straight line through MW1's ten by tons
// (least squares, provisional Q1): 172 x tons - 150 C-bills.
inline constexpr int32_t kRepairPerTon = 172, kRepairOffset = 150;
// The tuning knob (provisional Q3: MW1's numbers as they are).
inline constexpr int32_t kRepairScale = 1;
// The world's premium (planetMarketPremiumPercent, DS:0477; PLANET 081B on arrival): (tier + 1) x 18 + rand(0..7)
// percent, on every repair there, ours and the wrecks' (provisional Q2).
inline constexpr int32_t kPremiumStep = 18, kPremiumRandomMax = 7;
void roll_premium(Company& c, Rng& rng);

// A mech's full repair in C-bills at this world: 9 locations, the premium, the scale.
int64_t full_repair(const Company& c, int32_t pm_mech);

// A mech's damage as PM's lance bars read it (mech_lance_armor, mech_lance_structure): each bar's armor points and
// fraction left, its structure points and fraction left. A bar's share of the full repair is its points' share
// (armor and structure together; without structure points, armor alone).
struct Bars {
  float armor_points[11] = {}, armor_left[11] = {}, structure_points[11] = {}, structure_left[11] = {};
};
inline constexpr int32_t kWhole = 10000;  // damage in ten-thousandths of a full repair
int32_t damage_of(const Bars& b);
// What a field base mended: the damage before (the bars' fractions then, the points now) less the damage now.
int32_t mended(const Bars& now, const float armor_before[11], const float structure_before[11]);

// A company mech's mission: who flew it (0 the commander, else the lancemate's pilot id), the mech, the damage it
// came back with plus what field bases mended (ten-thousandths of a full repair), and whether it was destroyed.
struct MechBill {
  int32_t who = 0;
  int32_t mech = -1;
  int32_t damage = 0;
  bool destroyed = false;
};
int64_t bill_of(const Company& c, const MechBill& b);

// A wreck's repairs (the same rule, on its chassis), deducted from its sale price; never below its scrap value
// (provisional Q4).
int64_t wreck_repairs(const Company& c, int32_t pm_mech, int32_t damage);

// The repairs after a counted mission (won, or failed and accepted; after the fee, the salvage and the lancemates'
// fates): the bill for each mech come back (a destroyed one is lost, never billed), taken from the funds and never
// below 0, the rest forgiven. The commander's destroyed mech is lost: they take the first idle hangar mech, else a
// lancemate's (who stays behind); with none left, the last comes back fully repaired, on the bill. The lines for
// AFTER ACTION and the hub.
struct Repairs {
  int64_t billed = 0, paid = 0, forgiven = 0;
  std::vector<std::string> lines;
};
Repairs settle_repairs(Company& c, const std::vector<MechBill>& bills);

}  // namespace mw1::repairs

namespace mw1 {
using namespace repairs;
}  // namespace mw1
