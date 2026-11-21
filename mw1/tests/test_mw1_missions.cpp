// MW1's missions (src/mw1_missions.h): the category, description, objective and mission planet tables against the
// player's MW_MAIN.EXE and BTECH.EXE; the rolls MW1's chain makes; the map table (data/mw1maps.zrd) read; a row
// chosen by the battle's climate; a contract given its mission and map.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "gamez/zreader/zreader.h"
#include "mw1_campaign.h"
#include "mw1_missions.h"
#include "mw1_placement.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"
#include "gamez/zutil/zutl_path.h"

namespace fs = std::filesystem;

namespace {
std::vector<uint8_t> exe(const char* name) {
  std::error_code ec;
  fs::path path;
  for (const auto& e : fs::directory_iterator(MW1_DOS_GAME_DIR, ec)) {
    std::string n = e.path().filename().string();
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (n == name) path = e.path();
  }
  std::ifstream f(path, std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return b.size() > 0x1c && b[0] == 'M' && b[1] == 'Z' ? b : std::vector<uint8_t>{};
}
// MW_MAIN's data segment 0x1090; BTECH's 304E, at paragraph 0x2696 after its header.
std::vector<uint8_t> segment(const std::vector<uint8_t>& b, size_t paragraph) {
  if (b.empty()) return {};
  const size_t at = static_cast<size_t>(b[8] | b[9] << 8) * 16 + paragraph * 16;
  return at < b.size() ? std::vector<uint8_t>(b.begin() + static_cast<long>(at), b.end()) : std::vector<uint8_t>{};
}
int32_t word(const std::vector<uint8_t>& d, size_t at) { return d[at] | d[at + 1] << 8; }
}  // namespace

TEST(mw1_missions_tables_are_the_exes) {
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> m = segment(exe("MW_MAIN.EXE"), 0x1090 - 0x0800);
  const std::vector<uint8_t> bt = segment(exe("BTECH.EXE"), 0x2696);
  if (m.size() < 0x9000 || bt.size() < 0x1000) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  // houseMissionTypeLimits (DS:5CBA).
  for (int h = 0; h < mw1::kHouseCount; ++h)
    for (int t = 0; t < mw1::kTargetColumns; ++t) EXPECT(m[0x5CBA + static_cast<size_t>(h * 8 + t)] == mw1::kHouseTargets[h][t]);
  // The descriptions: DS:8A16's last index, DS:8A1B's lists, each entry's record and its first byte, the code.
  for (int c = 1; c < mw1::kCategoryCount; ++c) {
    EXPECT(m[0x8A16 + static_cast<size_t>(c)] + 1 == mw1::kDescriptions[c].count);
    const size_t list = static_cast<size_t>(word(m, 0x8A1B + 2 * static_cast<size_t>(c)));
    for (int i = 0; i < mw1::kDescriptions[c].count; ++i) {
      const mw1::Description& d = mw1::kDescriptions[c].d[i];
      EXPECT(word(m, list + 2 * static_cast<size_t>(i)) == d.ds && m[static_cast<size_t>(d.ds)] == d.code);
    }
  }
  // BTECH's combatMissionSideZeroRules (DS:092C), the House planet ranges (DS:655C, 6566), the defence choices
  // (DS:8E90's offsets, DS:8E9A's selectors).
  for (int i = 0; i < mw1::kMissionDefinitions; ++i) EXPECT(word(bt, 0x092C + 2 * static_cast<size_t>(i)) == mw1::kSideZeroObjective[i]);
  for (int h = 0; h < mw1::kHouseCount; ++h) {
    EXPECT(word(m, 0x655C + 2 * static_cast<size_t>(h)) == mw1::kHousePlanetStart[h]);
    EXPECT(word(m, 0x6566 + 2 * static_cast<size_t>(h)) == mw1::kHousePlanetCount[h]);
    const size_t at = static_cast<size_t>(word(m, 0x8E90 + 2 * static_cast<size_t>(h)));
    EXPECT(at == 0x8E9A + 64 * static_cast<size_t>(h));
    for (int t = 0; t < mw1::kTargetColumns; ++t)
      for (int k = 0; k < mw1::kPlanetChoices; ++k) EXPECT(m[at + static_cast<size_t>(t * 8 + k)] == mw1::kDefencePlanets[h][t][k]);
  }
  // The climates' labels (DS:5CA6, 5CAD, 5CB6).
  EXPECT(std::string(reinterpret_cast<const char*>(&m[0x5CA6])) == "DESERT" && std::string(reinterpret_cast<const char*>(&m[0x5CAD])) == "TROPICAL" &&
         std::string(reinterpret_cast<const char*>(&m[0x5CB6])) == "ICE");
  // Each description's objective, as MW1 pairs them (for research/contracts.md).
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  for (int c = 1; c < mw1::kCategoryCount; ++c)
    for (int i = 0; i < mw1::kDescriptions[c].count; ++i) {
      const int32_t code = mw1::kDescriptions[c].d[i].code;
      std::printf("  %-16s %2d %-22s %s\n", mw1::category_name(c), code, mw1::mw1_objective_name(mw1::objective_of_code(code)),
                  mw1::mission_title(code).c_str());
    }
  EXPECT(mw1::mission_title(5) == "DEFENSE OF A FUEL DUMP" && mw1::mission_title(99).empty());
  mw1::set_mw1_game_dir("");
}

TEST(mw1_missions_rolls_follow_mw1s_chain) {
  // Every employer and eligible target, many rolls: the category within houseMissionTypeLimits', a code of the
  // category's, no extended battle (they fall back to a raid), the objective the code's, a planet of the target
  // House's that takes the category (else the employer's choice), its landscape.
  mw1::Rng rng(11);
  std::map<int32_t, int32_t> objectives;
  for (int32_t h = 0; h < mw1::kHouseCount; ++h)
    for (int32_t t = 0; t < mw1::kTargetColumns; ++t) {
      if (!mw1::kHouseTargets[h][t]) continue;
      for (int i = 0; i < 200; ++i) {
        const mw1::Mission m = mw1::roll_mission(h, t, rng);
        EXPECT(m.category >= 1 && (m.category <= mw1::kHouseTargets[h][t] || m.category == mw1::kRaid));
        bool listed = false;
        for (int k = 0; k < mw1::kDescriptions[m.category].count; ++k) listed = listed || mw1::kDescriptions[m.category].d[k].code == m.code;
        EXPECT(listed && !mw1::extended_code(m.code) && m.objective == mw1::objective_of_code(m.code));
        if (m.category != mw1::kDefence && t < mw1::kHouseCount) {
          EXPECT(m.planet >= mw1::kHousePlanetStart[t] && m.planet <= mw1::kHousePlanetStart[t] + mw1::kHousePlanetCount[t]);
          EXPECT(m.planet >= 0 && mw1::kPlanets[m.planet].max_mission >= m.category);
        }
        if (m.planet >= 0) EXPECT(m.climate == mw1::kPlanets[m.planet].landscape);
        ++objectives[static_cast<int32_t>(m.objective)];
      }
    }
  for (const auto& [o, n] : objectives) std::printf("  %-22s %d\n", mw1::mw1_objective_name(static_cast<mw1::Mw1Objective>(o)), n);
  EXPECT(objectives.size() == 5);  // all five of MW1's turn up
  // Kurita against Steiner (limit 4): every category; Davion against Kurita (limit 1... column 7, a non-House: 1).
  EXPECT(mw1::roll_mission(mw1::kDavion, 7, rng).category == mw1::kDefence);
}

TEST(mw1_missions_the_map_table_reads_and_a_row_is_chosen) {
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW1_DATA_DIR);
  std::string why;
  const std::vector<mw1::MapRow> rows = mw1::parse_maps(mw1::kMapsFile, &why);
  std::printf("  %zu rows%s%s\n", rows.size(), why.empty() ? "" : ": ", why.c_str());
  EXPECT(rows.size() >= 20 && why.empty());
  std::map<mw1::RowKind, int> kinds;
  for (const mw1::MapRow& r : rows) ++kinds[r.kind];
  EXPECT(kinds[mw1::RowKind::kAny] >= 5 && kinds[mw1::RowKind::kReach] >= 1 && kinds[mw1::RowKind::kAttack] >= 1 &&
         kinds[mw1::RowKind::kDefend] >= 1);
  // A row of the kind in the climate; none in it (ice), the kind anywhere; none of the kind, -1.
  mw1::Rng rng(3);
  for (int i = 0; i < 50; ++i) {
    const int32_t a = mw1::choose_row(rows, mw1::RowKind::kAttack, mw1::kTropical, rng);
    EXPECT(a >= 0 && rows[static_cast<size_t>(a)].kind == mw1::RowKind::kAttack && rows[static_cast<size_t>(a)].climate == mw1::kTropical);
    const int32_t d = mw1::choose_row(rows, mw1::RowKind::kDefend, mw1::kIce, rng);
    EXPECT(d >= 0 && rows[static_cast<size_t>(d)].kind == mw1::RowKind::kDefend);
  }
  EXPECT(mw1::choose_row({}, mw1::RowKind::kReach, mw1::kDesert, rng) == -1);
  // FIXES.md item 46: the pools read (the three climates' and Ander's Moon's), each world in them a table world.
  const std::vector<mw1::MapPool> pools = mw1::parse_pools(mw1::kMapsFile, &why);
  std::printf("  %zu pools%s%s\n", pools.size(), why.empty() ? "" : ": ", why.c_str());
  EXPECT(pools.size() == 4 && why.empty());
  // FIXES.md item 44 (c): a map's climate, as the offer says it; none for a map not in the table.
  EXPECT(mw1::map_climate(rows, "c2", 1) == mw1::kDesert && mw1::map_climate(rows, "mw3c2", 1) == mw1::kTropical);
  EXPECT(mw1::map_climate(rows, "c9", 1) == -1 && mw1::map_climate(rows, "c2", 7) == -1);
  // A contract given its mission and the map for it.
  mw1::set_map_table(rows);
  mw1::Company c;
  c.funds = 1000000;
  int structures = 0;
  for (int i = 0; i < 100; ++i) {
    const mw1::Contract k = mw1::generate_contract(c, 0, rng);
    EXPECT(k.code > 0 && !k.world.empty() && k.ia >= 1);
    const mw1::RowKind want = mw1::kind_for(mw1::objective_of_code(k.code));
    EXPECT(k.objective == mw1::objective_for(want));
    if (k.objective != mw1::Objective::kDestroyAll) {
      EXPECT(!k.structure.empty() && !k.structure_words.empty());
      ++structures;
    }
  }
  EXPECT(structures > 0);
  // No table: the milestone's map (c2's first Instant Action map), destroy all.
  mw1::set_map_table({});
  const mw1::Contract k = mw1::generate_contract(c, 0, rng);
  EXPECT(k.world == "c2" && k.ia == 1 && k.objective == mw1::Objective::kDestroyAll && k.code > 0);
  zrdr::close_archives(true);
}

