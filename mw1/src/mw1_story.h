#pragma once
// MW1's story (VISION.md, "trying to clear the founder's name"; research/story.md): the event script MW1 keeps in its
// exe, read from the player's MW_MAIN.EXE at run time, and run as MW1 runs it. No SDL and no screens here.
//
// The script is data in MW_MAIN's data segment (0x1090): a list of event records (DS:6067 the lists, the new
// campaign's DS:0AB8), each an event id, the class saying where it fires (the hub, the Bar, a drink, the news, the
// Mech Complex, the House office, after a battle) and commands alternating condition and action up to 0xFF; the
// choices' captions (DS:5CF4); the texts in the exe's segment 1AA5. The rules are MW_MAIN 081B:6236..6666's (the
// port's MW_MAIN_081B_EVENTS.c and EVENT_UI.c, read as reference). None of MW1's words are kept here: every text,
// caption and name is read from the player's exe; without it there is no story and the campaign plays on.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::story_rules {

// Where the script lives (DS offsets in MW_MAIN's data segment, checked against the exe by test_mw1_story.cpp).
inline constexpr size_t kEventListDirectory = 0x6067;  // eventLists: six words, list offsets (0 none)
inline constexpr int32_t kEventListCount = 6;
inline constexpr size_t kNewCampaignList = 0x0AB8;     // eventDirectoryIndex's starting word: 1
inline constexpr size_t kCaptions = 0x5CF4;            // 0x1C captions, each a colour byte and a NUL-ended string
inline constexpr int32_t kCaptionCount = 0x1C;
inline constexpr size_t kRecordsStart = 0x6121, kRecordsEnd = 0x655C;  // the event records
inline constexpr size_t kFixedEvent = 0x6353;          // event 0x76's, CONTINUE OR QUIT (after the Dark Wing's defeat)
inline constexpr uint16_t kDataSegment = 0x1090, kTextSegment = 0x1AA5;  // the texts' segment
inline constexpr int32_t kTextCount = 0x77;            // its directory: 0x77 words at its start
inline constexpr size_t kTextStart = 0x00EE, kTextEnd = 0xE089;
inline constexpr size_t kCommanderName = 0x5CE2;       // campaignDefaultCommanderNameBytes
inline constexpr int32_t kPlanetFieldBytes = 0x10;     // a text's planet-name field (EVENT_PLANET_NAME_FIELD_BYTES)
inline constexpr int32_t kEventYearBase = 3000;        // EVENT_DATE_YEAR_BASE

// The classes: where an event fires (eventClassHandlers).
enum Class : int32_t { kClassArrival, kClassBarEntry, kClassBarDrink, kClassNews, kClassMechComplex, kClassOffice, kClassAfterBattle, kClassCount };
// The events the rules name (campaign_events.h).
inline constexpr int32_t kEventQuiet = 0x71;            // a drink with no tip: its text only
inline constexpr int32_t kEventDarkWingAttack = 0x72;   // ATTACK / DELAY on the Dark Wing's world; never marked seen
inline constexpr int32_t kEventTooLate = 0x73;
inline constexpr int32_t kEventPortArrest = 0x75;       // not after the Dark Wing's defeat
inline constexpr int32_t kEventContinueOrQuit = 0x76;
inline constexpr int32_t kFirstHatredArrival = 0x4F, kLastHatredArrival = 0x53;  // only on arriving, never seen

// The six story planets (campaignEventPlanetSelectors), rolled at the new campaign right after the start world
// (STARTUP 081B:181A): each from its pool (eventPlanetChoicePool0..5, DS:8FDA..8FF2; planet numbers) with
// MwRandomUpTo(size - 1). Pool 0 is the start worlds (mw1_campaign.h's kStartPlanets). In the script a negative
// planet selector names one: ~selector.
inline constexpr size_t kPoolAt[kStoryPlanets] = {0x8FDA, 0x8FE2, 0x8FE5, 0x8FE9, 0x8FEC, 0x8FF0};
inline constexpr int32_t kPoolSize[kStoryPlanets] = {8, 3, 4, 3, 4, 3};
inline constexpr int32_t kPools[kStoryPlanets][8] = {{10, 11, 121, 12, 129, 119, 136, 118}, {29, 4, 5}, {68, 83, 67, 73},
                                                     {145, 124, 129}, {13, 29, 8, 9}, {25, 22, 23}};
