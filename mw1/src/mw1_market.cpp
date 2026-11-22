#include "mw1_market.h"

#include <algorithm>

#include <cctype>
#include <cmath>
#include <utility>
#include <vector>

#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/pilot.h"
#include "mw1_dealer.h"
#include "mw1_upgrades.h"

namespace mw1::market {

namespace {
constexpr int32_t kLightMaxTons = 35, kMediumMaxTons = 55;  // VISION's classes for PM's mechs
constexpr int32_t kStandardConfigMessage = 0xf20;           // the dfn's standard configuration's name (Mech3Msg.dll)

int32_t tons_of(int32_t mech) { return static_cast<int32_t>(std::lround(mech3::max_mass(mech))); }

// The mechs the market deals in of a class and tech, in the roster's order.
std::vector<int32_t> mechs_of(int32_t cls, bool clan) {
  std::vector<int32_t> out;
  for (int32_t m = 0; m < mech3::kMechCount; ++m)
    if (sold_mech(m) && mech_class(m) == cls && clan_mech(m) == clan) out.push_back(m);
  return out;
}
}  // namespace

const ClanWorld kClanWorlds[kClanWorldCount] = {
    {51, "ANYWHERE"}, {50, "WINFIELD"}, {23, "KIRCHBACH"}, {25, "ALBIERO"}, {3, "QANDAHAR"}};

bool clan_world(int32_t planet_number) {
  for (const ClanWorld& w : kClanWorlds)
    if (w.number == planet_number) return true;
  return false;
}

int32_t tier0_price(int32_t mech) { return kPricePerTon * tons_of(mech) - kPriceOffset; }
bool clan_mech(int32_t mech) { return mech3::mech_tech(mech) == mech3::kClan; }

// Floored to 1,000 (research/economy.md), before the Clan's factor (kClanPriceFactor, 1 since FIXES 115). The
// shift (FIXES 135, permille) scales the price before the floor; 0 gives the base price exactly.
int64_t buy_price(int32_t mech, int32_t tier, int32_t shift) {
  const int64_t p = static_cast<int64_t>(tier0_price(mech)) * kTierMultipliers[tier] * 100 * (1000 + shift) / 1000 / 1000 * 1000;
  return clan_mech(mech) ? p * kClanPriceFactor : p;
}
int64_t sell_price(int32_t mech, int32_t tier, int32_t shift) {
  const int64_t p =
      static_cast<int64_t>(tier0_price(mech)) * (kTierMultipliers[tier] - 1) * 100 * (1000 + shift) / 1000 / 1000 * 1000;
  return clan_mech(mech) ? p * kClanPriceFactor : p;
}
int64_t buy_price(int32_t mech, int32_t tier) { return buy_price(mech, tier, 0); }
int64_t sell_price(int32_t mech, int32_t tier) { return sell_price(mech, tier, 0); }

int32_t roll_shift(Rng& rng) {
  int32_t s = 0;
  for (int i = 0; i < 3; ++i) s += rng.upto(2 * kShiftRoll) - kShiftRoll;
  return std::clamp(s, -kShiftMost, kShiftMost);
}
int32_t market_shift(const Company& c, int32_t mech) {
  const std::vector<int32_t>& s = c.market.shift;
  return mech >= 0 && mech < static_cast<int32_t>(s.size()) ? std::clamp(s[static_cast<size_t>(mech)], -kShiftMost, kShiftMost) : 0;
}
int64_t market_buy_price(const Company& c, int32_t mech) { return buy_price(mech, c.tier, market_shift(c, mech)); }
int64_t market_sell_price(const Company& c, int32_t mech) { return sell_price(mech, c.tier, market_shift(c, mech)); }

int32_t buy_percent(int32_t tier) { return kTierMultipliers[std::clamp(tier, 0, 3)] * 10; }
int32_t sell_percent(int32_t tier) { return (kTierMultipliers[std::clamp(tier, 0, 3)] - 1) * 10; }
int32_t dealer_most(int32_t tier) { return tier >= kDealerLimitTier ? kDealerLimitBase - tier + 1 : -1; }

int32_t mech_class(int32_t mech) {
  const int32_t t = tons_of(mech);
  return t <= kLightMaxTons ? 0 : t <= kMediumMaxTons ? 1 : 2;
}
std::string mech_title(int32_t mech) {
  const int32_t id = mech3::mech_name_message(mech);
  const char* shown = id >= 0 ? mech3::pilot_hooks().message(id) : nullptr;
  if (shown && *shown) return shown;
  std::string s = mech3::mech_name(mech);
  bool start = true;
  for (char& ch : s) {
    if (start) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    start = ch == '-' || ch == ' ';
  }
  return s;
}

bool sold_mech(int32_t mech) { return mech >= 0 && mech < mech3::kMechCount && mech != mech3::kElemental; }

void refresh_market(Company& c, Rng& rng) {
  Market& m = c.market;
  m.stock.clear();
  const int32_t n = rng.upto(kStockMax / (c.tier + 1));
  const bool clan_here = clan_world(c.planet_number);
  for (int32_t i = 0; i < n; ++i) {
    const int32_t r = rng.upto(kClassRollMax);
    const int32_t cls = r < kLightRolls ? 0 : r < kLightRolls + kMediumRolls ? 1 : 2;
    const bool clan = clan_here && rng.upto(kClanRollMax) == 0;
    const std::vector<int32_t> pool = mechs_of(cls, clan);
    if (pool.empty()) continue;
    m.stock.push_back(pool[static_cast<size_t>(rng.upto(static_cast<int32_t>(pool.size()) - 1))]);
  }
  m.sold = 0, m.rolled = true;
  m.limit = c.tier >= kDealerLimitTier ? rng.upto(kDealerLimitBase - c.tier) + 1 : -1;
  // FIXES 135: every model's shift, sold here or not (a mech the company brings in sells at its shift too).
  m.shift.assign(mech3::kMechCount, 0);
  for (int32_t mech = 0; mech < mech3::kMechCount; ++mech)
    if (sold_mech(mech)) m.shift[static_cast<size_t>(mech)] = roll_shift(rng);
  dealer::roll_stock(c);  // FIXES 122: the arms dealer restocks with the market (its own seed: `rng` untouched)
}

void clamp_stores(mech3::Store& s) {
  for (mech3::ConfigItem* it : s.items)
    if (it && it->count > kStoreMost) it->count = kStoreMost;
}

int32_t owned_count(const Company& c) { return 1 + static_cast<int32_t>(c.hangar.size()); }
int32_t owned_mech(const Company& c, int32_t i) {
  return i == 0 ? c.mech : i > 0 && i < owned_count(c) ? c.hangar[static_cast<size_t>(i - 1)].mech : -1;
}

const char* deal_words(Deal d) {
  switch (d) {
    case Deal::kDone: return "";
    case Deal::kNoSuch: return "Nothing selected.";
    case Deal::kNoFunds: return "Insufficient funds.";
    case Deal::kLastMech: return "You can't sell your last mech.";
    case Deal::kDealerFull: return "The dealer buys no more mechs until the market changes.";
    case Deal::kRefit: return "It lacks parts of its stock loadout: refit it first.";
    case Deal::kHangarFull: return "Your hangar is full: sell a 'Mech first.";
  }
  return "";
}

const mech3::Config* stock_loadout(int32_t mech) {
  const mech3::MechDfn* d = mech3::mech_dfn(mech);
  if (!d || !d->configs) return nullptr;
  if (const mech3::Config* c = d->configs->find(mech3::pilot_hooks().message(kStandardConfigMessage))) return c;
  return d->configs->configs.empty() ? nullptr : d->configs->configs.front();
}

Deal can_buy(const Company& c, int32_t stock_index) {
  const Market& m = c.market;
  if (stock_index < 0 || stock_index >= static_cast<int32_t>(m.stock.size())) return Deal::kNoSuch;
  if (c.funds < market_buy_price(c, m.stock[static_cast<size_t>(stock_index)])) return Deal::kNoFunds;
  if (owned_count(c) >= kOwnedMost) return Deal::kHangarFull;  // MW1's order: the funds, then the room
  return Deal::kDone;
}

Deal buy(Company& c, int32_t stock_index) {
  if (const Deal d = can_buy(c, stock_index); d != Deal::kDone) return d;
  Market& m = c.market;
  const int32_t mech = m.stock[static_cast<size_t>(stock_index)];
  c.funds -= market_buy_price(c, mech);
  m.stock.erase(m.stock.begin() + stock_index);
  OwnedMech o;
  o.mech = mech;
  c.hangar.push_back(std::move(o));
  return Deal::kDone;
}

namespace {
// The stores after `owned_index`'s sale (its loadout in, its stock loadout out), or false: a refit needed.
bool stores_after_sale(const Company& c, int32_t owned_index, const StockLoadout& stock, mech3::Store* after) {
  const int32_t mech = owned_mech(c, owned_index);
  const std::shared_ptr<mech3::Config>& loadout =
      owned_index == 0 ? c.loadout : c.hangar[static_cast<size_t>(owned_index - 1)].loadout;
  if (!loadout || !c.stores) return true;
  const mech3::Config* s = stock ? stock(mech) : nullptr;
  if (!s) return false;
  after->assign(*c.stores);
  after->add_config(loadout.get());
  if (!after->has_config(-1, static_cast<const mech3::Store*>(nullptr), s)) return false;
  after->remove_config(s);
  return true;
}
}  // namespace

Deal can_sell(const Company& c, int32_t owned_index, const StockLoadout& stock) {
  if (owned_index < 0 || owned_index >= owned_count(c)) return Deal::kNoSuch;
  if (owned_count(c) == 1) return Deal::kLastMech;
  if (c.market.limit >= 0 && c.market.sold >= c.market.limit) return Deal::kDealerFull;
  mech3::Store after;
  return stores_after_sale(c, owned_index, stock, &after) ? Deal::kDone : Deal::kRefit;
}

Deal sell(Company& c, int32_t owned_index, const StockLoadout& stock) {
  if (const Deal d = can_sell(c, owned_index, stock); d != Deal::kDone) return d;
  const int32_t mech = owned_mech(c, owned_index);
  const std::shared_ptr<mech3::Config>& loadout =
      owned_index == 0 ? c.loadout : c.hangar[static_cast<size_t>(owned_index - 1)].loadout;
  // Back to stock: its items into a copy of the stores, the stock loadout's taken out of it (can_sell checked
  // they're all there); a mech on its stock loadout already leaves the stores as they are.
  if (loadout && c.stores) {
    mech3::Store after;
    stores_after_sale(c, owned_index, stock, &after);
    c.stores->assign(after);
    clamp_stores(*c.stores);
  }
  c.funds += market_sell_price(c, mech);
  c.market.stock.push_back(mech);
  ++c.market.sold;
  // FIXES 118: its kits back in the storehouse; the next mech's go with it.
  if (owned_index == 0) {
    upgrades::return_kits(c, c.kits);
    c.mech = c.hangar.front().mech, c.loadout = c.hangar.front().loadout, c.kits = c.hangar.front().kits;
    c.hangar.erase(c.hangar.begin());
  } else {
    upgrades::return_kits(c, c.hangar[static_cast<size_t>(owned_index - 1)].kits);
    c.hangar.erase(c.hangar.begin() + (owned_index - 1));
  }
  return Deal::kDone;
}

Deal pilot(Company& c, int32_t owned_index) {
  if (owned_index <= 0 || owned_index >= owned_count(c)) return owned_index == 0 ? Deal::kDone : Deal::kNoSuch;
  OwnedMech& o = c.hangar[static_cast<size_t>(owned_index - 1)];
  std::swap(c.mech, o.mech);
  std::swap(c.loadout, o.loadout);
  std::swap(c.kits, o.kits);  // FIXES 118: the kits stay on their mech
  return Deal::kDone;
}

}  // namespace mw1::market
