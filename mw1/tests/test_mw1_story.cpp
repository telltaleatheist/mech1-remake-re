// MW1's story (src/mw1_story.h): the script and texts read from the player's MW_MAIN.EXE against its data segment,
// the story planets, the rules as MW1 runs them (conditions, actions, choices, the news), the errata, and the save.
// No words of MW1's here: the checks count and compare, and print counts.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_market.h"
#include "mw1_text_names.h"
#include "mw1_install.h"
#include "mw1_campaign.h"
#include "mw1_pilots.h"
#include "mw1_save.h"
#include "mw1_story.h"
#include "mw1_ladder.h"
#include "mw1_log.h"
#include "mw1_missions.h"
#include "mw1_travel.h"

namespace {

// The exe's data segment, for the tables' checks.
std::vector<uint8_t> data_segment() {
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  return mw1::mw_main_data_segment();
}

// A record of the list with this id and class (-1: none).
int32_t record_of(int32_t id, int32_t cls = -1) {
  const mw1::Script& s = mw1::script();
  for (int32_t r : s.list) {
    const mw1::Event& e = s.records[static_cast<size_t>(r)];
    if (e.id == id && (cls < 0 || e.cls == cls)) return r;
  }
  return -1;
}

// A company on the start world `start` with its story planets: 0 the start world, the rest the pools' firsts.
mw1::Company on_story(int32_t world_number) {
  mw1::Company c;
  c.funds = 1000000;
  for (int32_t i = 0; i < mw1::kStoryPlanets; ++i) c.story.planets[i] = mw1::kPools[i][0];
  mw1::set_planet(c, mw1::planet_index(world_number));
  return c;
}

// Every event a class fires now (choices answered with `pick`), in order.
std::vector<int32_t> fire(mw1::Company& c, int32_t cls, mw1::Rng& rng, int32_t pick = 0) {
  std::vector<int32_t> out;
  mw1::Dispatch d = mw1::start(cls);
  mw1::Shown s;
  for (int32_t guard = 0; guard < 40 && mw1::next(c, d, rng, &s); ++guard) {
    out.push_back(s.event);
    if (!s.captions.empty()) mw1::choose(d, s, pick);
  }
  return out;
}

bool has(const std::vector<int32_t>& v, int32_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

}  // namespace

TEST(mw1_story_the_pools_and_name_are_the_exes) {
  // The story planets' pools (DS:8FDA..8FF2), and pool 0 is the start worlds.
  for (int32_t i = 0; i < mw1::kStartPlanetCount; ++i) EXPECT(mw1::kPools[0][i] == mw1::kStartPlanets[i].number);
  for (int32_t i = 0; i < mw1::kStoryPlanets; ++i)
    for (int32_t k = 0; k < mw1::kPoolSize[i]; ++k) EXPECT(mw1::planet_index(mw1::kPools[i][k]) >= 0);
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> d = data_segment();
  EXPECT(d.size() > 0x9000);
  if (d.size() <= 0x9000) return;
  for (int32_t i = 0; i < mw1::kStoryPlanets; ++i)
    for (int32_t k = 0; k < mw1::kPoolSize[i]; ++k) EXPECT(d[mw1::kPoolAt[i] + static_cast<size_t>(k)] == mw1::kPools[i][k]);
  EXPECT((d[mw1::kNewCampaignList] | d[mw1::kNewCampaignList + 1] << 8) == 1);  // the new campaign's list
  const std::string name = mw1::commander_name();
  EXPECT(!name.empty() && name.size() < 0x12);
  for (char ch : name) EXPECT(ch >= 0x20 && ch < 0x7f);
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::commander_name().empty());
}

