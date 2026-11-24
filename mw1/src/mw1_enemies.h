#pragma once
// FIXES 104 (the owner, for 1.0.0): every 'Mech of Pirate's Moon's as an enemy, in its real loadouts. MW1 rolls its force
// as it always has (mw1_campaign.cpp: the budget, the classes, at most four, in tonnage slots; the ladder's rungs for
// a story battle); each slot then gets a chassis of the roster from its whole weight class (FIXES 112: all of
// The owner's 'Mechs, not those near MW1's own), and one of the chassis's stock loadouts. No engine: the tests roll it as the game does.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "mw1_campaign.h"

namespace mw1 {

// ---- The roster (the owner, FIXES 112): every chassis an enemy can be, MW3's and PM's 'Mechs. The one list: cut it to any
// subset and the rest follows (the slots field the nearest it has). The eight the game fielded before 104 were
// kPmFirefly, kPmOwens, kPmStrider, kPmBushwacker, kPmChampion, kPmAvatar, kPmOrion, kPmAwesome. The Elemental is here
// but fields only by kElementalsField. ----
inline constexpr int32_t kRoster[] = {
    kPmAnnihilator, kPmAtlas, kPmAvatar, kPmAwesome, kPmBlackhawk, kPmBushwacker, kPmCauldron, kPmCenturion, kPmChampion,
    kPmClint, kPmDaishi, kPmElemental, kPmFirefly, kPmMadCat, kPmMasakari, kPmOrion, kPmOwens, kPmPuma, kPmRyoken,
    kPmShadowCat, kPmStrider, kPmSunder, kPmSupernova, kPmThor, kPmVulture};
// The roster in use: kRoster, or (a test) another list in its place; an empty list puts kRoster back.
const std::vector<int32_t>& roster();
void set_roster_for_tests(const std::vector<int32_t>& chassis);

// ---- The owner's four (the lead, 2026-11-19: the defaults approved, each his to change here) ----

// PM's Elemental (battle armour, 10 t, one loadout) fields as an enemy 'Mech: no, it isn't a 'Mech.
inline constexpr bool kElementalsField = false;
// Clan and Inner Sphere chassis alike (the owner, FIXES 112: "the clan mechs arent that much better than the IS mechs.
// they can be treated generally the same"): a slot draws from both. The switch puts the gate back: a slot is then Clan
// by kClanPercent of the world's tier (0 to 3), else Inner Sphere.
inline constexpr bool kClanGated = false;
inline constexpr int32_t kClanPercent[kTierCount] = {20, 30, 40, 50};
// What a wreck pays from: its slot (the slot's stand-in chassis's price at the world, as before 104) or the chassis
// it really was.
enum class WreckPay { kSlot, kChassis };
inline constexpr WreckPay kWreckPay = WreckPay::kSlot;
// A story battle's enemies roll their chassis too (a scripted rung, the Dark Wing's, fields its own: mw1_ladder.h).
inline constexpr bool kStoryBattlesRoll = true;

// ---- The rest of the rule ----

// A slot's chassis: any of its weight class (PM's own classes, mech3::weight_class: light, medium, heavy, assault),
// uniformly, the slot's class by its tons as PM classes them (slot_class). MW1's balance holds at the class level:
// its budget and class counts are unchanged.
// The 100-tonners only where it's late: the world's tier at least this, or the company's record band (MW1's, by
// kills: 0 to 3) at least this; then in any heavy or assault slot.
inline constexpr int32_t kHundredTons = 100, kHundredTonTier = 1, kHundredTonBand = 3;
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

// May `chassis` field as an enemy at all (on the roster; the Elemental by kElementalsField).
bool fields(int32_t chassis);
bool is_clan(int32_t chassis);
// A slot's weight class as PM's (0 light .. 3 assault), by its tons: under 40 light, under 60 medium, under 80 heavy.
int32_t slot_class(int32_t slot);
// The chassis a slot may be, Clan or Inner Sphere, at this tier and record band (ascending ids).
std::vector<int32_t> slot_chassis(int32_t slot, bool clan, int32_t tier, int32_t band);
// Both techs' (ascending ids): the slot's pool when the Clan isn't gated.
std::vector<int32_t> slot_pool(int32_t slot, int32_t tier, int32_t band);
// A slot's stand-in: the roster chassis nearest its tons, Inner Sphere first, a tie to the lighter (the 100-tonners
// only when nothing else is on the roster). With the full roster: the eight the game fielded before 104. -1 for no
// roster or no slot.
int32_t stand_in(int32_t slot);

struct Fielded {
  int32_t chassis = -1, loadout = 0;
};
// One slot, rolled: a chassis of its pool, uniformly (kClanGated: Clan by kClanPercent, none of that tech in reach
// the other), a loadout by the weights. `seed` makes it repeatable (the contract's own, below).
class EnemyRng {
 public:
  explicit EnemyRng(uint32_t seed) : rng_(seed) {}
  int32_t upto(int32_t max) { return rng_.upto(max); }

 private:
  Rng rng_;
};
Fielded roll_enemy(int32_t slot, int32_t tier, int32_t band, EnemyRng& rng);
int32_t roll_loadout(int32_t chassis, EnemyRng& rng);

// The contract's force fielded: each enemy's chassis and loadout into k.chassis and k.loadouts. MW1's own rolls
// (`rng` of generate_contract) are untouched: these draw from their own generator, seeded by the contract and the
// company (its funds, kills, date and world), so the same offer rolls the same 'Mechs.
void field_force(const Company& c, Contract& k);
// Each enemy as the mission fields it: the rolled chassis and loadout, else (an older save's contract, a test's
// force) the slot's stand-in on its Primary, as before 104.
int32_t enemy_chassis(const Contract& k, size_t slot);
int32_t enemy_loadout(const Contract& k, size_t slot);
// The chassis a wreck pays as (kWreckPay).
int32_t pay_chassis(const Wreck& w);
// A wreck's chassis by name: the real one, else its slot's stand-in.
int32_t wreck_chassis(const Wreck& w);

}  // namespace mw1
