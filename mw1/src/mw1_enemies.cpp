#include "mw1_enemies.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"

namespace mw1 {
namespace {

// PM's stock loadouts per chassis, in its roster's order (mech3::mech_name: annihilator .. vulture), as its mech
// definitions list them: the Primary, Alternates, Exotics (research/mw1-104: PM's zrd; the engine test checks them).
constexpr StockLoadouts kStock[mech3::kMechCount] = {
    {0, 2},  // annihilator
    {0, 4},  // atlas
    {3, 3},  // avatar
    {0, 4},  // awesome
    {4, 0},  // blackhawk
    {0, 2},  // bushwacker
    {3, 1},  // cauldron
    {0, 4},  // centurion
    {0, 1},  // champion
    {0, 4},  // clint
    {2, 0},  // daishi
    {0, 0},  // elemental
    {0, 0},  // firefly
    {4, 3},  // madcat
    {3, 1},  // masakari
    {0, 1},  // orion
    {4, 0},  // owens
    {4, 1},  // puma
    {4, 1},  // ryoken
    {2, 2},  // shadowcat
    {4, 2},  // strider
    {2, 3},  // sunder
    {0, 1},  // supernova
    {4, 1},  // thor
    {3, 1},  // vulture
};

int32_t tons(int32_t chassis) { return static_cast<int32_t>(std::lround(mech3::max_mass(chassis))); }

// FNV-1a over the words: the contract's own seed.
uint32_t mix(uint32_t h, int64_t v) {
  for (int i = 0; i < 8; ++i) h = (h ^ static_cast<uint8_t>(v >> (8 * i))) * 16777619u;
  return h;
}

std::vector<int32_t>& roster_in_use() {
  static std::vector<int32_t> r(std::begin(kRoster), std::end(kRoster));
  return r;
}

}  // namespace

const std::vector<int32_t>& roster() { return roster_in_use(); }

void set_roster_for_tests(const std::vector<int32_t>& chassis) {
  roster_in_use() = chassis.empty() ? std::vector<int32_t>(std::begin(kRoster), std::end(kRoster)) : chassis;
}

StockLoadouts stock_loadouts(int32_t chassis) {
  return chassis >= 0 && chassis < mech3::kMechCount ? kStock[chassis] : StockLoadouts{};
}

int32_t loadout_count(int32_t chassis) {
  const StockLoadouts s = stock_loadouts(chassis);
  return 1 + s.alternates + s.exotics;
}

LoadoutKind loadout_kind(int32_t chassis, int32_t loadout) {
  const StockLoadouts s = stock_loadouts(chassis);
  if (loadout <= 0) return LoadoutKind::kPrimary;
  return loadout <= s.alternates ? LoadoutKind::kAlternate : LoadoutKind::kExotic;
}

bool fields(int32_t chassis) {
  if (chassis < 0 || chassis >= mech3::kMechCount) return false;
  const std::vector<int32_t>& r = roster();
  if (std::find(r.begin(), r.end(), chassis) == r.end()) return false;
  return chassis != mech3::kElemental || kElementalsField;
}

bool is_clan(int32_t chassis) { return mech3::mech_tech(chassis) == mech3::kClan; }

int32_t slot_class(int32_t slot) {
  if (slot < 0 || slot >= kSlotCount) return -1;
  const int32_t t = kForceSlots[slot].tons;
  return t < 40 ? 0 : t < 60 ? 1 : t < 80 ? 2 : 3;
}

std::vector<int32_t> slot_chassis(int32_t slot, bool clan, int32_t tier, int32_t band) {
  std::vector<int32_t> out;
  const int32_t cls = slot_class(slot);
  if (cls < 0) return out;
  const bool late = tier >= kHundredTonTier || band >= kHundredTonBand;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    if (!fields(m) || is_clan(m) != clan) continue;
    const bool in_reach = tons(m) >= kHundredTons ? late && cls >= 2 : mech3::weight_class(m) == cls;
    if (in_reach) out.push_back(m);
  }
  return out;
}

