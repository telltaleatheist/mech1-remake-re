// FIXES 118 (src/mw1_upgrades.h): the permanent upgrades' model. The price ladders climb and stop at kPriceMost; kits
// and steps bought against the funds; kits assigned, moved and back in the storehouse when their 'Mech is sold or
// lost, and with their 'Mech when the commander changes 'Mechs; the steps' effects; save version 13 round trips, and a
// version 12 save reads as nothing bought. MW1_PRICE_TABLE=1 prints the ladders for settling the prices.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_save.h"
#include "mw1_upgrades.h"

namespace up = mw1::upgrades;

namespace {
constexpr int32_t kMediumLaser = 17, kLrm20 = 26;  // PM's Laser_Medium and LRM20 (mech_names.cpp)
mw1::OwnedMech owned(int32_t mech, int32_t sinks = 0, int32_t armour = 0, int32_t pilot = 0) {
  mw1::OwnedMech o;
  o.mech = mech, o.pilot = pilot, o.kits = {sinks, armour};
  return o;
}
uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
  return h;
}
// The text's lines but those starting with any of `drop`, the version line set to `version`, checked again.
std::string rewritten(const std::string& text, int32_t version, const std::vector<std::string>& drop) {
  std::string body;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    bool skip = line.rfind("check=", 0) == 0;
    for (const std::string& d : drop) skip = skip || line.rfind(d, 0) == 0;
    if (skip) continue;
    if (line.rfind("version=", 0) == 0) line = "version=" + std::to_string(version);
    body += line + "\n";
  }
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(body));
  return body + check;
}
bool same_kits(const mw1::Kits& k, int32_t sinks, int32_t armour) { return k.sinks == sinks && k.armour == armour; }
}  // namespace

TEST(mw1_upgrades_the_ladders_climb_and_the_price_table) {
  EXPECT(up::ladder_price(500000, 1250, 0) == 500000 && up::ladder_price(500000, 1250, 1) == 625000);
  EXPECT(up::ladder_price(500000, 1250, 2) == 781000);  // 781,250 floored to 1,000
  EXPECT(up::ladder_price(100000, 1050, 1) == 105000 && up::ladder_price(1000000, 1500, 3) == 3375000);
  // Every next one dearer than the last, until the ceiling, where it stays.
  bool climbs = true;
  int64_t last = 0;
  int32_t n = 0;
  for (; n < 2000 && up::ladder_price(up::kArmourBase, up::kArmourRatio, n) < up::kPriceMost; ++n) {
    const int64_t p = up::ladder_price(up::kArmourBase, up::kArmourRatio, n);
    climbs = climbs && p > last, last = p;
  }
  EXPECT(climbs && n > 100 && n < 2000);
  EXPECT(up::ladder_price(up::kArmourBase, up::kArmourRatio, 5000) == up::kPriceMost);
  std::printf("  armour points before the ceiling: %d\n", n);
  const std::string table = up::price_table();
  EXPECT(table.find("PRICES sink kit") != std::string::npos && table.find("PRICES armour kit") != std::string::npos);
  if (std::getenv("MW1_PRICE_TABLE")) std::printf("%s", table.c_str());
}

