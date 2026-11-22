// FIXES 118, 1.1.0 slice 3 (src/mw1_kit_answers.h): MW1's answers to the engine's upgrade hooks. The armour kit's
// spread over the slots; the company's side only (the commander's 'Mech, each lancemate's by their pilot id, never
// another), each its own kits and every weapon of the side its type's steps; the sides cached once the launch is done,
// a lancemate's checked by its pilot (a mech made later at its address is not theirs);
// nothing when disarmed or uninstalled. Through the engine's own guard (src/upgrades/kits) as a port site reads it.
#include <array>
#include <cstdint>
#include <map>
#include <string>

#include "check.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/mech.h"
#include "mech3/weapon.h"
#include "mw1_campaign.h"
#include "mw1_kit_answers.h"
#include "mw1_upgrades.h"
#include "upgrades/kits/kit_hooks.h"

namespace {
using mw1::kit_answers::spread_armour;
using Spread = std::array<int32_t, 11>;

constexpr int32_t kMediumLaser = 17, kIs = 2, kClan = 1;  // PM's Laser_Medium; the techs as the loadouts' items

mw1::Company company_with_kits() {
  mw1::Company c;
  c.kits = {3, 4};
  mw1::OwnedMech flown, idle;
  flown.pilot = 7, flown.kits = {1, 2};
  idle.pilot = 0, idle.kits = {9, 9};  // no pilot: never flies
  c.hangar = {flown, idle};
  c.upgrades.steps.push_back({kMediumLaser, kIs, 2, 1});
  return c;
}
}  // namespace

TEST(mw1_kit_answers_the_armour_kit_spread_over_the_slots) {
  const int32_t even[11] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
  const int32_t none[11] = {};
  const int32_t some[11] = {20, 10, 10};
  EXPECT(spread_armour(0, even) == Spread{});
  EXPECT(spread_armour(-3, even) == Spread{});
  EXPECT(spread_armour(11, even) == (Spread{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}));
  EXPECT(spread_armour(13, even) == (Spread{2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}));  // equal remainders: the lower slots
  EXPECT(spread_armour(5, some) == (Spread{3, 1, 1}));    // 2.5, 1.25, 1.25: the largest remainder takes the fifth
  EXPECT(spread_armour(13, none) == (Spread{2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}));
  EXPECT(spread_armour(40, some) == (Spread{20, 10, 10}));  // an unarmoured slot never gets one
  int32_t total = 0;
  for (int32_t v : spread_armour(97, some)) total += v;
  EXPECT(total == 97);
}

TEST(mw1_kit_answers_the_company_s_side_only) {
  mw1::kit_answers::uninstall();
  const mw1::Company c = company_with_kits();
  static mech3::Mech commander, mate, enemy;
  static int32_t mate_side = 7;
  static std::map<const void*, std::string> pilots;  // each mech's AI pilot, as its brain would say
  pilots = {{&mate, "Mate"}, {&enemy, "Enemy"}};
  mw1::kit_answers::arm(c, [](const void* m) { return m == &commander ? 0 : m == &mate ? mate_side : -1; },
                        [](const void* m) { return pilots.count(m) ? pilots[m] : std::string(); });
  EXPECT(mw1::kit_answers::armed());
  auto kits = [](const void* m) { return mw1::kit_answers::kits_for(m); };
  EXPECT(kits(&commander) == (mw1::Kits{3, 4}) && kits(&mate) == (mw1::Kits{1, 2}) && kits(&enemy) == mw1::Kits{});
  const float boosted = mw1::upgrades::damage_scale(c, kMediumLaser, kIs);
  EXPECT(boosted > 1.0f && mw1::kit_answers::damage_for(&commander, kMediumLaser, kIs) == boosted);
  EXPECT(mw1::kit_answers::damage_for(&mate, kMediumLaser, kIs) == boosted);
  EXPECT(mw1::kit_answers::damage_for(&enemy, kMediumLaser, kIs) == 1.0f);   // the same weapon on an enemy: none
  EXPECT(mw1::kit_answers::damage_for(&commander, kMediumLaser, kClan) == 1.0f);  // the Clan one has no steps
  EXPECT(mw1::kit_answers::range_for(&mate, kMediumLaser, kIs) == 10.0f && mw1::kit_answers::range_for(&enemy, kMediumLaser, kIs) == 0.0f);

  // Settled: a lancemate's side as first looked up while the mech is still theirs (its pilot the same); an enemy's
  // cached as none.
  mw1::kit_answers::settle();
  EXPECT(kits(&mate) == (mw1::Kits{1, 2}) && kits(&enemy) == mw1::Kits{});
  mate_side = -1;
  EXPECT(kits(&mate) == (mw1::Kits{1, 2}));
  // The lancemate's mech gone and a script's reinforcement made at its address: another pilot, so looked up afresh,
  // and an enemy's is none.
  pilots[&mate] = "Reinforcement";
  EXPECT(kits(&mate) == mw1::Kits{} && mw1::kit_answers::damage_for(&mate, kMediumLaser, kIs) == 1.0f);
  // Still another mech there later (the game says a lancemate's, the pilot theirs again): theirs, checked as before.
  mate_side = 7, pilots[&mate] = "Mate";
  EXPECT(kits(&mate) == (mw1::Kits{1, 2}));

  mw1::kit_answers::disarm();
  EXPECT(!mw1::kit_answers::armed() && kits(&commander) == mw1::Kits{} &&
         mw1::kit_answers::damage_for(&commander, kMediumLaser, kIs) == 1.0f);
}

