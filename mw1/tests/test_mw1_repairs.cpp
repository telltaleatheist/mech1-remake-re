// MW1's repairs on PM's damage (src/mw1_repairs.h): the body prices against the player's MW_MAIN.EXE, the line by
// tons, the premium, a mech's damage from its bars (armor and structure, or armor alone), what a field base mended,
// the bill taken and forgiven, the commander's lost mech and the safety net, and save version 5.
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_results.h"
#include "mw1_save.h"

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
mw1::Company company() {
  mw1::Company c;
  c.funds = 1000000, c.repair_premium = 18;
  return c;
}
// An Owens-sized set of bars: 11 bars of 10 armor and 10 structure points, all whole.
mw1::Bars whole() {
  mw1::Bars b;
  for (int k = 0; k < 11; ++k) b.armor_points[k] = 10, b.armor_left[k] = 1, b.structure_points[k] = 10, b.structure_left[k] = 1;
  return b;
}
}  // namespace

TEST(mw1_repairs_body_prices_are_mw_main_exes_and_the_line) {
  // MW1's table against the exe (skipped without the install; the line below still checked).
  if (!mw1_test::no_mw1()) {
    const std::vector<uint8_t> d = mw_main_data();
    EXPECT(d.size() > 0x3CAB + mw1::kMw1KindCount);
    for (int k = 0; k < mw1::kMw1KindCount && d.size() > 0x3CAB + mw1::kMw1KindCount; ++k)
      EXPECT(d[0x3CAB + static_cast<size_t>(k)] == mw1::kBodyRepairPrices[k]);
  }
  // The line through MW1's ten (provisional Q1), each record's own price beside it.
  for (int k = 0; k < mw1::kMw1KindCount; ++k) {
    const int32_t line = mw1::kRepairPerTon * mw1::kMw1Kinds[k].tons - mw1::kRepairOffset;
    std::printf("  record %d, %2d t: MW1 %2d,000 a location, the line %5d\n", k, mw1::kMw1Kinds[k].tons,
                mw1::kBodyRepairPrices[k], line);
    EXPECT(std::abs(line - mw1::kBodyRepairPrices[k] * 1000) <= 3500);
  }
  // A full repair: 9 locations, the premium. The Owens (35 t) at 18%: 5,870 x 9 x 1.18 = 62,339.
  mw1::Company c = company();
  EXPECT(mw1::full_repair(c, mw1::kPmOwens) == 62339);
  c.repair_premium = 0;
  EXPECT(mw1::full_repair(c, mw1::kPmOwens) == 52830);
  // The premium on arrival: (tier + 1) x 18 + 0..7.
  mw1::Rng rng(5);
  for (int32_t tier = 0; tier < 4; ++tier)
    for (int i = 0; i < 50; ++i) {
      c.tier = tier;
      mw1::roll_premium(c, rng);
      EXPECT(c.repair_premium >= (tier + 1) * 18 && c.repair_premium <= (tier + 1) * 18 + 7);
    }
}

TEST(mw1_repairs_damage_by_points_and_what_a_field_base_mended) {
  mw1::Bars b = whole();
  EXPECT(mw1::damage_of(b) == 0);
  // One bar's armor gone: 10 of 220 points.
  b.armor_left[3] = 0;
  EXPECT(mw1::damage_of(b) == 455);  // 10 / 220 = 4.55%
  // Everything gone.
  for (int k = 0; k < 11; ++k) b.armor_left[k] = b.structure_left[k] = 0;
  EXPECT(mw1::damage_of(b) == mw1::kWhole);
  // Without structure points, armor alone.
  mw1::Bars a = whole();
  for (int k = 0; k < 11; ++k) a.structure_points[k] = 0, a.structure_left[k] = 0;
  a.armor_left[0] = 0.5f;
  EXPECT(mw1::damage_of(a) == 455);  // 5 of 110
  // A field base mends a leg's structure from half to whole: what it mended is that share.
  mw1::Bars now = whole();
  float armor[11], structure[11];
  for (int k = 0; k < 11; ++k) armor[k] = 1, structure[k] = 1;
  structure[9] = 0.5f;
  EXPECT(mw1::mended(now, armor, structure) == 227);  // 5 of 220
  EXPECT(mw1::mended(now, now.armor_left, now.structure_left) == 0);
}

