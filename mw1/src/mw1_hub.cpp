#include "mw1_hub.h"

#include <cstdlib>
#include <string>

#include "mech3/app_hooks.h"

namespace mw1::hub {
namespace {
// The buttons, by id: their names in the layout and their words.
const std::vector<ScreenButton> kHubButtons = {{"ACCEPT", "ACCEPT"},     {"DECLINE", "DECLINE"},     {"OK", "OK"},
                                               {"REPLAY", "REPLAY"},     {"MARKET", "MECH COMPLEX"}, {"BAR", "BAR"},
                                               {"TRAVEL", "STARPORT"},   {"QUIT", "MAIN MENU"},      {"OFFICE", "HOUSE OFFICE"},
                                               {"OPTIONS", "OPTIONS"}};
}  // namespace

const char* hub_button_name(int32_t id) { return id >= 1 && id <= kHubButtonCount ? kHubButtons[id - 1].name : ""; }

HubState::HubState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Hub State", kHubFile, kHubDialog, kHubButtons, kHubQuit, std::move(pressed)) {}

void HubState::show(const HubView& v) {
  // Every text at its place in the layout (mw1hub.zrd's TEXTS).
  ScreenView s;
  s.backdrop = v.scenery;
  s.blocks = {at_place("PLANET", {v.planet}),         at_place("HOUSE", {v.house}),
              at_place("DATE", {v.date}),             at_place("DAY", {v.day}),
              at_place("PANEL_TITLE", {v.title}),     at_place("PANEL_LINES", v.panel),
              at_place("TERM_TITLE", {"COMPANY"}),    at_place("TERM_LABELS", v.labels),
              at_place("TERM_VALUES", v.values),      at_place("TERM_MECHS", v.mechs),
              at_place("TERM_PILOTS", v.kinds),       at_place("STATUS_WORLD", {v.planet}),
              at_place("STATUS", {v.status})};
  s.buttons.resize(kHubButtonCount);
  s.buttons[kHubAccept - 1].there = s.buttons[kHubDecline - 1].there = !v.after_action && v.offer;
  s.buttons[kHubOk - 1].there = v.after_action;
  s.buttons[kHubReplay - 1].there = v.after_action && v.replay;
  // The market, the Bar and travel only between missions (not while AFTER ACTION waits).
  s.buttons[kHubMarket - 1].on = s.buttons[kHubTravel - 1].on = s.buttons[kHubBar - 1].on = !v.after_action;
  // The House office: a place, as MW1's, wherever the world has one (not while AFTER ACTION waits); an offer or not.
  s.buttons[kHubOffice - 1].on = !v.after_action && v.office_open;
  s.buttons[kHubOffice - 1].look = v.office;
  Mw1State::show(s);
}

bool HubState::update() {
  const bool r = Mw1State::update();
  if (++frames_ == 20)
    if (const char* sc = std::getenv("MW1_SCREEN"); sc && std::string(sc) == "quit") e().confirm_quit();
  return r;
}

}  // namespace mw1::hub