// Pools 1..5 rolled, story planet 0 the start world (new_company), as MW1 does.
void roll_story_planets(Company& c, Rng& rng);
// A company from before the story (a save before version 8): all six rolled, the start world's from pool 0 too.
void roll_story_planets_for_old_save(Company& c, Rng& rng);

// ---- The script ----

// A command (condition or action) as the exe has it.
enum Op : int32_t {
  // conditions
  kRandom = 0, kDate = 1, kOnPlanet = 2, kSeen = 3, kStanding = 4, kOffPlanet = 5, kNothing = 6, kCommanderLost = 8, kChoices = 9,
  // actions (in the action places)
  kRestart = 1, kDateWindow = 2, kFunds = 4, kUseAnd = 5, kUseOr = 6, kNamePlanet = 8,
};
struct Date3 {
  int32_t month = 0, day = 0, year = 0;  // MW1's codes: month 1..12, day 1..31 (submonth (day - 1) x 2), the year
};
struct Command {
  bool action = false;
  int32_t op = 0;
  int32_t value = 0;  // random: the threshold; planets: the selector (negative: ~story planet); seen: the event;
                      // standing: the House + 1; name a planet: the story planet
  int32_t threshold = 0;  // standing: favor - grievance under it
  Date3 date, end;        // a date; a window's start and end
  int64_t funds = 0;
  std::vector<std::string> captions;  // choices: from the exe's caption table
  std::vector<int32_t> branches;      // choices: the record each leads to, -1 none
};
struct Event {
  int32_t id = 0, cls = 0;
  size_t at = 0;  // its DS offset
  std::vector<Command> script;
};
struct Script {
  std::vector<Event> records;  // every record the list, the branches and the fixed event reach
  std::vector<int32_t> list;   // the new campaign's list, in order: records
  int32_t fixed = -1;          // event 0x76's record
  bool there() const { return !list.empty(); }
};
// The player's script, read once from MW_MAIN.EXE (mw1_pilots.h's set_mw1_game_dir); empty without it or when the
// exe isn't MW1's (anything out of place: none at all).
const Script& script();

// MW1's errata (research/story.md, Q9): five news dates with the day and month swapped, which MW1 shows a year late.
// Kept as MW1 has them (mw1 lead 2026-11-17, provisional); kFixErrata puts them right.
struct Erratum {
  int32_t event;
  Date3 as_mw1, fixed;
};
inline constexpr int32_t kErrataCount = 5;
extern const Erratum kErrata[kErrataCount];
inline constexpr bool kFixErrata = false;
void fix_errata(Script& s);  // what kFixErrata does to the script read

// ---- The texts ----

// A text's pages as the exe has them: lines (MW1's line breaks), a planet-name field (16 bytes) as one 0xFF, and whether
// the page carries a choice (its last lines are the captions, which MW1 draws its buttons over; the pages after it
// are never shown).
struct Page {
  std::vector<std::string> lines;
  bool choices = false;
};
std::vector<Page> text_pages(int32_t id);
// The commander's name MW1 starts with (DS:5CE2, a placeholder until the owner decides: research/story.md Q4); "" without it.
std::string commander_name();

// ---- Running it ----

// The Dark Wing's battle (research/story.md Q5, step 5; mw1_ladder.h's rung 6): ATTACK launches it. The story's deaths
// never restart the campaign (FIXES.md item 33, Q1 answered): the page shows, play goes on.
inline constexpr bool kDarkWingBattle = true;
// Ours (the lead, 2026-11-18): ATTACK / DELAY (0x72) asked on arriving at the Dark Wing's world, not each time the hub
// is drawn (MW1's), so a loss or DELAY doesn't put it straight back: travel away and come back.
inline constexpr bool kDarkWingOnArrival = true;
// Ours (the owner, FIXES.md 47): no deadline. MW1's 20 March 3029 is dropped: 0x72 asked whatever the date (its window's
// start kept), 0x73 (too late) never fires. The news keeps its dates.
inline constexpr bool kNoDeadline = true;
// Ours (FIXES 146): the late succession news (0x6D, 0x6E, 0x6F, dated April to May 3029: the council's choice made,
// the succession lost) never shows under kNoDeadline, as 0x73: with no deadline there is no lost succession to report.
inline constexpr int32_t kEventSuccessionFirst = 0x6D, kEventSuccessionLast = 0x6F;

