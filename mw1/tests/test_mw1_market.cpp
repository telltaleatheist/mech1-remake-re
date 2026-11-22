// MW1's market (src/mw1_market.h): the prices by the tonnage line and the tier, Clan as Inner Sphere (FIXES 115); the stock's size by
// tier, its classes' shares, Clan mechs only at the Clan worlds and about one in ten there; the dealer's limit; buying,
// selling (put back to stock first: both directions, and refused when the stores lack a stock part), piloting, the
// last mech kept; the wrecks' cash on the same sale price; the hangar and the market saved (version 2) and a version 1
// save read.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>

#include "check.h"
#include "mw1_market_screen.h"
#include "mw1_mech_icons.h"
#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_repairs.h"
#include "mw1_market.h"
#include "mw1_enemies.h"
#include "mw1_save.h"

namespace {
constexpr int32_t kAtlas = 1, kMadCat = mech3::kMadCat, kFirefly = mw1::kPmFirefly, kOwens = mw1::kPmOwens;
constexpr int32_t kErMediumLaser = 5, kMachineGun = 0x1f, kHeatSink = 10;

int32_t count_weapon(const mech3::Store& s, int32_t weapon) {
  int32_t n = 0;
  for (const mech3::ConfigItem* it : s.items)
    if (it->kind() == 1 && it->id() == weapon) n += it->count;
  return n;
}
int32_t count_equipment(const mech3::Store& s, int32_t kind) {
  int32_t n = 0;
  for (const mech3::ConfigItem* it : s.items)
    if (it->kind() == 3 && it->id() == kind) n += it->count;
  return n;
}
mw1::Company company_with_stores() {
  mw1::Company c;
  c.stores = std::make_shared<mech3::Store>();
  c.funds = 1000000;
  return c;
}
uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
  return h;
}
}  // namespace

TEST(mw1_market_prices_by_tonnage_tier_and_clan) {
  // 95 x tons - 245 thousand at tier 0; x multiplier x 100 to buy, x (multiplier - 1) x 100 to sell.
  EXPECT(mw1::tier0_price(kOwens) == 3080 && mw1::tier0_price(kFirefly) == 2605 && mw1::tier0_price(kAtlas) == 9255);
  EXPECT(mw1::buy_price(kOwens, 0) == 3080000 && mw1::sell_price(kOwens, 0) == 2772000);
  EXPECT(mw1::buy_price(kOwens, 3) == 4620000 && mw1::sell_price(kOwens, 3) == 4312000);
  // Buy at a tier 0 world, sell at a tier 3 one: 40% (research/economy.md).
  EXPECT(mw1::sell_price(kAtlas, 3) * 10 == mw1::buy_price(kAtlas, 0) * 14);
  // FIXES 115: a Clan chassis and an Inner Sphere one of the same tons price the same at every tier, bought, sold and
  // as a wreck: the Mad Cat and the Orion (75 t), the Puma and the Owens (35 t), the Daishi and the Atlas (100 t).
  EXPECT(mw1::clan_mech(kMadCat) && !mw1::clan_mech(mw1::kPmOrion) && mw1::market::kClanPriceFactor == 1);
  for (auto [clan, is] : {std::pair{kMadCat, int32_t{mw1::kPmOrion}}, std::pair{int32_t{mw1::kPmPuma}, kOwens},
                          std::pair{int32_t{mw1::kPmDaishi}, kAtlas}}) {
    EXPECT(mw1::clan_mech(clan) && !mw1::clan_mech(is));
    for (int32_t t = 0; t < mw1::kTierCount; ++t) {
      EXPECT(mw1::buy_price(clan, t) == mw1::buy_price(is, t) && mw1::sell_price(clan, t) == mw1::sell_price(is, t));
      mw1::Company w;
      w.tier = t, w.repair_premium = 18;
      EXPECT(mw1::wreck_value(w, mw1::Wreck{mw1::kSlot75, 60, 3000, clan}) == mw1::wreck_value(w, mw1::Wreck{mw1::kSlot75, 60, 3000, is}) ||
             mw1::kWreckPay != mw1::WreckPay::kChassis);
    }
  }
  std::printf("  Owens %lld to buy at tier 0, a new company's funds 1,000,000 to 1,255,000\n",
              static_cast<long long>(mw1::buy_price(kOwens, 0)));
  // The classes: never the Elemental.
  EXPECT(mw1::mech_class(kFirefly) == 0 && mw1::mech_class(kOwens) == 0 && mw1::mech_class(mw1::kPmCenturion) == 1 &&
         mw1::mech_class(mw1::kPmChampion) == 2 && mw1::mech_class(kAtlas) == 2 && !mw1::sold_mech(mech3::kElemental));
}