// FIXES.md item 46: 10,000 draws from each pool (the four kinds mixed, each falling back to a battle as the
// contract does), with the shares said: MOSTLY's about kMostlyPercent (all SOMETIMES' while MOSTLY has no map, the
// ice's until item 44), never a world outside the pool, never the same map twice running. Then 10,000 contracts
// as the game makes them (the last map carried as the company keeps it): each on its planet's pool.
TEST(mw1_missions_maps_drawn_by_the_planets_pool) {
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW1_DATA_DIR);
  const std::vector<mw1::MapRow> rows = mw1::parse_maps(mw1::kMapsFile);
  const std::vector<mw1::MapPool> pools = mw1::parse_pools(mw1::kMapsFile);
  zrdr::close_archives(true);
  EXPECT(pools.size() == 4);
  auto has = [](const std::vector<std::string>& l, const std::string& w) { return std::find(l.begin(), l.end(), w) != l.end(); };
  mw1::Rng rng(46);
  for (const mw1::MapPool& p : pools) {
    std::map<std::string, int> worlds, by_kind[4];
    int mostly = 0, sometimes = 0, outside = 0, repeats = 0, none = 0;
    std::string last_world;
    int32_t last_ia = 0;
    bool mostly_has_maps = false;
    for (const mw1::MapRow& r : rows) mostly_has_maps = mostly_has_maps || has(p.mostly, r.world);
    for (int i = 0; i < 10000; ++i) {
      const auto kind = static_cast<mw1::RowKind>(rng.upto(3));
      int32_t at = mw1::choose_pool_row(rows, p, kind, last_world, last_ia, rng);
      if (at < 0) at = mw1::choose_pool_row(rows, p, mw1::RowKind::kAny, last_world, last_ia, rng);
      if (at < 0) {
        ++none;
        continue;
      }
      const mw1::MapRow& r = rows[static_cast<size_t>(at)];
      ++worlds[r.world];
      ++by_kind[static_cast<int>(r.kind)][r.world];
      if (has(p.mostly, r.world)) ++mostly;
      else if (has(p.sometimes, r.world)) ++sometimes;
      else ++outside;
      repeats += r.world == last_world && r.ia == last_ia;
      last_world = r.world, last_ia = r.ia;
    }
    std::string said;
    for (const auto& [w, n] : worlds) said += " " + w + " " + std::to_string(n / 100) + "%";
    std::printf("  %-12s: mostly %d.%d%%, sometimes %d.%d%%, outside %d, repeats %d, none %d;%s\n",
                p.planet.empty() ? mw1::climate_name(p.climate) : p.planet.c_str(), mostly / 100, mostly / 10 % 10, sometimes / 100,
                sometimes / 10 % 10, outside, repeats, none, said.c_str());
    EXPECT(outside == 0 && repeats == 0 && none == 0);
    if (mostly_has_maps && !p.sometimes.empty()) EXPECT(mostly >= 7000 && mostly <= 8000);
    if (!mostly_has_maps) EXPECT(mostly == 0);
    // Even per world within a part, kind by kind: among the part's worlds with a row of the kind, each drawn as often
    // (a world without one, c1 for a reach, isn't drawn for it).
    for (int kd = 0; kd < 4; ++kd)
      for (const std::vector<std::string>* part : {&p.mostly, &p.sometimes}) {
        std::vector<int> n;
        for (const std::string& w : *part)
          if (std::any_of(rows.begin(), rows.end(), [&](const mw1::MapRow& r) { return r.world == w && static_cast<int>(r.kind) == kd; }))
            n.push_back(by_kind[kd][w]);
        if (n.size() < 2) continue;
        const int lo = *std::min_element(n.begin(), n.end()), hi = *std::max_element(n.begin(), n.end());
        if (hi > 200) EXPECT(lo * 10 >= hi * 8);  // within 20% of each other
        if (hi > 200 && lo * 10 < hi * 8) std::printf("    uneven: kind %d, %d to %d\n", kd, lo, hi);
      }
  }
  // As the game draws them: the planet rolled, its pool, the last map carried.
  mw1::set_map_table(rows);
  mw1::set_map_pools(pools);
  mw1::Company c;
  c.funds = 1000000;
  int off_pool = 0, repeats = 0, no_pool = 0;
  std::map<int32_t, int> by_climate;
  for (int i = 0; i < 10000; ++i) {
    const mw1::Contract k = mw1::generate_contract(c, 0, rng);
    const mw1::MapPool* p = mw1::pool_for(pools, k.mission_planet, k.climate);
    if (!p) ++no_pool;
    else if (!has(p->mostly, k.world) && !has(p->sometimes, k.world)) ++off_pool;
    repeats += k.world == c.last_map_world && k.ia == c.last_map_ia;
    ++by_climate[k.climate];
    c.last_map_world = k.world, c.last_map_ia = k.ia;
  }
  std::printf("  10000 contracts: %d desert, %d tropical, %d ice; off their pool %d, repeats %d, no pool %d\n", by_climate[mw1::kDesert],
              by_climate[mw1::kTropical], by_climate[mw1::kIce], off_pool, repeats, no_pool);
  EXPECT(off_pool == 0 && repeats == 0 && no_pool == 0);
  mw1::set_map_table({});
  mw1::set_map_pools({});
}