TEST(mw1_story_the_script_reads_from_the_exe) {
  mw1::set_mw1_game_dir("");
  EXPECT(!mw1::script().there() && mw1::text_pages(0x74).empty());  // no MW1: no story
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const mw1::Script& s = mw1::script();
  EXPECT(s.there());
  if (!s.there()) return;
  // 86 entries in the new campaign's list, every record within DS:6121..655C, the fixed one event 0x76.
  EXPECT(s.list.size() == 86);
  EXPECT(s.fixed >= 0 && s.records[static_cast<size_t>(s.fixed)].id == mw1::kEventContinueOrQuit);
  std::map<int32_t, int32_t> by_class;
  int32_t commands = 0, choices = 0;
  for (const mw1::Event& e : s.records) {
    EXPECT(e.at >= mw1::kRecordsStart && e.at < mw1::kRecordsEnd && e.id < mw1::kEventIds && e.cls < mw1::kClassCount);
    for (const mw1::Command& c : e.script) {
      ++commands;
      if (c.op == mw1::kChoices) {
        ++choices;
        EXPECT((c.captions.size() == 2 || c.captions.size() == 3) && c.branches.size() == c.captions.size());
        for (const std::string& cap : c.captions) EXPECT(!cap.empty());
      }
    }
  }
  for (int32_t r : s.list) ++by_class[s.records[static_cast<size_t>(r)].cls];
  std::printf("  %zu records, %d commands, %d choices; the list: %d arrival, %d Bar, %d drink, %d news, %d Mech Complex, %d office, "
              "%d after battle\n",
              s.records.size(), commands, choices, by_class[0], by_class[1], by_class[2], by_class[3], by_class[4], by_class[5], by_class[6]);
  EXPECT(by_class[3] == 56);  // the news
  // Event 0x72, the Dark Wing's: on story planet 5, after 0x43, in a window to 19 March 3029, two choices.
  const mw1::Event& dw = s.records[static_cast<size_t>(record_of(mw1::kEventDarkWingAttack))];
  bool window = false, two = false;
  for (const mw1::Command& c : dw.script) {
    if (c.action && c.op == mw1::kDateWindow) window = c.end.year == 3029 && c.end.month == 3 && c.end.day == 19;
    if (c.op == mw1::kChoices) two = c.captions.size() == 2;
  }
  EXPECT(window && two);
  // The hatred arrivals: each a standing under -4 with one House.
  for (int32_t id = mw1::kFirstHatredArrival; id <= mw1::kLastHatredArrival; ++id) {
    const int32_t r = record_of(id);
    EXPECT(r >= 0);
    if (r < 0) continue;
    const mw1::Command& c = s.records[static_cast<size_t>(r)].script[0];
    EXPECT(c.op == mw1::kStanding && c.threshold == -4 && c.value >= 1 && c.value <= mw1::kHouseCount);
  }
}

TEST(mw1_story_every_text_reads) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const mw1::Script& s = mw1::script();
  size_t widest = 0, lines = 0, pages = 0;
  for (const mw1::Event& e : s.records) {
    if (e.id >= mw1::kTextCount) continue;
    const std::vector<mw1::Page> ps = mw1::text_pages(e.id);
    EXPECT(!ps.empty());
    for (const mw1::Page& p : ps) {
      ++pages;
      for (const std::string& l : p.lines) {
        ++lines, widest = std::max(widest, l.size());
        for (char ch : l) EXPECT((ch >= 0x20 && ch < 0x7f) || static_cast<uint8_t>(ch) == 0xFF);
      }
    }
  }
  EXPECT(!mw1::text_pages(mw1::kEventQuiet).empty());
  std::printf("  %zu pages, %zu lines, the widest %zu characters\n", pages, lines, widest);
  EXPECT(widest <= 47);
  // A choice's page ends its text: the captions are its last lines.
  const std::vector<mw1::Page> dw = mw1::text_pages(mw1::kEventDarkWingAttack);
  EXPECT(!dw.empty() && dw.back().choices);
}

TEST(mw1_story_planets_roll_from_their_pools) {
  mw1::Rng rng(7);
  for (int32_t n = 0; n < 50; ++n) {
    mw1::Company c = mw1::new_company(rng);
    EXPECT(c.story.planets[0] == c.planet_number);
    for (int32_t i = 1; i < mw1::kStoryPlanets; ++i) {
      bool in = false;
      for (int32_t k = 0; k < mw1::kPoolSize[i]; ++k) in |= c.story.planets[i] == mw1::kPools[i][k];
      EXPECT(in);
    }
    mw1::Company old;
    mw1::roll_story_planets_for_old_save(old, rng);
    for (int32_t i = 0; i < mw1::kStoryPlanets; ++i) EXPECT(old.story.planets[i] != 0);
  }
}