TEST(mw1_repairs_the_bill_taken_forgiven_and_the_lost_mechs) {
  // A typical bill beside a typical fee (for the owner, Q3): the Owens back with a quarter of its repairs and a Firefly
  // with a tenth, at an 18% premium, against a tier 0 contract's 150,000.
  mw1::Company c = company();
  mw1::Repairs r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}, {1, mw1::kPmFirefly, 1000, false}});
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.billed == 62339 / 4 + 53206 / 10 && r.paid == r.billed && r.forgiven == 0 && c.funds == 1000000 - r.billed);
  // Never below 0: the rest forgiven.
  c = company();
  c.funds = 2000;
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, mw1::kWhole, false}});
  EXPECT(r.billed == 62339 && r.paid == 2000 && r.forgiven == 60339 && c.funds == 0);
  // A destroyed mech is lost, never billed. The commander takes the first idle hangar mech.
  c = company();
  mw1::OwnedMech firefly, centurion;
  firefly.mech = mw1::kPmFirefly, firefly.pilot = 1, centurion.mech = mw1::kPmCenturion;
  c.hangar = {firefly, centurion};
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 3000, true}});
  EXPECT(r.billed == 0 && c.mech == mw1::kPmCenturion && c.hangar.size() == 1 && c.hangar[0].mech == mw1::kPmFirefly);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  // No idle one: a lancemate's, who stays behind next time.
  c.recruits = {1};
  mw1::hire(c, 0);
  r = mw1::settle_repairs(c, {{0, mw1::kPmCenturion, 0, true}});
  EXPECT(c.mech == mw1::kPmFirefly && c.hangar.empty() && c.lance.size() == 1 && mw1::mech_of(c, 0) == -1);
  EXPECT(!r.lines.empty() && r.lines[0] == "Lost: your Centurion. You take the Firefly; Elli Gujar stays behind next time.");
  // None left at all: the last comes back fully repaired, on the bill (with 0 in the bank, forgiven).
  c.funds = 0;
  r = mw1::settle_repairs(c, {{0, mw1::kPmFirefly, mw1::kWhole, true}});
  EXPECT(c.mech == mw1::kPmFirefly && r.billed == 53206 && r.forgiven == 53206 && c.funds == 0);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
}

// FIXES 106: the log's fates at settlement (mw1_game.cpp, settle_pending), each mech and lancemate as the losses left
// them: the commander's mech back, a lancemate's mech recovered and its pilot missing, another's lost and its pilot
// killed, a third's whole and its pilot back.
TEST(mw1_repairs_the_fates_said_for_the_log) {
  mw1::AfterAction a;
  a.bills = {{0, mw1::kPmOwens, 120, false, false}, {1, mw1::kPmCenturion, mw1::kWhole, true, false},
             {41, mw1::kPmFirefly, mw1::kWhole, true, true}, {3, mw1::kPmStrider, 0, false, false}};
  a.lance = {{1, true, true, false, false}, {41, true, false, true, true}, {3, false}};
  const std::vector<std::string> l = mw1::fate_lines(a);
  for (const std::string& x : l) std::printf("  %s\n", x.c_str());
  const std::string one = mw1::pilot_title(1), two = mw1::pilot_title(41), three = mw1::pilot_title(3);
  EXPECT(l.size() == 7);
  EXPECT(l.size() == 7 && l[0] == "the commander's " + mw1::mech_title(mw1::kPmOwens) + " back (damage 120 of 10000)");
  EXPECT(l.size() == 7 && l[1] == one + "'s " + mw1::mech_title(mw1::kPmCenturion) + " destroyed, recovered (damage 10000 of 10000)");
  EXPECT(l.size() == 7 && l[2] == two + "'s " + mw1::mech_title(mw1::kPmFirefly) + " destroyed, lost for good (damage 10000 of 10000)");
  EXPECT(l.size() == 7 && l[3] == three + "'s " + mw1::mech_title(mw1::kPmStrider) + " back (damage 0 of 10000)");
  EXPECT(l.size() == 7 && l[4] == one + " missing in action" && l[5] == two + " killed" && l[6] == three + " back with the company");
  // Downed, neither killed nor missing: ejected and back.
  a.lance = {{1, true}};
  EXPECT(mw1::fate_lines(a).back() == one + " ejected, back with the company");
}