// FIXES 134: the placement measures (mw1_placement.h) on a triangle worked by hand. A at the origin, the enemies'
// places at (900, 0) and (1100, 0) (their centre B at (1000, 0), spread 100), the structure C at (0, 1000): a right
// angle at A, C level with A (along 0), B's closest approach to A->C is A itself (1000), the race 1000 / 1414.
// The ground: flat at 0 but for a 30 m ridge across x = 500, which the line from B to A->C's midpoint (0, 500) crosses.
TEST(mw1_placement_measures_a_triangle) {
  using namespace mw1::placement;
  const Ground ground = [](float x, float) { return std::fabs(x - 500.0f) < 20.0f ? 30.0f : 0.0f; };
  const Measures m = measure({0, 0}, {0, 1000}, {{900, 0}, {1100, 0}}, ground);
  auto near = [](float a, float b) { return std::fabs(a - b) < 0.5f; };
  std::printf("  AC %.0f CB %.0f AB %.0f along %.2f angle %.0f B-to-AC %.0f race %.3f ridge %.0f nearest C %.0f A %.0f AC %.0f\n",
              m.start_target, m.target_enemies, m.start_enemies, m.along, m.angle_at_start, m.centre_to_line, m.race, m.ridge,
              m.nearest_to_target, m.nearest_to_start, m.nearest_to_line);
  EXPECT(m.enemies == 2 && near(m.enemy_centre.x, 1000) && near(m.enemy_centre.z, 0) && near(m.spread, 100));
  EXPECT(near(m.start_target, 1000) && near(m.start_enemies, 1000) && near(m.target_enemies, 1414.2f));
  EXPECT(std::fabs(m.along) < 0.001f && near(m.angle_at_start, 90) && near(m.centre_to_line, 1000));
  EXPECT(std::fabs(m.race - 1000.0f / 1414.2f) < 0.001f && near(m.ridge, 30));
  EXPECT(near(m.nearest_to_target, 1345.4f) && near(m.nearest_to_start, 900) && near(m.nearest_to_line, 900));
  // Straight at the enemies: along 1, angle 0, B on the line; no ground, no ridge; no enemies, only |AC|.
  const Measures at = measure({0, 0}, {1000, 0}, {{900, 0}, {1100, 0}});
  EXPECT(near(at.along, 1) && near(at.angle_at_start, 0) && near(at.centre_to_line, 0) && at.ridge == 0);
  const Measures none = measure({0, 0}, {300, 400}, {});
  EXPECT(none.enemies == 0 && near(none.start_target, 500) && none.target_enemies < 0);
}

