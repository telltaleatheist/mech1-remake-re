// MW1's campaign rules for the first milestone (src/mw1_campaign.h): the
// tables against the player's MW_MAIN.EXE, the PM stand-ins, a new company,
// contracts as research/contracts.md works them, accepting and the settlement.
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_repairs.h"
#include "mw1_screen.h"
#include "mw1_results.h"
#include "mw1_loading.h"

namespace fs = std::filesystem;

namespace {
// MW_MAIN.EXE's data segment 0x1090 (loaded at 0x0800: its header's paragraphs, then 0x890 paragraphs on), as
// Elthial's port reads it (MwHostLoadCampaignInitialData). Empty when the player's MW1 isn't here.
std::vector<uint8_t> mw_main_data() {
  std::error_code ec;
  fs::path exe;
  for (const auto& e : fs::directory_iterator(MW1_DOS_GAME_DIR, ec)) {
    std::string n = e.path().filename().string();
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (n == "MW_MAIN.EXE") exe = e.path();
  }
  std::ifstream f(exe, std::ios::binary);
  const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (b.size() < 0x1c || b[0] != 'M' || b[1] != 'Z') return {};
  const size_t at = static_cast<size_t>(b[8] | b[9] << 8) * 16 + (0x1090 - 0x0800) * 16;
  return at < b.size() ? std::vector<uint8_t>(b.begin() + static_cast<long>(at), b.end()) : std::vector<uint8_t>{};
}
int32_t word(const std::vector<uint8_t>& d, size_t at) { return static_cast<int16_t>(d[at] | d[at + 1] << 8); }
}  // namespace

TEST(mw1_campaign_tables_are_mw_main_exes) {
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> d = mw_main_data();
  if (d.size() < 0x5000) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  EXPECT(static_cast<int32_t>(d[0x491] | d[0x492] << 8 | d[0x493] << 16 | d[0x494] << 24) == mw1::kStartingFunds);
  for (int i = 0; i < 4; ++i) EXPECT(d[0x3057 + i] == mw1::kTierMultipliers[i]);
  for (int h = 0; h < mw1::kHouseCount; ++h)
    for (int t = 0; t < mw1::kTargetColumns; ++t) EXPECT(d[0x5CBA + h * mw1::kTargetColumns + t] == mw1::kHouseTargets[h][t]);
  for (int k = 0; k < mw1::kKindCount; ++k) {
    EXPECT(word(d, 0x122B + 2 * k) == mw1::kMechKinds[k].tons);
    EXPECT(d[0x4B55 + k] == mw1::kMechKinds[k].weight);
    EXPECT(word(d, 0x305F + 2 * k) == mw1::kMechKinds[k].base_price);
  }
  for (int h = 0; h < mw1::kHouseCount; ++h) {
    const mw1::HouseRules& r = mw1::kHouses[h];
    EXPECT(word(d, 0x4B41 + 2 * h) == r.payment_bias && word(d, 0x4B4B + 2 * h) == r.payment_adjust);
    EXPECT(word(d, 0x4B5F + 2 * h) == r.salvage_base && word(d, 0x4B69 + 2 * h) == r.advance_base);
    EXPECT(word(d, 0x1241 + 2 * h) == r.deduction_roll && word(d, 0x124B + 2 * h) == r.reduced_payout);
  }
}

// Each MW1 kind's stand-in is the Pirate's Moon Inner Sphere mech nearest its tonnage (research/contracts.md).
TEST(mw1_campaign_stand_ins_are_pms_nearest) {
  const char* const want[mw1::kKindCount] = {"firefly", "firefly", "owens", "strider", "bushwacker",
                                             "bushwacker", "champion", "avatar", "orion", "awesome"};
  for (int k = 0; k < mw1::kKindCount; ++k) {
    const mw1::MechKind& m = mw1::kMechKinds[k];
    EXPECT(std::strcmp(mech3::mech_name(m.pm_mech), want[k]) == 0);
    std::printf("  %-13s %2d t -> %-10s %3.0f t\n", m.name, m.tons, mech3::mech_name(m.pm_mech), mech3::max_mass(m.pm_mech));
  }
  EXPECT(mw1::lance_weight_of_tons(35) == 1 && mw1::lance_weight_of_tons(45) == 2 && mw1::lance_weight_of_tons(55) == 2 &&
         mw1::lance_weight_of_tons(60) == 3);
}

