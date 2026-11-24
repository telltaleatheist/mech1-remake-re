#include "mw1_market_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#include "mech3/mech_names.h"
#include "mw1_market.h"
#include "mw1_mech_icons.h"
#include "mw1_mech_renders.h"
#include "mw1_pilots.h"
#include "mw1_supply_screen.h"
#include "mw1_travel.h"
#include "mw1_upgrades.h"

namespace mw1::market_ui {
namespace {
const std::vector<ScreenButton> kMarketButtons = {
    {"BUY", "BUY"},           {"SELL", "SELL"},       {"LAB", "MECH LAB"}, {"SUPPLY", "SUPPLY"}, {"BACK", "BACK"},
    {"STOCK_UP", "", true},   {"STOCK_DOWN", "", true}, {"OWN_UP", "", true},  {"OWN_DOWN", "", true}};
constexpr size_t kNameWidth = 14;  // the lists' columns (the line font is fixed-width)
constexpr size_t kWhoWidth = 11;
// The lists' terminals (the layouts' PANEL4 and PANEL5): left and right, the arrows' column inside the right edge.
constexpr int32_t kListLeft[2] = {188, 200}, kListRight[2] = {188 + 436, 200 + 637}, kArrowColumn = 40;

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
std::string tons(int32_t mech) {
  char t[16];  // "%3dt " fits 8 for any 'Mech's tons; room for any int (GCC's -Wformat-truncation)
  std::snprintf(t, sizeof t, "%3dt ", static_cast<int>(tier0_price(mech) + kPriceOffset) / kPricePerTon);
  return t;
}
// A Clan mech starred.
std::string listed(int32_t mech) { return clan_mech(mech) ? mech_title(mech) + "*" : mech_title(mech); }
std::string price_column(int64_t price) {
  std::string p = thousands(price);
  if (p.size() < 10) p.insert(0, 10 - p.size(), ' ');
  return p;
}
// Who pilots owned mech i: you (0), the lancemate assigned to it, or nobody (a spare).
std::string pilot_of(const Company& c, int32_t i) {
  if (i == 0) return "YOU";
  const OwnedMech& o = c.hangar[static_cast<size_t>(i - 1)];
  return o.pilot ? pilot_title(o.pilot) : "spare";
}
// The first row shown, moved only as far as keeps `chosen` in view.
int32_t scrolled(int32_t top, int32_t chosen, int32_t count) {
  top = std::min(top, chosen);
  top = std::max(top, chosen - (kMarketRows - 1));
  return std::clamp(top, 0, std::max(0, count - kMarketRows));
}
}  // namespace

const char* market_button_name(int32_t id) { return id >= 1 && id <= kMarketButtonCount ? kMarketButtons[id - 1].name : ""; }

void clamp_choice(const Company& c, MarketChoice& m) {
  const int32_t stock = static_cast<int32_t>(c.market.stock.size()), owned = owned_count(c);
  m.stock = stock == 0 ? 0 : std::clamp(m.stock, 0, stock - 1);
  m.owned = std::clamp(m.owned, 0, owned - 1);
  m.stock_top = scrolled(m.stock_top, m.stock, stock);
  m.owned_top = scrolled(m.owned_top, m.owned, owned);
}

void deal_choice(const Company& c, MarketChoice& m, bool bought) {
  if (bought) m.owned = owned_count(c) - 1;
  m.own_shown = true;
  clamp_choice(c, m);
}

std::string buy_question(const Company& c, int32_t stock_index) {
  if (stock_index < 0 || stock_index >= static_cast<int32_t>(c.market.stock.size())) return "";
  const int32_t mech = c.market.stock[static_cast<size_t>(stock_index)];
  return "Buy the " + mech_title(mech) + " for " + thousands(market_buy_price(c, mech)) + " C-bills?";
}

std::string sell_question(const Company& c, int32_t owned_index) {
  const int32_t mech = owned_mech(c, owned_index);
  if (mech < 0) return "";
  std::string q = "Sell the " + mech_title(mech) + " for " + thousands(market_sell_price(c, mech)) + " C-bills?";
  if (owned_index == 0 && owned_count(c) > 1) q += "\n\nYou will pilot the " + mech_title(owned_mech(c, 1)) + ".";
  return q;
}

bool market_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row) {
  const int32_t l = wide ? 1 : 0;
  if (x < kListLeft[l] || x >= kListRight[l] - kArrowColumn) return false;
  for (int32_t list = 0; list < 2; ++list) {
    const int32_t first = kMarketListTop[list] + kMarketRowsFrom - 3;
    if (y < first || y >= first + kMarketRows * kMarketRowGap) continue;
    if (own) *own = list == 1;
    if (row) *row = (y - first) / kMarketRowGap;
    return true;
  }
  return false;
}

namespace {
bool g_market_live = false;
}  // namespace

void set_market_live(bool on) { g_market_live = on; }
bool market_live() { return g_market_live; }

// The banner's second line (FIXES.md item 64: no tier, no percentage; MW1 never said them): the world's landscape
// and population as MW1's travel panel gives them, from the player's exe; the date without it.
static std::string world_words(const Company& c) {
  const int32_t here = planet_index(c.planet_number);
  const WorldText t = here >= 0 ? world_text(here) : WorldText{};
  return t.landscape.empty() ? date_words(c.date) : upper(t.landscape) + " \xB7 POP. " + t.population;
}

int32_t market_shown_mech(const Company& c, const MarketChoice& m) {
  if ((m.own_shown && owned_count(c) > 0) || c.market.stock.empty()) return owned_mech(c, m.owned);
  return c.market.stock[static_cast<size_t>(m.stock)];
}

