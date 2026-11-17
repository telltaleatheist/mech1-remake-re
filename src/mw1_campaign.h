#pragma once
// MW1's campaign rules for the first milestone (VISION.md, "The first
// milestone"): a new company, one contract generated as MW1 does it, and the
// settlement when the mission comes back. No SDL and no engine here, so the
// tests reach it all.
//
// The numbers are MW1's, from the player's MW_MAIN.EXE: its data segment
// 0x1090 (DS:xxxx below), read the way Elthial's SDL port loads it
// (SDLBackend_Files.c, MwHostLoadCampaignInitialData). The rules follow the
// port's MW_MAIN_081B_{ECONOMY,CONTRACTS,STARTUP,REPAIR}.c (research/contracts.md,
// research/economy.md). test_mw1_campaign.cpp checks each table against the
// player's exe when it is there.
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace mw1 {

// MW1's campaign mech kinds, in the exe's order (DS:122B tonnages, DS:305F prices).
enum Kind : int32_t { kLocust, kWasp, kJenner, kPhoenixHawk, kShadowHawk, kWolverine, kRifleman, kWarhammer, kMarauder, kBattleMaster, kKindCount };

struct MechKind {
  const char* name;
  int32_t tons;         // DS:122B
  int32_t weight;       // DS:4B55: the contract's force weight (1 light, 2 medium, 3 heavy)
  int32_t base_price;   // DS:305F: thousands per multiplier point (price = base x multiplier x 100)
  int32_t pm_mech;      // the Pirate's Moon Inner Sphere mech that stands in for it (research/contracts.md)
};
extern const MechKind kMechKinds[kKindCount];

// Pirate's Moon's mech ids (mech3/mech_names.cpp's roster order) that MW1 uses.
enum PmMech : int32_t { kPmAwesome = 3, kPmBushwacker = 5, kPmCenturion = 7, kPmChampion = 8, kPmFirefly = 12, kPmOrion = 15,
                        kPmOwens = 16, kPmStrider = 20, kPmAvatar = 2 };

enum House : int32_t { kKurita, kSteiner, kMarik, kLiao, kDavion, kHouseCount };
struct HouseRules {
  const char* name;
  int32_t payment_bias;       // DS:4B41: added to the payment roll
  int32_t payment_adjust;     // DS:4B4B: the rolled payment's sign, +1 or -1 (0 none)
  int32_t salvage_base;       // DS:4B5F: percent
  int32_t advance_base;       // DS:4B69: percent
  int32_t deduction_roll;     // DS:1241: a settlement roll (0..100) at or under it pays...
  int32_t reduced_payout;     // DS:124B: ...this percent of the fee and salvage
};
extern const HouseRules kHouses[kHouseCount];

inline constexpr int32_t kStartingFunds = 1000000;  // DS:0491
inline constexpr int32_t kTierMultipliers[4] = {10, 12, 13, 15};  // DS:3057

// MW1's random rolls: 0..max, both ends included (MwRandomUpTo). Seeded for the tests.
class Rng {
 public:
  explicit Rng(uint32_t seed) : g_(seed) {}
  int32_t upto(int32_t max) { return std::uniform_int_distribution<int32_t>(0, max)(g_); }

 private:
  std::mt19937 g_;
};

// One line of the company's stores: a PM store item (mech3::ConfigItem's kind
// and id) and how many.
struct StoreItem {
  int32_t kind = 0, id = 0, count = 0;
};

struct Company {
  int64_t funds = 0;
  int32_t mech = kPmOwens;  // the commander's mech (PM id), with its stock loadout
  std::vector<StoreItem> stores;
  int32_t kills = 0;  // playerCombatCredit: enemy mechs disabled, all campaign long
  int32_t favor[kHouseCount] = {}, grievance[kHouseCount] = {};
  // Where the company is. The milestone has one placeholder world.
  std::string planet = "Planet";
  int32_t planet_house = kDavion, tier = 0;
};

// MW1's "Lance weight" for a PM mech, by its tonnage as MW1's own kinds are
// classed: under 40 t light (1), up to 55 t medium (2), heavier heavy (3).
int32_t lance_weight_of_tons(int32_t tons);

enum class Objective { kDestroyAll, kReachStructure };

struct Contract {
  int32_t house = kDavion;     // the employer (the planet's House)
  int32_t budget = 0;          // the enemy force budget
  std::vector<int32_t> enemies;  // MW1 kinds
  int32_t payment = 0;         // thousands: the opening offer the milestone takes
  int32_t payment_ceiling = 0;  // thousands: negotiation's top (later)
  int32_t salvage_percent = 0, advance_percent = 0;
  int32_t remaining = 0;       // thousands: the payment less the advance, once accepted
  bool accepted = false;
  Objective objective = Objective::kDestroyAll;
  // The map: a PM world, its Instant Action archive and the mode (0 Battle).
  std::string world = "c2";
  int32_t ia = 1, mode = 0;
  std::string structure;       // kReachStructure: the building's node
};

// A new company as MW1 makes it: the starting funds plus 0..255 x 1,000, the
// commander's Owens (VISION: in place of MW1's Jenner), and the stores given.
Company new_company(Rng& rng, std::vector<StoreItem> starting_stores);

// MwGenerateMissionOffer (081B:4F6E) for offer slot `slot` (0..5), with the
// company's one mech as the lance; the objective and map are the milestone's.
Contract generate_contract(const Company& c, int32_t slot, Rng& rng);

// Accepting it (MwCreditAcceptedContractAdvanceContinuation): the advance paid.
void accept(Company& c, Contract& k);

enum class Outcome { kSuccess, kFailure, kRetreat };
struct Wreck {
  int32_t kind = 0;        // MW1 kind
  int32_t condition = 0;   // percent left; 35 or less is scrap
};
struct Settlement {
  int32_t payout_percent = 100;
  int64_t salvage = 0, fee = 0;  // C-bills paid
};
// The after-action settlement (081B:STARTUP): the deduction roll, the
// salvage share of the disabled enemies' value (every wreck counts: the
// original's overwrite is fixed, research/contracts.md), the fee on a
// success, the standing, and the kills.
Settlement settle(Company& c, Contract& k, Outcome o, const std::vector<Wreck>& wrecks, Rng& rng);

// A wreck's value in thousands at the company's tier (MwCalculateUnitConditionPrice, or scrap).
int32_t wreck_value(const Company& c, const Wreck& w);

}  // namespace mw1