// FIXES 108 (Owen: by design, as MW1 had it): MW1's own formula pinned, every chassis at every tier. Buying is the tier 0
// price times the tier's percent (100/120/130/150), selling 10 points less (90/110/120/140), floored to 1,000 C-bills,
// a Clan 'Mech's times kClanPriceFactor (1 since FIXES 115); so within one world selling always pays less than buying, and buying cheap at a low tier and
// selling at a dearer world pays (MW1's trade, kept).
TEST(mw1_market_mw1s_formula_pinned) {
  int32_t checked = 0, arbitrage = 0;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    if (!mw1::market::sold_mech(m)) continue;
    const int64_t base = int64_t{mw1::market::tier0_price(m)} * 1000, clan = mw1::market::clan_mech(m) ? mw1::market::kClanPriceFactor : 1;
    EXPECT(mw1::market::tier0_price(m) == mw1::market::kPricePerTon * static_cast<int32_t>(mech3::max_mass(m)) - mw1::market::kPriceOffset);
    for (int32_t t = 0; t < mw1::kTierCount; ++t) {
      const int64_t pct = mw1::kTierMultipliers[t] * 10;
      const int64_t buy = mw1::market::buy_price(m, t), sell = mw1::market::sell_price(m, t);
      EXPECT(buy == base * pct / 100 / 1000 * 1000 * clan);
      EXPECT(sell == base * (pct - 10) / 100 / 1000 * 1000 * clan);
      EXPECT(sell < buy);
      EXPECT(mw1::market::buy_percent(t) == pct && mw1::market::sell_percent(t) == pct - 10);
      arbitrage += sell > mw1::market::buy_price(m, 0);
      ++checked;
    }
  }
  std::printf("  %d prices checked; %d sales above a tier 0 purchase (MW1's trade)\n", checked, arbitrage);
  EXPECT(checked > 0 && arbitrage > 0);
}

TEST(mw1_market_stock_by_tier_classes_and_clan_worlds) {
  mw1::Rng rng(7);
  for (int32_t tier = 0; tier < 4; ++tier) {
    mw1::Company c = company_with_stores();
    c.tier = tier, c.planet_number = 118;  // Robinson: not a Clan world
    int32_t most = 0, classes[3] = {}, all = 0, limits_seen = 0;
    for (int i = 0; i < 2000; ++i) {
      mw1::refresh_market(c, rng);
      most = std::max(most, static_cast<int32_t>(c.market.stock.size()));
      EXPECT(static_cast<int32_t>(c.market.stock.size()) <= mw1::kStockMax / (tier + 1));
      for (int32_t m : c.market.stock) {
        EXPECT(mw1::sold_mech(m) && !mw1::clan_mech(m));
        ++classes[mw1::mech_class(m)], ++all;
      }
      // The dealer: no limit below tier 2; rand(0..4 - tier) + 1 from it.
      if (tier < 2) EXPECT(c.market.limit == -1);
      else {
        EXPECT(c.market.limit >= 1 && c.market.limit <= 5 - tier);
        limits_seen |= 1 << c.market.limit;
      }
    }
    std::printf("  tier %d: up to %d mechs; light %d%%, medium %d%%, heavy %d%%\n", tier, most, classes[0] * 100 / all,
                classes[1] * 100 / all, classes[2] * 100 / all);
    EXPECT(most == mw1::kStockMax / (tier + 1));
    EXPECT(classes[0] * 100 / all >= 55 && classes[0] * 100 / all <= 65 && classes[1] * 100 / all >= 25 &&
           classes[1] * 100 / all <= 35 && classes[2] * 100 / all >= 6 && classes[2] * 100 / all <= 14);
    if (tier == 2) EXPECT(limits_seen == (1 << 1 | 1 << 2 | 1 << 3));
    if (tier == 3) EXPECT(limits_seen == (1 << 1 | 1 << 2));
  }
  // A Clan world (Anywhere, Steiner, tier 3): about one in ten Clan, of the slot's class.
  mw1::Company c = company_with_stores();
  c.tier = 3, c.planet_number = 51;
  EXPECT(mw1::clan_world(51) && !mw1::clan_world(118));
  int32_t clan = 0, all = 0;
  for (int i = 0; i < 5000; ++i) {
    mw1::refresh_market(c, rng);
    for (int32_t m : c.market.stock) clan += mw1::clan_mech(m) ? 1 : 0, ++all;
  }
  std::printf("  Anywhere: %d of %d Clan\n", clan, all);
  EXPECT(clan * 100 / all >= 7 && clan * 100 / all <= 13);
}

