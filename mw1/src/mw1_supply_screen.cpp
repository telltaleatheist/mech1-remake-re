#include "mw1_supply_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_market.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"
#include "mw1_weapon_data.h"

namespace mw1::supply_ui {
namespace {
const std::vector<ScreenButton> kSupplyButtons = {
    {"BUY", "BUY"},         {"SELL", "SELL"},           {"ASSIGN", "ASSIGN"},         {"BACK", "BACK"},
    {"ADD_SINK", "+SINK"},  {"TAKE_SINK", "-SINK"},     {"ADD_ARMOUR", "+ARMOUR"},    {"TAKE_ARMOUR", "-ARMOUR"},
    {"SALE_UP", "", true},  {"SALE_DOWN", "", true},    {"OWN_UP", "", true},         {"OWN_DOWN", "", true}};
// The lists' columns (the line font is fixed-width): 4:3's terminal holds 46 of its characters before the arrows.
constexpr size_t kItemWidth = 22, kCountWidth = 6, kWhoWidth = 11;
// The lists' terminals (the layouts' PANEL4 and PANEL5): left and right, the arrows' column inside the right edge.
constexpr int32_t kListLeft[2] = {188, 200}, kListRight[2] = {188 + 436, 200 + 637}, kArrowColumn = 40;
constexpr const char* kRule = "  - - - - - - - - - - - - - - - - - - - -";

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string padded(std::string s, size_t w) {
  if (s.size() > w) s.resize(w);
  s.append(w - s.size(), ' ');
  return s;
}
std::string right(std::string s, size_t w) {
  if (s.size() < w) s.insert(0, w - s.size(), ' ');
  return s;
}
// PM's table name in words: "Laser_Medium" as "Laser Medium".
std::string spaced(const char* n) {
  std::string s = n ? n : "";
  std::replace(s.begin(), s.end(), '_', ' ');
  return s;
}
const char* equipment_title(int32_t kind) {
  static const char* const kTitles[] = {"Anti-Missile",   "Armour",     "Ferro Armour", "Fire Control", "Probe",
                                        "CASE",           "C3",         "C3 Slave",     "ECM",          "Heat Sink",
                                        "Double Sink",    "Jump Jets",  "MASC",         "TAG",          "Targeting Comp"};
  constexpr int32_t n = static_cast<int32_t>(sizeof kTitles / sizeof kTitles[0]);
  return kind >= 0 && kind < n ? kTitles[kind] : "Equipment";
}
const mech3::Config* loadout_of(const Company& c, int32_t owned) {
  if (owned == 0) return c.loadout ? c.loadout.get() : stock_loadout(c.mech);
  const OwnedMech& o = c.hangar[static_cast<size_t>(owned - 1)];
  return o.loadout ? o.loadout.get() : stock_loadout(o.mech);
}
std::string pilot_words(const Company& c, int32_t owned) {
  if (owned == 0) return "YOU";
  const OwnedMech& o = c.hangar[static_cast<size_t>(owned - 1)];
  return o.pilot ? pilot_title(o.pilot) : "spare";
}
bool choosable(const SupplyRow& r) { return r.kind != LineKind::kRule; }
// A list's chosen row kept on a choosable one: forward from `at`, else back; 0 when there's none.
int32_t settled(const std::vector<SupplyRow>& rows, int32_t at) {
  const int32_t n = static_cast<int32_t>(rows.size());
  if (n == 0) return 0;
  at = std::clamp(at, 0, n - 1);
  for (int32_t i = at; i < n; ++i)
    if (choosable(rows[static_cast<size_t>(i)])) return i;
  for (int32_t i = at; i >= 0; --i)
    if (choosable(rows[static_cast<size_t>(i)])) return i;
  return 0;
}
int32_t scrolled(int32_t top, int32_t chosen, int32_t count) {
  top = std::min(top, chosen);
  top = std::max(top, chosen - (kSupplyRows - 1));
  return std::clamp(top, 0, std::max(0, count - kSupplyRows));
}
}  // namespace

const char* supply_button_name(int32_t id) { return id >= 1 && id <= kSupplyButtonCount ? kSupplyButtons[id - 1].name : ""; }

std::vector<OwnedType> owned_weapon_types(const Company& c) {
  std::vector<OwnedType> out;
  auto add = [&](int32_t weapon, int32_t tech) {
    if (weapon < 0 || weapon >= mech3::weapon_count() || (tech != mech3::kClan && tech != mech3::kInnerSphere)) return;
    const OwnedType t{weapon, tech};
    if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
  };
  for (int32_t o = 0; o < owned_count(c); ++o)
    if (const mech3::Config* cfg = loadout_of(c, o))
      for (const mech3::ConfigItem* it : cfg->items)
        if (it && it->kind() == dealer::kWeapon) add(it->id(), static_cast<const mech3::WeaponItem*>(it)->tech);
  for (const dealer::Ware& w : dealer::wares())
    if (w.kind == dealer::kWeapon && dealer::in_stores(c, w) > 0) add(w.id, w.tech);
  for (const WeaponSteps& s : c.upgrades.steps) add(s.weapon, s.tech);  // bought for a type since sold: still theirs
  std::sort(out.begin(), out.end(), [](const OwnedType& a, const OwnedType& b) {
    return a.weapon != b.weapon ? a.weapon < b.weapon : a.tech > b.tech;  // Inner Sphere before Clan
  });
  return out;
}

int32_t type_range_most(int32_t weapon, int32_t tech) {
  (void)tech;  // one definition for both techs (the lab's two blocks): one RANGE
  const float r = weapon_data::range(weapon);  // the engine's definition in a mission, else MW1's read at the hub
  return r > 0.0f ? upgrades::range_steps_most(r) : -1;
}

std::vector<SupplyRow> sale_rows(const Company& c) {
  std::vector<SupplyRow> rows;
  for (int32_t i = 0; i < static_cast<int32_t>(c.dealer.stock.size()); ++i) {
    const DealerItem& d = c.dealer.stock[static_cast<size_t>(i)];
    SupplyRow r;
    r.kind = LineKind::kWare, r.index = i, r.ware = {d.kind, d.id, d.tech};
    rows.push_back(r);
  }
  if (!rows.empty()) rows.push_back(SupplyRow{});
  for (upgrades::Kit k : {upgrades::Kit::kSink, upgrades::Kit::kArmour}) {
    SupplyRow r;
    r.kind = LineKind::kKit, r.kit = k;
    rows.push_back(r);
  }
  for (const OwnedType& t : owned_weapon_types(c))
    for (upgrades::Step s : {upgrades::Step::kPower, upgrades::Step::kRange}) {
      SupplyRow r;
      r.kind = LineKind::kStep, r.weapon = t.weapon, r.tech = t.tech, r.step = s;
      rows.push_back(r);
    }
  return rows;
}

std::vector<SupplyRow> own_rows(const Company& c, bool assigning) {
  std::vector<SupplyRow> rows;
  if (assigning) {
    for (int32_t o = 0; o < owned_count(c); ++o) {
      SupplyRow r;
      r.kind = LineKind::kMech, r.index = o;
      rows.push_back(r);
    }
    return rows;
  }
  for (const dealer::Ware& w : dealer::wares())
    if (dealer::in_stores(c, w) > 0) {
      SupplyRow r;
      r.kind = LineKind::kWare, r.ware = w;
      rows.push_back(r);
    }
  if (!rows.empty()) rows.push_back(SupplyRow{});
  for (upgrades::Kit k : {upgrades::Kit::kSink, upgrades::Kit::kArmour}) {
    SupplyRow r;
    r.kind = LineKind::kKit, r.kit = k;
    rows.push_back(r);
  }
  for (const WeaponSteps& s : c.upgrades.steps)
    if (s.power > 0 || s.range > 0) {
      SupplyRow r;
      r.kind = LineKind::kStep, r.weapon = s.weapon, r.tech = s.tech;
      rows.push_back(r);
    }
  return rows;
}

void clamp_choice(const Company& c, SupplyChoice& s) {
  const std::vector<SupplyRow> sale = sale_rows(c), own = own_rows(c, s.assigning);
  s.sale = settled(sale, s.sale);
  s.own = settled(own, s.own);
  s.sale_top = scrolled(s.sale_top, s.sale, static_cast<int32_t>(sale.size()));
  s.own_top = scrolled(s.own_top, s.own, static_cast<int32_t>(own.size()));
}

void step_supply(const Company& c, SupplyChoice& s, bool own, int32_t by) {
  const std::vector<SupplyRow> rows = own ? own_rows(c, s.assigning) : sale_rows(c);
  int32_t& at = own ? s.own : s.sale;
  for (int32_t i = at + by; i >= 0 && i < static_cast<int32_t>(rows.size()); i += by)
    if (choosable(rows[static_cast<size_t>(i)])) {
      at = i;
      break;
    }
  s.own_chosen = own;
  s.note.clear();
  clamp_choice(c, s);
}

const SupplyRow* chosen_sale(const Company& c, const SupplyChoice& s, std::vector<SupplyRow>* rows) {
  *rows = sale_rows(c);
  if (s.sale < 0 || s.sale >= static_cast<int32_t>(rows->size())) return nullptr;
  const SupplyRow* r = &(*rows)[static_cast<size_t>(s.sale)];
  return choosable(*r) ? r : nullptr;
}

const SupplyRow* chosen_own(const Company& c, const SupplyChoice& s, std::vector<SupplyRow>* rows) {
  *rows = own_rows(c, s.assigning);
  if (s.own < 0 || s.own >= static_cast<int32_t>(rows->size())) return nullptr;
  const SupplyRow* r = &(*rows)[static_cast<size_t>(s.own)];
  return choosable(*r) ? r : nullptr;
}

// The names PM's lab shows (Mech3Msg's, by the tables' message ids), else the table's name in words.
static std::string weapon_words(int32_t weapon) {
  const int32_t id = mech3::weapon_name_message(weapon);
  const char* shown = id >= 0 ? mech3::pilot_hooks().message(id) : nullptr;
  return shown && *shown ? shown : spaced(mech3::weapon_name(weapon));
}
static std::string equipment_words(int32_t kind) {
  const int32_t id = mech3::equip_name_message(kind);
  const char* shown = id >= 0 ? mech3::pilot_hooks().message(id) : nullptr;
  return shown && *shown ? shown : equipment_title(kind);
}

std::string type_title(int32_t weapon, int32_t tech) { return weapon_words(weapon) + (tech == mech3::kClan ? "*" : ""); }

std::string ware_title(const dealer::Ware& w) {
  if (w.kind == dealer::kWeapon) return type_title(w.id, w.tech);
  if (w.kind == dealer::kAmmo) return "Ammo " + weapon_words(w.id);
  return equipment_words(w.id) + (w.tech == mech3::kClan ? "*" : "");
}

std::string steps_words(const WeaponSteps& s) {
  std::string out;
  if (s.power > 0) out += "+" + std::to_string(s.power) + " power";
  if (s.range > 0) out += std::string(out.empty() ? "" : ", ") + "+" + std::to_string(s.range) + " range";
  return out.empty() ? "none" : out;
}

std::string kits_words(const Kits& k) {
  if (k.sinks <= 0 && k.armour <= 0) return "";
  return "+" + std::to_string(k.sinks) + " S +" + std::to_string(k.armour) + " A";
}

int64_t row_price(const Company& c, const SupplyRow& r) {
  switch (r.kind) {
    case LineKind::kWare: return dealer::buy_price(r.ware, c.tier);
    case LineKind::kKit: return upgrades::kit_price(c, r.kit);
    case LineKind::kStep: return upgrades::step_price(c, r.weapon, r.tech);
    default: return 0;
  }
}

bool row_at_most(const Company& c, const SupplyRow& r) {
  if (r.kind == LineKind::kKit) return upgrades::kits_bought(c, r.kit) >= upgrades::kCountMost;
  if (r.kind != LineKind::kStep) return false;
  const WeaponSteps* s = upgrades::steps_of(c, r.weapon, r.tech);
  const int32_t have = s ? (r.step == upgrades::Step::kPower ? s->power : s->range) : 0;
  if (have >= upgrades::kCountMost) return true;
  const int32_t most = r.step == upgrades::Step::kRange ? type_range_most(r.weapon, r.tech) : -1;
  return most >= 0 && have >= most;
}

std::string buy_question(const Company& c, const SupplyRow& r) {
  const std::string price = thousands(row_price(c, r));
  if (r.kind == LineKind::kKit)
    return std::string(r.kit == upgrades::Kit::kSink ? "Buy a heat sink kit" : "Buy an armour kit") + " for " + price +
           " C-bills?\n\nKits are the company's for good: they are never sold back.";
  if (r.kind == LineKind::kStep)
    return std::string("Buy a ") + (r.step == upgrades::Step::kPower ? "power" : "range") + " step for every " +
           type_title(r.weapon, r.tech) + " for " + price + " C-bills?\n\nSteps are never sold back.";
  return "";
}

std::string row_words(const Company& c, const SupplyRow& r) {
  char line[160];
  switch (r.kind) {
    case LineKind::kWare: {
      const int32_t have = dealer::in_stores(c, r.ware);
      if (r.ware.kind == dealer::kAmmo)
        std::snprintf(line, sizeof line, "%s: a ton, %d rounds. In your stores: %d.", ware_title(r.ware).c_str(),
                      dealer::ammo_lot(r.ware.id), have);
      else
        std::snprintf(line, sizeof line, "%s%s: %g tons. In your stores: %d.", ware_title(r.ware).c_str(),
                      r.ware.tech == mech3::kClan ? " (Clan)" : "", static_cast<double>(dealer::ware_tons(r.ware)), have);
      return line;
    }
    case LineKind::kKit: {
      const bool sink = r.kit == upgrades::Kit::kSink;
      std::snprintf(line, sizeof line, "%s Spare: %d.", sink ? "Sink kit: +1 heat sink, no tons, no slots." : "Armour kit: +1 armour past the cap.",
                    sink ? c.upgrades.store.sinks : c.upgrades.store.armour);
      return line;
    }
    case LineKind::kStep: {
      const WeaponSteps* s = upgrades::steps_of(c, r.weapon, r.tech);
      const std::string has = s ? steps_words(*s) : "none";
      if (r.step == upgrades::Step::kRange)
        std::snprintf(line, sizeof line, "+%d m range, every %s. Has: %s.", upgrades::kRangeStepMetres,
                      type_title(r.weapon, r.tech).c_str(), has.c_str());
      else
        std::snprintf(line, sizeof line, "+%d%% damage, every %s. Has: %s.", upgrades::kPowerStepPercent,
                      type_title(r.weapon, r.tech).c_str(), has.c_str());
      return line;
    }
    case LineKind::kMech: {
      const Kits* k = upgrades::kits_on(c, r.index);
      std::snprintf(line, sizeof line, "%s: +%d sinks, +%d armour. Spare: %d sinks, %d armour.",
                    mech_title(owned_mech(c, r.index)).c_str(), k ? k->sinks : 0, k ? k->armour : 0, c.upgrades.store.sinks,
                    c.upgrades.store.armour);
      return line;
    }
    default: return "";
  }
}

bool supply_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row) {
  const int32_t l = wide ? 1 : 0;
  if (x < kListLeft[l] || x >= kListRight[l] - kArrowColumn) return false;
  for (int32_t list = 0; list < 2; ++list) {
    const int32_t first = kSupplyListTop[list] + kSupplyRowsFrom - 3;
    if (y < first || y >= first + kSupplyRows * kSupplyRowGap) continue;
    if (own) *own = list == 1;
    if (row) *row = (y - first) / kSupplyRowGap;
    return true;
  }
  return false;
}

