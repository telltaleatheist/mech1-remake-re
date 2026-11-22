#pragma once
// FIXES 118: permanent upgrades bought with money (research/permanent-upgrades-plan.md, research/plan-1.1.0.md). The
// model only: what the company owns (Company::upgrades, the kits on each 'Mech), the prices, buying and assigning.
// The supply screen buys and assigns through these; the engine's hooks (src/upgrades/kits) read the effects. No SDL,
// no screens and no engine here.
//
// - Kits, one unit a purchase: a heat sink (+1 sink, no crits, no tonnage) or an armour point (+1 past the 'Mech's
//   cap). The company owns them: assigned to any of its 'Mechs, moved later, back in the storehouse when the 'Mech
//   is sold or lost.
// - Weapon steps: power or range for every weapon of one type the company owns (PM's weapon id and its tech); heat
//   unchanged. The player's side only, never an enemy's.
// - Prices climb with each purchase: base x ratio^bought, floored to 1,000.
#include <cstdint>
#include <string>

#include "mw1_campaign.h"

namespace mw1::upgrades {

// The design's numbers: placeholders until the project settles them (plan-1.1.0.md section 4). Ratios in permille.
inline constexpr int64_t kSinkBase = 500000;
inline constexpr int32_t kSinkRatio = 1250;
inline constexpr int64_t kArmourBase = 100000;
inline constexpr int32_t kArmourRatio = 1050;
inline constexpr int64_t kStepBase = 1000000;
inline constexpr int32_t kStepRatio = 1500;
// true: each weapon type has its own ladder (its power and range steps together); false: one ladder for all steps.
inline constexpr bool kStepLadderPerType = true;
inline constexpr int32_t kPowerStepPercent = 5;  // of the weapon's damage, each power step
inline constexpr int32_t kRangeStepMetres = 10;  // each range step
inline constexpr int32_t kRangeMostPercent = 100;  // range steps add at most this share of the weapon's own range
inline constexpr int64_t kFitFee = 0;  // C-bills for each kit put on a 'Mech (taking one off is free)
// The most of any one count (a kit's, a step's): a price reaches kPriceMost long before.
inline constexpr int32_t kCountMost = 9999;
// A price never above this: past it, nothing a company can hold buys it (the save's funds bound is 2^40).
inline constexpr int64_t kPriceMost = int64_t{1} << 40;

enum class Kit { kSink, kArmour };
enum class Step { kPower, kRange };
enum class Bought { kDone, kNoFunds, kAtMost, kNoSuch };
const char* bought_words(Bought b);  // for a status line: "" done

// base x (ratio / 1000)^bought, each step floored to the C-bill, the whole floored to 1,000; kPriceMost at most.
int64_t ladder_price(int64_t base, int32_t ratio, int32_t bought);

int32_t kits_bought(const Company& c, Kit k);
int64_t kit_price(const Company& c, Kit k);  // the next one's
Bought buy_kit(Company& c, Kit k);           // into the storehouse

// The steps a weapon type has (null: none bought), and the next step's price: its type's ladder (both kinds count),
// or every step's when the ladder is shared.
const WeaponSteps* steps_of(const Company& c, int32_t weapon, int32_t tech);
int32_t step_ladder_count(const Company& c, int32_t weapon, int32_t tech);
int64_t step_price(const Company& c, int32_t weapon, int32_t tech);
// The most range steps a weapon of `range_metres` takes (kRangeMostPercent of it); 0 for none.
int32_t range_steps_most(float range_metres);
// `range_most`: the type's range_steps_most (the screen knows the weapon's range); -1 no limit.
Bought buy_step(Company& c, int32_t weapon, int32_t tech, Step s, int32_t range_most = -1);
// The steps' effects on a weapon of that type: its damage times this, its range plus this (metres).
float damage_scale(const Company& c, int32_t weapon, int32_t tech);
float range_add(const Company& c, int32_t weapon, int32_t tech);

// The company's 'Mechs as the market numbers them (mw1_market.h): 0 the commander's, i the hangar's i - 1. Null: no
// such 'Mech.
Kits* kits_on(Company& c, int32_t owned);
const Kits* kits_on(const Company& c, int32_t owned);
// One kit from the storehouse onto the 'Mech (`delta` +1, its fit fee paid) or back (-1). kNoSuch: no such 'Mech, or
// none to move.
Bought assign(Company& c, int32_t owned, Kit k, int32_t delta);
// A 'Mech leaving the company (sold, lost): its kits back in the storehouse, `kits` emptied.
void return_kits(Company& c, Kits& kits);
// Every kit the company owns, wherever it is (a kit is never lost: this only grows, by buying).
Kits all_kits(const Company& c);

// MW1_PRICE_TABLE: the ladders' prices and what a sum buys, as text, for the lead and the project to settle them.
std::string price_table();

}  // namespace mw1::upgrades