TEST(mw1_market_buy_sell_pilot_and_the_last_mech) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 0;
  c.market.stock = {kFirefly, kAtlas};
  // Not enough money; then enough: the Firefly into the hangar, out of the stock.
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kNoFunds && c.hangar.empty());
  c.funds = 3000000;
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kDone && c.funds == 3000000 - 2605000 && c.hangar.size() == 1 && c.hangar[0].mech == kFirefly);
  EXPECT(c.market.stock.size() == 1 && c.market.stock[0] == kAtlas && !c.hangar[0].loadout);
  EXPECT(mw1::buy(c, 5) == mw1::Deal::kNoSuch);
  // PILOT: the commander takes the Firefly; the Owens to the hangar with its loadout.
  c.loadout = std::make_shared<mech3::Config>(kOwens, "owens");
  EXPECT(mw1::pilot(c, 1) == mw1::Deal::kDone && c.mech == kFirefly && !c.loadout && c.hangar[0].mech == kOwens && c.hangar[0].loadout);
  EXPECT(mw1::pilot(c, 1) == mw1::Deal::kDone && c.mech == kOwens && c.hangar[0].mech == kFirefly);
  // SELL the Firefly (on its stock loadout: the stores untouched) for its price; it joins the stock.
  const int32_t items = c.stores->count();
  const int64_t before = c.funds;
  EXPECT(mw1::sell(c, 1) == mw1::Deal::kDone && c.funds == before + 2344000 && c.hangar.empty());
  EXPECT(c.stores->count() == items && c.market.stock.back() == kFirefly && c.market.sold == 1);
  // The last mech: never.
  EXPECT(mw1::sell(c, 0) == mw1::Deal::kLastMech && c.mech == kOwens);
  // The dealer's limit (a tier 2 world's).
  c.funds = 10000000, c.market.limit = 1, c.market.sold = 1;
  EXPECT(mw1::buy(c, 1) == mw1::Deal::kDone && mw1::sell(c, 1) == mw1::Deal::kDealerFull && c.hangar.size() == 1);
  // Selling the piloted one (on its stock loadout): the commander moves to the next.
  c.market.limit = -1, c.loadout.reset();
  EXPECT(mw1::sell(c, 0) == mw1::Deal::kDone && c.mech == kFirefly && c.hangar.empty());
}

// FIXES.md item 53: the most mechs owned (kOwnedMost: MW1's 12, Owen's 30; the piloted one included): BUY refused at it after
// the funds' check, nothing paid or taken from the stock; a sale makes room again. A save past it still loads (and
// still refuses). The stores' items clamped at kStoreMost.
TEST(mw1_market_the_hangar_holds_the_most) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 0, c.funds = 100000000;
  c.market.stock = {kFirefly, kFirefly};
  for (int32_t i = 1; i < mw1::kOwnedMost - 1; ++i) {
    mw1::OwnedMech o;
    o.mech = kFirefly;
    c.hangar.push_back(o);
  }
  EXPECT(mw1::owned_count(c) == mw1::kOwnedMost - 1 && mw1::buy(c, 0) == mw1::Deal::kDone && mw1::owned_count(c) == mw1::kOwnedMost);
  const int64_t funds = c.funds;
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kHangarFull && c.funds == funds && c.market.stock.size() == 1);
  EXPECT(std::string(mw1::deal_words(mw1::Deal::kHangarFull)).find("full") != std::string::npos);
  c.funds = 0;
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kNoFunds);  // MW1's order: the funds first
  c.funds = funds;
  EXPECT(mw1::sell(c, 1) == mw1::Deal::kDone && mw1::buy(c, 0) == mw1::Deal::kDone);
  // YOUR MECHS scrolls: the last of the most chosen, the list's top moves to show it within its rows, and the list
  // shows it marked, a full page of rows.
  mw1::MarketChoice m;
  m.owned = mw1::kOwnedMost - 1;
  mw1::clamp_choice(c, m);
  EXPECT(m.owned == mw1::kOwnedMost - 1 && m.owned_top > 0 && m.owned_top <= m.owned && m.owned < m.owned_top + mw1::kMarketRows);
  EXPECT(m.owned_top == mw1::kOwnedMost - mw1::kMarketRows);
  {
    const mw1::ScreenView v = mw1::market_view(c, m);
    const std::vector<std::string>* rows = nullptr;
    for (const mw1::TextBlock& b : v.blocks)
      if (b.at && std::string(b.at) == "O_ROWS") rows = &b.lines;
    EXPECT(rows && static_cast<int32_t>(rows->size()) == mw1::kMarketRows && rows->back().rfind("> ", 0) == 0);
  }
  // An old save with fifteen: read, nothing taken away; BUY still refused.
  for (int32_t i = 0; i < 3; ++i) {
    mw1::OwnedMech o;
    o.mech = kAtlas;
    c.hangar.push_back(o);
  }
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(mw1::save_text(c, nullptr), back, k, &has, &why) && mw1::owned_count(back) == mw1::kOwnedMost + 3);
  back.market.stock = {kFirefly};
  EXPECT(mw1::buy(back, 0) == mw1::Deal::kHangarFull);
  // The stores: an item past the most is cut to it; under it, left.
  mech3::Store s;
  s.add_ammo(kErMediumLaser, mw1::kStoreMost + 50);
  s.add_weapon(kErMediumLaser, mech3::kInnerSphere);
  mw1::clamp_stores(s);
  int32_t most = 0, least = mw1::kStoreMost;
  for (const mech3::ConfigItem* it : s.items) most = std::max(most, it->count), least = std::min(least, it->count);
  EXPECT(most == mw1::kStoreMost && least == 1);
}