TEST(mw1_upgrades_kits_and_steps_bought_against_the_funds) {
  mw1::Company c;
  c.funds = up::kSinkBase - 1;
  EXPECT(up::buy_kit(c, up::Kit::kSink) == up::Bought::kNoFunds && c.funds == up::kSinkBase - 1 && same_kits(c.upgrades.store, 0, 0));
  c.funds = up::kSinkBase;
  EXPECT(up::buy_kit(c, up::Kit::kSink) == up::Bought::kDone && c.funds == 0 && same_kits(c.upgrades.store, 1, 0));
  EXPECT(c.upgrades.sinks_bought == 1 && up::kit_price(c, up::Kit::kSink) > up::kSinkBase);
  EXPECT(up::kit_price(c, up::Kit::kArmour) == up::kArmourBase);  // each kit its own ladder
  c.funds = 100000000;
  for (int i = 0; i < 3; ++i) EXPECT(up::buy_kit(c, up::Kit::kArmour) == up::Bought::kDone);
  EXPECT(same_kits(c.upgrades.store, 1, 3) && c.upgrades.armour_bought == 3);
  EXPECT(c.funds == 100000000 - up::ladder_price(up::kArmourBase, up::kArmourRatio, 0) -
                        up::ladder_price(up::kArmourBase, up::kArmourRatio, 1) - up::ladder_price(up::kArmourBase, up::kArmourRatio, 2));
  // Steps: a weapon type's own ladder (power and range together); another type starts at the base.
  EXPECT(!up::steps_of(c, kMediumLaser, 0) && up::damage_scale(c, kMediumLaser, 0) == 1.0f && up::range_add(c, kMediumLaser, 0) == 0.0f);
  EXPECT(up::buy_step(c, kMediumLaser, 0, up::Step::kPower) == up::Bought::kDone);
  EXPECT(up::buy_step(c, kMediumLaser, 0, up::Step::kRange, 2) == up::Bought::kDone);
  EXPECT(up::buy_step(c, kMediumLaser, 0, up::Step::kRange, 2) == up::Bought::kDone);
  EXPECT(up::buy_step(c, kMediumLaser, 0, up::Step::kRange, 2) == up::Bought::kAtMost);  // its range's limit
  const mw1::WeaponSteps* s = up::steps_of(c, kMediumLaser, 0);
  EXPECT(s && s->power == 1 && s->range == 2 && up::step_ladder_count(c, kMediumLaser, 0) == 3);
  EXPECT(up::damage_scale(c, kMediumLaser, 0) == 1.0f + up::kPowerStepPercent / 100.0f);
  EXPECT(up::range_add(c, kMediumLaser, 0) == 2.0f * up::kRangeStepMetres);
  EXPECT(up::step_price(c, kMediumLaser, 0) == up::ladder_price(up::kStepBase, up::kStepRatio, 3));
  EXPECT(up::step_price(c, kLrm20, 0) == up::kStepBase && up::step_price(c, kMediumLaser, 1) == up::kStepBase);  // the Clan's its own
  EXPECT(!up::steps_of(c, kMediumLaser, 1) && up::damage_scale(c, kMediumLaser, 1) == 1.0f);
  EXPECT(up::buy_step(c, kMediumLaser, 2, up::Step::kPower) == up::Bought::kNoSuch);
  // The range limit: kRangeMostPercent of the weapon's range, in steps.
  EXPECT(up::range_steps_most(0) == 0 && up::range_steps_most(300.0f) == 300 * up::kRangeMostPercent / 100 / up::kRangeStepMetres);
  // No funds: nothing changes.
  c.funds = 0;
  EXPECT(up::buy_step(c, kLrm20, 0, up::Step::kPower) == up::Bought::kNoFunds && !up::steps_of(c, kLrm20, 0));
}

TEST(mw1_upgrades_kits_assigned_moved_and_never_lost) {
  mw1::Company c;
  c.funds = 50000000, c.repair_premium = 18, c.mech = mw1::kPmOwens;
  c.stores = std::make_shared<mech3::Store>();
  c.hangar = {owned(mw1::kPmFirefly), owned(mw1::kPmCenturion)};
  c.upgrades.store = {3, 2};
  const mw1::Kits all = up::all_kits(c);
  auto none_lost = [&] { return up::all_kits(c) == all; };
  // On the commander's 'Mech (owned 0) and a hangar one; none left to give; no such 'Mech.
  EXPECT(up::assign(c, 0, up::Kit::kSink, 1) == up::Bought::kDone && up::assign(c, 0, up::Kit::kSink, 1) == up::Bought::kDone);
  EXPECT(up::assign(c, 2, up::Kit::kSink, 1) == up::Bought::kDone && up::assign(c, 1, up::Kit::kSink, 1) == up::Bought::kNoSuch);
  EXPECT(up::assign(c, 1, up::Kit::kArmour, 1) == up::Bought::kDone && up::assign(c, 3, up::Kit::kArmour, 1) == up::Bought::kNoSuch);
  EXPECT(up::assign(c, 0, up::Kit::kArmour, 2) == up::Bought::kNoSuch);
  EXPECT(same_kits(c.kits, 2, 0) && same_kits(c.hangar[0].kits, 0, 1) && same_kits(c.hangar[1].kits, 1, 0));
  EXPECT(same_kits(c.upgrades.store, 0, 1) && none_lost() && c.funds == 50000000 - 4 * up::kFitFee);
  // Moved: off one, onto another.
  EXPECT(up::assign(c, 0, up::Kit::kSink, -1) == up::Bought::kDone && up::assign(c, 1, up::Kit::kSink, 1) == up::Bought::kDone);
  EXPECT(up::assign(c, 2, up::Kit::kArmour, -1) == up::Bought::kNoSuch);
  EXPECT(same_kits(c.kits, 1, 0) && same_kits(c.hangar[0].kits, 1, 1) && none_lost());
  // PILOT: the commander takes the Firefly; each 'Mech keeps its kits.
  EXPECT(mw1::pilot(c, 1) == mw1::Deal::kDone && c.mech == mw1::kPmFirefly && same_kits(c.kits, 1, 1));
  EXPECT(c.hangar[0].mech == mw1::kPmOwens && same_kits(c.hangar[0].kits, 1, 0) && none_lost());
  // The lance screen's swap (apply_lance): the commander into the Centurion, the Firefly to the hangar with its kits.
  EXPECT(mw1::apply_lance(c, {2}, {nullptr, nullptr, nullptr}) && c.mech == mw1::kPmCenturion && same_kits(c.kits, 1, 0));
  EXPECT(c.hangar[1].mech == mw1::kPmFirefly && same_kits(c.hangar[1].kits, 1, 1) && none_lost());
  // SELL the Owens: its kits back in the storehouse.
  EXPECT(mw1::sell(c, 1) == mw1::Deal::kDone && c.hangar.size() == 1 && same_kits(c.upgrades.store, 1, 1) && none_lost());
  // A lancemate's 'Mech lost with them: its kits back in the storehouse.
  c.lance.push_back({1, 0});
  c.pilots[1].where = mw1::PilotWhere::kHired;
  c.hangar[0].pilot = 1;
  mw1::after_mission(c, {{1, true, false, true, true}});
  EXPECT(c.hangar.empty() && same_kits(c.upgrades.store, 2, 2) && none_lost());
  // The commander's 'Mech lost: its kits back; the 'Mech taken from the hangar brings its own.
  c.hangar = {owned(mw1::kPmFirefly, 4, 5)};
  const mw1::Kits more = up::all_kits(c);
  mw1::settle_repairs(c, {{0, mw1::kPmCenturion, 3000, true}});
  EXPECT(c.mech == mw1::kPmFirefly && c.hangar.empty() && same_kits(c.kits, 4, 5) && same_kits(c.upgrades.store, 3, 2));
  EXPECT(up::all_kits(c) == more);
  // Selling the piloted one: the next 'Mech (and its kits) piloted, the sold one's kits stored.
  c.hangar = {owned(mw1::kPmOwens, 0, 7)};
  c.market.limit = -1;
  EXPECT(mw1::sell(c, 0) == mw1::Deal::kDone && c.mech == mw1::kPmOwens && same_kits(c.kits, 0, 7) && same_kits(c.upgrades.store, 7, 7));
}