TEST(mw1_repairs_saved_and_a_version_4_save_read) {
  mw1::Rng rng(9);
  mw1::Company c = mw1::new_company(rng);
  c.repair_premium = 23;
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, k);
  mw1::AfterAction p;
  p.outcome = mw1::Outcome::kFailure;
  p.wrecks = {{mw1::kSlot20, 40, 6100}};
  p.bills = {{0, mw1::kPmOwens, 1234, false}, {41, mw1::kPmFirefly, 0, true}};
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
  std::printf("  %s\n", why.c_str());
  EXPECT(pend && back.repair_premium == 23 && pb.wrecks.size() == 1 && pb.wrecks[0].damage == 6100 && pb.bills.size() == 2);
  EXPECT(pb.bills[0].who == 0 && pb.bills[0].mech == mw1::kPmOwens && pb.bills[0].damage == 1234 && !pb.bills[0].destroyed);
  EXPECT(pb.bills[1].who == 41 && pb.bills[1].destroyed);
  EXPECT(mw1::save_text(back, &kb, &pb) == text);
  // Version 4: no premium line (the world's without its roll), a wreck kind:condition (its damage the parts lost),
  // no bills.
  std::string v4;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("repair_premium=", 0) == 0 || line.rfind("pending_bills=", 0) == 0) continue;
    if (line.rfind("version=", 0) == 0) line = "version=4";
    if (line.rfind("pending_wrecks=", 0) == 0) line = line.substr(0, line.rfind(':'));
    v4 += line + "\n";
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : v4) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  mw1::Company old;
  mw1::AfterAction po;
  EXPECT(mw1::load_text(v4 + check, old, kb, &has, &why, &po, &pend));
  EXPECT(pend && old.repair_premium == (old.tier + 1) * 18 && po.wrecks.size() == 1 && po.wrecks[0].damage == 6000 && po.bills.empty());
}

// FIXES.md item 51 (the owner, 2026-11-18): the wrecks' worth pays the repairs first, the salvage term on what's left.
TEST(mw1_repairs_salvage_after_repairs_owens_numbers) {
  // The owner's example: 3,000,000 of wrecks, 2,000,000 of damage, a 10% term: 100,000 paid, the mech repaired, nothing
  // billed.
  mw1::SalvageSum m = mw1::salvage_after_repairs(3000000, {2000000}, 10, 100);
  EXPECT(m.repairs == 2000000 && m.covered == 2000000 && m.beyond == 0 && m.left == 1000000 && m.pay == 100000);
  // Repairs over the pool: the pool spent, the rest billed, nothing paid.
  m = mw1::salvage_after_repairs(1000000, {1400000}, 10, 100);
  EXPECT(m.covered == 1000000 && m.beyond == 400000 && m.left == 0 && m.pay == 0);
  // The order: the commander's first, then the lancemates' in turn; a short pool leaves the last one's billed.
  m = mw1::salvage_after_repairs(1000000, {700000, 200000, 300000}, 20, 100);
  EXPECT(m.from_pool.size() == 3 && m.from_pool[0] == 700000 && m.from_pool[1] == 200000 && m.from_pool[2] == 100000);
  EXPECT(m.beyond == 200000 && m.left == 0 && m.pay == 0);
  // A deduction (MW1's reduced payout) docks the pay, never the repairs: 60% of 10% of 1,000,000.
  m = mw1::salvage_after_repairs(3000000, {2000000}, 10, 60);
  EXPECT(m.covered == 2000000 && m.left == 1000000 && m.pay == 60000);
  // A story battle: all of what's left.
  m = mw1::salvage_after_repairs(3000000, {2000000}, mw1::kStorySalvagePercent, 100);
  EXPECT(m.pay == 1000000);
  // No wrecks (a loss): every repair billed.
  m = mw1::salvage_after_repairs(0, {500000}, 10, 100);
  EXPECT(m.covered == 0 && m.beyond == 500000 && m.pay == 0);
}