TEST(mw1_market_sale_puts_the_mech_back_to_stock) {
  // A stand-in stock loadout for the Owens: two ER medium lasers and a heat sink.
  mech3::Config stock(kOwens, "standard");
  stock.add_weapon(kErMediumLaser, mech3::kInnerSphere);
  stock.add_weapon(kErMediumLaser, mech3::kInnerSphere);
  stock.add_equipment(kHeatSink, mech3::kInnerSphere, 1);
  const mw1::StockLoadout stock_of = [&](int32_t m) { return m == kOwens ? &stock : nullptr; };
  mw1::Company c = company_with_stores();
  c.mech = kFirefly;
  mw1::OwnedMech o;
  o.mech = kOwens;
  // Refitted in the lab: one laser taken off, a machine gun fitted.
  o.loadout = std::make_shared<mech3::Config>(kOwens, "lab");
  o.loadout->add_weapon(kErMediumLaser, mech3::kInnerSphere);
  o.loadout->add_weapon(kMachineGun, mech3::kInnerSphere);
  c.hangar.push_back(o);
  c.stores->add_weapon(kErMediumLaser, mech3::kInnerSphere);  // the laser it lacks is here; the heat sink isn't
  // Refused: the stores don't hold its stock heat sink. Nothing changes.
  const int64_t funds = c.funds;
  EXPECT(mw1::sell(c, 1, stock_of) == mw1::Deal::kRefit && c.funds == funds && c.hangar.size() == 1);
  EXPECT(count_weapon(*c.stores, kErMediumLaser) == 1 && count_weapon(*c.stores, kMachineGun) == 0);
  std::printf("  refused: %s\n", mw1::deal_words(mw1::Deal::kRefit));
  // With a heat sink in the stores: sold. The machine gun (beyond stock) into the stores; the laser and the heat sink it
  // lacked out of them.
  c.stores->add_equipment(kHeatSink, mech3::kInnerSphere, 1);
  EXPECT(mw1::sell(c, 1, stock_of) == mw1::Deal::kDone && c.funds == funds + 2772000 && c.hangar.empty());
  EXPECT(count_weapon(*c.stores, kMachineGun) == 1 && count_weapon(*c.stores, kErMediumLaser) == 0 &&
         count_equipment(*c.stores, kHeatSink) == 0);
}

TEST(mw1_market_wrecks_pay_the_markets_sale_price) {
  // A wreck's cash is its PM chassis's sale price at this world, by its condition: a whole 35 t slot (the Owens) at
  // tier 0 pays what the market pays for an Owens there.
  mw1::Company c = company_with_stores();
  for (int32_t tier = 0; tier < 4; ++tier) {
    c.tier = tier;
    EXPECT(mw1::wreck_value(c, {mw1::kSlot35, 100}) == (mw1::sell_price(kOwens, tier) + 500) / 1000);
  }
  c.tier = 0, c.repair_premium = 18;
  EXPECT(mw1::wreck_value(c, {mw1::kSlot35, 50, 5000}) == (2772000 - 31169 + 500) / 1000);  // less half its repairs
  // Never below scrap (the Firefly's 30 t x 10): only a repair dearer than the sale, here a 10,000% premium.
  c.repair_premium = 10000;
  EXPECT(mw1::wreck_value(c, {mw1::kSlot20, 0, mw1::kWhole}) == 300);
}

TEST(mw1_market_saved_and_a_version_1_save_read) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  c.planet_number = 51, c.tier = 3, c.planet = "Anywhere", c.planet_house = mw1::kSteiner;  // a Clan world
  mw1::refresh_market(c, rng);
  c.market.stock = {kFirefly, kMadCat}, c.market.sold = 1;
  mw1::OwnedMech o;
  o.mech = kFirefly;
  c.hangar.push_back(o);
  o.mech = kAtlas;
  c.hangar.push_back(o);
  const std::string text = mw1::save_text(c, nullptr);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  EXPECT(back.planet_number == 51 && back.hangar.size() == 2 && back.hangar[1].mech == kAtlas && !back.hangar[1].loadout);
  EXPECT(back.market.stock == c.market.stock && back.market.sold == 1 && back.market.limit == c.market.limit && back.market.rolled);
  EXPECT(mw1::save_text(back, nullptr) == text);
  // Version 1: the same company without the new lines, under its own check. Read as a hangar of none, the world's
  // number from its name, the market not rolled yet (the loop rolls it).
  std::string v1;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    const std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("planet_number=", 0) == 0 || line.rfind("hangar", 0) == 0 ||
        line.rfind("market_", 0) == 0 || line.rfind("calendar=", 0) == 0)
      continue;
    v1 += (line.rfind("version=", 0) == 0 ? std::string("version=1") : line) + "\n";
  }
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(v1));
  mw1::Company old;
  old.planet = "x";
  why.clear();
  EXPECT(mw1::load_text(v1 + check, old, k, &has, &why));
  std::printf("  version 1: %s\n", why.empty() ? "read" : why.c_str());
  EXPECT(old.hangar.empty() && old.market.stock.empty() && !old.market.rolled && old.funds == c.funds);
}

