#pragma once
// MW1's missions (VISION.md, "Combat: missions" and "Combat: maps"; research/contracts.md): what a contract asks
// and where it's fought, as MW1 rolls it, and the Instant Action map that stands in for it. No SDL and no screens.
//
// MW1's chain (MwGenerateMissionOffer, 081B:4F6E), with no weights of our own:
//   the employer x the target column (houseMissionTypeLimits, DS:5CBA) gives the highest category on offer;
//   the category is rolled up to it (1 defence, 2 rescue/retrieval, 3 raid, 4 major campaign);
//   one of the category's descriptions (DS:8A16, 8A1B, 8A25) is picked, and its record's first byte is the
//   scenario code (records DS:8A69..8E46: the code, a byte, the title the offer shows);
//   BTECH fights code - 1 as its mission definition, whose objective is combatMissionSideZeroRules (BTECH DS:092C);
//   the mission planet is the target House's (a planet of its range, DS:655C/6566, that takes the category,
//   byte 3), or for a defence or a target that isn't a House the employer's own choice (DS:8E9A); the battle's
//   landscape is that planet's (byte 2: (code - 1) % 3).
// The objective then picks a row of MW1's map table (data/mw1maps.zrd): a map, and the structure to reach, attack
// or defend there, in the battle's climate when one has it.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::missions {

// MW1's five objectives (BTECH, combat_world.h's OBJECTIVE_*).
enum class Mw1Objective : int32_t { kEliminate, kWithdraw, kOpposeWithdrawal, kSpecialTarget, kDefendSpecialTarget };
const char* mw1_objective_name(Mw1Objective o);  // "eliminate opposition", ...

// The categories (MISSION_CATEGORY_*).
inline constexpr int32_t kDefence = 1, kRescue = 2, kRaid = 3, kMajorCampaign = 4, kCategoryCount = 5;
const char* category_name(int32_t category);  // "defence", ... ("" for none)

// A description: its scenario code and its record's DS offset (where the title is, read from the player's exe).
struct Description {
  int32_t code;
  int32_t ds;
};
// Per category (1..4), the descriptions in their list's order (DS:8A25's entries; DS:8A16 the last index).
inline constexpr int32_t kMaxDescriptions = 15;
struct CategoryDescriptions {
  int32_t count;
  Description d[kMaxDescriptions];
};
extern const CategoryDescriptions kDescriptions[kCategoryCount];  // [0] none

// BTECH's combatMissionSideZeroRules (BTECH DS:092C, 0x25 words): the objective of mission definition code - 1.
inline constexpr int32_t kMissionDefinitions = 0x25;
extern const uint8_t kSideZeroObjective[kMissionDefinitions];
Mw1Objective objective_of_code(int32_t code);

// The extended battles (CAMPAIGN_SCENARIO_EXTENDED_*): three stages in MW1, with 2 pilots and 2 mechs, else MW1
// rolls a raid in their place. Until the story milestone (mw1 lead 2026-11-17, provisional) they always fall back
// to a raid; codes 17 and 19 play as one ordinary battle.
inline constexpr int32_t kExtendedOffensive = 15, kExtendedDefensive = 16, kExtendedSiege = 18;
inline constexpr bool kExtendedBattlesAsRaids = true;
bool extended_code(int32_t code);

// The mission planet's tables: each House's planets (indices into the travel map's records, rand(count) + start,
// DS:655C and DS:6566) and each employer's choices for a defence (DS:8E9A: per target column, eight selectors,
// a planet index + 1, 0xFF none).
inline constexpr int32_t kHousePlanetStart[kHouseCount] = {0, 30, 60, 90, 115};
inline constexpr int32_t kHousePlanetCount[kHouseCount] = {30, 30, 30, 25, 30};
inline constexpr int32_t kPlanetChoices = 8;
extern const uint8_t kDefencePlanets[kHouseCount][kTargetColumns][kPlanetChoices];
inline constexpr uint8_t kNoPlanet = 0xFF;

// The battle's climates (the travel map's labels, DS:5CA6..5CB6).
inline constexpr int32_t kDesert = 0, kTropical = 1, kIce = 2, kClimateCount = 3;
const char* climate_name(int32_t climate);  // "desert", "tropical", "ice"

// What a contract's mission is, as MW1 rolls it.
struct Mission {
  int32_t category = kRaid;
  int32_t code = 0;          // the scenario code (1..36)
  Mw1Objective objective = Mw1Objective::kEliminate;
  int32_t planet = -1;       // the mission planet, an index into the travel map's records (-1 none)
  int32_t climate = kDesert;  // its landscape
};
// The rolls, after the target column (as MW1 makes them): the category up to houseMissionTypeLimits'
// ((rand((limit - 1) x 4) >> 2) + 1), a description, the extended battles' fallback, then the planet.
Mission roll_mission(int32_t employer, int32_t target, Rng& rng);

