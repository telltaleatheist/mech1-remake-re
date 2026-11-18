// MW1's story (src/mw1_story.h): the script and texts read from the player's MW_MAIN.EXE against its data segment,
// the story planets, the rules as MW1 runs them (conditions, actions, choices, the news), the errata, and the save.
// No words of MW1's here: the checks count and compare, and print counts.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_campaign.h"
#include "mw1_pilots.h"
#include "mw1_save.h"
#include "mw1_story.h"
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
    f = fire(g, mw1::kClassArrival, rng, pick);
    EXPECT(f.size() >= 2 && f[0] == 0x64 && f[1] == (pick ? 0x0F : 0x0E));
    EXPECT(g.story.seen[0x64] && g.story.seen[pick ? 0x0F : 0x0E] && !g.story.seen[pick ? 0x0E : 0x0F]);
  }
  // A planet named in the text: the man in the Bar (0x0C) names story planet 1.
  {
    mw1::Company g = on_story(mw1::kPools[0][0]);
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
  // The Dark Wing's world after the disk (0x43): ATTACK / DELAY until 19 March 3029, never marked seen; after it, too
  // late (0x73).
  {
    mw1::Company g = on_story(mw1::kPools[5][0]);
    g.story.seen[0x43] = 1;
    g.date = {0, 2, 3029};  // 1 March
    mw1::Dispatch d = mw1::start(mw1::kClassArrival);
    mw1::Shown s;
    EXPECT(mw1::next(g, d, rng, &s) && s.event == mw1::kEventDarkWingAttack && s.captions.size() == 2);
    EXPECT(!g.story.seen[mw1::kEventDarkWingAttack]);
    g.date = {40, 2, 3029};  // 21 March
    f = fire(g, mw1::kClassArrival, rng);
    EXPECT(has(f, mw1::kEventTooLate) && !has(f, mw1::kEventDarkWingAttack));
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
}