TEST(mw1_campaign_new_company) {
  for (uint32_t seed : {1u, 2u, 3u}) {
    mw1::Rng rng(seed);
    const mw1::Company c = mw1::new_company(rng);
    EXPECT(c.funds >= 1000000 && c.funds <= 1255000 && c.funds % 1000 == 0);
    EXPECT(c.mech == mw1::kPmOwens && c.kills == 0);
    // PM's starting store less its mechs: the guns, lasers, sinks and four ammunitions, and no mech.
    EXPECT(c.stores && c.stores->count() > 0 && !c.stores->full);
    for (int32_t r = 0; r < mech3::kMechCount; ++r) EXPECT(c.stores->has_mech(r) == nullptr);
  }
  mw1::Rng a(7), b(7);  // a seed gives the same company
  EXPECT(mw1::new_company(a).funds == mw1::new_company(b).funds);
}

// research/contracts.md's examples, an Owens (35 t: a light, 1) as the lance.
TEST(mw1_campaign_contracts_as_mw1_sizes_them) {
  mw1::Rng rng(1);
  mw1::Company c = mw1::new_company(rng);
  c.planet_house = mw1::kDavion;
  auto classes = [](const mw1::Contract& k) {
    int l = 0, m = 0, h = 0;
    for (int32_t e : k.enemies) (mw1::kMechKinds[e].weight == 1 ? l : mw1::kMechKinds[e].weight == 2 ? m : h)++;
    return std::to_string(h) + "h" + std::to_string(m) + "m" + std::to_string(l) + "l";
  };
  // The start, no kills: budget 10, one light; the x1.5 offer one medium, the x0.5 one light.
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  EXPECT(k.budget == 10 && classes(k) == "0h0m1l");
  EXPECT(classes(mw1::generate_contract(c, 1, rng)) == "0h1m0l");
  EXPECT(classes(mw1::generate_contract(c, 2, rng)) == "0h0m1l");
  // Davion (no payment bias, no adjustment): (10/2 + 1) x 100 = 600 thousand, the opening a quarter, 150;
  // salvage 20 and advance 25 percent, the openings a quarter (5, 6).
  EXPECT(k.payment_ceiling == 600 && k.payment == 150 && k.salvage_percent == 5 && k.advance_percent == 6);
  // 36 kills (band 3): budget 40, a heavy and a medium; the x1.5 offer (60) two heavies and a light.
  c.kills = 36;
  EXPECT(classes(mw1::generate_contract(c, 0, rng)) == "1h1m0l");
  EXPECT(classes(mw1::generate_contract(c, 1, rng)) == "2h0m1l");
  // The standing moves the terms: +2 favor is +8 percent, capped at 10.
  c.favor[mw1::kDavion] = 2;
  k = mw1::generate_contract(c, 0, rng);
  EXPECT(k.salvage_percent == (20 + 8) / 4 && k.advance_percent == (25 + 8) / 4);
  // Every House's payment stays in MW1's clamp, rounded to 10 thousand.
  for (int h = 0; h < mw1::kHouseCount; ++h) {
    c.planet_house = h;
    for (int i = 0; i < 50; ++i) {
      k = mw1::generate_contract(c, i % 6, rng);
      EXPECT(k.payment_ceiling >= 100 && k.payment_ceiling <= 9990 && k.payment_ceiling % 10 == 0);
      EXPECT(k.payment >= 100 && k.payment <= k.payment_ceiling && !k.enemies.empty() && k.enemies.size() <= 4);
    }
  }
}