TEST(mw1_story_runs_as_mw1_does) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  mw1::Rng rng(3);
  // The start world: the crest on the hub (0x74), once.
  mw1::Company c = on_story(mw1::kPools[0][0]);
  std::vector<int32_t> f = fire(c, mw1::kClassArrival, rng);
  EXPECT(has(f, 0x74) && c.story.seen[0x74]);
  EXPECT(!has(fire(c, mw1::kClassArrival, rng), 0x74));
  // The Bar's drinks there: a paid tip (0x06), then the next (0x0A), then nothing (the quiet text); each tip's price
  // is its own funds action.
  const int64_t funds = c.funds;
  f = fire(c, mw1::kClassBarDrink, rng);
  EXPECT(f.size() == 1 && f[0] == 0x06 && c.funds == funds - 5);
  f = fire(c, mw1::kClassBarDrink, rng);
  EXPECT(f.size() == 1 && f[0] == 0x0A && c.funds == funds - 10);
  f = fire(c, mw1::kClassBarDrink, rng);
  EXPECT(f.size() == 1 && f[0] == mw1::kEventQuiet);
  // Funds never go below none.
  mw1::Company poor = on_story(30);  // Land's End: event 0x0B costs 15
  poor.funds = 7;
  f = fire(poor, mw1::kClassBarDrink, rng);
  EXPECT(has(f, 0x0B) && poor.funds == 0);
  // A choice: Grig's base on story planet 1 after 0x0C (0x64, YES -> 0x0E; NO -> 0x0F); its branch runs at once.
  for (int32_t pick = 0; pick < 2; ++pick) {
    mw1::Company g = on_story(mw1::kPools[1][0]);
    g.story.seen[0x0C] = 1;
    g.story.rung_won = 1;  // ours (item 47): Grig's men beaten first
    f = fire(g, mw1::kClassArrival, rng, pick);
    EXPECT(f.size() >= 2 && f[0] == 0x64 && f[1] == (pick ? 0x0F : 0x0E));
    EXPECT(g.story.seen[0x64] && g.story.seen[pick ? 0x0F : 0x0E] && !g.story.seen[pick ? 0x0E : 0x0F]);
  }
  // A planet named in the text: the man in the Bar (0x0C) names story planet 1 (ours: on Land's End, item 47).
  {
    mw1::Company g = on_story(mw1::kLandsEnd);
    g.story.seen[0x0B] = 1;
    mw1::Dispatch d = mw1::start(mw1::kClassBarEntry);
    mw1::Shown s;
    EXPECT(mw1::next(g, d, rng, &s) && s.event == 0x0C && s.named == 1);
    std::string all;
    for (const mw1::Page& p : mw1::shown_pages(g, s))
      for (const std::string& l : p.lines) all += l + "\n";
    EXPECT(all.find(mw1::kPlanets[mw1::planet_index(mw1::kPools[1][0])].name) != std::string::npos);
    EXPECT(all.find('\xff') == std::string::npos);
  }
  // A death (Brown's FIGHT, FIGHT: 0x17 -> 0x18 -> 0x1A): MW1 would restart.
  {
    mw1::Company g = on_story(48);  // Dustball
    g.story.seen[0x0E] = 1;
    mw1::Dispatch d = mw1::start(mw1::kClassArrival);
    mw1::Shown s;
    bool restart = false;
    for (int32_t guard = 0; guard < 10 && mw1::next(g, d, rng, &s); ++guard) {
      restart |= s.restart;
      if (!s.captions.empty() && !s.restart) mw1::choose(d, s, 0);
    }
    EXPECT(restart && g.story.seen[0x1A]);
  }
  // The hatred arrivals: only on arriving, never marked seen.
  {
    mw1::Company g = on_story(mw1::kPools[0][0]);  // Matsuida, Kurita: 0x4F
    g.grievance[mw1::kKurita] = 10;
    EXPECT(!has(fire(g, mw1::kClassArrival, rng), 0x4F));
    g.story.arrived = true;
    EXPECT(has(fire(g, mw1::kClassArrival, rng), 0x4F) && !g.story.seen[0x4F]);
    EXPECT(has(fire(g, mw1::kClassArrival, rng), 0x4F));
  }
  // The Dark Wing's world after the disk (0x43): ATTACK / DELAY on arriving (ours: not each time the hub is drawn),
  // never marked seen; no deadline (ours, the owner): after 19 March 3029 still asked, never too late (0x73).
  {
    mw1::Company g = on_story(mw1::kPools[5][0]);
    g.story.seen[0x43] = 1;
    g.date = {0, 2, 3029};  // 1 March
    EXPECT(!has(fire(g, mw1::kClassArrival, rng), mw1::kEventDarkWingAttack));  // the hub drawn again: not asked
    g.story.arrived = true;
    mw1::Dispatch d = mw1::start(mw1::kClassArrival);
    mw1::Shown s;
    EXPECT(mw1::next(g, d, rng, &s) && s.event == mw1::kEventDarkWingAttack && s.captions.size() == 2);
    EXPECT(!g.story.seen[mw1::kEventDarkWingAttack]);
    g.date = {40, 2, 3029};  // 21 March
    f = fire(g, mw1::kClassArrival, rng);
    EXPECT(has(f, mw1::kEventDarkWingAttack) && !has(f, mw1::kEventTooLate));
    g.date = {0, 0, 3035};
    f = fire(g, mw1::kClassArrival, rng);
    EXPECT(has(f, mw1::kEventDarkWingAttack) && !has(f, mw1::kEventTooLate) && !g.story.seen[mw1::kEventTooLate]);
  }
}