int32_t roll_loadout(int32_t chassis, EnemyRng& rng) {
  const StockLoadouts s = stock_loadouts(chassis);
  if (!s.alternates && !s.exotics) return 0;
  const int32_t r = rng.upto(99);
  if (!s.alternates) return r < kPrimaryAlonePercent ? 0 : 1 + rng.upto(s.exotics - 1);
  if (r < kPrimaryPercent) return 0;
  if (r < kPrimaryPercent + kAlternatePercent || !s.exotics) return 1 + rng.upto(s.alternates - 1);
  return 1 + s.alternates + rng.upto(s.exotics - 1);
}

int32_t stand_in(int32_t slot) {
  if (slot < 0 || slot >= kSlotCount) return -1;
  const int32_t want = kForceSlots[slot].tons;
  int32_t best = -1;
  int64_t best_key = 0;
  for (int32_t m : roster()) {
    if (!fields(m)) continue;
    const int32_t t = tons(m);
    // Inner Sphere first, below 100 t first, then the nearest, then the lighter.
    const int64_t key = (int64_t{is_clan(m)} << 40) + (int64_t{t >= kHundredTons} << 32) + (int64_t{std::abs(t - want)} << 16) + t;
    if (best < 0 || key < best_key) best = m, best_key = key;
  }
  return best;
}

std::vector<int32_t> slot_pool(int32_t slot, int32_t tier, int32_t band) {
  std::vector<int32_t> out = slot_chassis(slot, false, tier, band);
  for (int32_t m : slot_chassis(slot, true, tier, band)) out.push_back(m);
  std::sort(out.begin(), out.end());
  return out;
}

Fielded roll_enemy(int32_t slot, int32_t tier, int32_t band, EnemyRng& rng) {
  Fielded f;
  std::vector<int32_t> pool;
  if (kClanGated) {
    const bool clan = rng.upto(99) < kClanPercent[tier >= 0 && tier < kTierCount ? tier : 0];
    pool = slot_chassis(slot, clan, tier, band);
    if (pool.empty()) pool = slot_chassis(slot, !clan, tier, band);
  } else {
    pool = slot_pool(slot, tier, band);
  }
  if (pool.empty()) {  // no roster chassis near it (a cut roster): its stand-in, on its Primary
    f.chassis = stand_in(slot);
    return f;
  }
  f.chassis = pool[static_cast<size_t>(rng.upto(static_cast<int32_t>(pool.size()) - 1))];
  f.loadout = roll_loadout(f.chassis, rng);
  return f;
}

void field_force(const Company& c, Contract& k) {
  uint32_t h = 2166136261u;
  for (int64_t v : {c.funds, int64_t{c.kills}, int64_t{c.planet_number}, int64_t{c.date.year}, int64_t{c.date.month},
                    int64_t{c.date.units}, int64_t{k.budget}, int64_t{k.payment_ceiling}, int64_t{k.rung}})
    h = mix(h, v);
  for (int32_t e : k.enemies) h = mix(h, e);
  EnemyRng rng(h);
  const int32_t band = record_band(c.kills);
  k.chassis.clear(), k.loadouts.clear();
  for (int32_t e : k.enemies) {
    const Fielded f = roll_enemy(e, c.tier, band, rng);
    k.chassis.push_back(f.chassis), k.loadouts.push_back(f.loadout);
  }
}

int32_t enemy_chassis(const Contract& k, size_t slot) {
  if (k.chassis.size() == k.enemies.size() && slot < k.chassis.size() && k.chassis[slot] >= 0) return k.chassis[slot];
  return slot < k.enemies.size() ? stand_in(k.enemies[slot]) : -1;
}

int32_t enemy_loadout(const Contract& k, size_t slot) {
  if (k.loadouts.size() != k.enemies.size() || k.chassis.size() != k.enemies.size() || slot >= k.loadouts.size()) return 0;
  const int32_t l = k.loadouts[slot];
  return l >= 0 && l < loadout_count(enemy_chassis(k, slot)) ? l : 0;
}

int32_t wreck_chassis(const Wreck& w) {
  if (w.chassis >= 0 && w.chassis < mech3::kMechCount) return w.chassis;
  return stand_in(w.slot);
}

int32_t pay_chassis(const Wreck& w) {
  if (kWreckPay == WreckPay::kChassis) return wreck_chassis(w);
  return stand_in(w.slot);
}

}  // namespace mw1