TEST(mw1_campaign_accept_and_settle) {
  mw1::Rng rng(3);
  mw1::Company c = mw1::new_company(rng);
  c.planet_house = mw1::kDavion, c.tier = 0;
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  const int64_t start = c.funds;
  mw1::accept(c, k);  // 6% of 150 thousand: 9 thousand now, 141 left
  EXPECT(c.funds == start + 9000 && k.remaining == 141 && k.accepted);
  // A wreck pays its PM chassis's sale price there less its repairs (mw1_repairs.h). An undamaged Jenner (the Owens)
  // at tier 0: (10-1) x 100 x (95 x 35 - 245) = 2,772 thousand. Half its repairs at an 18% premium: (172 x 35 - 150)
  // x 9 x 1.18 / 2 = 31,169 off: 2,741.
  c.repair_premium = 18;
  EXPECT(mw1::wreck_value(c, {mw1::kJenner, 100, 0}) == 2772 && mw1::wreck_value(c, {mw1::kJenner, 50, 5000}) == 2741);
  // Success (Davion always pays 100%): 5% of the wrecks, and the fee's remainder; +2 favor, the kills counted. A
  // Locust (the Firefly, 30 t) wholly to repair: 2,344,000 - 53,206 = 2,291 thousand.
  const mw1::Settlement s =
      mw1::settle(c, k, mw1::Outcome::kSuccess, {{mw1::kJenner, 100, 0}, {mw1::kLocust, 0, mw1::kWhole}}, rng);
  EXPECT(s.payout_percent == 100 && s.salvage == (2772 + 2291) * 5 / 100 * 1000 && s.fee == 141000);
  EXPECT(c.funds == start + 9000 + s.salvage + s.fee && c.favor[mw1::kDavion] == 2 && c.kills == 2 && !k.accepted);
  // A retreat pays nothing and costs a grievance; a failure pays nothing either way.
  const int64_t before = c.funds;
  mw1::Contract r = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, r);
  const int64_t advanced = c.funds;
  const mw1::Settlement t = mw1::settle(c, r, mw1::Outcome::kRetreat, {{mw1::kJenner, 100}}, rng);
  EXPECT(t.salvage == 0 && t.fee == 0 && c.funds == advanced && advanced > before && c.grievance[mw1::kDavion] == 1);
  mw1::Contract f = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, f);
  const int64_t f_advanced = c.funds;
  const mw1::Settlement u = mw1::settle(c, f, mw1::Outcome::kFailure, {{mw1::kJenner, 100}}, rng);
  EXPECT(u.fee == 0 && u.salvage == 0 && c.funds == f_advanced && c.grievance[mw1::kDavion] == 1);
  // The House you fought: +2 grievance at every ending, a target that isn't a House none.
  for (const mw1::Outcome o : {mw1::Outcome::kSuccess, mw1::Outcome::kFailure, mw1::Outcome::kRetreat})
    for (const int32_t target : {int32_t{mw1::kMarik}, int32_t{mw1::kTargetColumns - 1}}) {
      mw1::Contract g = mw1::generate_contract(c, 0, rng);
      g.target = target;
      mw1::accept(c, g);
      int32_t was[mw1::kHouseCount];
      for (int h = 0; h < mw1::kHouseCount; ++h) was[h] = c.grievance[h];
      mw1::settle(c, g, o, {}, rng);
      for (int h = 0; h < mw1::kHouseCount; ++h) {
        const int32_t want = was[h] + (h == target ? 2 : 0) + (h == mw1::kDavion && o == mw1::Outcome::kRetreat ? 1 : 0);
        EXPECT(c.grievance[h] == want);
      }
    }
}

// Whom a contract is against, as MW1 rolls it: only the employer's eligible columns (Davion's: Kurita, Marik,
// Liao and the one that isn't a House), never itself, each of them in time.
TEST(mw1_campaign_targets_are_the_employers) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  for (int h = 0; h < mw1::kHouseCount; ++h) {
    c.planet_house = h;
    int seen[mw1::kTargetColumns] = {};
    for (int i = 0; i < 400; ++i) ++seen[mw1::generate_contract(c, 0, rng).target];
    for (int t = 0; t < mw1::kTargetColumns; ++t) EXPECT((seen[t] > 0) == (mw1::kHouseTargets[h][t] != 0));
    EXPECT(seen[h] == 0);
  }
}

// A new company starts on one of the original's eight worlds (eventPlanetChoicePool0, DS:8FDA), with that world's
// House and market tier, each of them reachable.
TEST(mw1_new_company_starts_on_one_of_the_eight_worlds) {
  bool seen[mw1::kStartPlanetCount] = {};
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    mw1::Rng rng(seed);
    const mw1::Company c = mw1::new_company(rng);
    int found = -1;
    for (int i = 0; i < mw1::kStartPlanetCount; ++i) {
      std::string upper = c.planet;
      for (char& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      if (upper == mw1::kStartPlanets[i].name) found = i;
    }
    EXPECT(found >= 0);
    if (found < 0) continue;
    seen[found] = true;
    EXPECT(c.planet_house == mw1::kStartPlanets[found].house && c.tier == mw1::kStartPlanets[found].tier);
  }
  for (bool s : seen) EXPECT(s);
  mw1::Rng rng(3);
  const mw1::Company e = mw1::new_company(rng);
  std::printf("  e.g. %s, House %s, tier %d\n", e.planet.c_str(), mw1::kHouses[e.planet_house].name, e.tier);
}

// The eight starting worlds against the exe: pool 0's planet numbers (DS:8FDA), and each one's record
// (planetRecordOffsets, DS:6570): number, House, market tier, the name after the record's ten bytes.
TEST(mw1_start_planets_are_mw_main_exes) {
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> d = mw_main_data();
  if (d.size() < 0x9000) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  for (int i = 0; i < mw1::kStartPlanetCount; ++i) {
    const mw1::StartPlanet& p = mw1::kStartPlanets[i];
    EXPECT(d[0x8FDA + i] == p.number);
    const size_t rec = static_cast<size_t>(static_cast<uint16_t>(word(d, 0x6570 + 2 * (p.number - 1))));
    EXPECT(d[rec] == p.number && d[rec + 1] == p.house && (d[rec + 2] - 1) / 3 == p.tier);
    EXPECT(std::strcmp(reinterpret_cast<const char*>(&d[rec + 10]), p.name) == 0);
  }
}