// FIXES.md item 47 (ours, the mw1 lead's plan): each step waits for its battle. The gates hold the next step until the
// rung is won; FIGHT is the battle (step_fight); a step left without it waits for the next arrival (kSeenWaiting);
// a won battle's page is due until shown; MW1's deaths and stalling choices are setbacks, a fine capped at a third of
// the funds, a lancemate gone, or a turn-away from Ander's Moon.
TEST(mw1_story_steps_wait_for_their_battles) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  mw1::Rng rng(5);
  // Rung 1: 0x0C only on Land's End, a choice of MW1's FIGHT / RUN (0x27's captions), still naming story planet 1.
  {
    mw1::Company g = on_story(48);  // Dustball
    g.story.seen[0x0B] = 1;
    EXPECT(!has(fire(g, mw1::kClassBarEntry, rng), mw1::kEventGrigsMen));
    mw1::Company l = on_story(mw1::kLandsEnd);
    l.story.seen[0x0B] = 1;
    mw1::Dispatch d = mw1::start(mw1::kClassBarEntry);
    mw1::Shown s;
    EXPECT(mw1::next(l, d, rng, &s) && s.event == mw1::kEventGrigsMen && s.named == 1);
    const std::vector<std::string> fr = mw1::fight_or_run();
    EXPECT(fr.size() == 2 && s.captions == fr && s.branches == std::vector<int32_t>({-1, -1}));
    const mw1::Event& e27 = mw1::script().records[static_cast<size_t>(mw1::record_of(0x27))];
    bool same = false;
    for (const mw1::Command& cmd : e27.script)
      if (cmd.captions.size() >= 2) same = cmd.captions[0] == fr[0] && cmd.captions[1] == fr[1];
    EXPECT(same);
    // RUN (no branch): the step waits; on the next Bar entry there it's asked again; won, it's MW1's text alone.
    mw1::choose(d, s, 1);
    while (mw1::next(l, d, rng, &s)) {}
    mw1::steps_wait(l);
    EXPECT(l.story.seen[mw1::kEventGrigsMen] == mw1::kSeenWaiting);
    EXPECT(has(fire(l, mw1::kClassBarEntry, rng), mw1::kEventGrigsMen));
    l.story.rung_won = 1;
    mw1::steps_wait(l);
    EXPECT(l.story.seen[mw1::kEventGrigsMen] == 1);
  }
  // The gates: Grig's base (0x64) for rung 1, the Stone Arrow (0x24) for 2.
  {
    mw1::Company g = on_story(mw1::kPools[1][0]);
    g.story.seen[0x0C] = 1;
    EXPECT(!has(fire(g, mw1::kClassArrival, rng), 0x64));
    g.story.rung_won = 1;
    EXPECT(has(fire(g, mw1::kClassArrival, rng), 0x64));
    mw1::Company b = on_story(48);
    b.story.seen[0x17] = 1, b.story.rung_won = 1;
    EXPECT(!has(fire(b, mw1::kClassBarDrink, rng), 0x24));
    b.story.rung_won = 2;
    EXPECT(has(fire(b, mw1::kClassBarDrink, rng), 0x24));
    EXPECT(mw1::step_gate(0x2D) == 3 && mw1::step_gate(0x2E) == 3 && mw1::step_gate(0x40) == 4 && mw1::step_gate(0x43) == 5 &&
           mw1::step_gate(0x17) == 0);
  }
  // The FIGHTs: their rungs, the second at Dustball at its hardest; RUN isn't one.
  {
    struct Want {
      int32_t event, choice, rung;
      bool hardest;
    };
    for (const Want& w : {Want{0x0C, 0, 1, false}, Want{0x17, 0, 2, false}, Want{0x19, 0, 2, true}, Want{0x27, 0, 3, false},
                          Want{0x30, 1, 4, false}, Want{0x32, 0, 4, false}, Want{0x40, 0, 5, false}}) {
      const mw1::StepFight* f = mw1::step_fight(w.event, w.choice);
      EXPECT(f && f->rung == w.rung && f->hardest == w.hardest);
    }
    EXPECT(!mw1::step_fight(0x17, 1) && !mw1::step_fight(0x27, 1) && !mw1::step_fight(0x30, 0) && !mw1::step_fight(0x40, 1));
    // Each FIGHT is a choice of the exe's script there, and its branch MW1's (0x30's second: to 0x33).
    const mw1::Script& sc = mw1::script();
    for (int32_t ev : {0x17, 0x19, 0x27, 0x30, 0x32, 0x40}) {
      bool chooses = false;
      for (const mw1::Command& cmd : sc.records[static_cast<size_t>(mw1::record_of(ev))].script) chooses |= cmd.branches.size() >= 2;
      EXPECT(chooses);
    }
  }
  // A hub step left without its battle (Brown's, RUN then RUN): waits for the next arrival, not the hub drawn again.
  {
    mw1::Company g = on_story(48);
    g.story.seen[0x0E] = 1;
    std::vector<int32_t> f = fire(g, mw1::kClassArrival, rng, 1);
    EXPECT(has(f, 0x17) && has(f, 0x19) && has(f, 0x1C));
    mw1::steps_wait(g);
    EXPECT(g.story.seen[0x17] == mw1::kSeenWaiting);
    EXPECT(!has(fire(g, mw1::kClassArrival, rng, 1), 0x17));
    g.story.arrived = true;
    EXPECT(has(fire(g, mw1::kClassArrival, rng, 1), 0x17));
  }
  // A won battle's page: due until shown (0x18 marked, the exchange the battle stood for); a quiet one marked at once;
  // 0x3E's choice due until TASHA (0x43).
  {
    mw1::Company g = on_story(48);
    g.story.seen[0x0E] = g.story.seen[0x17] = 1;
    g.story.rung_won = 3;
    const mw1::StepFight* f = mw1::step_win_due(g);
    EXPECT(f && f->won == 0x1B && g.story.seen[0x18] && g.story.seen[0x29]);
    mw1::Shown s;
    EXPECT(mw1::run_event(g, mw1::record_of(0x1B), mw1::kClassArrival, rng, &s) == mw1::Ran::kShown && g.story.seen[0x1B]);
    EXPECT(!mw1::step_win_due(g));
    g.story.seen[0x30] = g.story.seen[0x33] = g.story.seen[0x40] = 1, g.story.rung_won = 5;
    f = mw1::step_win_due(g);
    EXPECT(f && f->won == 0x3E);
    EXPECT(mw1::run_event(g, mw1::record_of(0x3E), mw1::kClassArrival, rng, &s) == mw1::Ran::kChoice && s.captions.size() == 2);
    EXPECT(mw1::step_win_due(g) == f);  // the choice not made yet: still due
    g.story.seen[0x43] = 1;
    EXPECT(!mw1::step_win_due(g));
  }
  // The setbacks: every death and stalling choice the lead listed has one; fines capped at a third of the funds.
  {
    for (int32_t ev : {0x0F, 0x1A, 0x28, 0x34, 0x3F, 0x44, 0x4D, 0x75}) EXPECT(mw1::setback_of(ev) != nullptr);
    EXPECT(!mw1::setback_of(0x0E) && mw1::setback_of(0x44)->reoffer == 0x3E && mw1::setback_of(0x4D)->again == 0x45);
    mw1::Company g = on_story(48);
    g.funds = 90000;
    const mw1::SetbackDone d = mw1::apply_setback(g, *mw1::setback_of(0x3F), rng);
    EXPECT(d.fined == 30000 && g.funds == 60000 && !d.line.empty());
    EXPECT(!g.log.empty() && g.log[0].kind == mw1::LogKind::kStoryFunds && g.log[0].n == -30000 &&
           mw1::log_lines(g, g.log[0]) == std::vector<std::string>{"Paid 30,000 C-bills."});
    g.funds = 1000000;
    mw1::apply_setback(g, *mw1::setback_of(0x4D), rng);
    EXPECT(g.funds == 800000 && g.story.seen[0x45] == mw1::kSeenWaiting);
  }
  // A lancemate lost: back in the pool (missing), their mech kept; with none, a fine.
  {
    mw1::Company g = mw1::new_company(rng);
    g.lance = {{1, 0}, {41, 0}};
    for (int32_t id : {1, 41}) {
      g.pilots[id].where = mw1::PilotWhere::kHired;
      mw1::OwnedMech o;
      o.pilot = id;
      g.hangar.push_back(o);
    }
    const size_t hangar = g.hangar.size();
    const int64_t funds = g.funds;
    const mw1::SetbackDone d = mw1::apply_setback(g, *mw1::setback_of(0x34), rng);
    EXPECT((d.lost == 1 || d.lost == 41) && g.lance.size() == 1 && g.pilots[d.lost].where == mw1::PilotWhere::kFree &&
           g.hangar.size() == hangar && g.funds == funds);
    g.lance.clear();
    const mw1::SetbackDone n = mw1::apply_setback(g, *mw1::setback_of(0x44), rng);
    EXPECT(n.lost == 0 && n.fined == 100000 && g.funds == funds - 100000);
  }
  // Ander's Moon before the win: fined (10%, at least 50,000, at most a third) and turned away to another world, the
  // trip's time passing; 0x75 never marked seen, so it fires on every arrival there until the Dark Wing is won.
  {
    mw1::Company g = on_story(mw1::kAndersMoon);
    g.funds = 1000000;
    const mw1::Date before = g.date;
    mw1::Dispatch dd = mw1::start(mw1::kClassArrival);
    mw1::Shown s;
    EXPECT(mw1::next(g, dd, rng, &s) && s.event == mw1::kEventPortArrest && s.restart);
    const mw1::SetbackDone d = mw1::apply_setback(g, *mw1::setback_of(mw1::kEventPortArrest), rng);
    EXPECT(d.fined == 100000 && d.moved >= 0 && g.planet_number != mw1::kAndersMoon && !g.story.seen[mw1::kEventPortArrest]);
    EXPECT(g.date.year != before.year || g.date.month != before.month || g.date.units != before.units);
    // FIXES.md item 79: the fine and the move in the log (the soak found the funds and the world changed unlogged).
    bool paid = false, turned = false;
    for (const mw1::LogEntry& e : g.log) {
      paid = paid || (e.kind == mw1::LogKind::kStoryFunds && e.a == mw1::kEventPortArrest && e.n == -100000);
      turned = turned || (e.kind == mw1::LogKind::kTurnedAway && e.b == d.moved && e.a == mw1::planet_index(mw1::kAndersMoon));
    }
    EXPECT(paid && turned);
    mw1::Company p = on_story(mw1::kAndersMoon);
    p.funds = 120000;
    EXPECT(mw1::setback_fine(p, *mw1::setback_of(mw1::kEventPortArrest)) == 40000);
  }
  // A step battle at its hardest: an escort more and the next rung's aim.
  {
    mw1::Company g = on_story(48);
    const mw1::Contract k = mw1::story_battle(g, 2, rng, true), n = mw1::story_battle(g, 2, rng);
    EXPECT(k.hardest && k.enemies.size() == n.enemies.size() + 1 && mw1::story_aim(k) == mw1::rung(3).aim &&
           mw1::story_aim(n) == mw1::rung(2).aim);
  }
  // Saved: the waiting steps, and a battle's hardest.
  {
    mw1::Company g = mw1::new_company(rng);
    g.story.seen[0x17] = mw1::kSeenWaiting, g.story.seen[0x0C] = 1, g.story.rung_won = 1;
    mw1::Contract k = mw1::story_battle(g, 2, rng, true);
    const std::string text = mw1::save_text(g, &k);
    mw1::Company back;
    mw1::Contract kb;
    bool has_offer = false;
    std::string why;
    EXPECT(mw1::load_text(text, back, kb, &has_offer, &why));
    EXPECT(back.story.seen == g.story.seen && has_offer && kb.hardest && kb.rung == 2 && mw1::save_text(back, &kb) == text);
  }
}

