#include "mw1_event_screen.h"

namespace mw1::event_ui {
namespace {
const std::vector<ScreenButton> kEventButtons = {{"CHOICE1", ""}, {"CHOICE2", ""}, {"CHOICE3", ""},
                                                 {"NEXT", "NEXT"}, {"PREV", "PREV"}, {"BACK", "BACK"}};
constexpr int32_t kTextX = 48, kTextY = 72;  // MW1's text, its 44 columns well inside the screen
}  // namespace

const char* event_button_name(int32_t id) { return id >= 1 && id <= kEventButtonCount ? kEventButtons[static_cast<size_t>(id) - 1].name : ""; }

ScreenView event_view(const EventPanel& p) {
  ScreenView v;
  std::string where = p.place;
  if (p.news && p.stories > 0) where += " (" + std::to_string(p.story + 1) + " OF " + std::to_string(p.stories) + ")";
  const std::vector<std::string> none;
  const std::vector<std::string>& lines = p.page < p.pages.size() ? p.pages[p.page].lines : none;
  v.blocks = {{24, 14, kPlaqueFont, {p.planet}}, {24, 32, kPlaqueSmallFont, {where}}, {kTextX, kTextY, kLineFont, lines}};
  if (p.pages.size() > 1) v.blocks.push_back({560, 32, kPlaqueSmallFont, {std::to_string(p.page + 1) + "/" + std::to_string(p.pages.size())}});
  v.buttons.assign(kEventButtonCount, ButtonState{});
  for (int32_t i = 0; i < 3; ++i) {
    ButtonState& b = v.buttons[static_cast<size_t>(i)];
    const bool there = !p.news && p.choosing() && i < static_cast<int32_t>(p.captions.size());
    b.there = there;
    if (!there) continue;
    b.words = p.captions[static_cast<size_t>(i)];
    b.on = static_cast<size_t>(i) >= p.live.size() || p.live[static_cast<size_t>(i)];
  }
  ButtonState& next = v.buttons[kEventNext - 1];
  if (p.news) {
    next.on = !p.last_page() || p.story + 1 < p.stories;
    v.buttons[kEventPrev - 1].on = p.page > 0 || p.story > 0;
  } else {
    next.there = !p.choosing();
    next.words = p.last_page() ? "OK" : "NEXT";
    v.buttons[kEventPrev - 1].there = false;
    v.buttons[kEventBack - 1].there = false;
  }
  return v;
}

EventState::EventState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Event State", kEventFile, kEventDialog, kEventButtons, kEventNext, std::move(pressed)) {}

}  // namespace mw1::event_ui
