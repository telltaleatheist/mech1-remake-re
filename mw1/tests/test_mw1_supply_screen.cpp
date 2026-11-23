// 1.1.0 (FIXES 122 and 118): the supply screen's view (src/mw1_supply_screen.h), pure: FOR SALE's rows (the dealer's,
// a rule, the kits, a power and a range step for each weapon type the company owns), YOUR STORES' (the stores' items,
// a rule, the storehouse's kits, the steps each type has) and, assigning, the 'Mechs with their kits; the rule never
// chosen; the buttons on as each row allows; every list line inside 4:3's terminal. And the Mech Complex's YOUR MECHS
// with their kits: a column on the wide layout, the status line on 4:3's.
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_dealer.h"
#include "mw1_market.h"
#include "mw1_market_screen.h"
#include "mw1_supply_screen.h"
#include "mw1_upgrades.h"

namespace {
const mech3::WeaponLab* fake_lab(const char* name) {
  static const mech3::WeaponLab kPpc{{7.0f, 7.0f}, {3, 3}, 0.0f};
  static const mech3::WeaponLab kSmall{{0.5f, 1.0f}, {1, 1}, 0.0f};
  static const mech3::WeaponLab kSrm4{{1.0f, 2.0f}, {1, 1}, 0.01f};
  if (std::strcmp(name, "PPC") == 0) return &kPpc;
  if (std::strcmp(name, "ERLaser_Small") == 0) return &kSmall;
  if (std::strcmp(name, "SRM4") == 0) return &kSrm4;
  return nullptr;
}
struct FakeLab {
  FakeLab() { mech3::set_weapon_lab(fake_lab); }
  ~FakeLab() { mech3::set_weapon_lab(nullptr); }
};

const std::vector<std::string>* block(const mw1::ScreenView& v, const char* at) {
  for (const mw1::TextBlock& b : v.blocks)
    if (b.at && std::string(b.at) == at) return &b.lines;
  return nullptr;
}
bool on(const mw1::ScreenView& v, int32_t id) { return v.buttons[static_cast<size_t>(id - 1)].on; }
bool there(const mw1::ScreenView& v, int32_t id) { return v.buttons[static_cast<size_t>(id - 1)].there; }
bool any_line_has(const mw1::ScreenView& v, const char* at, const std::string& t) {
  if (const std::vector<std::string>* ls = block(v, at))
    for (const std::string& l : *ls)
      if (l.find(t) != std::string::npos) return true;
  return false;
}
// 4:3's terminal: 46 of the line font's characters before the arrows (mw1_supply_screen.cpp).
constexpr size_t kRoom = 46;
bool lines_fit(const mw1::ScreenView& v) {
  bool fit = true;
  for (const char* at : {"S_HEAD", "S_ROWS", "S_ROWS_DIM", "O_HEAD", "O_ROWS"})
    if (const std::vector<std::string>* ls = block(v, at))
      for (const std::string& l : *ls)
        if (l.size() > kRoom) fit = false, std::printf("  too long for 4:3 (%s, %zu): %s\n", at, l.size(), l.c_str());
  return fit;
}

struct Fixture {
  int32_t ppc = mech3::weapon_index("PPC"), small = mech3::weapon_index("ERLaser_Small"), srm4 = mech3::weapon_index("SRM4");
  mw1::Company c;
  Fixture() {
    c.planet = "Test", c.tier = 0, c.funds = 10000000, c.mech = mw1::kPmFirefly;
    c.stores = std::make_shared<mech3::Store>();
    c.loadout = std::make_shared<mech3::Config>(mw1::kPmFirefly, "lab");
    c.loadout->add_weapon(ppc, mech3::kInnerSphere);
    c.loadout->add_weapon(small, mech3::kClan);
    mw1::OwnedMech o;
    o.mech = mw1::kPmOwens, o.pilot = 1;
    o.loadout = std::make_shared<mech3::Config>(mw1::kPmOwens, "lab");
    o.loadout->add_weapon(srm4, mech3::kInnerSphere);
    o.loadout->add_weapon(ppc, mech3::kInnerSphere);  // a type the commander's carries too: listed once
    c.hangar.push_back(o);
    c.dealer.stock = {{mw1::dealer::kWeapon, ppc, mech3::kInnerSphere, 2}, {mw1::dealer::kAmmo, srm4, 0, 3}};
    c.dealer.rolled = true;
  }
};
}  // namespace