// The lists' rows by the mouse (FIXES.md item 13): FOR SALE's terminal over YOUR MECHS', each's rows from 52 under
// its top, 15 apart, in either layout; the arrows' column, the titles and the buttons down the left are no row.
TEST(mw1_market_rows_by_the_mouse) {
  bool own = true;
  int32_t row = -1;
  const int32_t stock = mw1::kMarketListTop[0] + mw1::kMarketRowsFrom, owned = mw1::kMarketListTop[1] + mw1::kMarketRowsFrom;
  const int32_t gap = mw1::kMarketRowGap;
  EXPECT(mw1::market_row_at(210, stock, false, &own, &row) && !own && row == 0);
  EXPECT(mw1::market_row_at(210, stock + 3 * gap + 5, false, &own, &row) && !own && row == 3);
  EXPECT(mw1::market_row_at(210, owned + gap, false, &own, &row) && own && row == 1);
  EXPECT(mw1::market_row_at(700, owned, true, &own, &row) && own && row == 0);  // the wide terminals are wider
  EXPECT(!mw1::market_row_at(700, owned, false, &own, &row));                    // past 4:3's
  EXPECT(!mw1::market_row_at(600, stock, false, &own, &row));                    // 4:3's arrows
  EXPECT(!mw1::market_row_at(815, stock, true, &own, &row));                     // the wide ones'
  EXPECT(!mw1::market_row_at(100, stock, false, &own, &row));                    // the buttons down the left
  EXPECT(!mw1::market_row_at(210, mw1::kMarketListTop[1] + 12, false, &own, &row));  // YOUR MECHS' title
  EXPECT(!mw1::market_row_at(210, stock + mw1::kMarketRows * gap, false, &own, &row));  // under FOR SALE's last row
}

// The Mech Complex's one page (Owen's shape): BUY, SELL, MECH LAB and BACK down the left (no REPAIR or RELOAD,
// FIXES.md item 59); the lists
// scrolled to keep the chosen row in view, kMarketRows of them shown.
TEST(mw1_market_complex_page) {
  mw1::Company c;
  c.planet = "Test";
  c.funds = 5000000;
  for (int i = 0; i < 12; ++i) c.market.stock.push_back(i % 2 ? mw1::kPmOwens : mw1::kPmFirefly);
  mw1::MarketChoice m;
  auto on = [](const mw1::ScreenView& v, int32_t id) { return v.buttons[static_cast<size_t>(id - 1)].on; };
  mw1::clamp_choice(c, m);
  mw1::ScreenView v = mw1::market_view(c, m);
  EXPECT(on(v, mw1::kMarketBuy) && !on(v, mw1::kMarketSell) && v.buttons.size() == 9);  // 1.1.0: SUPPLY too
  for (int32_t id = 1; id <= mw1::kMarketButtonCount; ++id)
    EXPECT(std::string(mw1::market_button_name(id)) != "REPAIR" && std::string(mw1::market_button_name(id)) != "RELOAD");
  EXPECT(on(v, mw1::kMarketLab) && on(v, mw1::kMarketBack) && !on(v, mw1::kMarketStockUp) && on(v, mw1::kMarketStockDown));
  EXPECT(v.panels[mw1::kMarketPanelStock] && v.panels[mw1::kMarketPanelOwned]);
  m.stock = 11;
  mw1::clamp_choice(c, m);
  EXPECT(m.stock_top == 12 - mw1::kMarketRows);  // scrolled to the last
  m.stock = 2;
  mw1::clamp_choice(c, m);
  EXPECT(m.stock_top == 2);  // and back up as far as the chosen one
  m.stock = 20, m.owned = 5;
  mw1::clamp_choice(c, m);
  EXPECT(m.stock == 11 && m.owned == 0 && m.owned_top == 0);
}

// The funds on the Mech Complex (FIXES.md item 41, Owen: he couldn't see what he could afford): on the plaque, the
// sum after a purchase and after a sale the moment the page is drawn again; a price over the funds dimmed (its row in
// S_ROWS_DIM, a blank in S_ROWS), an affordable one not.
TEST(mw1_market_funds_shown_and_unaffordable_dimmed) {
  mw1::Company c = company_with_stores();
  c.planet = "Test", c.mech = kOwens, c.tier = 0;
  c.market.stock = {kFirefly, kAtlas};
  c.funds = 3000000;  // the Firefly (2,605,000) affordable, the Atlas not
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  auto block = [](const mw1::ScreenView& v, const char* at) -> const std::vector<std::string>* {
    for (const mw1::TextBlock& b : v.blocks)
      if (b.at && std::string(b.at) == at) return &b.lines;
    return nullptr;
  };
  auto plaque = [&](const mw1::ScreenView& v) { return block(v, "PLACE") ? block(v, "PLACE")->front() : std::string(); };
  mw1::ScreenView v = mw1::market_view(c, m);
  EXPECT(plaque(v) == "3,000,000 C-BILLS");
  const std::vector<std::string>* rows = block(v, "S_ROWS");
  const std::vector<std::string>* dim = block(v, "S_ROWS_DIM");
  EXPECT(rows && dim && rows->size() == 2 && dim->size() == 2);
  if (rows && dim && rows->size() == 2 && dim->size() == 2) {
    EXPECT(!(*rows)[0].empty() && (*dim)[0].empty());  // the Firefly: affordable
    EXPECT((*rows)[1].empty() && !(*dim)[1].empty());  // the Atlas: dimmed
  }
  // Bought: the plaque says what's left at once; now nothing is affordable, both rows dimmed.
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kDone);
  mw1::clamp_choice(c, m);
  v = mw1::market_view(c, m);
  EXPECT(plaque(v) == "395,000 C-BILLS");
  dim = block(v, "S_ROWS_DIM");
  EXPECT(dim && dim->size() == 1 && !(*dim)[0].empty());
  // Sold: the funds up again.
  EXPECT(mw1::sell(c, 1) == mw1::Deal::kDone);
  v = mw1::market_view(c, m);
  EXPECT(plaque(v) == "2,739,000 C-BILLS");
  // Rich: nothing dimmed, no dim block.
  c.funds = 100000000;
  v = mw1::market_view(c, m);
  EXPECT(block(v, "S_ROWS_DIM") == nullptr);
}

