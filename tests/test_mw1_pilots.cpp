// MW1's pilots (src/mw1_pilots.h): the records and the pitches against the player's MW_MAIN.EXE; the Bar's recruits
// (counts by tier, the reputation's Gunnery, the portrait pairs); hiring, the full lance, declining, firing,
// assigning; wages, the warning and the lance leaving; promotion; the casualties' rule and their mechs; the lance's
// weight in the offer.
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_campaign.h"
#include "mw1_pilots.h"
#include "mw1_save.h"
#include "mw1_travel.h"

namespace fs = std::filesystem;

namespace {
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
int32_t uword(const std::vector<uint8_t>& d, size_t at) { return d[at] | d[at + 1] << 8; }
mw1::Company company(int32_t kills = 0, int32_t tier = 0) {
  mw1::Company c;
  c.kills = kills, c.tier = tier, c.funds = 1000000;
  return c;
}
int32_t id_named(const char* name) {
  for (int32_t id = 1; id <= mw1::kPilotCount; ++id)
    if (std::string(mw1::pilot_record(id)->name) == name) return id;
  return 0;
}
}  // namespace

TEST(mw1_pilots_records_and_wages_are_mw_main_exes) {
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> d = mw_main_data();
  if (d.size() < 0x9C40) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  EXPECT(uword(d, 0x9121) == 0);  // id 0: no pilot
  for (int32_t id = 1; id <= mw1::kPilotCount; ++id) {
    const size_t r = static_cast<size_t>(uword(d, 0x9121 + 2 * static_cast<size_t>(id)));
    const mw1::PilotRecord* p = mw1::pilot_record(id);
    EXPECT(r >= 0x9177 && r < 0x9C39 && d[r] == p->portrait && d[r + 1] == p->gunnery && d[r + 2] == p->piloting);
    EXPECT(std::string(reinterpret_cast<const char*>(&d[r + 3])) == p->name);
  }
  for (int i = 0; i < 4; ++i) EXPECT(uword(d, 0x2476 + 2 * static_cast<size_t>(i)) == mw1::kWages[i]);
  // The pitch, read at run time from the player's exe; none without it.
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const std::vector<std::string> nasty = mw1::pitch(id_named("NASTY BILL"));
  for (const std::string& l : nasty) std::printf("  Nasty Bill: %s\n", l.c_str());
  EXPECT(nasty.size() == 3 && nasty[0] == "I GOT MY NICKNAME" && nasty[2] == "STYLE. I WIN.");
  EXPECT(mw1::pitch(0).empty() && mw1::pitch(43).empty());
  mw1::set_mw1_game_dir("/nonexistent");
  EXPECT(mw1::pitch(1).empty());
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::pilot_title(id_named("J. HOLMAAS")) == "J. Holmaas" && mw1::pilot_title(1) == "Elli Gujar" && mw1::pilot_title(3) == "Taris Ren");
}

TEST(mw1_pilots_recruits_by_tier_reputation_and_portraits) {
  mw1::Rng rng(4);
  EXPECT(mw1::reputation(company(5)) == 0 && mw1::reputation(company(6)) == 1 && mw1::reputation(company(11)) == 2 &&
         mw1::reputation(company(21)) == 3);
  for (int32_t tier = 0; tier < 4; ++tier)
    for (int32_t kills : {0, 6, 11, 21}) {
      mw1::Company c = company(kills, tier);
      const int32_t rep = mw1::reputation(c);
      int32_t most = 0;
      for (int i = 0; i < 500; ++i) {
        mw1::refresh_recruits(c, rng);
        most = std::max(most, static_cast<int32_t>(c.recruits.size()));
        std::set<int32_t> pairs;
        for (int32_t id : c.recruits) {
          const int32_t g = mw1::gunnery(c, id);
          EXPECT(g == rep || g + 1 == rep);
          EXPECT(pairs.insert((id - 1) / 2).second);  // never two sharing a portrait
        }
        EXPECT(static_cast<int32_t>(c.recruits.size()) <= 9 - 2 * (tier + 1) + 1);  // rand(0..9 - 2(tier + 1)) + 1
      }
      if (kills == 6) std::printf("  tier %d, reputation %d: up to %d recruits\n", tier, rep, most);
      EXPECT(most >= 1);
    }
  // Hired and dead pilots, and a hired one's portrait partner, aren't offered.
  mw1::Company c = company(6);
  c.pilots[1].where = mw1::PilotWhere::kHired, c.lance.push_back({1, 0});
  c.pilots[3].where = mw1::PilotWhere::kDead;
  for (int i = 0; i < 300; ++i) {
    mw1::refresh_recruits(c, rng);
    for (int32_t id : c.recruits) EXPECT(id != 1 && id != 2 && id != 3);
  }
}

