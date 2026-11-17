#include "mw1_hub.h"

namespace mw1 {
namespace {
// The buttons, by id: their names in the layout and their words (the briefing's icons carry none; the round button
// is blank).
const std::vector<ScreenButton> kHubButtons = {{"ACCEPT", "ACCEPT"}, {"DECLINE", "DECLINE"}, {"OK", "OK"},
                                               {"REPLAY", "REPLAY"}, {"MARKET", "MARKET"},   {"BAR", "BAR"},
                                               {"TRAVEL", "TRAVEL"}, {"QUIT", "QUIT"}};
}  // namespace

const char* hub_button_name(int32_t id) { return id >= 1 && id <= kHubButtonCount ? kHubButtons[id - 1].name : ""; }

HubState::HubState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Hub State", kHubFile, kHubDialog, kHubButtons, kHubQuit, std::move(pressed)) {}

void HubState::show(const HubView& v) {
  // The plaque at the top left, the company on the right panel, the contract or AFTER ACTION on the middle one.
  ScreenView s;
  s.blocks = {{24, 14, kPlaqueFont, {v.planet}},
              {24, 32, kPlaqueSmallFont, {v.house}},
              {402, 72, kLineFont, v.company},
              {150, 72, kTitleFont, {v.title}},
              {150, 96, kLineFont, v.panel}};
  s.buttons.resize(kHubButtonCount);
  s.buttons[kHubAccept - 1].there = s.buttons[kHubDecline - 1].there = !v.after_action;
  s.buttons[kHubOk - 1].there = v.after_action;
  s.buttons[kHubReplay - 1].there = v.after_action && v.replay;
  // The market only between missions (not while AFTER ACTION waits); the Bar and travel come later.
  s.buttons[kHubMarket - 1].on = !v.after_action;
  s.buttons[kHubBar - 1].on = s.buttons[kHubTravel - 1].on = false;
  Mw1State::show(s);
}

}  // namespace mw1