TEST(mw1_story_the_news_comes_and_goes) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  mw1::Rng rng(4);
  mw1::Company c = on_story(mw1::kPools[0][0]);
  // The new campaign's first day: the frame-up (0x01) is news, announced once.
  std::vector<mw1::Shown> f = mw1::fresh_news(c, rng);
  EXPECT(f.size() == 1 && f[0].event == 0x01 && !mw1::headline(c, f[0]).empty());
  EXPECT(mw1::fresh_news(c, rng).empty());
  EXPECT(mw1::live_news(c, rng).size() == 1);
  // Six months on it's gone.
  c.date = {1, 10, 3024};
  bool first = false;
  for (const mw1::Shown& s : mw1::live_news(c, rng)) first |= s.event == 0x01;
  EXPECT(!first);
  // The birthday message comes each year, a record of its own each time: announced each year.
  int32_t birthdays = 0;
  for (int32_t y = 3025; y <= 3029; ++y) {
    c.date = {20, 3, y};  // 11 April
    for (const mw1::Shown& s : mw1::fresh_news(c, rng)) birthdays += s.event == 0x03;
  }
  EXPECT(birthdays == 5);
  // The story's news: by its conditions, from then on (0x2B after the escape, 0x29).
  c.date = {1, 3, 3024};
  c.story.seen[0x29] = 1;
  bool escape = false;
  for (const mw1::Shown& s : mw1::fresh_news(c, rng)) escape |= s.event == 0x2B && s.named == 2;
  EXPECT(escape);
}