// The title MW1's offer shows for a scenario code ("DEFENSE OF A FUEL DUMP"), from the player's MW_MAIN.EXE
// (mw1_pilots.h's mw_main_data_segment); "" without it.
std::string mission_title(int32_t code);

// ---- The map table (data/mw1maps.zrd) ----
enum class RowKind : int32_t { kAny, kReach, kAttack, kDefend };
struct MapRow {
  std::string world;      // "c2", "mw3c2"
  int32_t ia = 1;         // its readeria<n>.zbd
  int32_t climate = kDesert;
  RowKind kind = RowKind::kAny;  // kAny: a battle to destroy all (no structure)
  std::string node;       // the structure: a world node (reach) or a targets.zrd entity (attack, defend)
  std::string words;      // as the offer says it: "the portal gate"
};
// FIXES 136: a reach at a point on open ground, not a node (MAP ... KIND ("reach") POINT (x z) WORDS (...)). The row's
// node is then "@x,z" (no world node's name starts with '@'), so the contract and the save carry it as they carry a
// structure's node; the reach is the engine's reach_area at that point (node 0), its nav point there.
bool reach_point(const std::string& node, float* x, float* z);
std::string point_node(float x, float z);
inline constexpr const char* kMapsFile = "mw1maps.zrd";  // MW1's data folder
// Read from a zrd's text (MAP (WORLD ("c2") IA (2) CLIMATE ("desert") KIND ("attack") NODE ("x") WORDS ("y")) ...);
// rows that don't read are left out and said in `why`.
std::vector<MapRow> parse_maps(const std::string& zrd_path, std::string* why = nullptr);
// The table the contracts choose from (set at the start from mw1data/mw1maps.zrd; empty: the milestone's map).
void set_map_table(std::vector<MapRow> rows);
const std::vector<MapRow>& map_table();

// Ours for MW1's: eliminate a battle to destroy all, withdraw a reach, special target an attack, defend special
// target and oppose withdrawal a defence (VISION).
RowKind kind_for(Mw1Objective o);
Objective objective_for(RowKind k);
// A row for the mission: its kind in the battle's climate, else its kind anywhere; for a destroy-all any row's map
// (the climate's first); -1 when the table has none of the kind (the contract is then a destroy-all).
int32_t choose_row(const std::vector<MapRow>& rows, RowKind kind, int32_t climate, Rng& rng);

// FIXES.md item 46: where a world's battles are drawn from (mw1maps.zrd's POOL rows): about kMostlyPercent of the
// time from MOSTLY's maps, else from SOMETIMES'; never any other map. A pool is a climate's, or a planet's (by its
// travel-map name: Ander's Moon's), which comes first.
inline constexpr int32_t kMostlyPercent = 75;
struct MapPool {
  int32_t climate = -1;           // the climate's pool, or -1
  std::string planet;             // the planet's ("ANDER'S MOON"), or ""
  std::vector<std::string> mostly, sometimes;  // worlds (folders: "c1", "mw3c2")
};
std::vector<MapPool> parse_pools(const std::string& zrd_path, std::string* why = nullptr);
void set_map_pools(std::vector<MapPool> pools);
const std::vector<MapPool>& map_pools();
// The pool for a mission on `planet` (an index into the travel map's records, -1 none) in `climate`; null: none.
const MapPool* pool_for(const std::vector<MapPool>& pools, int32_t planet, int32_t climate);
// A row of `kind` from the pool: MOSTLY's rows kMostlyPercent of the time, else SOMETIMES'; the other part when the
// drawn one has none of the kind; never the map (world, ia) last drawn; -1 when neither part has one.
int32_t choose_pool_row(const std::vector<MapRow>& rows, const MapPool& pool, RowKind kind, const std::string& last_world,
                        int32_t last_ia, Rng& rng);

// FIXES.md item 44 (c): the climate of the map `world` Instant Action `ia` as the table has it (its first row on that
// map); -1 when no row is on it.
int32_t map_climate(const std::vector<MapRow>& rows, const std::string& world, int32_t ia);

// Sets the contract's mission and map: the rolls, the row, the contract's objective, world, archive and structure.
void choose_mission(Contract& k, Rng& rng, const std::string& last_world = "", int32_t last_ia = 0);

}  // namespace mw1::missions

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace missions;
}  // namespace mw1
