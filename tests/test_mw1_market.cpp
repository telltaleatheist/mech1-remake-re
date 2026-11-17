// MW1's market (src/mw1_market.h): the prices by the tonnage line and the tier, Clan double; the stock's size by
// tier, its classes' shares, Clan mechs only at the Clan worlds and about one in ten there; the dealer's limit; buying,
// selling (put back to stock first: both directions, and refused when the stores lack a stock part), piloting, the
// last mech kept; the wrecks' cash on the same sale price; the hangar and the market saved (version 2) and a version 1
// save read.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "check.h"
#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
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
  // The Mad Cat (75 t, Clan) against the Orion (75 t, Inner Sphere): double.
  EXPECT(mw1::clan_mech(kMadCat) && !mw1::clan_mech(mw1::kPmOrion));
  EXPECT(mw1::buy_price(kMadCat, 1) == 2 * mw1::buy_price(mw1::kPmOrion, 1));
  std::printf("  Owens %lld to buy at tier 0, a new company's funds 1,000,000 to 1,255,000\n",
              static_cast<long long>(mw1::buy_price(kOwens, 0)));
  // The classes: never the Elemental.
  EXPECT(mw1::mech_class(kFirefly) == 0 && mw1::mech_class(kOwens) == 0 && mw1::mech_class(mw1::kPmCenturion) == 1 &&
         mw1::mech_class(mw1::kPmChampion) == 2 && mw1::mech_class(kAtlas) == 2 && !mw1::sold_mech(mech3::kElemental));
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
  // A wreck's cash is its PM chassis's sale price at this world, by its condition: a whole Jenner (the Owens) at
  // tier 0 pays what the market pays for an Owens there.
  mw1::Company c = company_with_stores();
  for (int32_t tier = 0; tier < 4; ++tier) {
    c.tier = tier;
    EXPECT(mw1::wreck_value(c, {mw1::kJenner, 100}) == (mw1::sell_price(kOwens, tier) + 500) / 1000);
  }
  c.tier = 0;
  EXPECT(mw1::wreck_value(c, {mw1::kJenner, 50}) == (2772000 / 2 + 500) / 1000);
  EXPECT(mw1::wreck_value(c, {mw1::kLocust, 20}) == 300);  // scrap: the Firefly's 30 t x 10
}

TEST(mw1_market_saved_and_a_version_1_save_read) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  c.planet_number = 51, c.tier = 3;
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
        line.rfind("market_", 0) == 0)
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
