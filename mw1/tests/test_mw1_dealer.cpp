// FIXES 122 (src/mw1_dealer.h): the arms dealer's model. Every item FIXES 122 lists is a ware; rarity by tech and
// kind, and where each rarity is stocked; the stock rolled by world and date, from its own seed (the campaign's dice
// untouched); across every world and a run of dates every ware turns up, and the rare and uncommon ones only where
// they belong; buying into the stores and selling back out at the tier's prices; the dealer's line in save 13.
// MW1_PRICE_TABLE=1 prints every ware's prices with PM's weapon weights (from PM's weapons.zrd, when it's there).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
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
#include "mw1_save.h"
#include "mw1_travel.h"
#include "pm_weapon_labs.h"

namespace dl = mw1::dealer;

namespace {
constexpr int32_t kIs = mech3::kInnerSphere, kClan = mech3::kClan;
constexpr int32_t kRobinson = 118;  // a tier 0 world, not a Clan world

int32_t weapon(const char* name) { return mech3::weapon_index(name); }
int32_t equipment(const char* name) { return mech3::equip_kind(name); }
bool has_ware(const dl::Ware& w) {
  for (const dl::Ware& x : dl::wares())
    if (x == w) return true;
  return false;
}

// A weapon lab of our own for the prices (mech3::set_weapon_lab): a few weapons' weights, the rest none.
const mech3::WeaponLab* fake_lab(const char* name) {
  static const mech3::WeaponLab kPpc{{7.0f, 7.0f}, {3, 3}, 0.0f};
  static const mech3::WeaponLab kErPpc{{6.0f, 7.0f}, {2, 3}, 0.0f};
  static const mech3::WeaponLab kSmall{{0.5f, 1.0f}, {1, 1}, 0.0f};
  static const mech3::WeaponLab kSrm4{{1.0f, 2.0f}, {1, 1}, 0.01f};  // 100 rounds a ton
  if (std::strcmp(name, "PPC") == 0) return &kPpc;
  if (std::strcmp(name, "ERPPC") == 0) return &kErPpc;
  if (std::strcmp(name, "ERLaser_Small") == 0) return &kSmall;
  if (std::strcmp(name, "SRM4") == 0) return &kSrm4;
  return nullptr;
}
struct FakeLab {
  FakeLab() { mech3::set_weapon_lab(fake_lab); }
  ~FakeLab() { mech3::set_weapon_lab(nullptr); }
};

mw1::Company company_at(int32_t tier, int32_t planet_number) {
  mw1::Company c;
  c.tier = tier, c.planet_number = planet_number, c.funds = 10000000;
  c.stores = std::make_shared<mech3::Store>();
  return c;
}
uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
  return h;
}
std::string rechecked(std::string text) {
  text = text.substr(0, text.rfind("check="));
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(text));
  return text + check;
}
}  // namespace