TEST(mw1_supply_rows_rule_and_buttons) {
  FakeLab lab;
  Fixture f;
  mw1::Company& c = f.c;
  EXPECT(f.ppc >= 0 && f.small >= 0 && f.srm4 >= 0);
  // The weapon types, each once, by id (Inner Sphere before Clan).
  const std::vector<mw1::OwnedType> types = mw1::owned_weapon_types(c);
  EXPECT(types.size() == 3);
  // FOR SALE: two dealer lines, the rule, two kits, a power and a range step for each of the three types.
  std::vector<mw1::SupplyRow> sale = mw1::sale_rows(c);
  EXPECT(sale.size() == 2 + 1 + 2 + 6);
  EXPECT(sale[0].kind == mw1::LineKind::kWare && sale[1].kind == mw1::LineKind::kWare && sale[2].kind == mw1::LineKind::kRule &&
         sale[3].kind == mw1::LineKind::kKit && sale[5].kind == mw1::LineKind::kStep && sale[6].step == mw1::upgrades::Step::kRange);
  // The rule is never chosen: clamped past it, stepped over it both ways.
  mw1::SupplyChoice s;
  s.sale = 2;
  mw1::clamp_choice(c, s);
  EXPECT(s.sale == 3);
  mw1::step_supply(c, s, false, -1);
  EXPECT(s.sale == 1 && !s.own_chosen);
  mw1::step_supply(c, s, false, 1);
  EXPECT(s.sale == 3);
  // The page: twelve buttons; the kits' four not shown outside ASSIGN; nothing in the stores to SELL.
  mw1::ScreenView v = mw1::supply_view(c, s);
  EXPECT(v.buttons.size() == static_cast<size_t>(mw1::kSupplyButtonCount) && v.panels.size() == mw1::kSupplyPanelCount);
  EXPECT(on(v, mw1::kSupplyBuy) && !on(v, mw1::kSupplySell) && on(v, mw1::kSupplyAssign) && on(v, mw1::kSupplyBack));
  for (int32_t id : {mw1::kSupplyAddSink, mw1::kSupplyTakeSink, mw1::kSupplyAddArmour, mw1::kSupplyTakeArmour}) EXPECT(!there(v, id));
  EXPECT(any_line_has(v, "S_ROWS", "SINK KIT") && any_line_has(v, "O_TITLE", "YOUR STORES") && lines_fit(v));
  EXPECT(block(v, "STATUS") && block(v, "STATUS")->front().find("Sink kit") != std::string::npos);  // the chosen row said
  // A dealer's item bought: in the stores, SELL-able there (its row then the stores' first), the dealer's line down one.
  EXPECT(mw1::dealer::buy(c, 0) == mw1::dealer::Trade::kDone && c.dealer.stock[0].count == 1);
  std::vector<mw1::SupplyRow> own = mw1::own_rows(c, false);
  EXPECT(own.size() == 1 + 1 + 2 && own[0].kind == mw1::LineKind::kWare && own[1].kind == mw1::LineKind::kRule);
  s.own = 0, s.own_chosen = true;
  mw1::clamp_choice(c, s);
  v = mw1::supply_view(c, s);
  EXPECT(on(v, mw1::kSupplySell) && any_line_has(v, "O_ROWS", "PPC") && lines_fit(v));
  EXPECT(block(v, "STATUS") && block(v, "STATUS")->front().find("In your stores: 1") != std::string::npos);
  // Kits and steps ask before buying (never sold back); the dealer's items don't.
  EXPECT(!mw1::buy_question(c, sale[3]).empty() && !mw1::buy_question(c, sale[5]).empty() && mw1::buy_question(c, sale[0]).empty());
  // A step bought: YOUR STORES lists the type's steps; FOR SALE's row says how many it has.
  EXPECT(mw1::upgrades::buy_step(c, f.ppc, mech3::kInnerSphere, mw1::upgrades::Step::kPower) == mw1::upgrades::Bought::kDone);
  v = mw1::supply_view(c, s);
  EXPECT(any_line_has(v, "O_ROWS", "+1 power") && lines_fit(v));
  EXPECT(!mw1::row_at_most(c, sale[6]));  // no weapon data: no range limit known (the engine's guard holds it)
}