// MW1's results screen (FIXES.md item 19, mw1_results.h): its terminals' lines cut to fit, the time, the mech by
// location from PM's lance bars, and the view's texts and REPLAY.
TEST(mw1_results_lines_and_view) {
  std::vector<std::string> ten;
  for (int i = 0; i < 10; ++i) ten.push_back("line " + std::to_string(i));
  const std::vector<std::string> cut = mw1::fit_lines(ten);
  EXPECT(cut.size() == mw1::kTerminalLines && cut[6] == "line 6" && cut.back() == "... and 3 more");
  EXPECT(mw1::fit_lines({"a", "b"}).size() == 2);
  EXPECT(mw1::time_words(725) == "12:05" && mw1::time_words(59) == "0:59" && mw1::time_words(-1) == "unknown");
  // The torso's six bars together (by their points), the head, an arm gone, a leg without structure points.
  mw1::Bars b;
  for (int k = 0; k < 11; ++k) b.armor_points[k] = 10, b.armor_left[k] = 1, b.structure_points[k] = 5, b.structure_left[k] = 1;
  b.armor_points[0] = 30, b.armor_left[0] = 0.5f;  // the front torso: 30 of 80 points at half
  b.armor_left[7] = 0, b.structure_left[7] = 0;     // the left arm gone
  b.structure_points[10] = 0;                       // the right leg: no structure read
  std::vector<int32_t> armor, structure;
  mw1::location_left(b, &armor, &structure);
  std::printf("  armour by location:");
  for (int32_t v : armor) std::printf(" %d", v);
  std::printf("; structure:");
  for (int32_t v : structure) std::printf(" %d", v);
  std::printf("\n");
  EXPECT(armor.size() == mw1::kMechLocations && structure.size() == mw1::kMechLocations);
  EXPECT(armor[0] == 100 && armor[1] == 81 && armor[2] == 0 && armor[3] == 100 && structure[2] == 0 && structure[5] == -1);
  const std::vector<std::string> lines = mw1::mech_lines(armor, structure, false);
  for (const std::string& l : lines) std::printf("  [%s]\n", l.c_str());
  EXPECT(lines.size() == 7 && lines[3].rfind("Left arm", 0) == 0 && lines[3].find("0%") != std::string::npos);
  EXPECT(mw1::mech_lines({}, {}, true) == std::vector<std::string>{"Destroyed in the battle"});
  EXPECT(mw1::mech_lines({}, {}, false) == std::vector<std::string>{"Its damage wasn't read"});
  mw1::ResultsPanel p;
  p.planet = "SOL", p.employer = "HOUSE DAVION", p.outcome = "DEFEAT", p.scene = "mw1_hub_desert_city", p.replay = true;
  p.results = ten;
  const mw1::ScreenView v = mw1::results_view(p);
  EXPECT(v.backdrop == "mw1_hub_desert_city" && v.buttons.size() == 2 && v.buttons[mw1::kResultsReplay - 1].there);
  bool outcome = false, cut_shown = false;
  for (const mw1::TextBlock& t : v.blocks)
    for (const std::string& l : t.lines) outcome = outcome || l == "DEFEAT", cut_shown = cut_shown || l == "... and 3 more";
  EXPECT(outcome && cut_shown);
  p.replay = false;
  EXPECT(!mw1::results_view(p).buttons[mw1::kResultsReplay - 1].there);
}

// The loading screen's main enemy (FIXES.md item 18's screen, the lead's addition): framed beside the terminal when
// there is one, the frame and picture hidden without.
TEST(mw1_loading_view_enemy_picture) {
  mw1::LoadingPanel p;
  p.planet = "SOL", p.employer = "HOUSE DAVION", p.scene = "mw1_hub_desert_city", p.title = "CONTRACT";
  p.enemy = "mw1_icon_avatar";
  mw1::ScreenView v = mw1::loading_view(p);
  EXPECT(v.panels.size() == mw1::kLoadPanelCount && v.panels[mw1::kLoadEnemyFrame] && v.panels[mw1::kLoadEnemy] &&
         v.pictures[mw1::kLoadEnemy] == "mw1_icon_avatar" && v.panels[mw1::kLoadTerminal]);
  p.enemy.clear();
  v = mw1::loading_view(p);
  EXPECT(!v.panels[mw1::kLoadEnemyFrame] && !v.panels[mw1::kLoadEnemy] && v.panels[mw1::kLoadTerminal]);
}

// FIXES.md item 20: a line's highlighted spans (mw1_screen.h), marked and taken out again whole.
TEST(mw1_highlight_marks) {
  const std::string l = "Employer: " + mw1::highlighted("House Davion") + ", " + mw1::highlighted("2 Heavy");
  EXPECT(l.find(mw1::kHighlight) != std::string::npos && mw1::plain_line(l) == "Employer: House Davion, 2 Heavy");
  EXPECT(mw1::plain_line("no marks") == "no marks");
}