TEST(mw1_dealer_every_listed_item_a_ware_and_its_rarity) {
  // FIXES 122's lists: the items no fielded loadout carries, and those only Alternates and Exotics carry.
  struct Want {
    int32_t kind;
    const char* name;
    int32_t tech;
  };
  const Want kListed[] = {
      {dl::kWeapon, "AC2", kIs},          {dl::kWeapon, "AC5", kIs},           {dl::kWeapon, "LB2XAC", kIs},
      {dl::kWeapon, "LB5XAC", kIs},       {dl::kWeapon, "LB20XAC", kIs},       {dl::kWeapon, "UltraAC2", kIs},
      {dl::kWeapon, "UltraAC10", kIs},    {dl::kWeapon, "UltraAC20", kIs},     {dl::kWeapon, "PPC", kIs},
      {dl::kWeapon, "ERLaser_Small", kIs}, {dl::kWeapon, "HeavyLaser_Small", kClan}, {dl::kWeapon, "SRM2", kIs},
      {dl::kWeapon, "StreakSRM4", kIs},   {dl::kWeapon, "StreakSRM6", kIs},    {dl::kWeapon, "StreakSRM4", kClan},
      {dl::kWeapon, "MRM10", kIs},        {dl::kWeapon, "MRM40", kIs},         {dl::kWeapon, "TBOLT10", kIs},
      {dl::kWeapon, "TBOLT15", kIs},      {dl::kWeapon, "LRM5", kClan},        {dl::kWeapon, "MGUNLT", kClan},
      {dl::kEquipment, "masc", kIs},      {dl::kEquipment, "ecm", kIs},        {dl::kEquipment, "tc", kIs},
      {dl::kEquipment, "ams", kClan},     {dl::kEquipment, "ecm", kClan},      {dl::kEquipment, "c3", kIs},
      {dl::kEquipment, "tag", kClan},     {dl::kEquipment, "sink", kClan},     {dl::kWeapon, "MGUNH", kClan},
      {dl::kEquipment, "tc", kClan}};
  int32_t missing = 0;
  for (const Want& w : kListed) {
    const int32_t id = w.kind == dl::kWeapon ? weapon(w.name) : equipment(w.name);
    if (id < 0 || !has_ware({w.kind, id, w.tech})) {
      std::printf("  not a ware: %s %s\n", w.name, w.tech == kClan ? "(Clan)" : "");
      ++missing;
    }
  }
  EXPECT(missing == 0);
  std::printf("  %zu wares; FIXES 122's %zu listed items all among them\n", dl::wares().size(), sizeof kListed / sizeof kListed[0]);
  // Never plain armour; no ware twice.
  std::set<std::string> names;
  for (const dl::Ware& w : dl::wares()) {
    EXPECT(!(w.kind == dl::kEquipment && w.id == equipment("armor")));
    EXPECT(names.insert(dl::ware_name(w)).second);
  }
  // Rarity: Clan rare; the unfielded Inner Sphere uncommon; the rest common; ammunition as its weapon's.
  EXPECT(dl::rarity({dl::kWeapon, weapon("ERPPC"), kClan}) == dl::Rarity::kRare);
  EXPECT(dl::rarity({dl::kWeapon, weapon("PPC"), kIs}) == dl::Rarity::kUncommon);
  EXPECT(dl::rarity({dl::kWeapon, weapon("LRM20"), kIs}) == dl::Rarity::kCommon);
  EXPECT(dl::rarity({dl::kAmmo, weapon("SRM4"), 0}) == dl::Rarity::kCommon && dl::rarity({dl::kAmmo, weapon("SRM2"), 0}) == dl::Rarity::kUncommon);
  EXPECT(dl::rarity({dl::kAmmo, weapon("MGUNLT"), 0}) == dl::Rarity::kRare);  // a Clan-only weapon's
  EXPECT(dl::rarity({dl::kEquipment, equipment("masc"), kIs}) == dl::Rarity::kUncommon);
  EXPECT(dl::rarity({dl::kEquipment, equipment("jjets"), kIs}) == dl::Rarity::kCommon);
  // Where: common everywhere; uncommon from tier 1; rare at tier 3; both at a Clan world whatever its tier.
  const int32_t clan = mw1::market::kClanWorlds[0].number;
  EXPECT(dl::stocked_at(dl::Rarity::kCommon, 0, kRobinson) && !dl::stocked_at(dl::Rarity::kUncommon, 0, kRobinson));
  EXPECT(dl::stocked_at(dl::Rarity::kUncommon, 1, kRobinson) && !dl::stocked_at(dl::Rarity::kRare, 2, kRobinson));
  EXPECT(dl::stocked_at(dl::Rarity::kRare, 3, kRobinson) && dl::stocked_at(dl::Rarity::kRare, 0, clan) &&
         dl::stocked_at(dl::Rarity::kUncommon, 0, clan));
}