// FIXES 136: the reach row's route round the enemy (mw1_placement.h): the open field's straight way, a guarded goal,
// a wall of enemies to go round, the start's own disc let through, and the area bounding it.
TEST(mw1_placement_routes_round_the_enemy) {
  using namespace mw1::placement;
  const Route open = route({0, 0}, {1000, 0}, {}, {});
  EXPECT(open.found && open.length > 940 && open.length < 980 && open.detour < 1.03f);
  // The goal guarded: an enemy 50 m from it leaves no cell within the reach's 40 m that is 200 m clear.
  const Route guarded = route({0, 0}, {1000, 0}, {{1050, 0}}, {});
  EXPECT(!guarded.found && guarded.goal_clear == 50);
  // An enemy on the way: round it, every cell 200 m clear of it, a little longer.
  const Route round = route({0, 0}, {1000, 0}, {{500, 0}}, {});
  EXPECT(round.found && round.path_clear >= 195 && round.detour > 1.02f && round.detour < 1.3f);
  // A wall of enemies across the way, open only far to the north: found but more than twice as long.
  std::vector<Point> wall;
  for (float z = -1500; z <= 1500; z += 100) wall.push_back({500, z});
  const Route walled = route({0, 0}, {1000, 0}, wall, {});
  EXPECT(walled.found && walled.detour > 2.0f);
  // The area closes the gap round the wall: none.
  const Route boxed = route({0, 0}, {1000, 0}, wall, [](float, float z) { return std::fabs(z) < 1400; });
  EXPECT(!boxed.found);
  // An enemy 150 m from the start: its disc is the start's own, let through; start_clear says how near it is.
  const Route near_start = route({0, 0}, {1000, 0}, {{-150, 0}}, {});
  EXPECT(near_start.found && near_start.start_clear == 150);
  // A start outside the area: none.
  EXPECT(!route({0, 0}, {1000, 0}, {}, [](float x, float) { return x > 10; }).found);
}
