#include "mw1_event_screen.h"

#include <algorithm>

#include "mw1_chrome.h"

namespace mw1::event_ui {
namespace {
const std::vector<ScreenButton> kEventButtons = {{"CHOICE1", ""}, {"CHOICE2", ""}, {"CHOICE3", ""},
                                                 {"NEXT", "NEXT"}, {"PREV", "PREV"}, {"BACK", "BACK"}};
}  // namespace

std::string event_art(int32_t event) {
  switch (event) {
    case kEventDarkWingAttack: return "mw1_story_darkwing_base";
    case kEventPortArrest: return "mw1_story_arrested";
    default: return "";
  }
}

const char* event_dialog(Side s) { return s == Side::kBottom ? kEventBottomDialog : kEventDialog; }

std::vector<Page> boxed_pages(const std::vector<Page>& pages) {
  auto blank = [](const std::string& l) { return l.find_first_not_of(' ') == std::string::npos; };
  std::vector<Page> out;
  for (const Page& p : pages) {
    std::vector<std::string> ls = p.lines;
    while (!ls.empty() && blank(ls.back())) ls.pop_back();
    while (!ls.empty() && blank(ls.front())) ls.erase(ls.begin());
    // As few parts as fit, the lines shared evenly between them (seven: four and three, not six and one).
    const size_t most = static_cast<size_t>(chrome::kBoxLinesMax), parts = (ls.size() + most - 1) / most;
    const size_t each = parts ? (ls.size() + parts - 1) / parts : most;
    size_t at = 0;
    const size_t first = out.size();
    while (at < ls.size()) {
      while (at < ls.size() && blank(ls[at])) ++at;  // no part starts blank
      if (at >= ls.size()) break;
      Page part;
      const size_t end = std::min(ls.size(), at + each);
      part.lines.assign(ls.begin() + static_cast<long>(at), ls.begin() + static_cast<long>(end));
      while (!part.lines.empty() && blank(part.lines.back())) part.lines.pop_back();
      out.push_back(part);
      at = end;
    }
    if (out.size() == first) out.push_back(Page{});  // nothing but blanks: one empty page
    out.back().choices = p.choices;
  }
  return out;
}

const char* event_button_name(int32_t id) { return id >= 1 && id <= kEventButtonCount ? kEventButtons[static_cast<size_t>(id) - 1].name : ""; }

ScreenView event_view(const EventPanel& p) {
  ScreenView v;
  std::string where = p.place;
  if (p.news && p.stories > 0) where += " (" + std::to_string(p.story + 1) + " OF " + std::to_string(p.stories) + ")";
  const std::vector<std::string> none;
  const std::vector<std::string>& lines = p.page < p.pages.size() ? p.pages[p.page].lines : none;
  v.backdrop = p.scene;
  if (p.side == Side::kBottom) {
    // The box sized to the lines (one at the least), its texts moved down from the six-line box's places.
    const int32_t n = std::clamp(static_cast<int32_t>(lines.size()), 1, chrome::kBoxLinesMax);
    const int32_t dy = (chrome::kBoxLinesMax - n) * chrome::kBoxLineGap;
    v.pictures = {chrome::box_name(kBoxWidth, n)};
    v.blocks = {at_place_down("TITLE", {p.planet + " \xB7 " + where}, dy), at_place_down("LINES", lines, dy)};
    if (p.pages.size() > 1)
      v.blocks.push_back(at_place_down("PAGE", {std::to_string(p.page + 1) + "/" + std::to_string(p.pages.size())}, dy));
  } else {
    v.blocks = {at_place("PLANET", {p.planet}), at_place("PLACE", {where}), at_place("LINES", lines)};
    if (p.pages.size() > 1) v.blocks.push_back(at_place("PAGE", {std::to_string(p.page + 1) + "/" + std::to_string(p.pages.size())}));
  }
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

EventState::EventState(zgame::Handle h, Side side, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Event State", kEventFile, event_dialog(side), kEventButtons, kEventNext, std::move(pressed)), side_(side) {}

}  // namespace mw1::event_ui