TEST(mw1_dealer_stock_by_world_and_date_every_ware_somewhere) {
  mw1::Company c = company_at(0, kRobinson);
  dl::roll_stock(c);
  const std::vector<mw1::DealerItem> first = c.dealer.stock;
  EXPECT(c.dealer.rolled && static_cast<int32_t>(first.size()) >= dl::kKindsBase &&
         static_cast<int32_t>(first.size()) <= dl::kKindsBase + dl::kKindsRoll);
  for (const mw1::DealerItem& d : first) {
    EXPECT(d.count >= 1 && d.count <= dl::kCountMost);
    EXPECT(dl::rarity({d.kind, d.id, d.tech}) == dl::Rarity::kCommon);  // a tier 0 world
  }
  dl::roll_stock(c);
  EXPECT(c.dealer.stock == first);  // the same world and date: the same stock
  c.date.month += 1;
  dl::roll_stock(c);
  EXPECT(!(c.dealer.stock == first));
  // The market's refresh restocks the dealer from its own seed: two worlds a month apart get the same 'Mechs from
  // the same dice and different wares, and the dice go on the same.
  mw1::Company a = company_at(1, kRobinson), b = company_at(1, kRobinson);
  b.date.month += 1;
  mw1::Rng ra(7), rb(7);
  mw1::market::refresh_market(a, ra);
  mw1::market::refresh_market(b, rb);
  EXPECT(a.market.stock == b.market.stock && ra.upto(1 << 30) == rb.upto(1 << 30));
  EXPECT(a.dealer.rolled && b.dealer.rolled && !(a.dealer.stock == b.dealer.stock));
  // Every world over two years of months: every ware somewhere, each rarity only where it belongs.
  std::map<std::string, int32_t> seen;
  int32_t misplaced = 0, rolls = 0;
  for (const mw1::Planet& p : mw1::kPlanets)
    for (int32_t m = 0; m < 24; ++m) {
      mw1::Company w = company_at(p.tier, p.number);
      w.date.month = m % 12, w.date.year = 3024 + m / 12;
      dl::roll_stock(w);
      ++rolls;
      for (const mw1::DealerItem& d : w.dealer.stock) {
        const dl::Ware ware{d.kind, d.id, d.tech};
        ++seen[dl::ware_name(ware)];
        if (!dl::stocked_at(dl::rarity(ware), p.tier, p.number)) ++misplaced;
      }
    }
  int32_t never = 0;
  for (const dl::Ware& w : dl::wares())
    if (!seen.count(dl::ware_name(w))) {
      std::printf("  never stocked: %s\n", dl::ware_name(w).c_str());
      ++never;
    }
  std::printf("  %d rolls: %zu of %zu wares seen, %d misplaced\n", rolls, seen.size(), dl::wares().size(), misplaced);
  EXPECT(never == 0 && misplaced == 0);
}

TEST(mw1_dealer_buy_into_the_stores_and_sell_back) {
  FakeLab lab;
  const int32_t ppc = weapon("PPC"), srm4 = weapon("SRM4"), masc = equipment("masc");
  // Prices: 60,000 a ton, at least 50,000; Clan x1.5; ammunition 10,000 a lot; the tier's percentages.
  EXPECT(dl::tier0_price({dl::kWeapon, ppc, kIs}) == 420000 && dl::buy_price({dl::kWeapon, ppc, kIs}, 3) == 630000);
  EXPECT(dl::sell_price({dl::kWeapon, ppc, kIs}, 0) == 378000 && dl::sell_price({dl::kWeapon, ppc, kIs}, 3) == 588000);
  EXPECT(dl::tier0_price({dl::kWeapon, weapon("ERPPC"), kClan}) == 540000);
  EXPECT(dl::tier0_price({dl::kWeapon, weapon("ERLaser_Small"), kIs}) == 60000 &&
         dl::tier0_price({dl::kWeapon, weapon("ERLaser_Small"), kClan}) == 75000);  // the least, x1.5
  EXPECT(dl::tier0_price({dl::kAmmo, srm4, 0}) == 10000 && dl::ammo_lot(srm4) == 100);
  EXPECT(dl::tier0_price({dl::kEquipment, masc, kIs}) == 180000);  // PM's table (equip_weight): 3 t
  mw1::Company c = company_at(0, kRobinson);
  c.dealer.stock = {{dl::kWeapon, ppc, kIs, 2}, {dl::kAmmo, srm4, 0, 1}, {dl::kEquipment, masc, kIs, 1}};
  c.dealer.rolled = true;
  c.funds = 419999;
  EXPECT(dl::buy(c, 0) == dl::Trade::kNoFunds && c.funds == 419999 && c.stores->count() == 0);
  EXPECT(dl::buy(c, 5) == dl::Trade::kNoSuch);
  c.funds = 1000000;
  EXPECT(dl::buy(c, 0) == dl::Trade::kDone && c.funds == 580000 && c.dealer.stock[0].count == 1);
  EXPECT(dl::in_stores(c, {dl::kWeapon, ppc, kIs}) == 1 && dl::in_stores(c, {dl::kWeapon, ppc, kClan}) == 0);
  EXPECT(dl::buy(c, 1) == dl::Trade::kDone && c.funds == 570000 && c.dealer.stock.size() == 2);  // the lot's line gone
  EXPECT(dl::in_stores(c, {dl::kAmmo, srm4, 0}) == 1);
  EXPECT(dl::buy(c, 1) == dl::Trade::kDone && dl::in_stores(c, {dl::kEquipment, masc, kIs}) == 1 && c.dealer.stock.size() == 1);
  // SELL: one out of the stores at the tier's selling price, back into the stock.
  EXPECT(dl::sell(c, {dl::kWeapon, ppc, kIs}) == dl::Trade::kDone && c.funds == 390000 + 378000 && c.dealer.stock[0].count == 2);
  EXPECT(dl::in_stores(c, {dl::kWeapon, ppc, kIs}) == 0 && dl::sell(c, {dl::kWeapon, ppc, kIs}) == dl::Trade::kNoSuch);
  EXPECT(dl::sell(c, {dl::kAmmo, srm4, 0}) == dl::Trade::kDone && c.dealer.stock.size() == 2 && c.dealer.stock[1].kind == dl::kAmmo);
  // Ammunition sells only by whole lots: 99 rounds don't.
  c.stores->add_ammo(srm4, 99);
  EXPECT(dl::in_stores(c, {dl::kAmmo, srm4, 0}) == 0 && dl::sell(c, {dl::kAmmo, srm4, 0}) == dl::Trade::kNoSuch);
  // The stores' limit (mw1_market.h's kStoreMost).
  c.stores->add_equipment(masc, kIs, mw1::market::kStoreMost - 1);
  c.dealer.stock = {{dl::kEquipment, masc, kIs, 1}};
  EXPECT(dl::buy(c, 0) == dl::Trade::kStoresFull && c.dealer.stock.size() == 1);
}

