#pragma once
// MW1's event panel (mw1_story.h's events) on MW1's screen template (mw1_screen.h), its layout mw1event.zrd in MW1's
// data folder: a full screen of its own (FIXES.md item 10), the event's scene dimmed behind it, the world and the
// place on the plaque, the event's text page by page on the green terminal; a choice's captions on MW1's buttons
// (MW1's words, from the player's exe); NEXT for the next page or OK. The Bar's NEWS shows the news live now on it,
// story by story, with PREV, NEXT and BACK.
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

enum EventButton : int32_t { kEventChoice1 = 1, kEventChoice2, kEventChoice3, kEventNext, kEventPrev, kEventBack };
inline constexpr int32_t kEventButtonCount = 6;
const char* event_button_name(int32_t id);

// What the panel shows: where (the plaque), the pages and which, a choice's captions on its last page (each live or
// greyed), and for the news which story of how many.
struct EventPanel {
  std::string planet, place;
  std::string scene;  // the backdrop: the event's art, else the world's scenery (mw1image.zbd names)
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
  EventState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::event_ui

namespace mw1 {
using namespace event_ui;
}  // namespace mw1
