// FIXES 104: every 'Mech of PM's as an enemy, in its real loadouts (mw1_enemies.h). Pure: no engine.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mw1_campaign.h"
#include "mw1_enemies.h"
#include "mw1_ladder.h"
#include "mw1_save.h"

namespace {
int32_t tons(int32_t m) { return static_cast<int32_t>(std::lround(mech3::max_mass(m))); }
}  // namespace

// The rule's reach: every slot's chassis of its weight class (PM's), of the asked tech, never the Elemental; the
// 100-tonners only in a heavy or assault slot and only late; every 'Mech reachable from some slot.
TEST(mw1_enemies_each_slot_reaches_its_weight) {
  std::vector<bool> reached(mech3::kMechCount, false);
  for (int32_t slot = 0; slot < mw1::kSlotCount; ++slot)
    for (int32_t tier = 0; tier < mw1::kTierCount; ++tier)
      for (int32_t band = 0; band <= 3; ++band)
        for (bool clan : {false, true})
          for (int32_t m : mw1::slot_chassis(slot, clan, tier, band)) {
            reached[static_cast<size_t>(m)] = true;
            const bool late = tier >= mw1::kHundredTonTier || band >= mw1::kHundredTonBand;
            EXPECT(mw1::fields(m) && m != mech3::kElemental && mw1::is_clan(m) == clan);
            if (tons(m) >= mw1::kHundredTons) EXPECT(late && mw1::slot_class(slot) >= 2);
            else EXPECT(mech3::weight_class(m) == mw1::slot_class(slot));
          }
  int32_t n = 0;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    n += reached[static_cast<size_t>(m)];
    EXPECT(reached[static_cast<size_t>(m)] == (m != mech3::kElemental));
  }
  std::printf("  %d chassis reachable\n", n);
  EXPECT(n == 24);
  // The loadout table: every chassis's count, the Primary first.
  int32_t loadouts = 0;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    loadouts += mw1::fields(m) ? mw1::loadout_count(m) : 0;
    EXPECT(mw1::loadout_kind(m, 0) == mw1::LoadoutKind::kPrimary);
  }
  std::printf("  %d stock loadouts over the fielded chassis\n", loadouts);
}

// The soak: 20,000 contracts rolled as the game rolls them (generate_contract), across the tiers and the record bands.
// Every fielded chassis and every one of its loadouts appears; the Primary is each chassis's most common; each tier's
// Clan share near the rule's (ungated, FIXES 112: the pools' own Clan share, every chassis alike; gated, kClanPercent);
// no 100-tonner early; the Elemental never. The spread printed.
TEST(mw1_enemies_the_soak_fields_every_chassis_and_loadout) {
  mw1::Rng rng(104);
  mw1::Company c = mw1::new_company(rng);
  std::map<int32_t, std::map<int32_t, int32_t>> seen;  // chassis: loadout: count
  int32_t slots[mw1::kTierCount] = {}, clans[mw1::kTierCount] = {}, early_hundreds = 0, contracts = 0;
  double expected[mw1::kTierCount] = {};  // the Clan enemies the rule expects
  for (int32_t i = 0; i < 20000; ++i) {
    c.tier = i % mw1::kTierCount;
    c.kills = (i / mw1::kTierCount) % 50;  // bands 0 to 3
    c.funds = 1000000 + i * 7919;
    c.planet_house = i % mw1::kHouseCount;
    const mw1::Contract k = mw1::generate_contract(c, i % 6, rng);
    ++contracts;
    EXPECT(k.chassis.size() == k.enemies.size() && k.loadouts.size() == k.enemies.size());
    for (size_t s = 0; s < k.enemies.size(); ++s) {
      const int32_t m = mw1::enemy_chassis(k, s), l = mw1::enemy_loadout(k, s);
      ++seen[m][l];
      const int32_t band = mw1::record_band(c.kills);
      const size_t clan_pool = mw1::slot_chassis(k.enemies[s], true, c.tier, band).size();
      if (clan_pool) {
        ++slots[c.tier];
        clans[c.tier] += mw1::is_clan(m);
        expected[c.tier] += mw1::kClanGated ? mw1::kClanPercent[c.tier] / 100.0
                                            : static_cast<double>(clan_pool) / mw1::slot_pool(k.enemies[s], c.tier, band).size();
      } else {
        EXPECT(!mw1::is_clan(m));
      }
      const bool late = c.tier >= mw1::kHundredTonTier || mw1::record_band(c.kills) >= mw1::kHundredTonBand;
      if (tons(m) >= mw1::kHundredTons && !late) ++early_hundreds;
      EXPECT(m != mech3::kElemental && l >= 0 && l < mw1::loadout_count(m));
    }
  }
  int32_t chassis = 0, loadouts = 0, all_loadouts = 0;
  for (int32_t m = 0; m < mech3::kMechCount; ++m) {
    if (!mw1::fields(m)) continue;
    const auto it = seen.find(m);
    EXPECT(it != seen.end());
    if (it == seen.end()) continue;
    ++chassis;
    all_loadouts += mw1::loadout_count(m);
    int32_t total = 0, primary = it->second.count(0) ? it->second.at(0) : 0, most_other = 0;
    std::string spread;
    for (const auto& [l, n] : it->second) {
      total += n, ++loadouts;
      if (l) most_other = std::max(most_other, n);
      spread += " " + std::to_string(l) + ":" + std::to_string(n);
    }
    std::printf("  %-12s %5d (%s)%s\n", mech3::mech_name(m), total, mw1::is_clan(m) ? "Clan" : "IS", spread.c_str());
    EXPECT(static_cast<int32_t>(it->second.size()) == mw1::loadout_count(m));  // every loadout seen
    EXPECT(primary > most_other);                                               // the Primary the commonest
  }
  for (int32_t t = 0; t < mw1::kTierCount; ++t) {
    const double share = slots[t] ? 100.0 * clans[t] / slots[t] : 0, rule = slots[t] ? 100.0 * expected[t] / slots[t] : 0;
    std::printf("  tier %d: %d enemies a Clan chassis could be, %.1f%% Clan (the rule %.1f%%)\n", t, slots[t], share, rule);
    EXPECT(std::fabs(share - rule) <= 2.0);
  }
  std::printf("  %d contracts: %d chassis, %d of their %d loadouts; 100-tonners early: %d\n", contracts, chassis, loadouts,
              all_loadouts, early_hundreds);
  EXPECT(early_hundreds == 0 && chassis == 24 && loadouts == all_loadouts);
}