TEST(mw1_repairs_settled_from_the_pool) {
  // The Owens with a quarter of its repairs (62,339 / 4 = 15,584) and a Firefly with a tenth (5,320), at an 18%
  // premium (company()), against a pool of 100,000 and a 10% term: both repaired from the pool, 79,096 left, 7,909 paid
  // (settle_mission adds it); nothing from the funds.
  mw1::Company c = company();
  mw1::Repairs r = mw1::settle_repairs(c, {{1, mw1::kPmFirefly, 1000, false}, {0, mw1::kPmOwens, 2500, false}}, 100000, 10, 100);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.sum.repairs == 62339 / 4 + 53206 / 10 && r.sum.covered == r.sum.repairs && r.billed == 0 && c.funds == 1000000);
  EXPECT(r.sum.left == 100000 - r.sum.repairs && r.sum.pay == r.sum.left * 10 / 100);
  // DETAILS' lines (FIXES.md item 57): a heading, then each mech's, the commander's first, in plain words.
  EXPECT(r.lines.size() == 3 && r.lines[0] == "Repairs:" && r.lines[1] == "Owens (you): 15,584, paid from salvage");
  EXPECT(r.mechs.size() == 2 && r.mechs[0].who == 0 && r.mechs[0].from_pool == 15584 && r.mechs[1].who == 1);
  // 4:3's: a lancemate's by their pilot alone, as many lines.
  EXPECT(r.narrow.size() == r.lines.size() && r.narrow[1] == "Owens (you): 15,584 from salvage" &&
         r.narrow[2] == mw1::pilot_title(1) + ": 5,320 from salvage");
  // PAYMENT's line: two mechs, their total, fullest first, each shorter.
  std::vector<std::string> pl = mw1::repair_line_choices(r);
  EXPECT(pl.size() >= 2 && pl[0] == "2 'Mechs repaired: 20,904 C-bills, paid from salvage" && pl[1] == "2 'Mechs repaired: 20,904, paid from salvage");
  for (size_t i = 1; i < pl.size(); ++i) EXPECT(pl[i].size() < pl[i - 1].size());
  // One mech: its own name (the owner's words).
  c = company();
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}}, 100000, 10, 100);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "Owens repaired: 15,584 C-bills, paid from salvage");
  EXPECT(mw1::repair_line_choices(mw1::settle_repairs(c, {}, 100000, 10, 100)).empty());
  // A pool of 16,000: the commander's first (15,584 from it), the Firefly's 416 from it and 4,904 billed.
  c = company();
  r = mw1::settle_repairs(c, {{1, mw1::kPmFirefly, 1000, false}, {0, mw1::kPmOwens, 2500, false}}, 16000, 10, 100);
  for (const std::string& l : r.lines) std::printf("  %s\n", l.c_str());
  EXPECT(r.sum.from_pool.size() == 2 && r.sum.from_pool[0] == 62339 / 4 && r.sum.from_pool[1] == 16000 - 62339 / 4);
  EXPECT(r.billed == r.sum.repairs - 16000 && r.paid == r.billed && c.funds == 1000000 - r.billed && r.sum.pay == 0);
  EXPECT(r.lines.back().rfind("Repair bill: ", 0) == 0);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "2 'Mechs repaired: 20,904 C-bills, 4,904 billed to you" && pl.back() == "Repairs: 20,904, 4,904 billed");
  // Nothing from the pool (a failure): "billed" alone.
  c = company();
  r = mw1::settle_repairs(c, {{0, mw1::kPmOwens, 2500, false}}, 0, 10, 100);
  pl = mw1::repair_line_choices(r);
  EXPECT(!pl.empty() && pl[0] == "Owens repaired: 15,584 C-bills, billed to you" && r.lines[1] == "Owens (you): 15,584, billed");
  // The commander's last mech lost: its full repair comes off the top of the pool.
  c = company();
  c.mech = mw1::kPmFirefly;
  r = mw1::settle_repairs(c, {{0, mw1::kPmFirefly, mw1::kWhole, true}}, 100000, 10, 100);
  EXPECT(r.sum.covered == 53206 && r.billed == 0 && r.sum.left == 100000 - 53206);
}