// ---- The graded endings (ours: FIXES 146, a project decision of 2026-11-24) ----
//
// The hunt is timed from rung 1's first offer (Grig's men at Land's End's Bar: the player's own choice to begin) to the
// Dark Wing's defeat; the build-up before it doesn't count. Days as the calendar's: 30 a month, 2 sub-month units each.
// Exceptional: within MW1's own five years (1 April 3024 to 20 March 3029: 1789 days), now counted from the hunt's
// start. Good: within ten years. Late: beyond.
inline constexpr int32_t kGradeExceptionalDays = 1789;
inline constexpr int32_t kGradeGoodDays = 10 * 12 * 30;
enum class Grade { kNone, kExceptional, kGood, kLate };
int32_t days_between(const Date& from, const Date& to);  // whole days, from `from` to `to` (negative: before)
Grade grade(int32_t days);
Grade grade(const Company& c);  // the hunt's; kNone until it has begun and ended
const char* grade_name(Grade g);  // "Exceptional", "Good", "Late"; "" for none
void hunt_begins(Company& c);    // the clock started, once: now
void hunt_ends(Company& c);      // the clock stopped, once: now (begun or not)
// The ending's page of ours, after MW1's victory text and before the credits: the grade, when the hunt began and
// ended and how long it took, and the grade's line. Unwrapped; empty without a grade.
std::vector<std::string> ending_lines(const Company& c);

// ---- The step battles and the setbacks (ours: FIXES.md item 47, the mw1 lead's plan of 2026-11-18) ----
//
// Each step of the chain with a battle (mw1_ladder.h's rungs 1..5; 6 is 0x72's ATTACK) waits until its battle is won:
// a FIGHT launches it (step_fight), the next step's events don't fire before it (step_gate), and a step's event that
// ends without it (RUN, a stalling choice, the battle lost) waits to fire again (kSeenWaiting, steps_wait). MW1's
// deaths and stalling choices are setbacks (setback_of): a fine (a third of the company's funds at most), a lancemate
// lost, or a turn-away; never a death or a dead end.
inline constexpr int32_t kEventGrigsMen = 0x0C;      // rung 1: ours adds FIGHT / RUN (MW1's captions) under its text
inline constexpr int32_t kLandsEnd = 30, kAndersMoon = 127;  // planet numbers (0x0B's and 0x75's conditions)
// The rung won an event needs to fire (0: none).
int32_t step_gate(int32_t event);
// A choice that launches its step's battle: event `event`'s choice `choice` (0 first), rung `rung`, at its hardest
// (MW1's second FIGHT, a death there), and what a win goes on to: event `won` run next at the hub (-1 none; `needs`
// marked seen first, the exchange the battle stands for), or with `won_quiet` only marked seen (its page MW1's
// escape, not the battle's); the win's page is due until `won_done` is seen (a quit before it: shown again).
struct StepFight {
  int32_t event, choice, rung;
  bool hardest;
  int32_t won, needs;
  bool won_quiet;
  int32_t won_done;
};
const StepFight* step_fight(int32_t event, int32_t choice);  // null: not one
// A won step battle's page still due (the rungs won whose `won_done` isn't seen): its StepFight, else null. Quiet
// wins are marked here.
const StepFight* step_win_due(Company& c);
int32_t record_of(int32_t event);  // the script's record of an event (its first), -1 none
// Every step's own event (its battle's rung, or `done`'s seen for 0x64), and whether its step is done.
struct StepRoot {
  int32_t event, rung, done;
};
bool step_done(const Company& c, const StepRoot& r);
// After a class's events: each step's event seen and its step not done waits to fire again.
void steps_wait(Company& c);
// MW1's caption for FIGHT and RUN (0x27's choices, from the exe): 0x0C's choice. Empty without the script.
std::vector<std::string> fight_or_run();

