#include "mw1_dealer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_market.h"
#include "mw1_weapon_data.h"

namespace mw1::dealer {

namespace {
constexpr int32_t kEquipKinds = 15;  // PM's equipment table (mech_names.cpp's kEquipment)
constexpr int32_t kPlainArmour = 1;  // "armor": every 'Mech's, never an item to buy

// The Inner Sphere wares no fielded loadout carries (FIXES 111's report, FIXES 122's list), and their families.
bool uncommon_name(const char* n) {
  static const char* const kUncommon[] = {"AC2",        "AC5",        "PPC",        "ERLaser_Small", "SRM2",     "LB2XAC",
                                          "LB5XAC",     "LB10XAC",    "LB20XAC",    "UltraAC2",      "UltraAC5", "UltraAC10",
                                          "UltraAC20",  "StreakSRM2", "StreakSRM4", "StreakSRM6",    "MRM10",    "MRM20",
                                          "MRM30",      "MRM40",      "TBOLT5",     "TBOLT10",       "TBOLT15",  "TBOLT20",
                                          "masc",       "ecm",        "tc"};
  for (const char* u : kUncommon)
    if (std::strcmp(n, u) == 0) return true;
  return false;
}
const char* equipment_name(int32_t kind) {
  static const char* const kNames[kEquipKinds] = {"ams", "armor", "armorff", "fcs",   "bap", "case", "c3", "c3slave",
                                                  "ecm", "sink",  "dsink",   "jjets", "masc", "tag", "tc"};
  return kind >= 0 && kind < kEquipKinds ? kNames[kind] : "none";
}
int64_t floored(int64_t n) { return n / 1000 * 1000; }
std::string thousands(int64_t n) {
  std::string d = std::to_string(n < 0 ? -n : n), out;
  for (size_t i = 0; i < d.size(); ++i) {
    if (i && (d.size() - i) % 3 == 0) out += ',';
    out += d[i];
  }
  return (n < 0 ? "-" : "") + out;
}
// The stores' item of the ware, null none.
mech3::ConfigItem* stores_item(const Company& c, const Ware& w) {
  if (!c.stores) return nullptr;
  for (mech3::ConfigItem* it : c.stores->items) {
    if (!it || it->kind() != w.kind || it->id() != w.id) continue;
    if (const auto* x = dynamic_cast<const mech3::WeaponItem*>(it); x && x->tech != w.tech) continue;
    if (const auto* x = dynamic_cast<const mech3::EquipmentItem*>(it); x && x->tech != w.tech) continue;
    return it;
  }
  return nullptr;
}
int32_t units_of(const Ware& w) { return w.kind == kAmmo ? ammo_lot(w.id) : 1; }
}  // namespace

const char* rarity_words(Rarity r) {
  switch (r) {
    case Rarity::kCommon: return "common";
    case Rarity::kUncommon: return "uncommon";
    case Rarity::kRare: return "rare";
  }
  return "";
}

std::vector<Ware> wares() {
  std::vector<Ware> out;
  for (int32_t w = 0; w < mech3::kWeaponCount; ++w) {
    for (int32_t tech : {mech3::kInnerSphere, mech3::kClan})
      if (mech3::weapon_has_tech(w, tech)) out.push_back({kWeapon, w, tech});
  }
  for (int32_t w = 0; w < mech3::kWeaponCount; ++w)
    if (mech3::weapon_uses_ammo(w) && (mech3::weapon_has_tech(w, mech3::kInnerSphere) || mech3::weapon_has_tech(w, mech3::kClan)))
      out.push_back({kAmmo, w, 0});
  for (int32_t k = 0; k < kEquipKinds; ++k) {
    if (k == kPlainArmour) continue;
    for (int32_t tech : {mech3::kInnerSphere, mech3::kClan})
      if (mech3::equip_has_tech(k, tech)) out.push_back({kEquipment, k, tech});
  }
  return out;
}

Rarity rarity(const Ware& w) {
  if (w.kind == kAmmo) {  // its weapon's least rare: the Inner Sphere's when it has one
    if (!mech3::weapon_has_tech(w.id, mech3::kInnerSphere)) return Rarity::kRare;
    return uncommon_name(mech3::weapon_name(w.id)) ? Rarity::kUncommon : Rarity::kCommon;
  }
  if (w.tech == mech3::kClan) return Rarity::kRare;
  const char* n = w.kind == kWeapon ? mech3::weapon_name(w.id) : equipment_name(w.id);
  return uncommon_name(n) ? Rarity::kUncommon : Rarity::kCommon;
}

bool stocked_at(Rarity r, int32_t tier, int32_t planet_number) {
  switch (r) {
    case Rarity::kCommon: return true;
    case Rarity::kUncommon: return tier >= kUncommonTier || market::clan_world(planet_number);
    case Rarity::kRare: return tier >= kRareTier || market::clan_world(planet_number);
  }
  return false;
}

std::string ware_name(const Ware& w) {
  const std::string clan = w.tech == mech3::kClan ? " (Clan)" : "";
  if (w.kind == kWeapon) return std::string("weapon ") + mech3::weapon_name(w.id) + clan;
  if (w.kind == kAmmo) return std::string("ammo ") + mech3::weapon_name(w.id);
  return std::string("equipment ") + equipment_name(w.id) + clan;
}

float ware_tons(const Ware& w) {
  if (w.kind == kWeapon) return weapon_data::tons(w.id, w.tech);  // the engine's, else MW1's read at the hub
  if (w.kind == kEquipment) return mech3::equip_weight(w.id, w.tech);
  return 1.0f;
}

int32_t ammo_lot(int32_t weapon) {
  const float per_round = weapon_data::round_tons(weapon);
  return per_round > 0 ? std::max(1, static_cast<int32_t>(std::lround(1.0f / per_round))) : 1;
}

int64_t tier0_price(const Ware& w) {
  if (w.kind == kAmmo) return kAmmoPerTon;
  int64_t p = std::max(kPriceLeast, static_cast<int64_t>(std::llround(static_cast<double>(ware_tons(w)) * kPricePerTon)));
  if (w.tech == mech3::kClan) p = p * kClanPercent / 100;
  return floored(p);
}
int64_t buy_price(const Ware& w, int32_t tier) { return floored(tier0_price(w) * market::buy_percent(tier) / 100); }
int64_t sell_price(const Ware& w, int32_t tier) { return floored(tier0_price(w) * market::sell_percent(tier) / 100); }

uint32_t stock_seed(const Company& c) {
  uint32_t h = 2166136261u;
  for (int32_t v : {c.planet_number, c.date.year, c.date.month, c.date.units, c.market_clock}) {
    for (int i = 0; i < 4; ++i) h = (h ^ static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * i))) * 16777619u;
  }
  return h;
}