// The whole settlement as CONTINUE makes it (FIXES.md item 51): a win's funds move by the fee and the salvage paid on
// what the wrecks left after the repairs; a story win (no employer) is paid all of what's left; a loss has no pool.
TEST(mw1_repairs_settle_mission_pays_after_the_repairs) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  c.repair_premium = 18;
  mw1::Contract k = mw1::generate_contract(c, 0, rng);
  k.salvage_percent = 10;
  mw1::accept(c, k);
  mw1::AfterAction a;
  a.outcome = mw1::Outcome::kSuccess;
  a.wrecks = {{mw1::kSlot35, 100, 0}, {mw1::kSlot20, 0, mw1::kWhole}};
  a.bills = {{0, c.mech, 5000, false}};
  const int64_t before = c.funds;
  mw1::MissionSettled m = mw1::settle_mission(c, k, a, rng);
  std::printf("  won: tier %d, wrecks %d + %d thousand; pool %lld, repairs %lld, left %lld, paid %lld (%d%%), fee %lld\n", c.tier,
              mw1::wreck_value(c, a.wrecks[0]), mw1::wreck_value(c, a.wrecks[1]), static_cast<long long>(m.r.sum.pool),
              static_cast<long long>(m.r.sum.repairs), static_cast<long long>(m.r.sum.left), static_cast<long long>(m.s.salvage),
              m.term, static_cast<long long>(m.s.fee));
  // Seed 5 starts at a tier 1 world (FIXES 120's portable roll): the 35 t slot's wreck 3,388, the 20 t
  // slot's 2,812 thousand.
  EXPECT(c.tier == 1 && m.term == 10 && m.s.pool == (3388 + 2812) * 1000 && m.r.sum.repairs > 0 && m.r.billed == 0);
  EXPECT(m.r.sum.left == m.s.pool - m.r.sum.repairs && m.s.salvage == m.r.sum.left * 10 * m.s.payout_percent / 10000);
  EXPECT(c.funds == before + m.s.fee + m.s.salvage);
  // A story battle won: no fee, all of what's left.
  mw1::Contract story = mw1::generate_contract(c, 0, rng);
  story.rung = 1;
  const int64_t before2 = c.funds;
  m = mw1::settle_mission(c, story, a, rng);
  EXPECT(m.term == mw1::kStorySalvagePercent && m.s.fee == 0 && m.s.salvage == m.r.sum.left && c.funds == before2 + m.s.salvage);
  // Lost: no pool, the repairs billed from the funds.
  mw1::Contract f = mw1::generate_contract(c, 0, rng);
  mw1::accept(c, f);
  a.outcome = mw1::Outcome::kFailure;
  const int64_t before3 = c.funds;
  m = mw1::settle_mission(c, f, a, rng);
  EXPECT(m.s.pool == 0 && m.s.salvage == 0 && m.r.sum.covered == 0 && m.r.billed == m.r.sum.repairs && c.funds == before3 - m.r.paid);
}