// The rows' mech outlines (research/hires-2d.md B2 item 1, Owen 2026-11-18): one after each shown row's cursor, on
// that row's line in FOR SALE and YOUR MECHS (a dimmed row's in S_ROWS still), the chosen row's lit, the others dim.
TEST(mw1_market_rows_show_their_outlines) {
  mw1::Company c = company_with_stores();
  c.planet = "Test", c.mech = kOwens;
  for (int i = 0; i < 12; ++i) c.market.stock.push_back(i % 2 ? kOwens : kFirefly);
  c.funds = 2700000;  // the Firefly affordable, the Owens' row dimmed
  mw1::MarketChoice m;
  m.stock = 9;
  mw1::clamp_choice(c, m);
  const mw1::ScreenView v = mw1::market_view(c, m);
  int32_t stock = 0, owned = 0, lit = 0;
  for (const mw1::ScreenView::InlinePicture& p : v.inline_pictures) {
    const std::string at = p.at ? p.at : "";
    const int32_t i = p.line + (at == "S_ROWS" ? m.stock_top : m.owned_top);
    const int32_t mech = at == "S_ROWS" ? c.market.stock[static_cast<size_t>(i)] : kOwens;
    const bool chosen = at == "S_ROWS" ? i == m.stock : i == m.owned;
    EXPECT(p.prefix == "> " && p.h == mw1::kRowIconH && p.image == mw1::row_icon_image(mech, chosen));
    stock += at == "S_ROWS", owned += at == "O_ROWS", lit += chosen;
  }
  EXPECT(stock == mw1::kMarketRows && owned == 1 && lit == 2);
  EXPECT(mw1::row_icon_image(kOwens, true) != mw1::row_icon_image(kOwens, false));
}

// FIXES.md item 58 (Owen 2026-11-18): the live panel's 'Mech follows the chosen row. FOR SALE's Atlas chosen, BUY the
// Firefly by index: YOUR MECHS' new last row chosen and shown, the panel's the Firefly (it kept the FOR SALE row's).
// SELL it: the row in its place (the commander's), shown.
TEST(mw1_market_panel_follows_buy_and_sell) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 0, c.funds = 10000000;
  c.market.stock = {kFirefly, kAtlas};
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  EXPECT(mw1::market_shown_mech(c, m) == kFirefly);  // the first entry: FOR SALE's first
  m.stock = 1;
  EXPECT(mw1::market_shown_mech(c, m) == kAtlas);
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kDone);
  mw1::deal_choice(c, m, true);
  EXPECT(m.own_shown && mw1::owned_mech(c, m.owned) == kFirefly && mw1::market_shown_mech(c, m) == kFirefly);
  m.own_shown = false;  // FOR SALE chosen again, then SELL (YOUR MECHS' Firefly)
  EXPECT(mw1::sell(c, m.owned) == mw1::Deal::kDone);
  mw1::deal_choice(c, m, false);
  EXPECT(m.own_shown && m.owned == 0 && mw1::market_shown_mech(c, m) == kOwens);
  // Every deal leaves the panel on YOUR MECHS' chosen row.
  EXPECT(mw1::market_shown_mech(c, m) == mw1::owned_mech(c, m.owned));
}

