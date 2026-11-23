#pragma once
// The journal (FIXES.md item 47, the owner 2026-11-18: "a journal that says how to do the next thing in the story and
// points right to the spot"): the STORY page's words. The chapters are every story event seen, MW1's own text; the
// lead is our own short line naming the planet and the place of the next step; the strength is the step's battle's
// expected force, from the ladder (mw1_ladder.h). No SDL and no screens here, and nothing written: the journal reads
// the company's story (seen, its planets, the rungs won) and the ladder, and nothing more.
//
// The trail (research/story.md's chain, the leads checked against the exe's script by test_mw1_journal.cpp): MW1's
// steps fire on a class (arrival, Bar, a drink, Mech Complex, office, news) on a planet, a fixed one or one of the
// six story planets, which the lead names only once MW1's own text has named it. No choice ever ends or stalls the
// trail (the mw1 lead, 2026-11-18; dev 1's setbacks): a choice that would, fires again on the next arrival there, and
// the lead says "<what happened>. Try again: <place> on <planet>."
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_story.h"

namespace mw1::journal {

// Where a lead's step fires: MW1's classes, as the player finds them.
enum class Place : int32_t { kNone, kArrival, kBar, kMechComplex, kOffice, kNews };
const char* place_words(Place p);  // "the Bar", "the Mech Complex", ...

struct Lead {
  int32_t step = 0;         // the chain's step (1..kLeadSteps), kLeadSteps + 1 when the story is won
  int32_t event = -1;       // the event the lead points to (-1: none, the story won)
  int32_t planet = -1;      // kPlanets index; -1 anywhere (a drink in any Bar, any Mech Complex)
  Place place = Place::kNone;
  std::string line;         // our words: "Go to the House office on Galedon V."
  int32_t rung = 0;         // the step's battle still to win (mw1_ladder.h), 0 none
  std::string strength;     // the battle's expected force ("" without one)
  std::string side;         // the side line (Grig's bounty), "" none
  std::string caution;      // "Don't go home to Ander's Moon yet." until the Dark Wing is beaten
  bool retry = false;       // a setback's: the step fires again there
};
inline constexpr int32_t kLeadSteps = 18;

// The lead now (the rungs won: Story::rung_won, written by the step battles; they're won in order).
Lead current_lead(const Company& c);

// "Expect 4 heavy 'Mechs, about 265 tons, sharp pilots. Bring a full lance.": rung `r`'s force against a lance of
// `lancemates` (rung_force), its classes by MW1's tonnage, its pilots by the rung's aim.
std::string strength_line(int32_t r, int32_t lancemates);
const char* weight_class(int32_t tons);  // "light", "medium", "heavy", "assault"
const char* pilot_words(float aim);      // "regular", "seasoned", "sharp", "hardened", "veteran", "elite"

// A chapter: a story event seen, its pages as MW1 shows them (shown_pages), in the trail's order (no dates; the mw1
// lead 2026-11-18). The news, the hatred arrivals and the quiet drink aren't chapters.
struct Chapter {
  int32_t event = -1;
  std::string title;        // its headline (the first line before a blank one)
  std::vector<Page> pages;
};
std::vector<Chapter> chapters(const Company& c);

// The planets the lead's words use: a story planet's kPlanets index (-1 unrolled), a fixed one's by its name.
int32_t story_planet(const Company& c, int32_t ref);
int32_t planet_named(const char* name);  // "GALEDON V" -> its index, -1 none

// The STORY page's pages (on the event panel's full page, mw1_event_screen.h): every chapter's, then the lead's last
// (the page opens on it; PREV reads back through the chapters), each with its title for the plaque ("CHAPTER 3 OF 27",
// "THE LEAD"). The lead's words wrapped to kColumns: what and where, the way there (days from here), the strength, the
// side line and the caution.
inline constexpr size_t kColumns = 50;  // the full page's terminal (mw1_term_436x300, 8 pixels a letter)
Page lead_page(const Company& c, const Lead& l);
struct JournalPage {
  Page page;
  std::string title;
};
std::vector<JournalPage> journal_pages(const Company& c);
// GO THERE (the lead's page's button): the lead names a world that isn't the company's.
bool can_go_there(const Company& c, const Lead& l);
std::vector<std::string> wrapped(const std::string& text, size_t columns);  // words to lines, none longer (but a word)
// FIXES 95: MW1's lines of one speech (a recruit's pitch: the exe breaks it for its own screen) as one quote: the lines
// joined by a space (their own spaces at the ends dropped, a blank one skipped), a quote mark before the first word and
// after the last, wrapped to `columns` with the marks counted. MW1's words and case kept; nothing else added. None: none.
std::vector<std::string> quoted(const std::vector<std::string>& lines, size_t columns);

// For the shots (MW1_SCREEN=journal, MW1_STORY_STEP): the trail taken up to step `step` (each earlier step's events
// seen and its battle won), as a walk leaves it, near enough for a picture.
void seed_for_shots(Company& c, int32_t step);

}  // namespace mw1::journal
