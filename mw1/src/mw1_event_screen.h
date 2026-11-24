#pragma once
// MW1's event panel (mw1_story.h's events) on MW1's screen template (mw1_screen.h), its layout mw1event.zrd in MW1's
// data folder: the world and the place on the plaque, the event's text page by page on the green terminal; a
// choice's captions on MW1's buttons (MW1's words, from the player's exe); NEXT for the next page or OK. The Bar's
// NEWS shows the news live now on it, story by story, with PREV, NEXT and BACK.
// Where (FIXES.md item 42, the owner: unobtrusive, revising item 34's side panel): over the screen it fires on, that
// screen's own picture behind it, undimmed, the text in a box at the screen's foot sized to it (up to kBoxLinesMax
// lines, then pages; boxed_pages); an event with its own art (event_art) a full page of its own (FIXES.md item 10),
// that art dimmed behind it (the page is the picture).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_screen.h"
#include "mw1_story.h"

namespace mw1::event_ui {

inline constexpr const char* kEventFile = "mw1event.zrd";
inline constexpr const char* kEventDialog = "MW1EVENTDIALOG";

// Where the panel sits: a full page (kFull, kEventDialog), or a box at the foot of a screen's picture (kBottom,
// kEventBottomDialog).
enum class Side : int32_t { kFull, kBottom };
inline constexpr int32_t kSides = 2;
inline constexpr const char* kEventBottomDialog = "MW1EVENTBOTTOM";
inline constexpr int32_t kBoxWidth = 560;      // the box's (mw1_box_560_<lines>)
inline constexpr int32_t kBoxWideWidth = 760;  // at 16:9 (MW1EVENTBOTTOM_WIDE, mw1_box_760_<lines>)
// The letters a line of the box holds ("mw1_term_line", 8 pixels a letter, from 16 in to 16 from the box's right,
// a few spare), at 4:3 and at 16:9; and MW1's own lines' (its texts wrap at 44: mw1_story_every_text_reads).
inline constexpr size_t kBoxColumns = 64, kBoxWideColumns = 88, kMw1Columns = 44;
// A line at least this long not ending a sentence is MW1's break inside one too (its texts break some lines a word or
// two short: a tip's); shorter lines (a headline's, a list's) keep their breaks.
inline constexpr size_t kMw1SoftColumns = 34;
const char* event_dialog(Side s);

// The screens a message fires over: each one's picture (its layout's BACKGROUND; the hub's is the world's scenery,
// scenery_image).
inline constexpr const char* kBarScene = "mw1_bar_scene";
inline constexpr const char* kMarketScene = "mw1_mechcomplex_hangar";
inline constexpr const char* kOfficeScene = "mw1_office_liaison";

enum EventButton : int32_t { kEventChoice1 = 1, kEventChoice2, kEventChoice3, kEventNext, kEventPrev, kEventBack, kEventRead };
inline constexpr int32_t kEventButtonCount = 7;
const char* event_button_name(int32_t id);

// What the panel shows: where (the plaque), the pages and which, a choice's captions on its last page (each live or
// greyed), and for the news which story of how many.
struct EventPanel {
  std::string planet, place;
  std::string scene;  // the backdrop: the event's art, else the picture of the screen it fires over (mw1image.zbd names)
  Side side = Side::kFull;
  bool wide = false;  // the 16:9 layout's box (kBottom; MW1EVENTBOTTOM_WIDE is taken on a canvas wider than 640)
  std::vector<Page> pages;
  size_t page = 0;
  std::vector<std::string> captions;
  std::vector<bool> live;  // by caption; missing: live
  bool news = false;
  int32_t story = 0, stories = 0;
  // FIXES.md item 47: the STORY page (mw1_journal.h's journal_pages): each page's title on the plaque's second line;
  // PREV and NEXT through the pages, BACK; on the last (the lead's) GO THERE in NEXT's place (captions[0], live or not).
  bool journal = false;
  std::vector<std::string> titles;
  // FIXES.md item 63: the LOG (mw1_log.h's log_pages), a journal-like page (journal set too): READ (its own button,
  // top right) on any page with news (`live`, one a page).
  bool log = false;
  bool last_page() const { return page + 1 >= pages.size(); }
  bool choosing() const { return last_page() && !captions.empty(); }
};

// The screen's texts and buttons (pure: the tests read it).
ScreenView event_view(const EventPanel& p);

// MW1's pages for the box (kBottom): each cut into as few pages as hold kBoxLinesMax lines at the most, the lines
// shared evenly, none starting or ending blank; a choice's page's last part the one with the choice. A page with no lines stays one (empty) page.
// Each page's lines first rewrapped to the box's width (rewrapped).
std::vector<Page> boxed_pages(const std::vector<Page>& pages, size_t columns = kBoxColumns);
// MW1's lines rewrapped to `columns` letters: a line MW1 broke only for its width (the next line's first word
// wouldn't have fitted on it within kMw1Columns, or it's kMw1SoftColumns long and doesn't end a sentence) joined to
// the next; a blank line, a short line (a heading, a list's) and an indented one kept where they are. A word longer
// than a line stays whole.
std::vector<std::string> rewrapped(const std::vector<std::string>& lines, size_t columns);

// An event's own scene behind its text (FIXES.md item 10), by event id: "" none (the world's scenery then). The ones
// with a full scene (854x480); the rest, the first arrival's crest among them (an emblem, not a scene), wait on the owner's
// art list (research/story.md Q11).
std::string event_art(int32_t event);

// The panel's state: pushed over the screen the events fire on by the loop. Escape is NEXT (past the last story of the
// news: BACK).
class EventState : public Mw1State {
 public:
  EventState(zgame::Handle h, Side side, std::function<void(int32_t)> pressed);
  Side side() const { return side_; }

 private:
  Side side_;
};

}  // namespace mw1::event_ui

namespace mw1 {
using namespace event_ui;
}  // namespace mw1