// FIXES.md item 60: BUY and SELL ask first, the reason at once when the deal can't be. can_buy / can_sell answer as
// buy / sell would and change nothing; the questions in Owen's words.
TEST(mw1_market_asks_before_a_deal) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 0, c.funds = 1000;
  c.market.stock = {kFirefly};
  const mw1::Company before = c;
  EXPECT(mw1::can_buy(c, 0) == mw1::Deal::kNoFunds && mw1::can_buy(c, 3) == mw1::Deal::kNoSuch);
  EXPECT(mw1::can_sell(c, 0) == mw1::Deal::kLastMech);
  c.funds = 3000000;
  EXPECT(mw1::can_buy(c, 0) == mw1::Deal::kDone && c.funds == 3000000 && c.hangar.empty() && c.market.stock.size() == 1);
  EXPECT(mw1::buy_question(c, 0) == "Buy the Firefly for 2,605,000 C-bills?");
  EXPECT(mw1::buy_question(c, 5).empty());
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kDone);
  // Selling: the hangar's one, or the piloted one (who you pilot next named); nothing changed by asking.
  const int32_t items = c.stores->count();
  const int64_t funds = c.funds;
  EXPECT(mw1::can_sell(c, 1) == mw1::Deal::kDone && mw1::can_sell(c, 0) == mw1::Deal::kDone);
  EXPECT(c.funds == funds && c.hangar.size() == 1 && c.stores->count() == items && c.market.sold == 0);
  EXPECT(mw1::sell_question(c, 1) == "Sell the Firefly for 2,344,000 C-bills?");
  EXPECT(mw1::sell_question(c, 0).rfind("Sell the Owens for ", 0) == 0 &&
         mw1::sell_question(c, 0).find("You will pilot the Firefly.") != std::string::npos);
  c.market.limit = 0;
  EXPECT(mw1::can_sell(c, 1) == mw1::Deal::kDealerFull);
  (void)before;
}

// FIXES 135: a little variety in 'Mech prices. One shift per model per market refresh, a bell curve within +-5%
// (permille), shared by buying and selling at that world; the market screen, BUY, SELL, their checks and a wreck's
// value all take it; saved with the market (version 12), none from an older save.
namespace {
std::string with_commas(int64_t v) {
  std::string s = std::to_string(v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return s;
}
}  // namespace

TEST(mw1_market_shift_range_and_spread) {
  mw1::Rng rng(135);
  constexpr int kRolls = 40000;
  int64_t sum = 0, sq = 0;
  int lo = 0, hi = 0, within_sd = 0, far = 0;
  for (int i = 0; i < kRolls; ++i) {
    const int32_t s = mw1::roll_shift(rng);
    EXPECT(s >= -mw1::kShiftMost && s <= mw1::kShiftMost);
    sum += s, sq += int64_t{s} * s, lo = std::min(lo, s), hi = std::max(hi, s);
    within_sd += s >= -17 && s <= 17, far += s <= -40 || s >= 40;
  }
  const double mean = static_cast<double>(sum) / kRolls, sd = std::sqrt(static_cast<double>(sq) / kRolls - mean * mean);
  std::printf("  %d rolls: mean %.2f permille, sd %.1f, %d..%d, %.1f%% within +-1.7%%, %.2f%% beyond +-4%%\n", kRolls, mean, sd,
              lo, hi, 100.0 * within_sd / kRolls, 100.0 * far / kRolls);
  // Three rolls of -17..17: sd about 17.5 permille; a bell, not flat (a flat +-5% would put 34% within +-1.7%).
  EXPECT(std::abs(mean) < 0.6 && sd > 16 && sd < 19 && lo <= -40 && hi >= 40);
  EXPECT(within_sd > kRolls * 60 / 100 && far < kRolls * 3 / 100);
  // A refresh gives every model the market deals in a shift, and they vary.
  mw1::Company c;
  c.tier = 1, c.planet_number = 10;
  mw1::refresh_market(c, rng);
  EXPECT(c.market.shift.size() == static_cast<size_t>(mech3::kMechCount));
  int distinct = 0;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    if (!mw1::sold_mech(m)) EXPECT(mw1::market_shift(c, m) == 0);
    distinct += mw1::sold_mech(m) && mw1::market_shift(c, m) != mw1::market_shift(c, kOwens);
  }
  EXPECT(distinct > 0);
}

TEST(mw1_market_shift_buy_and_sell_share_it) {
  // Every model, tier and shift: the shift 0 is the base price; buying and selling scale together, on 1,000s, and
  // selling stays below buying (no profit from buying and selling back at one world).
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    if (!mw1::sold_mech(m)) continue;
    for (int32_t t = 0; t < 4; ++t) {
      EXPECT(mw1::buy_price(m, t, 0) == mw1::buy_price(m, t) && mw1::sell_price(m, t, 0) == mw1::sell_price(m, t));
      for (int32_t s = -mw1::kShiftMost; s <= mw1::kShiftMost; s += 5) {
        const int64_t buy = mw1::buy_price(m, t, s), sell = mw1::sell_price(m, t, s);
        EXPECT(buy % 1000 == 0 && sell % 1000 == 0 && sell < buy);
        EXPECT(std::llabs(buy * 1000 - mw1::buy_price(m, t) * (1000 + s)) <= 1000 * 1000);  // the shift, floored
      }
    }
  }
  EXPECT(mw1::buy_price(kOwens, 0, 50) == 3234000 && mw1::sell_price(kOwens, 0, 50) == 2910000);
  EXPECT(mw1::buy_price(kOwens, 0, -50) == 2926000 && mw1::sell_price(kOwens, 0, -50) == 2633000);
  // The market's: one shift per model for both directions.
  mw1::Company c;
  c.tier = 2, c.market.shift.assign(mech3::kMechCount, 0);
  c.market.shift[static_cast<size_t>(kOwens)] = 37;
  EXPECT(mw1::market_buy_price(c, kOwens) == mw1::buy_price(kOwens, 2, 37) &&
         mw1::market_sell_price(c, kOwens) == mw1::sell_price(kOwens, 2, 37));
  EXPECT(mw1::market_sell_price(c, kOwens) < mw1::market_buy_price(c, kOwens));
  EXPECT(mw1::market_buy_price(c, kFirefly) == mw1::buy_price(kFirefly, 2));  // no shift: the base
  // The travel map's readout stays the tier's share.
  EXPECT(mw1::buy_percent(2) == 130 && mw1::sell_percent(2) == 120);
}