TEST(mw1_supply_assigning_the_kits) {
  FakeLab lab;
  Fixture f;
  mw1::Company& c = f.c;
  mw1::SupplyChoice s;
  s.assigning = true, s.own_chosen = true, s.own = 1;  // the hangar's Owens
  mw1::clamp_choice(c, s);
  mw1::ScreenView v = mw1::supply_view(c, s);
  EXPECT(mw1::own_rows(c, true).size() == 2 && any_line_has(v, "O_TITLE", "KITS") && v.buttons[mw1::kSupplyAssign - 1].words == "STORES");
  for (int32_t id : {mw1::kSupplyAddSink, mw1::kSupplyTakeSink, mw1::kSupplyAddArmour, mw1::kSupplyTakeArmour}) EXPECT(there(v, id) && !on(v, id));
  EXPECT(!on(v, mw1::kSupplySell));
  // A sink kit bought: +SINK on; onto the Owens: -SINK on, its row "+1 S +0 A"; moved to the commander's.
  EXPECT(mw1::upgrades::buy_kit(c, mw1::upgrades::Kit::kSink) == mw1::upgrades::Bought::kDone && c.upgrades.store.sinks == 1);
  v = mw1::supply_view(c, s);
  EXPECT(on(v, mw1::kSupplyAddSink) && !on(v, mw1::kSupplyTakeSink) && !on(v, mw1::kSupplyAddArmour));
  EXPECT(mw1::upgrades::assign(c, 1, mw1::upgrades::Kit::kSink, 1) == mw1::upgrades::Bought::kDone);
  v = mw1::supply_view(c, s);
  EXPECT(!on(v, mw1::kSupplyAddSink) && on(v, mw1::kSupplyTakeSink) && any_line_has(v, "O_ROWS", "+1 S +0 A") && lines_fit(v));
  EXPECT(block(v, "STATUS") && block(v, "STATUS")->front().find("+1 sinks") != std::string::npos);
  EXPECT(mw1::upgrades::assign(c, 1, mw1::upgrades::Kit::kSink, -1) == mw1::upgrades::Bought::kDone &&
         mw1::upgrades::assign(c, 0, mw1::upgrades::Kit::kSink, 1) == mw1::upgrades::Bought::kDone);
  EXPECT(c.kits.sinks == 1 && c.hangar[0].kits.sinks == 0 && c.upgrades.store.sinks == 0);
  // The rows by the mouse: the Mech Complex's places.
  bool own = false;
  int32_t row = -1;
  const int32_t first = mw1::kSupplyListTop[1] + mw1::kSupplyRowsFrom;
  EXPECT(mw1::supply_row_at(210, first + mw1::kSupplyRowGap, false, &own, &row) && own && row == 1);
  EXPECT(!mw1::supply_row_at(100, first, false, &own, &row));  // the buttons down the left
}

// The Mech Complex's YOUR MECHS with their kits (FIXES 118): a KITS column on the wide layout; 4:3's has no room, so
// its status line says the chosen 'Mech's.
TEST(mw1_market_your_mechs_kits) {
  mw1::Company c;
  c.planet = "Test", c.funds = 1000000, c.mech = mw1::kPmFirefly, c.kits = {3, 12};
  c.stores = std::make_shared<mech3::Store>();
  mw1::MarketChoice m;
  m.own_shown = true;
  mw1::clamp_choice(c, m);
  const mw1::ScreenView wide = mw1::market_view(c, m, true), narrow = mw1::market_view(c, m, false);
  EXPECT(any_line_has(wide, "O_ROWS", "+3 S +12 A") && any_line_has(wide, "O_HEAD", "KITS"));
  EXPECT(!any_line_has(narrow, "O_ROWS", "+3 S") && !any_line_has(narrow, "O_HEAD", "KITS"));
  EXPECT(any_line_has(narrow, "STATUS", "+3 sinks, +12 armour") && !any_line_has(wide, "STATUS", "+3 sinks"));
  EXPECT(mw1::kits_words(mw1::Kits{}).empty() && mw1::kits_words({0, 4}) == "+0 S +4 A");
  // The SUPPLY button, above BACK.
  EXPECT(std::string(mw1::market_button_name(mw1::kMarketSupply)) == "SUPPLY" && mw1::kMarketBack == mw1::kMarketSupply + 1);
}