// The same offer rolls the same 'Mechs, and MW1's own draws are untouched: the contract's rolls come from its own
// generator (two generators from one seed agree after a contract with the chassis rolled).
TEST(mw1_enemies_an_offer_rolls_the_same_and_leaves_mw1s_draws) {
  mw1::Rng a(7), b(7);
  mw1::Company c = mw1::new_company(a);
  mw1::Company d = mw1::new_company(b);
  c.kills = d.kills = 40, c.tier = d.tier = 3;
  const mw1::Contract ka = mw1::generate_contract(c, 1, a), kb = mw1::generate_contract(d, 1, b);
  EXPECT(ka.enemies == kb.enemies && ka.chassis == kb.chassis && ka.loadouts == kb.loadouts);
  mw1::Contract again = ka;
  mw1::field_force(c, again);
  EXPECT(again.chassis == ka.chassis && again.loadouts == ka.loadouts);
  EXPECT(a.upto(1000000) == b.upto(1000000));
  // MW1's draws: the force's classes and the pay as the ladder of seeds gives them before 104 (mw1_campaign's own
  // pinned test, mw1_campaign_contracts_as_mw1_sizes_them, still passes unchanged).
}

// Older saves and in-flight contracts: a contract with no chassis fields its slots' stand-ins on their Primary; a
// wreck with no chassis is named and paid as its slot's stand-in. The offer's chassis and the wrecks' save and read.
TEST(mw1_enemies_older_saves_keep_their_stand_ins) {
  mw1::Contract k;
  k.enemies = {mw1::kSlot20, mw1::kSlot85};
  EXPECT(mw1::enemy_chassis(k, 0) == mw1::kPmFirefly && mw1::enemy_loadout(k, 0) == 0);
  EXPECT(mw1::enemy_chassis(k, 1) == mw1::kPmAwesome && mw1::enemy_loadout(k, 1) == 0);
  k.chassis = {mech3::kMadCat};  // not one each: ignored
  k.loadouts = {3};
  EXPECT(mw1::enemy_chassis(k, 0) == mw1::kPmFirefly && mw1::enemy_loadout(k, 0) == 0);
  mw1::Wreck w{mw1::kSlot85, 50, 100};
  EXPECT(mw1::wreck_chassis(w) == mw1::kPmAwesome && mw1::pay_chassis(w) == mw1::wreck_chassis(w));
  w.chassis = mech3::kMadCat;
  EXPECT(mw1::wreck_chassis(w) == mech3::kMadCat);
  EXPECT(mw1::pay_chassis(w) == (mw1::kWreckPay == mw1::WreckPay::kChassis ? mech3::kMadCat : mw1::kPmAwesome));

  // Round trip: an offer rolled, saved, read; then the same text without the new lines (an older save) reads too.
  mw1::Rng rng(3);
  mw1::Company c = mw1::new_company(rng);
  c.kills = 40, c.tier = 2;
  mw1::Contract offer = mw1::generate_contract(c, 1, rng);
  const std::string text = mw1::save_text(c, &offer);
  mw1::Company c2;
  mw1::Contract back;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, c2, back, &has, &why) && has);
  EXPECT(back.enemies == offer.enemies && back.chassis == offer.chassis && back.loadouts == offer.loadouts);
  mw1::Contract before_104 = offer;  // as an older build wrote it: no chassis lines
  before_104.chassis.clear(), before_104.loadouts.clear();
  const std::string old = mw1::save_text(c, &before_104);
  EXPECT(old.find("offer_chassis=") == std::string::npos && text.find("offer_chassis=") != std::string::npos);
  mw1::Company c3;
  mw1::Contract older;
  EXPECT(mw1::load_text(old, c3, older, &has, &why) && has);
  EXPECT(older.enemies == offer.enemies && older.chassis.empty());
  for (size_t s = 0; s < older.enemies.size(); ++s)
    EXPECT(mw1::enemy_chassis(older, s) == mw1::stand_in(older.enemies[s]) && mw1::enemy_loadout(older, s) == 0);
  if (!why.empty()) std::printf("  %s\n", why.c_str());
}