TEST(mw1_market_shift_shown_is_charged) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 1, c.funds = 10000000;
  c.market.stock = {kFirefly};
  c.market.shift.assign(mech3::kMechCount, 0);
  c.market.shift[static_cast<size_t>(kFirefly)] = -43;
  c.market.shift[static_cast<size_t>(kOwens)] = 29;
  const int64_t buy = mw1::market_buy_price(c, kFirefly);
  EXPECT(buy != mw1::buy_price(kFirefly, 1));
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  auto block = [](const mw1::ScreenView& v, const char* at) -> const std::vector<std::string>* {
    for (const mw1::TextBlock& b : v.blocks)
      if (b.at && std::string(b.at) == at) return &b.lines;
    return nullptr;
  };
  mw1::ScreenView v = mw1::market_view(c, m);
  const std::vector<std::string>* rows = block(v, "S_ROWS");
  EXPECT(rows && !rows->empty() && rows->front().find(with_commas(buy)) != std::string::npos);
  EXPECT(mw1::buy_question(c, 0).find(with_commas(buy)) != std::string::npos);
  // Affordable at the shifted price exactly, not a C-bill less.
  c.funds = buy - 1;
  EXPECT(mw1::can_buy(c, 0) == mw1::Deal::kNoFunds);
  c.funds = buy;
  EXPECT(mw1::can_buy(c, 0) == mw1::Deal::kDone && mw1::buy(c, 0) == mw1::Deal::kDone && c.funds == 0);
  // Selling the Owens (the hangar's Firefly piloted next): YOUR MECHS, the question and the funds all agree.
  const int64_t sell = mw1::market_sell_price(c, kOwens);
  EXPECT(sell != mw1::sell_price(kOwens, 1));
  mw1::clamp_choice(c, m);
  v = mw1::market_view(c, m);
  rows = block(v, "O_ROWS");
  EXPECT(rows && !rows->empty() && rows->front().find(with_commas(sell)) != std::string::npos);
  EXPECT(mw1::sell_question(c, 0).find(with_commas(sell)) != std::string::npos);
  EXPECT(mw1::sell(c, 0, [](int32_t) { return static_cast<const mech3::Config*>(nullptr); }) == mw1::Deal::kDone &&
         c.funds == sell);
}

TEST(mw1_market_shift_wreck_agrees) {
  mw1::Company c = company_with_stores();
  c.market.shift.assign(mech3::kMechCount, 0);
  for (int32_t tier = 0; tier < 4; ++tier)
    for (int32_t s : {-50, -12, 0, 31, 50}) {
      c.tier = tier, c.market.shift[static_cast<size_t>(kOwens)] = s;
      EXPECT(mw1::wreck_value(c, {mw1::kSlot35, 100}) == (mw1::market_sell_price(c, kOwens) + 500) / 1000);
    }
}

TEST(mw1_market_shift_saved) {
  mw1::Company c = company_with_stores();
  c.mech = kOwens, c.tier = 1, c.planet_number = 10;
  mw1::Rng rng(7);
  mw1::refresh_market(c, rng);
  const std::string text = mw1::save_text(c, nullptr);
  EXPECT(text.find("\nmarket_shift=") != std::string::npos);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  EXPECT(back.market.shift == c.market.shift);  // (a bare company's other lines needn't re-save byte for byte)
  // A version 11 save (before the shifts): read, no shift, the base prices.
  std::string v11;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("market_shift=", 0) == 0) continue;
    if (line.rfind("version=", 0) == 0) line = "version=11";
    v11 += line + "\n";
  }
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(v11));
  mw1::Company old;
  EXPECT(mw1::load_text(v11 + check, old, k, &has, &why));
  std::printf("  version 11: %s\n", why.empty() ? "read" : why.c_str());
  EXPECT(old.market.shift.empty() && mw1::market_shift(old, kOwens) == 0 &&
         mw1::market_buy_price(old, kOwens) == mw1::buy_price(kOwens, 1));
  // A version 12 save without the line, or with a shift past 5%: refused.
  std::string bad = v11;
  bad.replace(0, bad.find('\n'), "version=" + std::to_string(mw1::kSaveVersion));
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(bad));
  EXPECT(!mw1::load_text(bad + check, old, k, &has, &why) && why.find("shift") != std::string::npos);
  bad += "market_shift=0,51\n";
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(bad));
  EXPECT(!mw1::load_text(bad + check, old, k, &has, &why) && why.find("shift") != std::string::npos);
}