TEST(mw1_pilots_hire_decline_fire_assign) {
  mw1::Company c = company();
  c.recruits = {1, 3, 5, 7, 9};
  EXPECT(mw1::hire(c, 9) == mw1::Hire::kNoSuch);
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kDone && c.lance.size() == 1 && c.lance[0].id == 1 && c.lance[0].wage_tier == 0);
  EXPECT(c.pilots[1].where == mw1::PilotWhere::kHired && c.recruits.size() == 4);
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kDone && mw1::hire(c, 0) == mw1::Hire::kDone && c.lance.size() == 3);
  // Full: refused, the offer used all the same.
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kFull && c.lance.size() == 3 && c.recruits.size() == 1 && c.pilots[7].where == mw1::PilotWhere::kFree);
  EXPECT(mw1::decline(c, 0) && c.recruits.empty() && !mw1::decline(c, 0));
  // ASSIGN: through the hangar's free mechs, then none.
  mw1::OwnedMech a, b;
  a.mech = mw1::kPmFirefly, b.mech = mw1::kPmCenturion;
  c.hangar = {a, b};
  EXPECT(mw1::mech_of(c, 0) == -1 && mw1::assign(c, 0) && mw1::mech_of(c, 0) == 0);
  EXPECT(mw1::assign(c, 1) && mw1::mech_of(c, 1) == 1);  // the Firefly taken: the Centurion
  EXPECT(mw1::assign(c, 2) && mw1::mech_of(c, 2) == -1);  // none free
  EXPECT(mw1::assign(c, 0) && mw1::mech_of(c, 0) == -1);  // past the last: none
  EXPECT(mw1::assign(c, 2) && mw1::mech_of(c, 2) == 0);
  // The lance weight: the Owens 1, the Firefly 1, the Centurion 2.
  EXPECT(mw1::lance_weight(c) == 1 + 1 + 2);
  // FIRE: back to the pool, the mech free.
  EXPECT(mw1::fire(c, 2) && c.lance.size() == 2 && c.pilots[5].where == mw1::PilotWhere::kFree && c.hangar[0].pilot == 0);
  EXPECT(!mw1::fire(c, 5));
}

TEST(mw1_pilots_wages_warning_and_leaving) {
  mw1::Rng rng(2);
  mw1::Company c = company();
  c.recruits = {1, 41};  // Poor (400) and Excellent (2,000)
  mw1::hire(c, 0), mw1::hire(c, 0);
  EXPECT(mw1::monthly_wages(c) == 2400);
  // A month begins on the calendar: paid.
  c.date.units = 59;
  mw1::advance_time(c, 1, rng);
  EXPECT(c.funds == 1000000 - 2400 && c.lance.size() == 2);
  // Short: a warning, nothing paid; the next shortfall, everyone leaves.
  c.funds = 1000;
  mw1::charge_wages(c, 1);
  EXPECT(c.funds == 1000 && c.wage_warned && c.lance.size() == 2);
  c.funds = 5000;
  mw1::charge_wages(c, 1);  // paid; the warning stays (MW1's)
  EXPECT(c.funds == 2600 && c.wage_warned);
  c.funds = 0;
  mw1::charge_wages(c, 1);
  EXPECT(c.lance.empty() && !c.wage_warned && c.pilots[1].where == mw1::PilotWhere::kFree);
  for (const std::string& n : c.notices) std::printf("  %s\n", n.c_str());
  // Two months short at once: they leave without a warning.
  c = company();
  c.recruits = {1};
  mw1::hire(c, 0);
  c.funds = 100;
  mw1::charge_wages(c, 2);
  EXPECT(c.lance.empty());
}

TEST(mw1_pilots_promotion_and_casualties) {
  mw1::Company c = company();
  c.recruits = {1, 41};
  mw1::hire(c, 0), mw1::hire(c, 0);
  const int32_t poor = 1, ace = 41;
  // Poor to Average at 3 missions, Piloting set to it; the wage stays.
  for (int i = 0; i < 2; ++i) mw1::after_mission(c, {{poor, false}, {ace, false}});
  EXPECT(mw1::gunnery(c, poor) == 0 && c.pilots[poor].missions == 2);
  mw1::after_mission(c, {{poor, false}});
  EXPECT(mw1::gunnery(c, poor) == 1 && mw1::piloting(c, poor) == 1 && c.pilots[poor].missions == 0 && c.lance[0].wage_tier == 0);
  for (int i = 0; i < 9; ++i) mw1::after_mission(c, {{poor, false}});
  EXPECT(mw1::gunnery(c, poor) == 1);
  mw1::after_mission(c, {{poor, false}});
  EXPECT(mw1::gunnery(c, poor) == 2);  // Average to Good at 10
  EXPECT(mw1::gunnery(c, ace) == 3 && c.pilots[ace].missions == 0);  // Excellent stays
  // Killed with their mech: gone for good, the mech lost; the other untouched.
  mw1::OwnedMech m;
  m.mech = mw1::kPmFirefly, m.pilot = ace;
  c.hangar = {m};
  EXPECT(mw1::pilot_killed({ace, true}) && !mw1::pilot_killed({ace, false}));
  mw1::after_mission(c, {{poor, false}, {ace, true}});
  EXPECT(c.lance.size() == 1 && c.lance[0].id == poor && c.hangar.empty() && c.pilots[ace].where == mw1::PilotWhere::kDead);
  for (const std::string& n : c.notices) std::printf("  %s\n", n.c_str());
}