// Story battles roll too (kStoryBattlesRoll); the Dark Wing's scripted force (FIXES 114, the owner) is MW1's text's
// 'Mechs as ours: a Thor and four Atlases, the escorts the 75 t record's (kOurs75), on their Primary, its hardest's
// extra one too.
TEST(mw1_enemies_story_battles_roll_but_the_dark_wing) {
  mw1::Rng rng(11);
  mw1::Company c = mw1::new_company(rng);
  c.tier = 2;
  for (int32_t r = 1; r <= mw1::kDarkWingRung; ++r) {
    const mw1::Contract k = mw1::story_battle(c, r, rng, false);
    EXPECT(!k.enemies.empty());
    if (r == mw1::kDarkWingRung) {
      const std::vector<int32_t> want = {mw1::kPmThor, mw1::kPmAtlas, mw1::kPmAtlas, mw1::kPmAtlas,
                                         mw1::kPmAtlas, mw1::kOurs75, mw1::kOurs75, mw1::kOurs75};
      EXPECT(k.chassis == want && k.loadouts == std::vector<int32_t>(8, 0));
      for (size_t s = 0; s < k.enemies.size(); ++s) EXPECT(mw1::enemy_chassis(k, s) == want[s]);
      const mw1::Contract h = mw1::story_battle(c, r, rng, true);
      EXPECT(h.chassis.size() == h.enemies.size() && h.chassis.back() == mw1::kOurs75);
    } else if (mw1::kStoryBattlesRoll) {
      EXPECT(k.chassis.size() == k.enemies.size());
    } else {
      EXPECT(k.chassis.empty());
    }
  }
}

