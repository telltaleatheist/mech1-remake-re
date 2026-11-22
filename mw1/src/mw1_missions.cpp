#include "mw1_missions.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "gamez/zreader/zreader.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"

namespace mw1::missions {

const CategoryDescriptions kDescriptions[kCategoryCount] = {
    {0, {}},
    // Defence (DS:8A25's first 10 entries).
    {10, {{1, 0x8a69}, {2, 0x8a79}, {3, 0x8a91}, {4, 0x8aae}, {5, 0x8acd}, {6, 0x8ae6}, {7, 0x8b02}, {8, 0x8b1e},
          {9, 0x8b3e}, {10, 0x8b59}}},
    // Rescue and retrieval.
    {4, {{11, 0x8b76}, {12, 0x8b92}, {13, 0x8ba7}, {14, 0x8bc6}}},
    // Raid.
    {15, {{20, 0x8c74}, {21, 0x8c8c}, {22, 0x8ca2}, {23, 0x8cc3}, {24, 0x8ce4}, {25, 0x8d05}, {26, 0x8d22}, {27, 0x8d40},
          {30, 0x8d62}, {31, 0x8d83}, {32, 0x8da2}, {33, 0x8dc3}, {34, 0x8de4}, {35, 0x8e05}, {36, 0x8e26}}},
    // Major campaign.
    {5, {{15, 0x8be4}, {16, 0x8c05}, {17, 0x8c26}, {18, 0x8c3c}, {19, 0x8c59}}},
};

const uint8_t kSideZeroObjective[kMissionDefinitions] = {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 2, 3, 3, 3, 3, 3, 4, 0, 3,
                                                        2, 1, 1, 2, 3, 3, 3, 3, 3, 3, 3, 0, 3, 3, 0, 3, 3, 3};

const uint8_t kDefencePlanets[kHouseCount][kTargetColumns][kPlanetChoices] = {
    {{255, 255, 255, 255, 255, 255, 255, 255}, {16, 17, 18, 20, 23, 26, 27, 15}, {13, 16, 13, 16, 13, 16, 13, 16},
     {13, 16, 13, 16, 13, 16, 13, 16}, {7, 8, 9, 10, 15, 12, 13, 14}, {255, 255, 255, 255, 255, 255, 255, 255},
     {3, 4, 5, 30, 3, 4, 5, 30}, {3, 4, 5, 30, 3, 4, 5, 30}},
    {{43, 45, 46, 47, 49, 52, 58, 59}, {255, 255, 255, 255, 255, 255, 255, 255}, {35, 41, 44, 54, 55, 56, 58, 42},
     {44, 58, 44, 58, 44, 58, 44, 58}, {39, 44, 58, 39, 44, 58, 39, 44}, {35, 57, 36, 35, 57, 36, 35, 57},
     {50, 51, 50, 51, 50, 51, 50, 51}, {255, 255, 255, 255, 255, 255, 255, 255}},
    {{66, 76, 80, 66, 76, 80, 66, 76}, {66, 74, 75, 76, 80, 85, 86, 90}, {255, 255, 255, 255, 255, 255, 255, 255},
     {65, 66, 68, 69, 76, 81, 82, 83}, {66, 76, 80, 66, 76, 80, 66, 76}, {64, 73, 86, 64, 73, 86, 64, 73},
     {255, 255, 255, 255, 255, 255, 255, 255}, {255, 255, 255, 255, 255, 255, 255, 255}},
    {{99, 100, 101, 99, 100, 101, 99, 100}, {99, 100, 101, 99, 100, 101, 99, 100}, {92, 93, 96, 103, 106, 107, 110, 111},
     {255, 255, 255, 255, 255, 255, 255, 255}, {95, 97, 102, 105, 108, 112, 113, 114}, {255, 255, 255, 255, 255, 255, 255, 255},
     {255, 255, 255, 255, 255, 255, 255, 255}, {255, 255, 255, 255, 255, 255, 255, 255}},
    {{119, 121, 126, 126, 129, 133, 134, 143}, {121, 133, 121, 133, 121, 133, 121, 133}, {133, 133, 133, 133, 133, 133, 133, 133},
     {121, 123, 131, 133, 138, 139, 140, 143}, {255, 255, 255, 255, 255, 255, 255, 255}, {255, 255, 255, 255, 255, 255, 255, 255},
     {255, 255, 255, 255, 255, 255, 255, 255}, {124, 132, 137, 124, 132, 137, 124, 132}},
};

const char* mw1_objective_name(Mw1Objective o) {
  switch (o) {
    case Mw1Objective::kEliminate: return "eliminate opposition";
    case Mw1Objective::kWithdraw: return "withdraw";
    case Mw1Objective::kOpposeWithdrawal: return "oppose withdrawal";
    case Mw1Objective::kSpecialTarget: return "special target";
    case Mw1Objective::kDefendSpecialTarget: return "defend special target";
  }
  return "";
}
const char* category_name(int32_t category) {
  static const char* const kNames[kCategoryCount] = {"", "defence", "rescue/retrieval", "raid", "major campaign"};
  return category >= 0 && category < kCategoryCount ? kNames[category] : "";
}
const char* climate_name(int32_t climate) {
  static const char* const kNames[kClimateCount] = {"desert", "tropical", "ice"};
  return climate >= 0 && climate < kClimateCount ? kNames[climate] : "";
}

Mw1Objective objective_of_code(int32_t code) {
  return code >= 1 && code <= kMissionDefinitions ? static_cast<Mw1Objective>(kSideZeroObjective[code - 1]) : Mw1Objective::kEliminate;
}
bool extended_code(int32_t code) { return code == kExtendedOffensive || code == kExtendedDefensive || code == kExtendedSiege; }

namespace {
int32_t roll_description(int32_t category, Rng& rng) {
  const CategoryDescriptions& c = kDescriptions[category];
  // MwRandomUpTo(last x 2) >> 1: the last index is DS:8A16's (count - 1).
  return c.d[rng.upto((c.count - 1) << 1) >> 1].code;
}
}  // namespace

Mission roll_mission(int32_t employer, int32_t target, Rng& rng) {
  Mission m;
  const int32_t limit = kHouseTargets[employer][target];
  m.category = limit > 0 ? (rng.upto((limit - 1) << 2) >> 2) + 1 : kDefence;
  m.code = roll_description(m.category, rng);
  // The extended battles: a raid in their place (MW1's when the company has under 2 pilots and 2 mechs).
  if (kExtendedBattlesAsRaids && extended_code(m.code)) m.category = kRaid, m.code = roll_description(kRaid, rng);
  m.objective = objective_of_code(m.code);
  // The mission planet: the target House's, else the employer's choice (a defence, or a target that isn't a House).
  if (m.category != kDefence && target < kHouseCount) {
    for (int tries = 0; tries < 1000; ++tries) {
      const int32_t i = rng.upto(kHousePlanetCount[target]) + kHousePlanetStart[target];
      if (i >= kPlanetCount) continue;  // the 146th record, left out of the map (mw1_travel.h): rolled again
      const int32_t most = kPlanets[i].max_mission;
      if (most == 0 || m.category > most) continue;
      m.planet = i;
      break;
    }
  } else {
    const uint8_t s = kDefencePlanets[employer][target][rng.upto(kPlanetChoices - 1)];
    m.planet = s == kNoPlanet || s == 0 || s > kPlanetCount ? -1 : s - 1;
  }
  m.climate = m.planet >= 0 ? kPlanets[m.planet].landscape : kDesert;
  return m;
}

std::string mission_title(int32_t code) {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  for (const CategoryDescriptions& c : kDescriptions)
    for (int32_t i = 0; i < c.count; ++i) {
      if (c.d[i].code != code) continue;
      const size_t at = static_cast<size_t>(c.d[i].ds);
      if (d.size() <= at + 2 || d[at] != code) return "";  // another exe: no title
      std::string s;
      for (size_t j = at + 2; j < d.size() && d[j] && s.size() < 64; ++j) s += static_cast<char>(d[j]);
      return s;
    }
  return "";
}

// ---- The map table ----

namespace {
std::vector<MapRow> g_rows;

std::string lower(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}
}  // namespace

bool reach_point(const std::string& node, float* x, float* z) {
  if (node.size() < 4 || node[0] != '@') return false;
  const size_t comma = node.find(',');
  if (comma == std::string::npos) return false;
  char* end = nullptr;
  const float px = std::strtof(node.c_str() + 1, &end);
  if (end != node.c_str() + comma) return false;
  const float pz = std::strtof(node.c_str() + comma + 1, &end);
  if (end != node.c_str() + node.size() || !std::isfinite(px) || !std::isfinite(pz)) return false;
  *x = px, *z = pz;
  return true;
}

std::string point_node(float x, float z) {
  char b[48];
  std::snprintf(b, sizeof b, "@%.0f,%.0f", x, z);
  return b;
}

std::vector<MapRow> parse_maps(const std::string& zrd_path, std::string* why) {
  std::vector<MapRow> rows;
  zrdr::Node* root = zrdr::load(zrd_path.c_str());
  if (!root) {
    if (why) *why = "no " + zrd_path;
    return rows;
  }
  // The file's one list (the outer parentheses) holds the rows.
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const int32_t n = top->type == zrdr::List ? zrdr::list_count(top) : 0;
  int32_t index = 0;
  for (int32_t i = 1; i + 1 < n; ++i) {
    zrdr::Node* key = &top->list[i];
    if (key->type != zrdr::String || lower(key->s) != "map") continue;
    zrdr::Node* m = &top->list[++i];
    ++index;
    MapRow r;
    const char* world = zrdr::get_string(m, "WORLD");
    const char* climate = zrdr::get_string(m, "CLIMATE");
    const char* kind = zrdr::get_string(m, "KIND");
    const char* node = zrdr::get_string(m, "NODE");
    const char* words = zrdr::get_string(m, "WORDS");
    int32_t ia = 0;
    zrdr::get_int(m, "IA", &ia);
    r.world = world ? world : "", r.ia = ia, r.node = node ? node : "", r.words = words ? words : "";
    const std::string c = climate ? lower(climate) : "", k = kind ? lower(kind) : "";
    r.climate = c == "desert" ? kDesert : c == "tropical" ? kTropical : c == "ice" ? kIce : -1;
    r.kind = k == "any" ? RowKind::kAny : k == "reach" ? RowKind::kReach : k == "attack" ? RowKind::kAttack : k == "defend" ? RowKind::kDefend
                                                                                                 : static_cast<RowKind>(-1);
    float at[2] = {};
    if (r.kind == RowKind::kReach && r.node.empty() && zrdr::get_vec2(m, "POINT", at)) r.node = point_node(at[0], at[1]);
    const bool ok = !r.world.empty() && r.ia >= 1 && r.climate >= 0 && static_cast<int32_t>(r.kind) >= 0 &&
                    (r.kind == RowKind::kAny || (!r.node.empty() && !r.words.empty()));
    if (ok) rows.push_back(r);
    else if (why) *why += "MAP " + std::to_string(index) + " doesn't read; ";
  }
  zrdr::free_node(root);
  return rows;
}

std::vector<MapPool> parse_pools(const std::string& zrd_path, std::string* why) {
  std::vector<MapPool> pools;
  zrdr::Node* root = zrdr::load(zrd_path.c_str());
  if (!root) return pools;
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const int32_t n = top->type == zrdr::List ? zrdr::list_count(top) : 0;
  auto words = [](zrdr::Node* m, const char* key) {
    std::vector<std::string> out;
    zrdr::Node* l = zrdr::find_flat(m, key, 1);
    if (!l || l->type != zrdr::List) return out;  // the value after the key
    for (int32_t i = 1; i < zrdr::list_count(l); ++i)
      if (l->list[i].type == zrdr::String) out.push_back(l->list[i].s);
    return out;
  };
  int32_t index = 0;
  for (int32_t i = 1; i + 1 < n; ++i) {
    zrdr::Node* key = &top->list[i];
    if (key->type != zrdr::String || lower(key->s) != "pool") continue;
    zrdr::Node* m = &top->list[++i];
    ++index;
    MapPool p;
    const char* climate = zrdr::get_string(m, "CLIMATE");
    const char* planet = zrdr::get_string(m, "PLANET");
    const std::string c = climate ? lower(climate) : "";
    p.climate = c == "desert" ? kDesert : c == "tropical" ? kTropical : c == "ice" ? kIce : -1;
    p.planet = planet ? planet : "";
    p.mostly = words(m, "MOSTLY"), p.sometimes = words(m, "SOMETIMES");
    if ((p.climate >= 0) != p.planet.empty() || (p.mostly.empty() && p.sometimes.empty())) {
      if (why) *why += "POOL " + std::to_string(index) + " doesn't read; ";
      continue;
    }
    pools.push_back(p);
  }
  zrdr::free_node(root);
  return pools;
}

namespace {
std::vector<MapPool> g_pools;
}  // namespace
void set_map_pools(std::vector<MapPool> pools) { g_pools = std::move(pools); }
const std::vector<MapPool>& map_pools() { return g_pools; }

const MapPool* pool_for(const std::vector<MapPool>& pools, int32_t planet, int32_t climate) {
  if (planet >= 0)
    for (const MapPool& p : pools)
      if (!p.planet.empty() && p.planet == kPlanets[planet].name) return &p;
  for (const MapPool& p : pools)
    if (p.planet.empty() && p.climate == climate) return &p;
  return nullptr;
}

int32_t choose_pool_row(const std::vector<MapRow>& rows, const MapPool& pool, RowKind kind, const std::string& last_world,
                        int32_t last_ia, Rng& rng) {
  auto from = [&](const std::vector<std::string>& worlds) {
    std::vector<int32_t> out;
    for (int32_t i = 0; i < static_cast<int32_t>(rows.size()); ++i) {
      const MapRow& r = rows[static_cast<size_t>(i)];
      if (r.kind != kind || (r.world == last_world && r.ia == last_ia)) continue;
      if (std::find(worlds.begin(), worlds.end(), r.world) != worlds.end()) out.push_back(i);
    }
    return out;
  };
  // Even per world (the lead, 2026-11-18): a world of the part that has a row of the kind first, then one of its
  // rows; a world with more structure rows is no likelier than one with fewer.
  const bool mostly = rng.upto(99) < kMostlyPercent;
  for (const std::vector<std::string>* part : {mostly ? &pool.mostly : &pool.sometimes, mostly ? &pool.sometimes : &pool.mostly}) {
    const std::vector<int32_t> c = from(*part);
    if (c.empty()) continue;
    std::vector<std::string> worlds;
    for (int32_t i : c)
      if (std::find(worlds.begin(), worlds.end(), rows[static_cast<size_t>(i)].world) == worlds.end())
        worlds.push_back(rows[static_cast<size_t>(i)].world);
    const std::string& w = worlds[static_cast<size_t>(rng.upto(static_cast<int32_t>(worlds.size()) - 1))];
    std::vector<int32_t> its;
    for (int32_t i : c)
      if (rows[static_cast<size_t>(i)].world == w) its.push_back(i);
    return its[static_cast<size_t>(rng.upto(static_cast<int32_t>(its.size()) - 1))];
  }
  return -1;
}

void set_map_table(std::vector<MapRow> rows) { g_rows = std::move(rows); }
const std::vector<MapRow>& map_table() { return g_rows; }

RowKind kind_for(Mw1Objective o) {
  switch (o) {
    case Mw1Objective::kEliminate: return RowKind::kAny;
    case Mw1Objective::kWithdraw: return RowKind::kReach;
    case Mw1Objective::kSpecialTarget: return RowKind::kAttack;
    case Mw1Objective::kOpposeWithdrawal:
    case Mw1Objective::kDefendSpecialTarget: return RowKind::kDefend;
  }
  return RowKind::kAny;
}
Objective objective_for(RowKind k) {
  switch (k) {
    case RowKind::kReach: return Objective::kReachStructure;
    case RowKind::kAttack: return Objective::kAttackStructure;
    case RowKind::kDefend: return Objective::kDefendStructure;
    case RowKind::kAny: break;
  }
  return Objective::kDestroyAll;
}

int32_t choose_row(const std::vector<MapRow>& rows, RowKind kind, int32_t climate, Rng& rng) {
  std::vector<int32_t> here, anywhere;
  for (int32_t i = 0; i < static_cast<int32_t>(rows.size()); ++i) {
    if (rows[static_cast<size_t>(i)].kind != kind) continue;
    anywhere.push_back(i);
    if (rows[static_cast<size_t>(i)].climate == climate) here.push_back(i);
  }
  const std::vector<int32_t>& from = !here.empty() ? here : anywhere;
  return from.empty() ? -1 : from[static_cast<size_t>(rng.upto(static_cast<int32_t>(from.size()) - 1))];
}

int32_t map_climate(const std::vector<MapRow>& rows, const std::string& world, int32_t ia) {
  for (const MapRow& r : rows)
    if (r.world == world && r.ia == ia) return r.climate;
  return -1;
}

void choose_mission(Contract& k, Rng& rng, const std::string& last_world, int32_t last_ia) {
  const Mission m = roll_mission(k.house, k.target, rng);
  k.category = m.category, k.code = m.code, k.mission_planet = m.planet, k.climate = m.climate;
  const std::vector<MapRow>& rows = map_table();
  k.objective = Objective::kDestroyAll, k.structure.clear(), k.structure_words.clear();
  if (rows.empty()) return;  // the milestone's map (Contract's defaults)
  const RowKind want = kind_for(m.objective);
  int32_t r = -1;
  if (const MapPool* pool = pool_for(map_pools(), m.planet, m.climate)) {
    // Item 46: the planet's pool (none of the kind there: a battle from it).
    r = choose_pool_row(rows, *pool, want, last_world, last_ia, rng);
    if (r < 0) r = choose_pool_row(rows, *pool, RowKind::kAny, last_world, last_ia, rng);
  } else {
    r = choose_row(rows, want, m.climate, rng);
    if (r < 0) r = choose_row(rows, RowKind::kAny, m.climate, rng);  // none of the kind: a battle on any map
  }
  if (r < 0) return;
  const MapRow& row = rows[static_cast<size_t>(r)];
  k.world = row.world, k.ia = row.ia, k.mode = 0;
  k.objective = objective_for(row.kind);
  if (row.kind != RowKind::kAny) k.structure = row.node, k.structure_words = row.words;
}

}  // namespace mw1::missions
