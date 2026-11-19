#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mech_objective.h"
#include "mech3/pilot.h"
#include "pm/pm_game.h"

// pm-tests item 9: a mission on a fresh world (0x00435e40) gets no world of the last one. The win from mission 1 writes
// mission 2's (the same world, c1); the win from mission 2 writes none for mission 3 (c2), with the fix or without
// (next_mission's own rule). And a game for mission 3 whose file is there (the port's builds before 2026-11-08 wrote
// one) reads nothing back.
namespace {
struct FreshHooks : mech3::PilotHooks {
  std::vector<std::string> saved;
  int32_t loaded = 0;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  int32_t message_box(int32_t, int32_t, int32_t) override { return 1; }
  void save_world(const char* path, int32_t) override { saved.push_back(path ? path : ""); }
  bool load_world(const char*, int32_t) override { return ++loaded, true; }
};
}  // namespace
TEST(pm_campaign_fresh_world_keeps_no_world) {
  zutl::mounts().read.clear();
  const std::string dir = std::string(MW3_TEST_TMP_ROOT) + "/test_pm_fresh_world";
  std::filesystem::create_directories(dir + "/pilots/games");
  zutl::set_write_dir(dir.c_str());
  zgame::set_game(pm::table());
  FreshHooks h;
  mech3::set_pilot_hooks(&h);
  for (const bool fix : {true, false}) {
    zfix::set_all(fix);
    mech3::Pilot* p = mech3::create_pilot("Fresh");
    EXPECT(p && p->select_campaign(0));
    h.saved.clear();
    p->record_mission(true);  // mission 1 won: mission 2, on c1 still
    EXPECT(h.saved.size() == 1);
    h.saved.clear();
    p->record_mission(true);  // mission 2 won: mission 3, on c2
    auto* g = dynamic_cast<mech3::CampaignGame*>(p->current);
    EXPECT(g && g->mission_number() == 3);
    EXPECT(h.saved.empty());
    if (g) {
      std::FILE* f = std::fopen((dir + "/" + g->path()).c_str(), "wb");  // an old build's file
      if (f) std::fclose(f);
      h.loaded = 0;
      g->b0 = 0;
      EXPECT(!g->restore() && h.loaded == 0);
    }
    while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  }
  zfix::set_all(true);
  mech3::set_pilot_hooks(nullptr);
  zgame::set_game(zgame::mw3_table());
}

// PM's objective persist (0x004356c0 with 0x0045fe35 and 0x004a1e17; ported for preservation, not wired: no shipped
// objective has a persist and no shipped aiv record sets bit 4, docs/pm-tests.md). A completed visible objective's
// persist joins the campaign's strings, done or failed, outside a campaign nowhere; the lookup is the exact name.
namespace {
struct PersistHooks : mech3::ObjectiveHooks {
  std::vector<std::string>* list = nullptr;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  zgame::Handle player_vehicle() override { return 1; }
  zgame::Handle view_vehicle() override { return 0; }
  std::vector<std::string>* campaign_strings() override { return list; }
};
}  // namespace

TEST(pm_objective_persist_kept_for_the_campaign) {
  zutl::mounts().read.clear();
  const std::string dir = std::string(MW3_TEST_TMP_ROOT) + "/test_pm_persist";
  std::filesystem::create_directories(dir + "/pilots/games");
  zutl::set_write_dir(dir.c_str());
  zgame::set_game(pm::table());
  FreshHooks ph;
  mech3::set_pilot_hooks(&ph);
  mech3::Pilot* p = mech3::create_pilot("Persist");
  EXPECT(p && p->select_campaign(0));
  auto* g = p ? dynamic_cast<mech3::CampaignGame*>(p->current) : nullptr;
  EXPECT(g != nullptr);
  if (g) {
    EXPECT(!g->has_string("Gate_mech"));  // empty: none
    PersistHooks h;
    h.list = &g->strings;
    mech3::Objective o(mech3::Objective::kDestroy, nullptr);
    o.persist = "Gate_mech";
    mech3::complete(o, 1, h, 0);  // failed, still kept
    EXPECT(g->strings.size() == 1 && g->has_string("Gate_mech"));
    EXPECT(!g->has_string("gate_mech") && !g->has_string("Gate"));
    mech3::Objective hidden(mech3::Objective::kDestroy, nullptr);
    hidden.persist = "Hidden";
    hidden.visible = 0;
    mech3::complete(hidden, 1, h, 1);
    EXPECT(!g->has_string("Hidden"));  // only a visible objective's
    h.list = nullptr;  // outside a campaign
    mech3::Objective other(mech3::Objective::kDestroy, nullptr);
    other.persist = "Other";
    mech3::complete(other, 1, h, 1);
    EXPECT(g->strings.size() == 1);
  }
  while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  mech3::set_pilot_hooks(nullptr);
  zgame::set_game(zgame::mw3_table());
}


// Ours (MW1's FIXES.md item 48): Instant Action's line-up holds the exe's ten past the player's unless a program
// raises action_lineup_capacity (MW1: 16, in a tail after PM's arrays, which keep their own slots and bounds).
TEST(pm_action_lineup_ten_unless_raised) {
  mech3::ActionGame g(nullptr);
  EXPECT(mech3::action_lineup_capacity() == 10);
  for (int32_t i = 0; i < 10; ++i) EXPECT(g.add(i, i < 3 ? 1 : 0, nullptr, 0) != nullptr);  // allies first, as the screen
  EXPECT(g.add(10, 0, nullptr, 0) == nullptr && g.count == 10);  // the exe's count > 9
  EXPECT(g.slot_at(10) == &g.lineup[9] && g.slot_at(11) == nullptr && !g.ally(11));
  // Raised (MW1): 16; slots 11.. in the tail; add refuses the 17th.
  mech3::set_action_lineup_capacity(16);
  for (int32_t i = 10; i < 16; ++i) EXPECT(g.add(i, 0, nullptr, 0) != nullptr);
  EXPECT(g.add(16, 0, nullptr, 0) == nullptr && g.count == 16);
  EXPECT(g.slot_at(11) == &g.lineup_more[0] && g.slot_at(16) == &g.lineup_more[5] && g.slot_at(17) == nullptr);
  EXPECT(g.slot_at(13)->mech == 12 && g.ally(3) && !g.ally(4) && !g.ally(13));
  // Remove across the boundary: slot 9 (0-based) out, the tail's first moves into PM's last.
  EXPECT(g.remove(9));
  EXPECT(g.count == 15 && g.lineup[9].mech == 10 && g.lineup_more[0].mech == 11 && g.slot_at(12)->mech == 12);
  g.drop_enemies();
  EXPECT(g.count == 3 && g.ally(1) && g.ally(2) && g.ally(3));  // the enemies out, the tail's too
  // A side kept in the tail: twelve allies.
  mech3::ActionGame h(nullptr);
  for (int32_t i = 0; i < 12; ++i) h.add(i, 1, nullptr, 0);
  h.add(12, 0, nullptr, 0);
  EXPECT(h.count == 13 && h.ally(12) && !h.ally(13) && h.side_more[1] == 1 && h.side_more[2] == 0);
  // Clamped: never below the exe's ten nor past the tail.
  mech3::set_action_lineup_capacity(40);
  EXPECT(mech3::action_lineup_capacity() == 16);
  mech3::set_action_lineup_capacity(3);
  EXPECT(mech3::action_lineup_capacity() == 10);
}