// FIXES.md item 75 (the owner): a retreat counts as a loss does: no fee and no salvage, the repairs billed from the funds,
// and a downed lancemate's 'Mech lost and its pilot KILLED (or MISSING).
TEST(mw1_repairs_a_retreat_bills_repairs_and_settles_the_fates) {
  mw1::Rng rng(7);
  mw1::Company c = mw1::new_company(rng);
  c.repair_premium = 18;
  for (const bool missing : {false, true}) {
    const int32_t id = missing ? 3 : 1;
    c.lance = {{id, 0}};
    c.pilots[static_cast<size_t>(id)].where = mw1::PilotWhere::kHired;
    mw1::OwnedMech o;
    o.mech = mw1::kPmFirefly, o.pilot = id;
    c.hangar = {o};
    mw1::Contract k = mw1::generate_contract(c, 0, rng);
    mw1::accept(c, k);
    mw1::AfterAction a;
    a.outcome = mw1::Outcome::kRetreat;
    a.wrecks = {{mw1::kSlot35, 100, 0}};
    a.lance = {{id, true, missing, !missing, true}};  // as roll_losses leaves a retreat's (FIXES 98): the mech lost
    a.bills = {{0, c.mech, 5000, false}, {id, mw1::kPmFirefly, mw1::kWhole, true, true}};
    const int64_t before = c.funds;
    const mw1::MissionSettled m = mw1::settle_mission(c, k, a, rng);
    std::printf("  retreat (%s): fee %lld, salvage %lld, repairs %lld, billed %lld, paid %lld; funds %lld -> %lld\n",
                missing ? "missing" : "killed", static_cast<long long>(m.s.fee), static_cast<long long>(m.s.salvage),
                static_cast<long long>(m.r.sum.repairs), static_cast<long long>(m.r.billed), static_cast<long long>(m.r.paid),
                static_cast<long long>(before), static_cast<long long>(c.funds));
    EXPECT(m.s.fee == 0 && m.s.pool == 0 && m.s.salvage == 0 && m.r.sum.repairs > 0 && m.r.billed == m.r.sum.repairs &&
           m.r.paid > 0 && c.funds == before - m.r.paid);
    EXPECT(c.hangar.empty() && c.lance.empty() &&
           c.pilots[static_cast<size_t>(id)].where == (missing ? mw1::PilotWhere::kFree : mw1::PilotWhere::kDead));
  }
}

