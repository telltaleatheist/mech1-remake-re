#include "mw1_hub.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "mech3/app_hooks.h"
#include "mw1_chrome.h"
#include "mw1_music.h"

namespace mw1::hub {
namespace {
// The buttons, by id: their names in the layout and their words.
const std::vector<ScreenButton> kHubButtons = {{"OK", "OK"},           {"REPLAY", "REPLAY"},   {"MARKET", "MECH COMPLEX"},
                                               {"BAR", "BAR"},         {"TRAVEL", "STARPORT"}, {"OFFICE", "HOUSE OFFICE"},
                                               {"OPTIONS", "OPTIONS"}, {"COMPANY", "COMPANY"}, {"DEPLOY", "DEPLOY"},
                                               {"STORY", "STORY"},     {"READ", "READ"},       {"LOG", "LOG"},
                                               {"QUIT", "QUIT"}};
const std::vector<ScreenButton> kHubOptionsButtons = {{"SETTINGS", "SETTINGS"},   {"MAIN_MENU", "MAIN MENU"}, {"BACK", "BACK"},
                                                      {"SAVE_GAME", "SAVE GAME"}, {"LOAD_GAME", "LOAD GAME"}};
// The middle panel's lines wrapped at its words to kHubPanelColumns (4:3's panel, the narrower; a blank line kept).
std::vector<std::string> wrapped(const std::vector<std::string>& lines) {
  std::vector<std::string> out;
  for (const std::string& text : lines) {
    if (text.size() <= static_cast<size_t>(kHubPanelColumns)) {
      out.push_back(text);
      continue;
    }
    std::string line, word;
    auto flush = [&] {
      if (word.empty()) return;
      if (!line.empty() && line.size() + 1 + word.size() > static_cast<size_t>(kHubPanelColumns)) out.push_back(line), line = "  ";
      line += (line.empty() || line == "  " ? "" : " ") + word;
      word.clear();
    };
    for (char ch : text) {
      if (ch == ' ') flush();
      else word += ch;
    }
    flush();
    if (!line.empty()) out.push_back(line);
  }
  return out;
}
}  // namespace

size_t hub_pages(const std::vector<std::string>& panel) {
  const size_t n = wrapped(panel).size(), most = static_cast<size_t>(chrome::kBoxLinesMax);
  return std::max<size_t>(1, (n + most - 1) / most);
}

const char* hub_button_name(int32_t id) { return id >= 1 && id <= kHubButtonCount ? kHubButtons[id - 1].name : ""; }
const char* hub_options_button_name(int32_t id) {
  return id >= 1 && id <= kHubOptionsButtonCount ? kHubOptionsButtons[id - 1].name : "";
}

// Escape: OPTIONS (where the main menu is now).
HubState::HubState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Hub State", kHubFile, kHubDialog, kHubButtons, kHubOptions, std::move(pressed)) {}

void HubState::show(const HubView& v) {
  // Every text at its place in the layout (mw1hub.zrd's TEXTS).
  ScreenView s;
  s.backdrop = v.scenery;
  // The box at the foot (FIXES.md item 42): this page's lines, the box sized to them, the texts moved down to its top.
  const std::vector<std::string> all = wrapped(v.panel);
  const size_t most = static_cast<size_t>(chrome::kBoxLinesMax), pages = hub_pages(v.panel), page = std::min(v.page, pages - 1);
  const std::vector<std::string> lines(all.begin() + static_cast<long>(std::min(all.size(), page * most)),
                                       all.begin() + static_cast<long>(std::min(all.size(), (page + 1) * most)));
  const int32_t n = std::clamp(static_cast<int32_t>(lines.size()), 1, chrome::kBoxLinesMax);
  const int32_t dy = (chrome::kBoxLinesMax - n) * chrome::kBoxLineGap;
  const std::string title = pages > 1 ? v.title + "  " + std::to_string(page + 1) + "/" + std::to_string(pages) : v.title;
  s.blocks = {at_place("PLANET", {v.planet}),         at_place("HOUSE", {v.house}),
              at_place("DATE", {v.date}),             at_place("DAY", {v.day}),
              at_place_down("PANEL_TITLE", {title}, dy), at_place_down("PANEL_LINES", lines, dy),
              at_place("STATUS_WORLD", {v.planet}),   at_place("STATUS", {v.status})};
  s.pictures = {"", "", chrome::box_name(kHubBoxWidth, n)};  // PANEL3, after the plaques
  // The company's strip between the plaques (C-BILLS, PAYROLL, KILLS: the owner on item 16); the middle panel only with
  // something to say, OK (and AFTER ACTION's REPLAY) under it.
  s.blocks.push_back(at_place("STRIP_LABELS", v.labels));
  s.blocks.push_back(at_place("STRIP_VALUES", v.values));
  const bool box = v.after_action || v.message;
  s.panels = {true, true, box, true, true};  // PANEL..PANEL5: the plaques, the middle panel, the strip, the status line
  s.buttons.resize(kHubButtonCount);
  auto b = [&](int32_t id) -> ButtonState& { return s.buttons[static_cast<size_t>(id - 1)]; };
  b(kHubOk).there = box;
  if (page + 1 < pages) b(kHubOk).words = "NEXT";  // more to read: OK on the last page
  b(kHubReplay).there = v.after_action && v.replay;
  // MW1's two columns (DS:1083): COMPANY, MECH COMPLEX, HOUSE OFFICE; STARPORT, BAR, OPTIONS. The office's slot is
  // empty on a world with none (DS:1101). A contract accepted (DS:10D7): COMPANY, MECH COMPLEX, DEPLOY; no right column.
  b(kHubOffice).there = !v.contract && v.office_open;
  b(kHubOffice).look = v.office;
  b(kHubDeploy).there = v.contract;
  b(kHubTravel).there = b(kHubBar).there = b(kHubOptions).there = !v.contract;
  b(kHubStory).there = v.story;
  b(kHubRead).there = v.message && v.read && !v.after_action;  // in REPLAY's place (never both)
  b(kHubLog).there = true;
  b(kHubQuit).there = !box;  // the box's foot is there while it shows
  // Nothing but OK (and REPLAY) while AFTER ACTION waits.
  for (int32_t id : {kHubMarket, kHubBar, kHubTravel, kHubOffice, kHubCompany, kHubDeploy, kHubOptions, kHubStory, kHubLog, kHubQuit}) b(id).on = !v.after_action;
  Mw1State::show(s);
}

HubOptionsState::HubOptionsState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Hub Options State", kHubFile, kHubOptionsDialog, kHubOptionsButtons, kHubOptBack, std::move(pressed)) {}

void HubOptionsState::show(const std::string& scenery, const std::string& planet) {
  ScreenView s;
  s.backdrop = scenery;
  s.blocks = {at_place("TITLE", {"OPTIONS"}), at_place("STATUS_WORLD", {planet}),
              at_place("STATUS", {"SETTINGS: game, sound, controls. SAVE, LOAD: the 3 slots."})};
  s.buttons.resize(kHubOptionsButtonCount);
  Mw1State::show(s);
}

bool HubState::update() {
  const bool r = Mw1State::update();
  if (++frames_ == 20)
    if (const char* sc = std::getenv("MW1_SCREEN"); sc && std::string(sc) == "quit") e().confirm_quit();
  return r;
}

}  // namespace mw1::hub
