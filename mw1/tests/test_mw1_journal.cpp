// The journal (FIXES.md item 47, mw1_journal.h): a headless walk of the whole chain through the leads (each lead's
// planet and place where its event fires next, each step's battle pending until its rung is won), the setbacks'
// retry lines, the strength lines from the ladder, and the chapters as MW1's own text.
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_campaign.h"
#include "mw1_journal.h"
#include "mw1_ladder.h"
#include "mw1_pilots.h"
#include "mw1_story.h"
#include "mw1_travel.h"
#include "mw1_event_screen.h"
#include "mw1_travel_screen.h"

namespace {

namespace J = mw1::journal;

bool story_there() {
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  return mw1::script().there();
}

// The class a lead's place fires as.
int32_t class_of(J::Place p) {
  switch (p) {
    case J::Place::kBar: return -1;  // a Bar: its entry, then a drink
    case J::Place::kMechComplex: return mw1::kClassMechComplex;
    case J::Place::kOffice: return mw1::kClassOffice;
    default: return mw1::kClassArrival;
  }
}

// The choices that carry the trail on (the commander lives and follows): by event.
const std::map<int32_t, int32_t> kPick = {{0x64, 0}, {0x17, 1}, {0x18, 1}, {0x19, 1}, {0x27, 1}, {0x2F, 0}, {0x30, 1},
                                          {0x32, 0}, {0x37, 0}, {0x40, 0}, {0x3E, 0}, {0x45, 1}, {0x72, 0}};

// Every event a class fires now, the choices answered from kPick (or `pick` for this event).
std::vector<int32_t> fire(mw1::Company& c, int32_t cls, mw1::Rng& rng, int32_t only_event = -1, int32_t pick = -1) {
  std::vector<int32_t> out;
  mw1::Dispatch d = mw1::start(cls);
  mw1::Shown s;
  for (int32_t guard = 0; guard < 40 && mw1::next(c, d, rng, &s); ++guard) {
    out.push_back(s.event);
    if (s.captions.empty()) continue;
    const auto k = kPick.find(s.event);
    const int32_t i = s.event == only_event && pick >= 0 ? pick : k != kPick.end() ? k->second : 0;
    mw1::choose(d, s, i);
  }
  return out;
}

std::vector<int32_t> visit(mw1::Company& c, const J::Lead& l, mw1::Rng& rng) {
  if (l.planet >= 0) mw1::set_planet(c, l.planet);
  if (l.place == J::Place::kNews) {
    std::vector<int32_t> out;
    for (const mw1::Shown& s : mw1::live_news(c, rng)) out.push_back(s.event);
    return out;
  }
  const int32_t cls = class_of(l.place);
  if (cls == mw1::kClassArrival) {  // as the game arrives: the hub's events once, then the flag cleared
    c.story.arrived = true;
    std::vector<int32_t> out = fire(c, cls, rng);
    c.story.arrived = false;
    return out;
  }
  if (cls >= 0) return fire(c, cls, rng);
  std::vector<int32_t> out = fire(c, mw1::kClassBarEntry, rng);
  for (int32_t e : fire(c, mw1::kClassBarDrink, rng)) out.push_back(e);
  return out;
}

bool has(const std::vector<int32_t>& v, int32_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

mw1::Company on_story() {
  mw1::Rng rng(7);
  mw1::Company c = mw1::new_company(rng);
  for (int32_t i = 1; i < mw1::kStoryPlanets; ++i) c.story.planets[i] = mw1::kPools[i][0];
  c.story.planets[0] = c.planet_number;
  c.date = mw1::Date{1, 4, 3026};  // past the Dark Wing's window's start
  return c;
}

}  // namespace

TEST(mw1_journal_walks_the_whole_chain_by_its_leads) {
  if (!story_there()) return;
  mw1::Company c = on_story();
  mw1::Rng rng(11);
  const std::vector<int32_t> planned = {0x06, 0x07, 0x08, 0x0B, 0x0C, 0x64, 0x17, 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2F, 0x36, 0x37, 0x40, 0x72};
  // The new campaign's first arrival: the crest (74).
  J::Lead first;
  first.place = J::Place::kArrival;
  EXPECT(has(visit(c, first, rng), 0x74) && J::current_lead(c).step == 1);
  int32_t last = 0;
  for (int32_t guard = 0; guard < 60; ++guard) {
    const J::Lead l = J::current_lead(c);
    if (l.step > J::kLeadSteps) break;
    EXPECT(l.step >= last);  // never back
    last = l.step;
    std::printf("  step %2d %-15s %-16s %s%s%s\n", l.step, l.planet >= 0 ? mw1::title_case_name(mw1::kPlanets[l.planet].name).c_str() : "(anywhere)",
                J::place_words(l.place), l.line.c_str(), l.strength.empty() ? "" : "  | ", l.strength.c_str());
    EXPECT(l.event == planned[static_cast<size_t>(l.step - 1)]);
    EXPECT(!l.retry && l.line.find("%s") == std::string::npos && l.line.find("  ") == std::string::npos);
    // A named planet is the one its step fires on; the line names it.
    if (l.planet >= 0) EXPECT(l.line.find(mw1::title_case_name(mw1::kPlanets[l.planet].name)) != std::string::npos);
    const std::vector<int32_t> fired = visit(c, l, rng);
    if (!has(fired, l.event)) {
      std::printf("  step %d: %02X didn't fire; fired:", l.step, l.event);
      for (int32_t e : fired) std::printf(" %02X", e);
      std::printf("\n");
    }
    EXPECT(has(fired, l.event));
    // The step's battle (a stand-in for dev 1's: won as it's offered), pending until won.
    if (l.rung) {
      EXPECT(!l.strength.empty() && l.strength.find("Expect ") == 0);
      if (l.event == mw1::kEventDarkWingAttack) c.story.dark_wing_won = true;
      c.story.rung_won = l.rung;
    }
  }
  const J::Lead end = J::current_lead(c);
  EXPECT(end.step == J::kLeadSteps + 1 && end.event == -1 && end.caution.empty() && end.side.empty());
  EXPECT(last == J::kLeadSteps && c.story.rung_won == mw1::kDarkWingRung);
  // The chapters: MW1's text for every step seen, the crest's first; no news, no hatred lines.
  const std::vector<J::Chapter> ch = J::chapters(c);
  std::printf("  %zu chapters, the first %02X\n", ch.size(), ch.empty() ? -1 : ch.front().event);
  EXPECT(ch.size() >= 25 && ch.front().event == 0x74);
  for (const J::Chapter& k : ch) EXPECT(!k.pages.empty() && !k.title.empty() && k.event != 0x36 && k.event != 0x4F);
}

TEST(mw1_journal_a_battle_waits_until_won) {
  if (!story_there()) return;
  mw1::Company c = on_story();
  for (int32_t id : {0x06, 0x07, 0x08, 0x0B}) c.story.seen[static_cast<size_t>(id)] = 1;
  J::Lead l = J::current_lead(c);
  EXPECT(l.step == 5 && l.rung == 1 && l.planet == J::planet_named("LAND'S END") && l.place == J::Place::kBar && !l.strength.empty());
  c.story.seen[0x0C] = 1;  // its FIGHT lost: seen (dev 1 clears it to fire again; either way the battle waits)
  EXPECT(J::current_lead(c).step == 5);
  c.story.rung_won = 1;
  l = J::current_lead(c);
  EXPECT(l.step == 6 && l.rung == 0 && l.strength.empty());
}

TEST(mw1_journal_setbacks_say_try_again_there) {
  if (!story_there()) return;
  mw1::Company c = on_story();
  for (int32_t id : {0x06, 0x07, 0x08, 0x0B, 0x0C, 0x0F}) c.story.seen[static_cast<size_t>(id)] = 1;
  c.story.rung_won = 1;
  // 64 NO (dev 1 clears 64's seen; 0F stays): try again on ref1.
  J::Lead l = J::current_lead(c);
  const std::string ref1 = mw1::title_case_name(mw1::kPlanets[J::story_planet(c, 1)].name);
  EXPECT(l.step == 6 && l.retry && l.line.find("Try again: arrive on " + ref1) != std::string::npos);
  // 44 (TRUST KEARNEY): 3E re-offered on Albiero.
  for (int32_t id = 0x0E; id <= 0x40; ++id) c.story.seen[static_cast<size_t>(id)] = 1;
  c.story.seen[0x3F] = 0, c.story.seen[0x43] = 0, c.story.seen[0x44] = 1;
  c.story.rung_won = 4;
  l = J::current_lead(c);
  EXPECT(l.step == 17 && l.retry && l.line.find("trust Tasha") != std::string::npos && l.line.find("Albiero") != std::string::npos);
  EXPECT(l.rung == 5 && l.strength.find("6 'Mechs") != std::string::npos);
}

TEST(mw1_journal_strength_from_the_ladder) {
  for (int32_t r = 1; r <= mw1::kRungs; ++r) std::printf("  rung %d: %s\n", r, J::strength_line(r, 3).c_str());
  EXPECT(J::strength_line(1, 0) == "Expect 2 medium 'Mechs, about 110 tons, regular pilots. Bring a lancemate.");
  const std::string six = J::strength_line(mw1::kDarkWingRung, 3);
  EXPECT(six.find("'Mechs, heavies and assaults") != std::string::npos && six.find("elite pilots") != std::string::npos);
  EXPECT(std::string(J::weight_class(35)) == "light" && std::string(J::weight_class(85)) == "assault");
}

// The STORY page: the chapters then the lead's page, opening on it with GO THERE in NEXT's place (live only when the
// lead names another world), each page's title on the plaque, every lead line within the terminal's width.
TEST(mw1_journal_the_page_and_go_there) {
  if (!story_there()) return;
  mw1::Company c = on_story();
  J::seed_for_shots(c, 7);
  const std::vector<J::JournalPage> pages = J::journal_pages(c);
  EXPECT(pages.size() >= 2 && pages.back().title == "THE LEAD" && pages.front().title.rfind("CHAPTER 1 OF ", 0) == 0);
  for (const std::string& l : pages.back().page.lines) EXPECT(l.size() <= J::kColumns);
  const J::Lead lead = J::current_lead(c);
  EXPECT(lead.planet == J::planet_named("DUSTBALL") && J::can_go_there(c, lead));
  mw1::EventPanel p;
  p.journal = true, p.planet = "STORY";
  for (const J::JournalPage& j : pages) p.pages.push_back(j.page), p.titles.push_back(j.title);
  p.page = p.pages.size() - 1, p.captions = {"GO THERE"}, p.live = {true};
  mw1::ScreenView v = mw1::event_view(p);
  EXPECT(v.buttons[mw1::kEventChoice2 - 1].there && v.buttons[mw1::kEventChoice2 - 1].on && v.buttons[mw1::kEventChoice2 - 1].words == "GO THERE");
  EXPECT(!v.buttons[mw1::kEventNext - 1].there && v.buttons[mw1::kEventPrev - 1].on && v.buttons[mw1::kEventBack - 1].there);
  EXPECT(!v.buttons[mw1::kEventChoice1 - 1].there && !v.buttons[mw1::kEventChoice3 - 1].there);
  p.page = 0;
  v = mw1::event_view(p);
  EXPECT(!v.buttons[mw1::kEventChoice2 - 1].there && v.buttons[mw1::kEventNext - 1].there && !v.buttons[mw1::kEventPrev - 1].on);
  // Already there: GO THERE off; anywhere: no world.
  mw1::set_planet(c, lead.planet);
  EXPECT(!J::can_go_there(c, J::current_lead(c)));
  // The star map marks the lead's world and names it with STORY (not when the company is there).
  mw1::Company away = on_story();
  J::seed_for_shots(away, 7);
  mw1::TravelChoice t;
  t.story = lead.planet;
  auto named_story = [](const mw1::ScreenView& sv) {
    for (const auto& b : sv.blocks)
      for (const std::string& l : b.lines)
        if (l.find("Dustball \xB7 STORY") != std::string::npos) return true;
    return false;
  };
  EXPECT(named_story(mw1::travel_view(away, t)));
  EXPECT(!named_story(mw1::travel_view(c, t)));
}