TEST(mw1_pilots_lance_weight_makes_the_offer_harder) {
  // MW1's rule: the lance weight is in the enemy budget. The same seed with the commander alone, then with three
  // lancemates in a Firefly, a Centurion and an Awesome.
  mw1::Company c = company();
  c.stores = nullptr;
  mw1::Rng r1(17), r2(17);
  const mw1::Contract alone = mw1::generate_contract(c, 0, r1);
  c.recruits = {1, 3, 5};
  for (int i = 0; i < 3; ++i) mw1::hire(c, 0);
  for (int32_t mech : {mw1::kPmFirefly, mw1::kPmCenturion, mw1::kPmAwesome}) {
    mw1::OwnedMech o;
    o.mech = mech;
    c.hangar.push_back(o);
  }
  for (int i = 0; i < 3; ++i) mw1::assign(c, i);
  const mw1::Contract lance = mw1::generate_contract(c, 0, r2);
  auto names = [](const mw1::Contract& k) {
    std::string s;
    for (int32_t e : k.enemies) s += std::string(s.empty() ? "" : ", ") + mw1::kMechKinds[e].name;
    return s;
  };
  std::printf("  alone (weight %d): budget %d, %s, %d thousand\n", 1, alone.budget, names(alone).c_str(), alone.payment);
  std::printf("  a lance of four (weight %d): budget %d, %s, %d thousand\n", mw1::lance_weight(c), lance.budget, names(lance).c_str(),
              lance.payment);
  EXPECT(mw1::lance_weight(c) == 1 + 1 + 2 + 3 && lance.budget > alone.budget);
}

TEST(mw1_pilots_saved_and_a_version_3_save_read) {
  mw1::Rng rng(8);
  mw1::Company c = mw1::new_company(rng);
  c.recruits = {1, 3, 41};
  mw1::hire(c, 0), mw1::hire(c, 1);  // Elli Gujar and J. Holmaas
  mw1::OwnedMech o;
  o.mech = mw1::kPmFirefly;
  c.hangar.push_back(o);
  mw1::assign(c, 1);
  c.pilots[1].missions = 2, c.pilots[5].where = mw1::PilotWhere::kDead, c.pilots[7].gunnery = c.pilots[7].piloting = 2;
  c.wage_warned = true;
  const std::string text = mw1::save_text(c, nullptr);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  EXPECT(back.lance.size() == 2 && back.lance[0].id == 1 && back.lance[1].id == 41 && back.lance[1].wage_tier == 3);
  EXPECT(back.hangar.size() == 1 && back.hangar[0].pilot == 41 && mw1::mech_of(back, 1) == 0);
  EXPECT(back.pilots[1].missions == 2 && back.pilots[1].where == mw1::PilotWhere::kHired && back.pilots[5].where == mw1::PilotWhere::kDead);
  EXPECT(mw1::gunnery(back, 7) == 2 && back.recruits == std::vector<int32_t>({3}) && back.wage_warned);
  EXPECT(mw1::save_text(back, nullptr) == text);
  // Version 3: no pilots' lines; no lancemates, nobody hired.
  std::string v3;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    const std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("lance=", 0) == 0 || line.rfind("recruits=", 0) == 0 ||
        line.rfind("wage_warned=", 0) == 0 || line.rfind("pilot", 0) == 0 || line.find("_pilot=") != std::string::npos)
      continue;
    v3 += (line.rfind("version=", 0) == 0 ? std::string("version=3") : line) + "\n";
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : v3) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  mw1::Company old;
  EXPECT(mw1::load_text(v3 + check, old, k, &has, &why));
  EXPECT(old.lance.empty() && old.recruits.empty() && old.hangar.size() == 1 && old.hangar[0].pilot == 0);
  for (int32_t id = 1; id < mw1::kPilotIds; ++id) EXPECT(old.pilots[id].where == mw1::PilotWhere::kFree);
}

TEST(mw1_pilots_who_stayed_behind) {
  // Three deployed, the map's two allied places taken by the first two: the third stayed behind, and the counted
  // mission's experience and casualties are only the two who flew.
  EXPECT(mw1::stayed_behind({1, 41, 3}, {{1, false}, {41, true}}) == std::vector<int32_t>({3}));
  EXPECT(mw1::stayed_behind({1}, {{1, false}}).empty() && mw1::stayed_behind({}, {}).empty());
  mw1::Company c = company();
  c.recruits = {1, 41, 3};
  for (int i = 0; i < 3; ++i) mw1::hire(c, 0);
  mw1::after_mission(c, {{1, false}, {41, true}});
  EXPECT(c.pilots[1].missions == 1 && c.pilots[3].missions == 0 && c.pilots[3].where == mw1::PilotWhere::kHired &&
         c.pilots[41].where == mw1::PilotWhere::kDead);
}