void roll_stock(Company& c) {
  Rng rng(stock_seed(c));
  const std::vector<Ware> all = wares();
  std::vector<size_t> pool;
  for (size_t i = 0; i < all.size(); ++i)
    if (stocked_at(rarity(all[i]), c.tier, c.planet_number)) pool.push_back(i);
  auto weight = [&](size_t i) {
    const Rarity r = rarity(all[i]);
    return r == Rarity::kCommon ? kCommonWeight : r == Rarity::kUncommon ? kUncommonWeight : kRareWeight;
  };
  const int32_t kinds = std::min(static_cast<int32_t>(pool.size()), kKindsBase + rng.upto(kKindsRoll));
  std::vector<std::pair<size_t, int32_t>> drawn;  // the ware's index, its count
  for (int32_t k = 0; k < kinds; ++k) {
    int32_t total = 0;
    for (size_t i : pool) total += weight(i);
    int32_t r = rng.upto(total - 1);
    size_t at = 0;
    while (at + 1 < pool.size() && r >= weight(pool[at])) r -= weight(pool[at]), ++at;
    drawn.push_back({pool[at], 1 + rng.upto(kCountMost - 1)});
    pool.erase(pool.begin() + static_cast<long>(at));
  }
  std::sort(drawn.begin(), drawn.end());
  c.dealer.stock.clear();
  for (const auto& [i, n] : drawn) c.dealer.stock.push_back({all[i].kind, all[i].id, all[i].tech, n});
  c.dealer.rolled = true;
}