TEST(mw1_story_errata_are_off_until_fixed) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  EXPECT(!mw1::kFixErrata);
  mw1::Script s = mw1::script();  // a copy
  int32_t as_mw1 = 0;
  for (const mw1::Event& e : s.records)
    for (const mw1::Command& c : e.script)
      for (const mw1::Erratum& x : mw1::kErrata)
        as_mw1 += e.id == x.event && c.op == mw1::kDate && !c.action && c.date.month == x.as_mw1.month && c.date.day == x.as_mw1.day;
  EXPECT(as_mw1 == mw1::kErrataCount);
  mw1::fix_errata(s);
  int32_t fixed = 0;
  for (const mw1::Event& e : s.records)
    for (const mw1::Command& c : e.script)
      for (const mw1::Erratum& x : mw1::kErrata)
        fixed += e.id == x.event && c.op == mw1::kDate && !c.action && c.date.month == x.fixed.month && c.date.day == x.fixed.day;
  EXPECT(fixed == mw1::kErrataCount);
  // As MW1 has it, 28/1/3027's story (0x5E) isn't news in February 3027 but is in 3028.
  mw1::Rng rng(5);
  mw1::Company c = on_story(mw1::kPools[0][0]);
  auto live = [&](int32_t id) {
    for (const mw1::Shown& sh : mw1::live_news(c, rng))
      if (sh.event == id) return true;
    return false;
  };
  c.date = {0, 1, 3027};
  EXPECT(!live(0x5E));
  c.date = {0, 1, 3028};
  EXPECT(live(0x5E));
}