// The banner's second line: the world's landscape and population, as the Mech Complex's.
static std::string world_words(const Company& c) {
  const int32_t here = planet_index(c.planet_number);
  const WorldText t = here >= 0 ? world_text(here) : WorldText{};
  return t.landscape.empty() ? date_words(c.date) : upper(t.landscape) + " \xB7 POP. " + t.population;
}

ScreenView supply_view(const Company& c, const SupplyChoice& s) {
  std::vector<SupplyRow> sale, own;
  const SupplyRow* sale_row = chosen_sale(c, s, &sale);
  const SupplyRow* own_row = chosen_own(c, s, &own);
  const SupplyRow* told = s.own_chosen ? own_row : sale_row;
  ScreenView v;
  v.blocks = {at_place("PLANET", {upper(c.planet)}),
              at_place("HOUSE", {"HOUSE " + upper(kHouses[c.planet_house].name) + " \xB7 SUPPLY"}),
              at_place("PLACE", {thousands(c.funds) + " C-BILLS"}),
              at_place("PLACE_SUB", {world_words(c)}),
              at_place("STATUS_WORLD", {upper(c.planet)}),
              at_place("STATUS", {!s.note.empty() ? s.note : told ? row_words(c, *told) : "Nothing to choose."})};
  v.panels.assign(kSupplyPanelCount, true);

  // FOR SALE: the dealer's lines, a rule, the upgrades; what the funds can't pay dimmed (the Mech Complex's way).
  std::vector<std::string> lines, dim;
  for (int32_t i = s.sale_top; i < static_cast<int32_t>(sale.size()) && i < s.sale_top + kSupplyRows; ++i) {
    const SupplyRow& r = sale[static_cast<size_t>(i)];
    const std::string cursor = i == s.sale ? "> " : "  ";
    std::string row;
    bool afford = true;
    if (r.kind == LineKind::kRule) {
      row = kRule;
    } else {
      std::string what, count;
      if (r.kind == LineKind::kWare) {
        what = ware_title(r.ware), count = "x" + std::to_string(c.dealer.stock[static_cast<size_t>(r.index)].count);
      } else if (r.kind == LineKind::kKit) {
        what = r.kit == upgrades::Kit::kSink ? "SINK KIT" : "ARMOUR KIT";
        count = "+" + std::to_string(upgrades::kits_bought(c, r.kit));
      } else {
        what = type_title(r.weapon, r.tech) + (r.step == upgrades::Step::kPower ? " +POWER" : " +RANGE");
        const WeaponSteps* st = upgrades::steps_of(c, r.weapon, r.tech);
        count = "+" + std::to_string(st ? (r.step == upgrades::Step::kPower ? st->power : st->range) : 0);
      }
      const bool most = row_at_most(c, r);
      const int64_t price = row_price(c, r);
      afford = !most && price <= c.funds;
      row = cursor + padded(what, kItemWidth) + padded(count, kCountWidth) + right(most ? "MOST" : thousands(price), 12);
    }
    lines.push_back(afford ? row : ""), dim.push_back(afford ? "" : row);
  }
  if (lines.empty()) lines.push_back("  Nothing for sale here.");
  v.blocks.push_back(at_place("S_TITLE", {"FOR SALE"}));
  v.blocks.push_back(at_place("S_HEAD", {"  " + padded("ITEM", kItemWidth) + padded("HAS", kCountWidth) + right("PRICE", 12)}));
  v.blocks.push_back(at_place("S_ROWS", lines));
  if (std::any_of(dim.begin(), dim.end(), [](const std::string& l) { return !l.empty(); })) v.blocks.push_back(at_place("S_ROWS_DIM", dim));

  // The lower terminal: YOUR STORES, or (ASSIGN) the 'Mechs and their kits.
  std::vector<std::string> rows;
  for (int32_t i = s.own_top; i < static_cast<int32_t>(own.size()) && i < s.own_top + kSupplyRows; ++i) {
    const SupplyRow& r = own[static_cast<size_t>(i)];
    const std::string cursor = i == s.own ? "> " : "  ";
    if (r.kind == LineKind::kRule) {
      rows.push_back(kRule);
    } else if (r.kind == LineKind::kMech) {
      const Kits* k = upgrades::kits_on(c, r.index);
      const std::string kits = k ? kits_words(*k) : "";
      rows.push_back(cursor + padded(mech_title(owned_mech(c, r.index)), 14) + padded(pilot_words(c, r.index), kWhoWidth) +
                     (kits.empty() ? "-" : kits));
    } else if (r.kind == LineKind::kWare) {
      rows.push_back(cursor + padded(ware_title(r.ware), kItemWidth) + padded("x" + std::to_string(dealer::in_stores(c, r.ware)), kCountWidth) +
                     right(dealer::kBuysBack ? thousands(dealer::sell_price(r.ware, c.tier)) : "-", 12));
    } else if (r.kind == LineKind::kKit) {
      const bool sink = r.kit == upgrades::Kit::kSink;
      rows.push_back(cursor + padded(sink ? "SINK KITS" : "ARMOUR KITS", kItemWidth) +
                     padded("x" + std::to_string(sink ? c.upgrades.store.sinks : c.upgrades.store.armour), kCountWidth) + right("spare", 12));
    } else {
      const WeaponSteps* st = upgrades::steps_of(c, r.weapon, r.tech);
      rows.push_back(cursor + padded(type_title(r.weapon, r.tech), kItemWidth - 6) + (st ? steps_words(*st) : "none"));
    }
  }
  if (rows.empty()) rows.push_back("  Nothing in your stores.");
  if (s.assigning) {
    v.blocks.push_back(at_place("O_TITLE", {"YOUR MECHS' KITS"}));
    v.blocks.push_back(at_place("O_HEAD", {"  " + padded("MECH", 14) + padded("PILOT", kWhoWidth) + "KITS"}));
  } else {
    v.blocks.push_back(at_place("O_TITLE", {"YOUR STORES"}));
    v.blocks.push_back(at_place("O_HEAD", {"  " + padded("ITEM", kItemWidth) + padded("HAS", kCountWidth) + right("VALUE", 12)}));
  }
  v.blocks.push_back(at_place("O_ROWS", rows));

  v.buttons.resize(kSupplyButtonCount);
  auto set = [&](int32_t id, bool there, bool on) {
    ButtonState& b = v.buttons[static_cast<size_t>(id - 1)];
    b.there = there, b.on = on;
  };
  set(kSupplyBuy, true, sale_row && !row_at_most(c, *sale_row));
  set(kSupplySell, true, !s.assigning && dealer::kBuysBack && own_row && own_row->kind == LineKind::kWare);
  set(kSupplyAssign, true, true);
  if (s.assigning) v.buttons[static_cast<size_t>(kSupplyAssign - 1)].words = "STORES";
  const Kits* on_mech = s.assigning && own_row && own_row->kind == LineKind::kMech ? upgrades::kits_on(c, own_row->index) : nullptr;
  set(kSupplyAddSink, s.assigning, on_mech && c.upgrades.store.sinks > 0);
  set(kSupplyTakeSink, s.assigning, on_mech && on_mech->sinks > 0);
  set(kSupplyAddArmour, s.assigning, on_mech && c.upgrades.store.armour > 0);
  set(kSupplyTakeArmour, s.assigning, on_mech && on_mech->armour > 0);
  auto can_step = [&](bool own_list, int32_t by) {
    const std::vector<SupplyRow>& rs = own_list ? own : sale;
    for (int32_t i = (own_list ? s.own : s.sale) + by; i >= 0 && i < static_cast<int32_t>(rs.size()); i += by)
      if (choosable(rs[static_cast<size_t>(i)])) return true;
    return false;
  };
  set(kSupplySaleUp, true, can_step(false, -1));
  set(kSupplySaleDown, true, can_step(false, 1));
  set(kSupplyOwnUp, true, can_step(true, -1));
  set(kSupplyOwnDown, true, can_step(true, 1));
  return v;
}

SupplyState::SupplyState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Supply State", kSupplyFile, kSupplyDialog, kSupplyButtons, kSupplyBack, std::move(pressed)) {}

}  // namespace mw1::supply_ui
