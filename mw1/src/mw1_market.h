#pragma once
// MW1's market (VISION.md, "The economy and travel" and "Mechs, the mech lab
// and salvage"; research/economy.md): Pirate's Moon's mechs bought and sold
// with MW1's logic. One price per mech, sold with its stock loadout, no
// damaged stock; the world's tier sets the price; a dealer at a tier 2 or 3
// world buys only a few between refreshes; Clan mechs cost double and are sold
// only at a few Kurita and Steiner edge worlds. No SDL and no screens here.
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"

namespace mech3 {
struct Config;
}

namespace mw1::market {

// A mech's price at a tier 0 world, in thousands: VISION's straight line
// through MW1's own prices (DS:305F, 75 to 100 per ton): 95 x tons - 245.
inline constexpr int32_t kPricePerTon = 95, kPriceOffset = 245;
inline constexpr int32_t kClanPriceFactor = 2;  // VISION (Owen 2026-11-16, tuned by playing)
int32_t tier0_price(int32_t mech);              // thousands
bool clan_mech(int32_t mech);                   // PM's table's tech (the Clint is Clan there)
// Buying (081B:4645): base x multiplier[tier] x 100; selling (081B:32BA): base x (multiplier - 1) x 100, where
// base is the tier 0 price in thousands; C-bills, floored to 1,000; a Clan mech's doubled.
int64_t buy_price(int32_t mech, int32_t tier);
int64_t sell_price(int32_t mech, int32_t tier);
// The same as a share of the tier 0 price, for the travel map's readout (FIXES.md item 21): buying 100/120/130/150%,
// selling 90/110/120/140%.
int32_t buy_percent(int32_t tier);
int32_t sell_percent(int32_t tier);

// MW1's classes for PM's mechs (VISION, "Combat: mechs"): 0 light (35 t and under), 1 medium (40 to 55 t), 2 heavy
// (60 t and over: MW1's heavy takes in assault). PM's roster less the Elemental.
int32_t mech_class(int32_t mech);
bool sold_mech(int32_t mech);  // a mech the market deals in (PM's roster but the Elemental and "no mech")
// The mech's name as PM shows it (its Mech3Msg.dll name, "Mad Cat"), else its roster name title-cased ("Madcat").
std::string mech_title(int32_t mech);

// The worlds that sell Clan mechs (VISION: "a few edge worlds of Kurita and Steiner, near the Clan border").
// Provisional, pending Owen (research/economy.md): the tier 3 Kurita and Steiner worlds at the top of MW1's map
// (map y 60 or less), by planet number (the record's byte 0, DS:6570).
struct ClanWorld {
  int32_t number;
  const char* name;
};
inline constexpr int32_t kClanWorldCount = 5;
extern const ClanWorld kClanWorlds[kClanWorldCount];
bool clan_world(int32_t planet_number);

// The stock (MwGenerateUnitStock, 081B:3D09): rand(0..12 / (tier + 1)) mechs, each slot a class at 60% light, 30%
// medium, 10% heavy, then one of that class's Inner Sphere mechs; at a Clan world one slot in ten is a Clan mech
// of the class instead. Every mech whole (VISION: no damaged stock). The dealer's limit (081B:3314): at tier 2
// and above, rand(0..4 - tier) + 1 mechs bought between refreshes; none below.
inline constexpr int32_t kStockMax = 12, kClassRollMax = 9, kLightRolls = 6, kMediumRolls = 3, kClanRollMax = 9;
inline constexpr int32_t kDealerLimitTier = 2, kDealerLimitBase = 4;
void refresh_market(Company& c, Rng& rng);
// The most mechs a dealer at this tier buys between refreshes (the limit's top): -1 no limit (tiers 0 and 1).
int32_t dealer_most(int32_t tier);

// The company's mechs, 0 the one the commander pilots (Company::mech), then the hangar's.
int32_t owned_count(const Company& c);
// FIXES.md item 53: the most mechs the company owns, the piloted one included. MW1's was 12 (MW_MAIN 081B:4674, BUY:
// `cmp word [0530], 0Ch`, its table DS:0532 twelve words), raised to 30 by Owen on 2026-11-18. BUY refuses at it
// (Deal::kHangarFull; salvage never brings mechs). A save past it still loads (nothing is taken away).
inline constexpr int32_t kOwnedMost = 30;
// The most of one item the stores hold (each item's count, clamped where MW1 adds to them: salvage, a sale, the hub).
inline constexpr int32_t kStoreMost = 9999;
void clamp_stores(mech3::Store& s);
int32_t owned_mech(const Company& c, int32_t i);

enum class Deal { kDone, kNoSuch, kNoFunds, kLastMech, kDealerFull, kRefit, kHangarFull };
const char* deal_words(Deal d);  // the line the market shows for a refusal ("" when done)

// The mech's stock loadout (its dfn's standard configuration, as the starting
// Owens's), or null when the game's data isn't there.
using StockLoadout = std::function<const mech3::Config*(int32_t mech)>;
const mech3::Config* stock_loadout(int32_t mech);

// BUY: stock[i] into the hangar, with its stock loadout, for its price.
// What BUY or SELL would answer, nothing changed (FIXES.md item 60: the reason said at once, else the question
// asked): buy's and sell's own refusals in their order, kDone when the deal would go through.
Deal can_buy(const Company& c, int32_t stock_index);
Deal can_sell(const Company& c, int32_t owned_index, const StockLoadout& stock = stock_loadout);
Deal buy(Company& c, int32_t stock_index);
// SELL: owned mech i for its one price, put back to its stock loadout first (Owen 2026-11-17): what it carries
// beyond stock goes to the stores, what it lacks of stock comes from them, and when they don't hold it the sale is
// refused ("refit it first"). Never the last mech (VISION), never past the dealer's limit. The mech joins this
// world's stock (as MW1's sale, research/economy.md). Selling the piloted one moves the commander to the next.
Deal sell(Company& c, int32_t owned_index, const StockLoadout& stock = stock_loadout);
// PILOT: the commander takes owned mech i (its loadout with it); the one piloted goes to the hangar.
Deal pilot(Company& c, int32_t owned_index);

}  // namespace mw1::market

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace market;
}  // namespace mw1