TEST(mw1_kit_answers_through_the_engine_s_hooks) {
  const mw1::Company c = company_with_kits();
  static mech3::Mech commander, enemy;
  mw1::kit_answers::install();
  mw1::kit_answers::arm(c, [](const void* m) { return m == &commander ? 0 : -1; });
  // Sinks and armour as the heat step and a part's setup read them (no configuration: the armour a slot at a time).
  EXPECT(upgrades::kit_extra_sinks(&commander) == 3 && upgrades::kit_extra_sinks(&enemy) == 0);
  EXPECT(upgrades::kit_extra_armour(&commander, 0) == 1 && upgrades::kit_extra_armour(&commander, 3) == 1 &&
         upgrades::kit_extra_armour(&commander, 4) == 0 && upgrades::kit_extra_armour(&enemy, 0) == 0);
  mech3::Config cfg(0, "kits");
  cfg.armor[0] = 30, cfg.armor[7] = 10;  // the four points: three on the torso, one on the left arm
  commander.config = &cfg;
  EXPECT(upgrades::kit_extra_armour(&commander, 0) == 3 && upgrades::kit_extra_armour(&commander, 7) == 1 &&
         upgrades::kit_extra_armour(&commander, 1) == 0);
  commander.config = nullptr;
  // A weapon: its owner's side, its item's id and tech.
  zwep::WeaponDef def{};
  def.range = 100.0f;
  mech3::WeaponManager mgr(nullptr), theirs(nullptr);
  mgr.owner = &commander, theirs.owner = &enemy;
  mech3::WeaponItem item;
  item.weapon = kMediumLaser, item.tech = kIs;
  mech3::Weapon ours(&def, nullptr, &mgr, true), enemy_gun(&def, nullptr, &theirs, true);
  ours.item = &item, enemy_gun.item = &item;
  EXPECT(upgrades::kit_damage_scale(&ours) == mw1::upgrades::damage_scale(c, kMediumLaser, kIs));
  EXPECT(upgrades::kit_range_add(&ours) == 10.0f);
  EXPECT(upgrades::kit_damage_scale(&enemy_gun) == 1.0f && upgrades::kit_range_add(&enemy_gun) == 0.0f);
  // Disarmed, then uninstalled: none, and the hooks null again.
  mw1::kit_answers::disarm();
  EXPECT(upgrades::kit_extra_sinks(&commander) == 0 && upgrades::kit_damage_scale(&ours) == 1.0f);
  mw1::kit_answers::uninstall();
  EXPECT(!upgrades::kit_hooks().extra_sinks && !upgrades::kit_hooks().extra_armour && !upgrades::kit_hooks().damage_scale &&
         !upgrades::kit_hooks().range_add);
  ours.item = enemy_gun.item = nullptr;
}