// FIXES 98 (the owner: losses rare, a narrow win still a win). A win recovers every downed mech of the company's (billed as
// repairs, said "recovered"); one lost for good counts at its value; the mission's net (fee + salvage pay - repairs paid
// - mechs lost) never under kWinFloorPercent of the payment, the House covering the rest; a clean win nets more. A
// loss keeps today's rule (no floor).
TEST(mw1_repairs_a_narrow_win_nets_the_floor) {
  // `dear`: repairs at four times the price and no wrecks (no pool, no salvage), so a downed lance's repairs eat the pay
  // (today's prices rebuild all four mechs for about 40% of a first contract's).
  auto run = [](bool hurt, bool lose_one, mw1::Outcome o, bool dear = false) {
    mw1::Rng rng(11);
    mw1::Company c = mw1::new_company(rng);
    c.repair_premium = dear ? 400 : 18;
    c.funds = 50000000;
    c.recruits = {1, 3, 41};
    for (int i = 0; i < 3; ++i) mw1::hire(c, 0);
    const int32_t mechs[3] = {mw1::kPmFirefly, mw1::kPmOrion, mw1::kPmAwesome};
    c.hangar.clear();
    for (int i = 0; i < 3; ++i) {
      mw1::OwnedMech m;
      m.mech = mechs[i], m.pilot = c.lance[static_cast<size_t>(i)].id;
      c.hangar.push_back(m);
    }
    c.planet_house = mw1::kDavion;  // never docks: the fee whole
    mw1::Contract k = mw1::generate_contract(c, 0, rng);
    mw1::accept(c, k);
    mw1::AfterAction a;
    a.outcome = o;
    if (!dear) a.wrecks = {{mw1::kSlot20, 60, 4000}};
    a.bills.push_back({0, c.mech, hurt ? 9000 : 500, false});
    for (int i = 0; i < 3; ++i) {
      const int32_t id = c.lance[static_cast<size_t>(i)].id;
      const bool down = hurt, lost = down && (o != mw1::Outcome::kSuccess || (lose_one && i == 2));
      a.lance.push_back({id, down, false, false, lost});
      a.bills.push_back({id, mechs[i], down ? mw1::kWhole : 300, down, lost});
    }
    const int64_t before = c.funds;
    const int64_t floor = int64_t{k.payment} * 1000 * mw1::kWinFloorPercent / 100;
    const mw1::MissionSettled m = mw1::settle_mission(c, k, a, rng);
    const int64_t net = int64_t{m.advance} * 1000 + m.s.fee + m.s.salvage - m.r.paid - m.r.lost_value + m.r.house;
    const std::vector<std::string> pay = mw1::repair_line_choices(m.r);
    std::printf("  %s%s%s: payment %lld, fee %lld, salvage %lld, repairs %lld (paid %lld), lost %lld, House %lld: net %lld (floor %lld); "
                "funds %+lld; paid %d%%; PAYMENT \"%s\"\n",
                o == mw1::Outcome::kSuccess ? "won" : "lost", hurt ? ", all down" : ", clean", lose_one ? ", one lost" : "",
                static_cast<long long>(k.payment), static_cast<long long>(m.s.fee), static_cast<long long>(m.s.salvage),
                static_cast<long long>(m.r.sum.repairs), static_cast<long long>(m.r.paid), static_cast<long long>(m.r.lost_value),
                static_cast<long long>(m.r.house), static_cast<long long>(net), static_cast<long long>(floor),
                static_cast<long long>(c.funds - before), m.s.payout_percent, pay.empty() ? "" : pay[0].c_str());
    for (const std::string& l : m.r.lines) std::printf("    %s\n", l.c_str());
    struct Out {
      mw1::MissionSettled m;
      mw1::Company c;
      int64_t net, floor, delta;
      std::vector<std::string> pay;
    };
    return Out{m, c, net, floor, c.funds - before + int64_t{m.advance} * 1000, pay};
  };
  // Every mech down, the commander's badly hurt: all recovered, billed; the net exactly the floor, the House's line.
  const auto narrow = run(true, false, mw1::Outcome::kSuccess, true);
  EXPECT(narrow.m.r.house > 0 && narrow.net == narrow.floor && narrow.delta == narrow.floor);
  EXPECT(narrow.c.lance.size() == 3 && narrow.c.hangar.size() == 3);  // every lancemate back, every mech recovered
  int32_t recovered = 0;
  for (const mw1::MechRepair& r : narrow.m.r.mechs) recovered += r.recovered;
  EXPECT(recovered == 3);
  bool said_recovered = false, said_house = false;
  for (const std::string& l : narrow.m.r.lines) said_recovered = said_recovered || l.find(" recovered: ") != std::string::npos,
                                                said_house = said_house || l.rfind("The House covers ", 0) == 0;
  EXPECT(said_recovered && said_house);
  // PAYMENT's line: the repairs and the House's part, one wording fitting 4:3.
  EXPECT(!narrow.pay.empty() && narrow.pay[0].find("the House covers") != std::string::npos);
  bool fits = false;
  for (const std::string& l : narrow.pay) fits = fits || l.size() <= mw1::results::kResultsNarrow.status;
  EXPECT(fits);
  // One mech lost for good on the win: at its value, the floor still holds; it leaves the hangar, its pilot stays.
  const auto lost = run(true, true, mw1::Outcome::kSuccess);
  EXPECT(lost.m.r.lost_value > 0 && lost.m.r.house > 0 && lost.net == lost.floor && lost.c.hangar.size() == 2 && lost.c.lance.size() == 3);
  // A clean win nets more than the narrow one, no House (and so does today's prices' downed lance).
  const auto clean = run(false, false, mw1::Outcome::kSuccess, true), downed = run(true, false, mw1::Outcome::kSuccess);
  EXPECT(downed.m.r.house == 0 && downed.delta > downed.floor);
  EXPECT(clean.m.r.house == 0 && clean.delta > narrow.delta);
  // A loss: no floor, the downed mechs lost, it can cost money.
  const auto loss = run(true, false, mw1::Outcome::kFailure);
  EXPECT(loss.m.r.house == 0 && loss.m.r.paid > 0 && loss.m.s.fee == 0 && loss.c.hangar.empty());
}
