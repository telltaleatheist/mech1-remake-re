// MW1's missions (src/mw1_missions.h): the category, description, objective and mission planet tables against the
// player's MW_MAIN.EXE and BTECH.EXE; the rolls MW1's chain makes; the map table (data/mw1maps.zrd) read; a row
// chosen by the battle's climate; a contract given its mission and map.
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