const char* trade_words(Trade t) {
  switch (t) {
    case Trade::kDone: return "";
    case Trade::kNoSuch: return "Nothing selected.";
    case Trade::kNoFunds: return "Insufficient funds.";
    case Trade::kStoresFull: return "The stores hold no more of these.";
    case Trade::kNotBuying: return "The dealer isn't buying.";
  }
  return "";
}

int32_t in_stores(const Company& c, const Ware& w) {
  const mech3::ConfigItem* it = stores_item(c, w);
  return it ? it->count / units_of(w) : 0;
}

Trade buy(Company& c, int32_t index) {
  if (index < 0 || index >= static_cast<int32_t>(c.dealer.stock.size())) return Trade::kNoSuch;
  DealerItem& line = c.dealer.stock[static_cast<size_t>(index)];
  const Ware w{line.kind, line.id, line.tech};
  if (line.count <= 0) return Trade::kNoSuch;
  const int64_t price = buy_price(w, c.tier);
  if (c.funds < price) return Trade::kNoFunds;
  const int32_t units = units_of(w);
  if (const mech3::ConfigItem* it = stores_item(c, w); it && it->count + units > market::kStoreMost) return Trade::kStoresFull;
  if (!c.stores) c.stores = std::make_shared<mech3::Store>();
  if (w.kind == kWeapon) c.stores->add_weapon(w.id, w.tech);
  else if (w.kind == kAmmo) c.stores->add_ammo(w.id, units);
  else c.stores->add_equipment(w.id, w.tech, 1);
  c.funds -= price;
  if (--line.count == 0) c.dealer.stock.erase(c.dealer.stock.begin() + index);
  return Trade::kDone;
}

Trade sell(Company& c, const Ware& w) {
  if (!kBuysBack) return Trade::kNotBuying;
  mech3::ConfigItem* it = stores_item(c, w);
  const int32_t units = units_of(w);
  if (!it || it->count < units) return Trade::kNoSuch;
  c.stores->remove(it, units);
  c.funds += sell_price(w, c.tier);
  for (DealerItem& line : c.dealer.stock)
    if (line.kind == w.kind && line.id == w.id && line.tech == w.tech) {
      line.count = std::min(line.count + 1, kStockMost);
      return Trade::kDone;
    }
  c.dealer.stock.push_back({w.kind, w.id, w.tech, 1});
  return Trade::kDone;
}

std::string price_table() {
  std::string out;
  char line[256];
  std::snprintf(line, sizeof line,
                "DEALER (placeholders): %s a ton (at least %s), Clan x%d%%, ammunition %s a one-ton lot; tiers buy %d/%d/%d/%d%%, "
                "sell %d/%d/%d/%d%%%s\n",
                thousands(kPricePerTon).c_str(), thousands(kPriceLeast).c_str(), kClanPercent, thousands(kAmmoPerTon).c_str(),
                market::buy_percent(0), market::buy_percent(1), market::buy_percent(2), market::buy_percent(3), market::sell_percent(0),
                market::sell_percent(1), market::sell_percent(2), market::sell_percent(3), kBuysBack ? "" : " (not buying)");
  out += line;
  out += "DEALER   ware                              rarity       tons   tier 0 buy   tier 3 buy  tier 3 sell\n";
  for (const Ware& w : wares()) {
    std::snprintf(line, sizeof line, "DEALER   %-32s  %-9s  %6.2f  %11s  %11s  %11s\n", ware_name(w).c_str(), rarity_words(rarity(w)),
                  static_cast<double>(ware_tons(w)), thousands(buy_price(w, 0)).c_str(), thousands(buy_price(w, 3)).c_str(),
                  thousands(sell_price(w, 3)).c_str());
    out += line;
  }
  return out;
}

}  // namespace mw1::dealer