// FIXES 112: the roster is the one list. Cut to the eight the game fielded before 104, every slot still fields (its
// pool, else its stand-in), only chassis of the list ever appear, the stand-ins are those eight's nearest, and the
// wrecks of a pre-104 save pay as they did. The full roster back afterwards.
TEST(mw1_enemies_a_cut_roster_still_fields_every_slot) {
  const std::vector<int32_t> eight = {mw1::kPmFirefly, mw1::kPmOwens,  mw1::kPmStrider, mw1::kPmBushwacker,
                                      mw1::kPmChampion, mw1::kPmAvatar, mw1::kPmOrion,   mw1::kPmAwesome};
  EXPECT(mw1::roster().size() == std::size(mw1::kRoster) && std::size(mw1::kRoster) == static_cast<size_t>(mech3::kMechCount));
  mw1::Company c0;
  c0.tier = 0, c0.repair_premium = 18;
  const mw1::Wreck old_wreck{mw1::kSlot55, 100, 0};  // a pre-104 save's: no chassis
  const int32_t full_pay = mw1::wreck_value(c0, old_wreck);
  mw1::set_roster_for_tests(eight);
  for (int32_t m = 0; m < mech3::kMechCount; ++m)
    EXPECT(mw1::fields(m) == (std::find(eight.begin(), eight.end(), m) != eight.end()));
  const int32_t want[mw1::kSlotCount] = {mw1::kPmFirefly, mw1::kPmOwens,    mw1::kPmStrider, mw1::kPmBushwacker,
                                         mw1::kPmChampion, mw1::kPmAvatar, mw1::kPmOrion,   mw1::kPmAwesome};
  for (int32_t s = 0; s < mw1::kSlotCount; ++s) EXPECT(mw1::stand_in(s) == want[s]);
  EXPECT(mw1::wreck_value(c0, old_wreck) == full_pay);
  mw1::Rng rng(112);
  mw1::Company c = mw1::new_company(rng);
  std::set<int32_t> seen;
  for (int32_t i = 0; i < 4000; ++i) {
    c.tier = i % mw1::kTierCount, c.kills = (i / mw1::kTierCount) % 50, c.funds = 1000000 + i * 7919;
    const mw1::Contract k = mw1::generate_contract(c, i % 6, rng);
    for (size_t s = 0; s < k.enemies.size(); ++s) {
      const int32_t m = mw1::enemy_chassis(k, s);
      EXPECT(std::find(eight.begin(), eight.end(), m) != eight.end() && !mw1::is_clan(m));
      seen.insert(m);
    }
  }
  std::printf("  the old eight as the roster: %zu of them met\n", seen.size());
  EXPECT(seen.size() == eight.size());
  // A roster of one: every slot is it.
  mw1::set_roster_for_tests({mw1::kPmMadCat});
  for (int32_t s = 0; s < mw1::kSlotCount; ++s) EXPECT(mw1::stand_in(s) == mw1::kPmMadCat);
  mw1::EnemyRng er(5);
  for (int32_t s = 0; s < mw1::kSlotCount; ++s) EXPECT(mw1::roll_enemy(s, 3, 3, er).chassis == mw1::kPmMadCat);
  mw1::set_roster_for_tests({});
  EXPECT(mw1::roster().size() == std::size(mw1::kRoster));
}

// FIXES 112 (the owner: "only the original 8 mechs are showing up"): per tier, 2,000 contracts as the game rolls them
// (the record bands across them), how many of the 24 chassis appear and what share of the enemies are the eight the
// game fielded before 104. Most of the roster at every tier; the old eight no longer most of it. And each tier's
// pool, by slot class (Inner Sphere and Clan, with tons), printed for the owner.
TEST(mw1_enemies_every_tier_fields_most_of_the_roster) {
  const std::set<int32_t> eight = {mw1::kPmFirefly, mw1::kPmOwens,  mw1::kPmStrider, mw1::kPmBushwacker,
                                   mw1::kPmChampion, mw1::kPmAvatar, mw1::kPmOrion,   mw1::kPmAwesome};
  static const char* kClass[] = {"light", "medium", "heavy", "assault"};
  for (int32_t tier = 0; tier < mw1::kTierCount; ++tier) {
    mw1::Rng rng(1120 + static_cast<uint32_t>(tier));
    mw1::Company c = mw1::new_company(rng);
    c.tier = tier;
    std::map<int32_t, int32_t> seen;
    int32_t enemies = 0, old = 0, clan = 0;
    for (int32_t i = 0; i < 2000; ++i) {
      c.kills = i % 50, c.funds = 1000000 + i * 7919, c.planet_house = i % mw1::kHouseCount;
      const mw1::Contract k = mw1::generate_contract(c, i % 6, rng);
      for (size_t s = 0; s < k.enemies.size(); ++s) {
        const int32_t m = mw1::enemy_chassis(k, s);
        ++seen[m], ++enemies, old += eight.count(m) ? 1 : 0, clan += mw1::is_clan(m) ? 1 : 0;
      }
    }
    std::printf("  tier %d: %zu of 24 chassis in 2,000 contracts (%d enemies), the old eight %.0f%%, Clan %.0f%%\n", tier,
                seen.size(), enemies, 100.0 * old / enemies, 100.0 * clan / enemies);
    EXPECT(seen.size() >= 20 && old * 100 < enemies * 60);
    for (int32_t cls = 0; cls < 4; ++cls) {
      std::set<int32_t> pool;  // over the record bands, the slots of this class
      for (int32_t slot = 0; slot < mw1::kSlotCount; ++slot)
        if (mw1::slot_class(slot) == cls)
          for (int32_t band = 0; band <= 3; ++band)
            for (bool clan : {false, true})
              for (int32_t m : mw1::slot_chassis(slot, clan, tier, band)) pool.insert(m);
      std::string line;
      for (int32_t m : pool)
        line += std::string(" ") + mech3::mech_name(m) + " " + std::to_string(tons(m)) + (mw1::is_clan(m) ? "c" : "") + " (" +
                std::to_string(seen.count(m) ? seen.at(m) : 0) + ")";
      std::printf("    %-7s slots:%s\n", kClass[cls], line.c_str());
    }
  }
}
