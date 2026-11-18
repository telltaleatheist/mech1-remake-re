#pragma once
// MW1's event panel (mw1_story.h's events) on MW1's screen template (mw1_screen.h), its layout mw1event.zrd in MW1's
// data folder: the world and the place on the plaque, the event's text page by page on the green terminal; a
// choice's captions on MW1's buttons (MW1's words, from the player's exe); NEXT for the next page or OK. The Bar's
// NEWS shows the news live now on it, story by story, with PREV, NEXT and BACK.
// Where (FIXES.md item 34, Owen: as MW1's): over the screen it fires on, that screen's own picture behind it,
// undimmed, the panel in the screen's free side (Side); an event with its own art (event_art) a full page of its own
// (FIXES.md item 10), that art dimmed behind it.
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

// Where the panel sits: a full page (kFull, kEventDialog), or in a screen's free side over its picture
// (kEventLeftDialog, kEventRightDialog).
enum class Side : int32_t { kFull, kLeft, kRight };
inline constexpr int32_t kSides = 3;
inline constexpr const char* kEventLeftDialog = "MW1EVENTLEFT";
inline constexpr const char* kEventRightDialog = "MW1EVENTRIGHT";
const char* event_dialog(Side s);

// The screens a message fires over: each one's picture (its layout's BACKGROUND; the hub's is the world's scenery,
// scenery_image) and its free side, where the picture has nothing that matters (the Bar's window, left of the
// bartender, Owen's).
inline constexpr Side kHubSide = Side::kRight;     // the scenery's buildings; its mech left of the middle
inline constexpr Side kBarSide = Side::kLeft;      // the window, the bartender right of it
inline constexpr Side kMarketSide = Side::kLeft;   // the hangar's wall, the mech right of the middle
inline constexpr Side kOfficeSide = Side::kRight;
inline constexpr const char* kBarScene = "mw1_bar_scene";
inline constexpr const char* kMarketScene = "mw1_mechcomplex_hangar";
inline constexpr const char* kOfficeScene = "mw1_office_liaison";

enum EventButton : int32_t { kEventChoice1 = 1, kEventChoice2, kEventChoice3, kEventNext, kEventPrev, kEventBack };
inline constexpr int32_t kEventButtonCount = 6;
const char* event_button_name(int32_t id);

// What the panel shows: where (the plaque), the pages and which, a choice's captions on its last page (each live or
// greyed), and for the news which story of how many.
struct EventPanel {
  std::string planet, place;
  std::string scene;  // the backdrop: the event's art, else the picture of the screen it fires over (mw1image.zbd names)
  Side side = Side::kFull;
  std::vector<Page> pages;
  size_t page = 0;
  std::vector<std::string> captions;
  std::vector<bool> live;  // by caption; missing: live
  bool news = false;
  int32_t story = 0, stories = 0;
  bool last_page() const { return page + 1 >= pages.size(); }
  bool choosing() const { return last_page() && !captions.empty(); }
};

// The screen's texts and buttons (pure: the tests read it).
ScreenView event_view(const EventPanel& p);

// An event's own scene behind its text (FIXES.md item 10), by event id: "" none (the world's scenery then). The ones
// with a full scene (854x480); the rest, the first arrival's crest among them (an emblem, not a scene), wait on Owen's
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