enum class Cost { kFine, kLancemate, kTurnAway };
struct Setback {
  int32_t event;     // MW1's death or stalling choice's event
  Cost cost;
  int64_t fine;      // kFine's, or kLancemate's without a lance; kTurnAway's least (10% of the funds above it)
  int32_t again;     // the step's event that waits to fire again (-1: steps_wait's own)
  int32_t reoffer;   // an event whose choice is offered again at once (-1 none): 0x44's, 0x3E
  const char* line;  // our words, in place of MW1's death page: "%s" the lancemate lost, else unused
};
const Setback* setback_of(int32_t event);  // null: not one
int64_t setback_fine(const Company& c, const Setback& s);  // the fine, capped at a third of the funds
// The setback applied: the fine paid, a lancemate gone (MISSING: back in the pool, their mech kept), the company
// turned away (to the nearest other world, the trip's time passing); its event waits (0x75: never seen until won).
struct SetbackDone {
  int64_t fined = 0;
  int32_t lost = 0;    // the lancemate's pilot id, 0 none
  int32_t moved = -1;  // kPlanets index turned away to, -1 none
  std::string line;    // the panel's
};
SetbackDone apply_setback(Company& c, const Setback& s, Rng& rng);

// An event to show: its text (the event's id), its choices, and what MW1 does after it.
struct Shown {
  int32_t event = -1;
  int32_t record = -1;                // -1: the quiet drink
  std::vector<std::string> captions;  // its choices (none: just the text)
  std::vector<int32_t> branches;      // the record each choice leads to, -1 none
  bool restart = false;               // MW1 would restart the campaign after it (a death)
  int32_t named = -1;                 // the story planet its text names (MW1 writes its name into the text)
};
// The pages as shown: the named planet written into its field (its name as the exe has it), the captions' lines
// taken off (the screen draws them as buttons), nothing after a choice's page.
std::vector<Page> shown_pages(const Company& c, const Shown& s);
// A story's title: the line before its first page's first blank one (else its first).
std::string news_title(const Company& c, const Shown& s);
// Whether a line ends a sentence (., ! or ?, a closing quote or bracket after it): FIXES.md item 73.
bool sentence_ends(const std::string& line);
// A story's headline (FIXES.md item 67): its title; when another news story of the script has the same title (MW1's
// round-ups share one), the first line no other story under that title has, past a dateline, instead, so no two say
// the same; ending in "..." when it doesn't end a sentence (item 73). Its title still when it has no such line.
std::string headline(const Company& c, const Shown& s);
// Whether a news title is shared by more than one news record of the script.
bool title_shared(const Company& c, const std::string& title);
// A story as shown, rebuilt from its record (the log keeps the record, mw1_log.h): its event, the planet its text
// names (kNamePlanet's), no captions. record -1 or out of range: event -1.
Shown shown_of_record(int32_t record);

// One event run as MW1 runs it (081B:639B): its conditions and actions in turn (funds, a planet named, a restart
// queued, a date window, AND/OR); a choice opens when the conditions hold (marking it seen, but for the Dark Wing's
// attack); else, the conditions held, it's accepted: seen (but for the hatred arrivals, which need an arrival) and
// shown. `cls` is the class it fires as (a branch fires as its parent's).
enum class Ran { kNot, kShown, kChoice };
Ran run_event(Company& c, int32_t record, int32_t cls, Rng& rng, Shown* out);

// A class's events fired in turn (081B:6236): each in the list of that class, not seen yet (the news: always), run;
// each shown, or a choice, waits for the player. A choice made runs its branch first, then the list goes on.
struct Dispatch {
  int32_t cls = 0;
  size_t at = 0;        // the list's next entry
  int32_t shown = 0;    // events shown (dispatchedEventCount)
  int32_t branch = -1;  // a choice's branch to run next
  bool done = false;
};
Dispatch start(int32_t cls);
// The next event to show; false when the class is done (a drink with nothing: the quiet text once, record -1).
bool next(Company& c, Dispatch& d, Rng& rng, Shown* out);
// The player's choice `i` of `s` (shown by next): its branch runs next (none: nothing more of it).
void choose(Dispatch& d, const Shown& s, int32_t i);

// The news live now (class 3, MW1's NewsNet: each from its date for six months, or from its conditions on), in the
// list's order; each found is marked seen, as MW1's browsing marks it.
std::vector<Shown> live_news(Company& c, Rng& rng);
// The news live now that the hub hasn't announced yet (Story::news_heard), now announced.
std::vector<Shown> fresh_news(Company& c, Rng& rng);

}  // namespace mw1::story_rules

namespace mw1 {
using namespace story_rules;
}  // namespace mw1
