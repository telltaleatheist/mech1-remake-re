#include "mw1_market_screen.h"

#include <cctype>
#include <cstdio>
#include <vector>

#include "mech3/mech_names.h"
#include "mw1_market.h"

namespace mw1::market_ui {
namespace {
const std::vector<ScreenButton> kMarketButtons = {
    {"BUY", "BUY"},         {"SELL", "SELL"},        {"PILOT", "PILOT"},   {"BACK", "BACK"},
    {"STOCK_UP", ""},       {"STOCK_DOWN", ""},      {"OWN_UP", ""},       {"OWN_DOWN", ""}};
constexpr size_t kNameWidth = 14;  // the lists' columns (the line font is fixed-width)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
// A Clan mech starred.
std::string listed(int32_t mech) { return clan_mech(mech) ? mech_title(mech) + "*" : mech_title(mech); }
// "> Owens          35t  3,080,000": chosen or not, the name, the tons, a price.
std::string row(bool chosen, int32_t mech, int64_t price) {
  std::string n = listed(mech);
  if (n.size() < kNameWidth) n.append(kNameWidth - n.size(), ' ');
  char tons[8];
  std::snprintf(tons, sizeof tons, "%3dt ", static_cast<int>(tier0_price(mech) + kPriceOffset) / kPricePerTon);
  std::string p = thousands(price);
  if (p.size() < 10) p.insert(0, 10 - p.size(), ' ');
  return std::string(chosen ? "> " : "  ") + n + tons + p;
}
}  // namespace

const char* market_button_name(int32_t id) { return id >= 1 && id <= kMarketButtonCount ? kMarketButtons[id - 1].name : ""; }

void clamp_choice(const Company& c, MarketChoice& m) {
  const int32_t stock = static_cast<int32_t>(c.market.stock.size());
  m.stock = stock == 0 ? 0 : m.stock < 0 ? 0 : m.stock >= stock ? stock - 1 : m.stock;
  const int32_t owned = owned_count(c);
  m.owned = m.owned < 0 ? 0 : m.owned >= owned ? owned - 1 : m.owned;
}

ScreenView market_view(const Company& c, const MarketChoice& m) {
  ScreenView v;
  std::vector<std::string> stock, owned;
  for (size_t i = 0; i < c.market.stock.size(); ++i)
    stock.push_back(row(static_cast<int32_t>(i) == m.stock, c.market.stock[i], buy_price(c.market.stock[i], c.tier)));
  if (stock.empty()) stock.push_back("Nothing for sale here.");
  for (int32_t i = 0; i < owned_count(c); ++i) owned.push_back(row(i == m.owned, owned_mech(c, i), sell_price(owned_mech(c, i), c.tier)));
  bool any_clan = false;
  for (int32_t mech : c.market.stock) any_clan = any_clan || clan_mech(mech);
  // The world's terms under the lists: the funds, the tier's prices, the dealer's limit.
  const int32_t mult = kTierMultipliers[c.tier];
  std::vector<std::string> terms = {
      "Funds: " + thousands(c.funds) + " C-bills",
      "Tier " + std::to_string(c.tier) + " world: buying at " + std::to_string(mult * 10) + "%, selling at " +
          std::to_string((mult - 1) * 10) + "% of the base price",
      c.market.limit < 0 ? std::string("The dealer buys any number of mechs.")
                         : "The dealer buys " + std::to_string(c.market.limit - c.market.sold) + " more mechs until the market changes."};
  if (any_clan) terms.push_back("* Clan: twice the price.");
  v.blocks = {{24, 14, kPlaqueFont, {upper(c.planet)}},
              {24, 32, kPlaqueSmallFont, {"HOUSE " + upper(kHouses[c.planet_house].name) + ": MARKET"}},
              {30, 72, kTitleFont, {"FOR SALE"}},
              {30, 96, kLineFont, stock},
              {398, 72, kTitleFont, {"YOUR MECHS"}},
              {398, 96, kLineFont, owned},
              {398, 260, kLineFont, {"Piloted: " + mech_title(c.mech)}},
              {30, 360, kLineFont, terms},
              {30, 428, kNoteFont, {m.note}}};
  v.buttons.resize(kMarketButtonCount);
  v.buttons[kMarketBuy - 1].on = !c.market.stock.empty();
  v.buttons[kMarketSell - 1].on = owned_count(c) > 1;
  v.buttons[kMarketPilot - 1].on = m.owned != 0;
  v.buttons[kMarketStockUp - 1].on = m.stock > 0;
  v.buttons[kMarketStockDown - 1].on = m.stock + 1 < static_cast<int32_t>(c.market.stock.size());
  v.buttons[kMarketOwnUp - 1].on = m.owned > 0;
  v.buttons[kMarketOwnDown - 1].on = m.owned + 1 < owned_count(c);
  return v;
}

MarketState::MarketState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Market State", kMarketFile, kMarketDialog, kMarketButtons, kMarketBack, std::move(pressed)) {}

}  // namespace mw1::market_ui
