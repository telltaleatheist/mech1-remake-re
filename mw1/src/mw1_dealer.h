#pragma once
// FIXES 122: the arms dealer (research/plan-1.1.0.md sections 4 and 5). The model only: what each world's dealer
// stocks, the prices, buying into the company's stores (which the mech lab fits from) and selling back out of them.
// No SDL, no screens, no engine change: the stores are PM's own (mech3::Store), as salvage fills them.
//
// - The wares: PM's weapons in each tech they come in, their ammunition (one-ton lots) and the equipment the lab fits,
//   but plain armour.
// - Rarity: a Clan ware is rare (tier 3 worlds and the Clan worlds, mw1_market.h's kClanWorlds); the Inner Sphere
//   wares no fielded loadout carries (FIXES 111's report) are uncommon (tier 1 and up); the rest common, everywhere.
// - The stock is rolled at each market refresh (refresh_market) from its own seed (the world and the date), so the
//   campaign's dice are untouched.
// - Prices: a ton of the item's weight at kPricePerTon (at least kPriceLeast), Clan by kClanPercent, ammunition
//   kAmmoPerTon a lot; times the world's tier as a 'Mech's are (mw1_market.h's buy and sell percentages).
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::dealer {

// The design's numbers: placeholders until the project settles them (plan-1.1.0.md sections 4 and 5).
inline constexpr int64_t kPricePerTon = 60000;
inline constexpr int64_t kPriceLeast = 50000;
inline constexpr int32_t kClanPercent = 150;
inline constexpr int64_t kAmmoPerTon = 10000;
inline constexpr bool kBuysBack = true;  // the dealer buys from the stores, at the market's selling percentages
// The stock: kKindsBase + rand(0..kKindsRoll) wares drawn (each kind once), each rand(1..kCountMost) of them; a draw
// weighs a common ware kCommonWeight, an uncommon kUncommonWeight, a rare kRareWeight.
inline constexpr int32_t kKindsBase = 6, kKindsRoll = 8, kCountMost = 3;
inline constexpr int32_t kCommonWeight = 6, kUncommonWeight = 3, kRareWeight = 2;
inline constexpr int32_t kUncommonTier = 1, kRareTier = 3;
inline constexpr int32_t kStockMost = 999;  // one line's count at most (sold back to it)

enum class Rarity { kCommon, kUncommon, kRare };
const char* rarity_words(Rarity r);  // "common", ...

// A ware: a DealerItem without its count.
struct Ware {
  int32_t kind = 1, id = 0, tech = 2;
  friend bool operator==(const Ware& a, const Ware& b) { return a.kind == b.kind && a.id == b.id && a.tech == b.tech; }
};
inline constexpr int32_t kWeapon = 1, kAmmo = 2, kEquipment = 3;  // PM's item kinds (mech_config.h)

std::vector<Ware> wares();  // everything a dealer can stock, in PM's tables' order
Rarity rarity(const Ware& w);
// Whether a world of `tier` (and its record's number, for the Clan worlds) stocks wares of this rarity.
bool stocked_at(Rarity r, int32_t tier, int32_t planet_number);
std::string ware_name(const Ware& w);  // PM's table name: "weapon PPC", "weapon LRM20 (Clan)", "ammo SRM4", "equipment masc"

// The item's tons (PM's weapon and equipment tables: the weapons' need PM's weapon data loaded); ammunition 1.
float ware_tons(const Ware& w);
// An ammunition lot's rounds: a ton's (mech3::ammo_weight), at least 1.
int32_t ammo_lot(int32_t weapon);
int64_t tier0_price(const Ware& w);
int64_t buy_price(const Ware& w, int32_t tier);   // floored to 1,000
int64_t sell_price(const Ware& w, int32_t tier);  // floored to 1,000

// The stock rolled for the company's world and date (at the market's refresh; a save without the dealer's line).
void roll_stock(Company& c);
// Its seed: the world's number, the date and the market's clock.
uint32_t stock_seed(const Company& c);

enum class Trade { kDone, kNoSuch, kNoFunds, kStoresFull, kNotBuying };
const char* trade_words(Trade t);  // for a status line: "" done
// BUY: one of stock line `index` (ammunition: a lot) into the stores, for its buying price.
Trade buy(Company& c, int32_t index);
// SELL: one of the ware out of the stores (ammunition: a whole lot), for its selling price; into the stock. Never a
// fitted item (the stores hold only what no loadout carries).
Trade sell(Company& c, const Ware& w);
// How many of the ware the stores hold (ammunition: whole lots).
int32_t in_stores(const Company& c, const Ware& w);

// MW1_PRICE_TABLE: every ware's rarity and prices at tiers 0 and 3, as text.
std::string price_table();

}  // namespace mw1::dealer