ScreenView market_view(const Company& c, const MarketChoice& m, bool wide) {
  ScreenView v;
  const Kits* chosen_kits = m.own_shown ? upgrades::kits_on(c, m.owned) : nullptr;
  std::string said = m.note;
  if (said.empty() && !wide && chosen_kits && !kits_words(*chosen_kits).empty())
    said = "Kits on the " + mech_title(owned_mech(c, m.owned)) + ": +" + std::to_string(chosen_kits->sinks) + " sinks, +" +
           std::to_string(chosen_kits->armour) + " armour.";
  v.blocks = {at_place("PLANET", {upper(c.planet)}),
              at_place("HOUSE", {"HOUSE " + upper(kHouses[c.planet_house].name) + " \xB7 THE MECH COMPLEX"}),
              at_place("PLACE", {thousands(c.funds) + " C-BILLS"}),
              at_place("PLACE_SUB", {world_words(c)}),
              at_place("STATUS_WORLD", {upper(c.planet)}),
              at_place("STATUS", {said.empty() ? "BUY from FOR SALE; SELL and MECH LAB on YOUR MECHS." : said})};
  v.panels.assign(kMarketPanelCount, true);
  // FIXES.md item 12: the chosen 'Mech under the buttons, drawn live in 3D (the panel left see-through); none without
  // the 3D (no stored picture of it: the renders are the loading screen's slot pictures only).
  const int32_t shown = market_shown_mech(c, m);
  v.pictures.assign(kMarketPanelCount, "");
  if (market_live() && shown >= 0) v.see_through.push_back({kMarketPanelMech, kRenderPanelW, kRenderPanelH});
  v.panels[kMarketPanelMechFrame] = v.panels[kMarketPanelMech] = market_live() && shown >= 0;

  // FOR SALE: the stock at the buying price; a price over the funds dimmed (FIXES.md item 41, the owner: what he can
  // afford at a glance): its row in S_ROWS_DIM's darker green, a blank line in its place in S_ROWS (and the other way).
  std::vector<std::string> stock, stock_dim;
  bool any_clan = false;
  for (int32_t i = m.stock_top; i < static_cast<int32_t>(c.market.stock.size()) && i < m.stock_top + kMarketRows; ++i) {
    const int32_t mech = c.market.stock[static_cast<size_t>(i)];
    const std::string row = std::string(i == m.stock ? "> " : "  ") + kOutlineRoom + padded(listed(mech), kNameWidth) +
                            tons(mech) + price_column(market_buy_price(c, mech));
    v.inline_pictures.push_back({"S_ROWS", i - m.stock_top, kRowIconH, "> ", row_icon_image(mech, i == m.stock)});
    const bool afford = market_buy_price(c, mech) <= c.funds;
    stock.push_back(afford ? row : ""), stock_dim.push_back(afford ? "" : row);
  }
  for (int32_t mech : c.market.stock) any_clan = any_clan || clan_mech(mech);
  if (stock.empty()) stock.push_back("  Nothing for sale here.");
  v.blocks.push_back(at_place("S_TITLE", {"FOR SALE"}));
  v.blocks.push_back(at_place("S_HEAD", {"  " + std::string(kOutlineRoom) + padded("MECH", kNameWidth) + "TONS      PRICE" + (any_clan ? "   * Clan" : "")}));
  v.blocks.push_back(at_place("S_ROWS", stock));
  if (std::any_of(stock_dim.begin(), stock_dim.end(), [](const std::string& l) { return !l.empty(); }))
    v.blocks.push_back(at_place("S_ROWS_DIM", stock_dim));

  // YOUR MECHS: who pilots each, its value.
  std::vector<std::string> owned;
  for (int32_t i = m.owned_top; i < owned_count(c) && i < m.owned_top + kMarketRows; ++i) {
    const int32_t mech = owned_mech(c, i);
    std::string line = std::string(i == m.owned ? "> " : "  ") + kOutlineRoom + padded(listed(mech), kNameWidth) + tons(mech) +
                       padded(pilot_of(c, i), kWhoWidth) + price_column(market_sell_price(c, mech));
    if (wide)
      if (const Kits* k = upgrades::kits_on(c, i)) line += "  " + kits_words(*k);
    owned.push_back(line);
    v.inline_pictures.push_back({"O_ROWS", i - m.owned_top, kRowIconH, "> ", row_icon_image(mech, i == m.owned)});
  }
  v.blocks.push_back(at_place("O_TITLE", {"YOUR MECHS"}));
  v.blocks.push_back(at_place("O_HEAD", {"  " + std::string(kOutlineRoom) + padded("MECH", kNameWidth) + "TONS " + padded("PILOT", kWhoWidth) + "     VALUE" + (wide ? "  KITS" : "")}));
  v.blocks.push_back(at_place("O_ROWS", owned));

  v.buttons.resize(kMarketButtonCount);
  auto on = [&](int32_t id, bool b) { v.buttons[static_cast<size_t>(id - 1)].on = b; };
  on(kMarketBuy, !c.market.stock.empty());
  on(kMarketSell, owned_count(c) > 1);
  on(kMarketStockUp, m.stock > 0);
  on(kMarketStockDown, m.stock + 1 < static_cast<int32_t>(c.market.stock.size()));
  on(kMarketOwnUp, m.owned > 0);
  on(kMarketOwnDown, m.owned + 1 < owned_count(c));
  return v;
}

MarketState::MarketState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Market State", kMarketFile, kMarketDialog, kMarketButtons, kMarketBack, std::move(pressed)) {}

void MarketState::resume(uint32_t arg) {
  if (resumed_) resumed_();
  Mw1State::resume(arg);
}

}  // namespace mw1::market_ui