TEST(mw1_upgrades_saved_as_version_13_and_older_saves_read_none) {
  mw1::Rng rng(118);
  mw1::Company c = mw1::new_company(rng);
  c.hangar = {owned(mw1::kPmFirefly, 2, 0), owned(mw1::kPmCenturion, 0, 9)};
  c.kits = {1, 30};
  c.upgrades.store = {4, 6};
  c.upgrades.sinks_bought = 7, c.upgrades.armour_bought = 45;
  c.upgrades.steps = {{kMediumLaser, 0, 3, 1}, {kLrm20, 1, 0, 2}};
  const std::string text = mw1::save_text(c, nullptr);
  EXPECT(text.rfind("version=13\n", 0) == 0);
  EXPECT(text.find("\nkits=4:6\nkits_bought=7:45\nmech_kits=1:30\nhangar0_kits=2:0\nhangar1_kits=0:9\nsteps=17:0:3:1,26:1:0:2\n") !=
         std::string::npos);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  std::printf("  version 13: %s\n", why.empty() ? "read" : why.c_str());
  EXPECT(mw1::save_text(back, nullptr) == text);
  EXPECT(same_kits(back.kits, 1, 30) && same_kits(back.upgrades.store, 4, 6) && back.upgrades.sinks_bought == 7 &&
         back.upgrades.armour_bought == 45 && back.upgrades.steps == c.upgrades.steps);
  EXPECT(back.hangar.size() == 2 && same_kits(back.hangar[0].kits, 2, 0) && same_kits(back.hangar[1].kits, 0, 9));
  // A version 12 save (no upgrade lines): read, nothing bought.
  const std::string v12 = rewritten(text, 12, {"kits=", "kits_bought=", "mech_kits=", "hangar0_kits=", "hangar1_kits=", "steps="});
  why.clear();
  mw1::Company old;
  EXPECT(mw1::load_text(v12, old, k, &has, &why));
  std::printf("  version 12: %s\n", why.empty() ? "read" : why.c_str());
  EXPECT(old.hangar.size() == 2 && up::all_kits(old) == mw1::Kits{} && old.upgrades.steps.empty() && old.upgrades.sinks_bought == 0);
  EXPECT(old.funds == c.funds);
  // Out of range or twice: refused, the company untouched.
  auto refused = [&](const std::string& from, const std::string& to, const char* what) {
    std::string t = text.substr(0, text.rfind("check="));
    const size_t at = t.find(from);
    if (at == std::string::npos) return false;
    t.replace(at, from.size(), to);
    mw1::Company into;
    into.funds = 77;
    std::string w;
    const bool ok = mw1::load_text(rewritten(t, 13, {}), into, k, &has, &w);
    std::printf("  %s: %s\n", what, ok ? "read (!)" : w.c_str());
    return !ok && into.funds == 77;
  };
  EXPECT(refused("steps=17:0:3:1", "steps=99:0:3:1", "a weapon past PM's"));
  EXPECT(refused("steps=17:0:3:1", "steps=17:2:3:1", "a tech past the Clan's"));
  EXPECT(refused("steps=17:0:3:1,26:1:0:2", "steps=17:0:3:1,17:0:0:2", "a type twice"));
  EXPECT(refused("kits=4:6", "kits=4:-6", "a count below none"));
  EXPECT(refused("mech_kits=1:30", "mech_kits=1", "a kit line short"));
}
