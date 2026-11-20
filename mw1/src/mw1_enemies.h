#pragma once
// FIXES 104 (Owen, for 1.0.0): every 'Mech of Pirate's Moon's as an enemy, in its real loadouts. MW1 rolls its force
// as it always has (mw1_campaign.cpp: the budget, the classes, at most four; the ladder's rungs for a story battle);
// each enemy then gets a PM chassis near its MW1 kind's tonnage, and one of the chassis's stock loadouts. No engine:
// the tests roll it as the game does.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "mw1_campaign.h"

namespace mw1 {

// ---- Owen's four (the lead, 2026-11-19: the defaults approved, each his to change here) ----

// PM's Elemental (battle armour, 10 t, one loadout) fields as an enemy 'Mech: no, it isn't a 'Mech.
inline constexpr bool kElementalsField = false;
// A slot's chance (percent) of a Clan chassis, by the world's tier (0 to 3): MW1 is 3025, Clan tech a rare prize
// that the harder worlds field more (and whose salvage the market pays double for).
inline constexpr int32_t kClanPercent[kTierCount] = {0, 10, 25, 40};
// What a wreck pays from: MW1's kind (its stand-in chassis's price at the world, as before 104) or the chassis it
// really was.
enum class WreckPay { kMw1Kind, kChassis };
inline constexpr WreckPay kWreckPay = WreckPay::kMw1Kind;
// A story battle's enemies roll their chassis too (the Dark Wing's last rung as it was: its kinds' stand-ins).
inline constexpr bool kStoryBattlesRoll = true;

// ---- The rest of the rule ----

// A slot's chassis: within this many tons of its MW1 kind's (MW1's balance kept, slot by slot).
inline constexpr int32_t kSlotTons = 10;
// The 100-tonners only where it's late: the world's tier at least this, or the company's record band (MW1's, by
// kills: 0 to 3) at least this.
inline constexpr int32_t kHundredTons = 100, kHundredTonTier = 2, kHundredTonBand = 3;
// The loadout: Primary this often (percent), the Alternates sharing kAlternatePercent, the Exotics the rest; a chassis
// with no Alternates, Primary kPrimaryAlonePercent and the Exotics the rest.
inline constexpr int32_t kPrimaryPercent = 60, kAlternatePercent = 30, kPrimaryAlonePercent = 75;

// A chassis's stock loadouts as PM has them, in its order: the Primary (0), then the Alternates, then the Exotics.
struct StockLoadouts {
  int32_t alternates = 0, exotics = 0;
};
StockLoadouts stock_loadouts(int32_t chassis);  // none for an unknown chassis
enum class LoadoutKind { kPrimary, kAlternate, kExotic };
LoadoutKind loadout_kind(int32_t chassis, int32_t loadout);
int32_t loadout_count(int32_t chassis);  // 1 + alternates + exotics

// May `chassis` field as an enemy at all (a 'Mech, the Elemental by kElementalsField).
bool fields(int32_t chassis);
bool is_clan(int32_t chassis);
// The chassis an MW1 kind's slot may be, Clan or Inner Sphere, at this tier and record band (ascending ids).
std::vector<int32_t> slot_chassis(int32_t kind, bool clan, int32_t tier, int32_t band);

struct Fielded {
  int32_t chassis = -1, loadout = 0;
};
// One slot, rolled: Clan by kClanPercent (none of that tech in reach: the other), a chassis among those in reach, a
// loadout by the weights. `seed` makes it repeatable (the contract's own, below).
class EnemyRng {
 public:
  explicit EnemyRng(uint32_t seed) : rng_(seed) {}
  int32_t upto(int32_t max) { return rng_.upto(max); }

 private:
  Rng rng_;
};
Fielded roll_enemy(int32_t kind, int32_t tier, int32_t band, EnemyRng& rng);
int32_t roll_loadout(int32_t chassis, EnemyRng& rng);

// The contract's force fielded: each enemy's chassis and loadout into k.chassis and k.loadouts. MW1's own rolls
// (`rng` of generate_contract) are untouched: these draw from their own generator, seeded by the contract and the
// company (its funds, kills, date and world), so the same offer rolls the same 'Mechs.
void field_force(const Company& c, Contract& k);
// Each enemy as the mission fields it: the rolled chassis and loadout, else (an older save's contract, a test's
// force) the kind's stand-in on its Primary, as before 104.
int32_t enemy_chassis(const Contract& k, size_t slot);
int32_t enemy_loadout(const Contract& k, size_t slot);
// The chassis a wreck pays as (kWreckPay).
int32_t pay_chassis(const Wreck& w);
// A wreck's chassis by name: the real one, else its kind's stand-in.
int32_t wreck_chassis(const Wreck& w);

}  // namespace mw1