TEST(mw1_story_saves_and_reads_back) {
  mw1::Rng rng(1);
  mw1::Company c = mw1::new_company(rng);
  c.story.seen[0x74] = c.story.seen[0x06] = c.story.seen[0x43] = 1;
  for (int32_t i = 0; i < mw1::kStoryPlanets; ++i) c.story.planets[i] = mw1::kPools[i][1];
  c.story.news_heard = {0x6400, 0x6410};
  c.story.dark_wing_won = true;
  const std::string text = mw1::save_text(c, nullptr);
  EXPECT(text.find("version=" + std::to_string(mw1::kSaveVersion) + "\n") != std::string::npos);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  EXPECT(back.story.seen == c.story.seen && back.story.news_heard == c.story.news_heard && back.story.dark_wing_won);
  for (int32_t i = 0; i < mw1::kStoryPlanets; ++i) EXPECT(back.story.planets[i] == c.story.planets[i]);
  EXPECT(mw1::save_text(back, nullptr) == text);
  // A version 7 save (no story lines): nothing seen, the planets not rolled (the loop rolls them at the load).
  std::string v7;
  {
    std::istringstream in(text.substr(0, text.rfind("check=")));
    std::string line;
    while (std::getline(in, line))
      if (line.rfind("story_", 0) != 0) v7 += (line.rfind("version=", 0) == 0 ? std::string("version=7") : line) + "\n";
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : v7) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  mw1::Company old;
  EXPECT(mw1::load_text(v7 + check, old, k, &has, &why));
  EXPECT(old.story.planets[0] == 0 && !old.story.seen[0x74] && old.story.news_heard.empty());
  // Story lines out of range: refused.
  std::string bad = text.substr(0, text.rfind("check="));
  bad.replace(bad.find("story_seen=") + 11, 0, "140,");
  h = 2166136261u;
  for (unsigned char ch : bad) h = (h ^ ch) * 16777619u;
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  why.clear();
  EXPECT(!mw1::load_text(bad + check, old, k, &has, &why) && why.find("story") != std::string::npos);
  // FIXES.md item 47: the rung won saves; a save without its line reads none won, or the Dark Wing's with it won.
  auto signed_text = [](std::string t) {
    uint32_t hh = 2166136261u;
    for (unsigned char ch : t) hh = (hh ^ ch) * 16777619u;
    char ck[32];
    std::snprintf(ck, sizeof ck, "check=%08x\n", hh);
    return t + ck;
  };
  mw1::Company r3 = c;
  r3.story.dark_wing_won = false, r3.story.rung_won = 3;
  const std::string t3 = mw1::save_text(r3, nullptr);
  EXPECT(t3.find("story_rung=3\n") != std::string::npos && mw1::load_text(t3, back, k, &has, &why) && back.story.rung_won == 3);
  std::string none = t3.substr(0, t3.rfind("check="));
  none.erase(none.find("story_rung=3\n"), 13);
  mw1::Company n0;
  EXPECT(mw1::load_text(signed_text(none), n0, k, &has, &why) && n0.story.rung_won == 0);
  std::string dw = text.substr(0, text.rfind("check="));
  dw.erase(dw.find("story_rung=0\n"), 13);
  mw1::Company n6;
  EXPECT(mw1::load_text(signed_text(dw), n6, k, &has, &why) && n6.story.rung_won == mw1::kDarkWingRung);
  std::string far = none + "story_rung=7\n";
  EXPECT(!mw1::load_text(signed_text(far), n0, k, &has, &why));
}

// FIXES.md item 47 (mw1_ladder.h): the ladder in one table: six rungs, their forces rising; rung 6 the Dark Wing's
// eight (a 70 t and four 85 t, three escorts, 635 t), all eight with a full lance
// (item 48's line-up of 16), the escorts trimmed past that (a lance the line-up can't hold) and never its core; a story battle a destroy-all with no pay on a map that holds it.
TEST(mw1_ladder_rungs_and_the_dark_wing_force) {
  int32_t last = 0;
  for (int32_t r = 1; r <= mw1::kRungs; ++r) {
    const std::vector<int32_t> f = mw1::rung_force(r, 0);
    std::printf("  rung %d: %zu mechs, %d t, aim x%.1f\n", r, f.size(), mw1::rung_tons(f), mw1::rung(r).aim);
    EXPECT(mw1::rung_tons(f) > last && static_cast<int32_t>(f.size()) <= mw1::kRungMechs);
    last = mw1::rung_tons(f);
  }
  const std::vector<int32_t> dw = mw1::rung_force(mw1::kDarkWingRung, 0);
  int32_t heaviest = 0;
  for (int32_t k : dw) heaviest += k == mw1::kSlot85;
  EXPECT(dw.size() == 8 && dw[0] == mw1::kSlot70 && heaviest == 4 && mw1::rung_tons(dw) == 635);
  EXPECT(mw1::rung_force(mw1::kDarkWingRung, 2).size() == 8);
  const std::vector<int32_t> full = mw1::rung_force(mw1::kDarkWingRung, 3);
  EXPECT(full == dw);
  const std::vector<int32_t> trimmed = mw1::rung_force(mw1::kDarkWingRung, 9);  // 16 - 9: seven
  EXPECT(trimmed.size() == 7 && std::equal(trimmed.begin(), trimmed.end(), dw.begin()));
  EXPECT(mw1::rung_force(mw1::kDarkWingRung, 12).size() == 5);  // never below its core
  // As a contract: the world's House, no House against, nothing paid, a map of the climate that holds it.
  mw1::Rng rng(3);
  mw1::Company c = mw1::new_company(rng);
  mw1::set_map_table({{"c1", 1, mw1::kDesert, mw1::RowKind::kAny, "", ""}, {"mw3c2", 1, mw1::kTropical, mw1::RowKind::kAny, "", ""},
                      {"zz", 1, mw1::kDesert, mw1::RowKind::kAny, "", ""}});
  for (int32_t i = 0; i < 20; ++i) {
    const mw1::Contract k = mw1::story_battle(c, mw1::kDarkWingRung, rng);
    EXPECT(k.rung == 6 && k.accepted && k.payment == 0 && k.remaining == 0 && k.target >= mw1::kHouseCount && k.enemies.size() == 8);
    EXPECT(k.objective == mw1::Objective::kDestroyAll && k.world != "zz");  // "zz": not surveyed, never the Dark Wing's
    EXPECT(k.climate != mw1::kDesert || k.world == "c1");
  }
  mw1::set_map_table({});
}