TEST(mw1_dealer_saved_in_version_13) {
  mw1::Rng rng(122);
  mw1::Company c = mw1::new_company(rng);
  EXPECT(mw1::save_text(c, nullptr).find("dealer_stock=") == std::string::npos);  // not rolled: no line
  c.dealer.stock = {{dl::kWeapon, weapon("PPC"), kIs, 2}, {dl::kAmmo, weapon("SRM4"), 0, 1}, {dl::kEquipment, equipment("masc"), kClan, 3}};
  c.dealer.rolled = true;
  const std::string text = mw1::save_text(c, nullptr);
  const std::string line = "\ndealer_stock=1:" + std::to_string(weapon("PPC")) + ":2:2,2:" + std::to_string(weapon("SRM4")) + ":0:1,3:" +
                           std::to_string(equipment("masc")) + ":1:3\n";
  EXPECT(text.find(line) != std::string::npos);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why) && back.dealer.rolled && back.dealer.stock == c.dealer.stock);
  EXPECT(mw1::save_text(back, nullptr) == text);
  // An empty stock (all bought) stays empty, not rolled again.
  c.dealer.stock.clear();
  EXPECT(mw1::load_text(mw1::save_text(c, nullptr), back, k, &has, &why) && back.dealer.rolled && back.dealer.stock.empty());
  // Out of range: refused.
  auto refused = [&](const std::string& to, const char* what) {
    std::string t = text;
    t.replace(t.find(line) + 1, line.size() - 2, to);
    mw1::Company into;
    std::string w;
    const bool ok = mw1::load_text(rechecked(t), into, k, &has, &w);
    std::printf("  %s: %s\n", what, ok ? "read (!)" : w.c_str());
    return !ok;
  };
  EXPECT(refused("dealer_stock=4:0:2:1", "a kind past equipment"));
  EXPECT(refused("dealer_stock=3:15:2:1", "equipment past PM's table"));
  EXPECT(refused("dealer_stock=2:0:2:1", "ammunition with a tech"));
  EXPECT(refused("dealer_stock=1:0:0:1", "a weapon without one"));
  EXPECT(refused("dealer_stock=1:0:2:0", "none of it"));
}

TEST(mw1_dealer_price_table) {
  const bool pm = mw1_tests::use_pm_weapon_labs();
  if (pm) {
    // PM's weights: every weapon ware weighs something; the PPC its 7 tons.
    int32_t weightless = 0;
    for (const dl::Ware& w : dl::wares())
      if (w.kind == dl::kWeapon && !(dl::ware_tons(w) > 0)) ++weightless, std::printf("  no weight: %s\n", dl::ware_name(w).c_str());
    EXPECT(weightless == 0);
    EXPECT(dl::ware_tons({dl::kWeapon, weapon("PPC"), kIs}) == 7.0f && dl::ammo_lot(weapon("SRM4")) > 1);
    std::printf("  %d of PM's weapons read\n", mw1_tests::use_pm_weapon_labs_count());
  }
  const std::string table = dl::price_table();
  EXPECT(table.find("DEALER   weapon PPC ") != std::string::npos);
  if (std::getenv("MW1_PRICE_TABLE")) std::printf("%s", table.c_str());
  mw1_tests::stop_pm_weapon_labs();
}
