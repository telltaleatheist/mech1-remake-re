// MW1's pilots (src/mw1_pilots.h): the records and the pitches against the player's MW_MAIN.EXE; the Bar's recruits
// (counts by tier, the reputation's Gunnery, the portrait pairs); hiring, the full lance, declining, firing,
// assigning; wages, the warning and the lance leaving; promotion; the casualties' rule and their mechs; the lance's
// weight in the offer.
#include <cctype>
#include <cstdlib>
#include <memory>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/pilot.h"
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

TEST(mw1_pilots_hire_fire_assign) {
  mw1::Company c = company();
  c.recruits = {1, 3, 5, 7, 9};
  EXPECT(mw1::hire(c, 9) == mw1::Hire::kNoSuch);
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kDone && c.lance.size() == 1 && c.lance[0].id == 1 && c.lance[0].wage_tier == 0);
  EXPECT(c.pilots[1].where == mw1::PilotWhere::kHired && c.recruits.size() == 4);
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kDone && mw1::hire(c, 0) == mw1::Hire::kDone && c.lance.size() == 3);
  // Full: refused, the offer used all the same.
  EXPECT(mw1::hire(c, 0) == mw1::Hire::kFull && c.lance.size() == 3 && c.recruits.size() == 1 && c.pilots[7].where == mw1::PilotWhere::kFree);
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

// FIXES.md item 32: a downed lancemate MISSING by kMissingPercent in 100, else KILLED. Missing: out of the lance, back
// in the hiring pool with their ratings and record (a Bar may offer them again); the mech lost either way.
TEST(mw1_pilots_missing_in_action) {
  mw1::Company c = company();
  c.recruits = {1, 41, 3};
  for (int i = 0; i < 3; ++i) mw1::hire(c, 0);
  // Rated at the commander's reputation, so a Bar may offer them; a mission into their record.
  c.pilots[41].gunnery = c.pilots[41].piloting = mw1::reputation(c), c.pilots[41].missions = 1;
  mw1::OwnedMech m;
  m.mech = mw1::kPmFirefly, m.pilot = 41;
  c.hangar = {m};
  EXPECT(mw1::pilot_missing({41, true, true}) && !mw1::pilot_killed({41, true, true}));
  EXPECT(!mw1::pilot_missing({41, false, false}) && !mw1::pilot_killed({41, false, false}));
  // Their ratings and record as if they'd come back: the mission counted, then out of the lance.
  mw1::Company twin = c;
  mw1::after_mission(twin, {{1, false}, {41, false}, {3, false}});
  mw1::after_mission(c, {{1, false}, {41, true, true}, {3, false}});
  EXPECT(c.lance.size() == 2 && c.lance[0].id == 1 && c.lance[1].id == 3 && c.hangar.empty());
  EXPECT(c.pilots[41].where == mw1::PilotWhere::kFree && c.pilots[41].gunnery == twin.pilots[41].gunnery &&
         c.pilots[41].piloting == twin.pilots[41].piloting && c.pilots[41].missions == twin.pilots[41].missions &&
         c.pilots[41].missions == 2);
  bool said = false;
  for (const std::string& n : c.notices) said = said || n.find("missing in action") != std::string::npos;
  EXPECT(said);
  // Back in the pool: a later Bar's recruits may be them (refresh_recruits draws from the free pilots).
  bool offered = false;
  mw1::Rng rng(5);
  for (int k = 0; k < 200 && !offered; ++k) {
    mw1::refresh_recruits(c, rng);
    for (int32_t id : c.recruits) offered = offered || id == 41;
  }
  EXPECT(offered);

  // The roll: never for a mech that came back; about kMissingPercent in 100 of the downed; the same seed the same.
  mw1::Rng r(77);
  int32_t missing = 0, n = 4000;
  for (int32_t i = 0; i < n; ++i) {
    std::vector<mw1::LanceFate> f = {{1, false}, {41, true}};
    mw1::roll_missing(f, r);
    EXPECT(!f[0].missing);
    missing += f[1].missing ? 1 : 0;
  }
  std::printf("  missing %d of %d downed (kMissingPercent %d)\n", missing, n, mw1::kMissingPercent);
  EXPECT(std::abs(missing * 100 - mw1::kMissingPercent * n) <= 5 * n);  // within 5 points
  mw1::Rng a(3), b(3);
  std::vector<mw1::LanceFate> fa(8, {41, true}), fb = fa;
  mw1::roll_missing(fa, a), mw1::roll_missing(fb, b);
  for (size_t i = 0; i < fa.size(); ++i) EXPECT(fa[i].missing == fb[i].missing);
}

// The pending mission's lance saved id:destroyed:missing; an older save's id:destroyed still reads (killed, as then).
TEST(mw1_pilots_missing_saved_and_an_older_lance_read) {
  mw1::Rng rng(9);
  mw1::Company c = mw1::new_company(rng);
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, k);
  mw1::AfterAction p;
  p.outcome = mw1::Outcome::kSuccess;
  p.lance = {{1, false, false}, {41, true, true}, {3, true, false}};
  p.salvage = std::make_shared<mech3::Store>();
  p.launch_stores = std::make_shared<mech3::Store>();
  p.launch_stores->assign(*c.stores);
  const std::string text = mw1::save_text(c, &k, &p);
  mw1::Company back;
  mw1::Contract kb;
  mw1::AfterAction pb;
  bool has = false, pend = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, kb, &has, &why, &pb, &pend));
  EXPECT(pend && pb.lance.size() == 3 && !pb.lance[0].destroyed && !pb.lance[0].missing);
  EXPECT(pb.lance[1].id == 41 && pb.lance[1].destroyed && pb.lance[1].missing);
  EXPECT(pb.lance[2].id == 3 && pb.lance[2].destroyed && !pb.lance[2].missing);
  EXPECT(mw1::save_text(back, &kb, &pb) == text);
  // The older lance: each fate id:destroyed, no third field.
  std::string old;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0) continue;
    if (line.rfind("pending_lance=", 0) == 0) {
      std::string two = "pending_lance=";
      for (size_t f = 14, comma; f < line.size(); f = comma + 1) {
        comma = line.find(',', f);
        if (comma == std::string::npos) comma = line.size();
        const std::string fate = line.substr(f, comma - f);
        two += (f > 14 ? "," : "") + fate.substr(0, fate.rfind(':'));
      }
      EXPECT(two != line);
      line = two;
    }
    old += line + "\n";
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : old) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  mw1::AfterAction po;
  pend = false;
  EXPECT(mw1::load_text(old + check, back, kb, &has, &why, &po, &pend));
  std::printf("  %s\n", why.c_str());
  EXPECT(pend && po.lance.size() == 3 && po.lance[1].destroyed && !po.lance[1].missing && po.lance[2].destroyed);
  // A bad third field is refused.
  std::string bad = text.substr(0, text.find("check="));
  const size_t at = bad.find("41:1:1");
  EXPECT(at != std::string::npos);
  if (at != std::string::npos) {
    bad.replace(at, 6, "41:1:7");
    h = 2166136261u;
    for (unsigned char ch : bad) h = (h ^ ch) * 16777619u;
    std::snprintf(check, sizeof check, "check=%08x\n", h);
    EXPECT(!mw1::load_text(bad + check, back, kb, &has, &why, &po, &pend));
  }
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