// FIXES 114: the swap itself, on made-up names in the table's places (MW1's own never kept here): any case, a space
// or none, a plural, a possessive, upper case kept, whole words only; the 70 t record the Thor, the 85 t the Atlas.
TEST(mw1_story_our_names_for_mw1s) {
  std::vector<std::string> names = {"KIND ZERO", "KINDONE", "KINDTWO", "KIND THREE", "KINDFOUR",
                                    "KINDFIVE",  "KINDSIX", "OLDHAMMER", "KINDEIGHT", "BIGCHIEF"};
  const std::string thor = mw1::mech_title(mw1::kPmThor), atlas = mw1::mech_title(mw1::kPmAtlas);
  auto upper = [](std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
  };
  EXPECT(mw1::our_names("an Oldhammer and four BigChiefs.", names) == "an " + thor + " and four " + atlas + "es.");
  EXPECT(mw1::our_names("THE BIG CHIEF'S GUNS", names) == "THE " + upper(atlas) + "'S GUNS");
  EXPECT(mw1::our_names("Oldhammers' and oldhammer-like", names) == thor + "s' and " + thor + "-like");
  EXPECT(mw1::our_names("Kind-Three, KINDZERO", names) ==
         mw1::mech_title(mw1::kPmShadowCat) + ", " + upper(mw1::mech_title(mw1::kPmOwens)));
  EXPECT(mw1::our_names("Oldhammered Oldhammerz xOldhammer", names) == "Oldhammered Oldhammerz xOldhammer");
  EXPECT(mw1::our_names("no names", {}) == "no names");
  for (int32_t k = 0; k < mw1::kMw1KindCount; ++k) {  // every record's ours a fielding 'Mech of its class or near
    const int32_t m = mw1::kOurChassis[k];
    EXPECT(m >= 0 && m < mw1::kPmMechCount && m != mw1::kPmElemental);
  }
}

// With the player's exe: MW1's kind names read from it (ten, upper case), and no story text, choice or Bar pitch we
// draw names one of them any more; the Dark Wing's texts (0x2C, 0x43) name the Thor and the Atlas instead. Counts
// only (MW1's words are never printed).
TEST(mw1_story_no_mw1_mech_names_drawn) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const std::vector<std::string> names = mw1::mw1_kind_names();
  EXPECT(names.size() == static_cast<size_t>(mw1::kMw1KindCount));
  auto letters = [](const std::string& s) {
    std::string o;
    for (char c : s)
      if (std::isalpha(static_cast<unsigned char>(c))) o += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
  };
  auto named = [&](const std::string& line) {  // MW1's names, the line's letters run together (spaces dropped)
    const std::string l = letters(line);
    int32_t n = 0;
    for (const std::string& k : names) n += l.find(letters(k)) != std::string::npos;
    return n;
  };
  int32_t lines = 0, left = 0, thor = 0, atlas = 0;
  const std::string t = letters(mw1::mech_title(mw1::kPmThor)), a = letters(mw1::mech_title(mw1::kPmAtlas));
  for (int32_t id = 0; id < mw1::story_rules::kTextCount; ++id)
    for (const mw1::story_rules::Page& p : mw1::story_rules::text_pages(id))
      for (const std::string& l : p.lines) {
        ++lines, left += named(l);
        if (id == 0x2C || id == 0x43) thor += letters(l).find(t) != std::string::npos, atlas += letters(l).find(a) != std::string::npos;
      }
  for (const mw1::story_rules::Event& e : mw1::story_rules::script().records)
    for (const mw1::story_rules::Command& c : e.script)
      for (const std::string& cap : c.captions) ++lines, left += named(cap);
  for (int32_t id = 1; id < mw1::kPilotCount + 1; ++id)
    for (const std::string& l : mw1::pitch(id)) ++lines, left += named(l);
  std::printf("  %d lines drawn from the exe: %d name one of MW1's 'Mechs; the Dark Wing's texts: %d with the Thor, %d the Atlas\n",
              lines, left, thor, atlas);
  EXPECT(lines > 500 && left == 0 && thor >= 1 && atlas >= 1);
  mw1::set_mw1_game_dir("");
}
