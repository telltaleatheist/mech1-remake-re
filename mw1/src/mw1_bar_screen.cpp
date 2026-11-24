#include "mw1_bar_screen.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "mw1_market.h"
#include "mw1_mech_icons.h"
#include "mw1_mech_renders.h"
#include "mw1_chrome.h"
#include "mw1_journal.h"
#include "mw1_pilots.h"
#include "mw1_story.h"

namespace mw1::bar_ui {
namespace {
const std::vector<ScreenButton> kBarButtons = {
    {"HIRE", "HIRE"},       {"FIRE", "FIRE"},   {"BACK", "LEAVE"},
    {"RECRUIT_UP", "PREV"}, {"RECRUIT_DOWN", "NEXT"}, {"LANCE_UP", "PREV"}, {"LANCE_DOWN", "NEXT"}, {"DRINK", "ORDER DRINK"},
    {"NEWS", "NEWS"},       {"CREW", "RECRUIT CREW"}, {"LANCE", "YOUR LANCE"}, {"DONE", "DONE"}};
const char* const kColumnPlaces[kBarColumns] = {"COL1", "COL2", "COL3", "COL4"};
constexpr size_t kColumnChars = 14;  // a grid column's line (104 pixels of mw1_panel_line)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
// An image name's part from words: lower case, the dots dropped, spaces as underscores ("J.J. Kitter": jj_kitter).
std::string slug(const std::string& words) {
  std::string out;
  for (char ch : words) {
    const unsigned char u = static_cast<unsigned char>(ch);
    if (std::isalnum(u)) out += static_cast<char>(std::tolower(u));
    else if (ch == ' ' && !out.empty() && out.back() != '_') out += '_';
  }
  while (!out.empty() && out.back() == '_') out.pop_back();
  return out;
}
std::string column_line(std::string s) {
  if (s.size() > kColumnChars) s.resize(kColumnChars);
  return s;
}
}  // namespace

const char* bar_button_name(int32_t id) { return id >= 1 && id <= kBarButtonCount ? kBarButtons[id - 1].name : ""; }

std::string portrait_image(int32_t pilot, bool small) {
  const std::string s = slug(pilot_title(pilot));
  return s.empty() ? "" : "mw1_recruit_" + s + (small ? "_s" : "");
}
std::string commander_image(bool small) { return small ? "mw1_char_gideon_s" : "mw1_char_gideon"; }
// The owner (2026-11-18): the mech's outline (PM's, redrawn: mw1_mech_icons.h), any chassis the company can own.
std::string mech_image(int32_t pm_mech) { return pm_mech >= 0 ? mech_icon_image(pm_mech) : ""; }

void clamp_choice(const Company& c, BarChoice& b) {
  const int32_t r = static_cast<int32_t>(c.recruits.size()), l = static_cast<int32_t>(c.lance.size());
  b.recruit = r == 0 ? 0 : std::clamp(b.recruit, 0, r - 1);
  b.mate = l == 0 ? 0 : std::clamp(b.mate, 0, l - 1);
}

ScreenView bar_view(const Company& c, const BarChoice& b, bool story) {
  ScreenView v;
  const bool menu = b.page == BarPage::kMenu, recruits = b.page == BarPage::kRecruits, lance = b.page == BarPage::kLance;
  // The status line: 54 letters at most, so it fits at 4:3 after the longest world's name (mw1screenfit's case).
  std::string status = b.note;
  if (status.empty() && c.wage_warned) status = "Wages unpaid: miss another month and the lance leaves.";
  if (status.empty())
    status = menu ? "Order a drink, read the news, or find pilots for hire."
             : recruits ? "HIRE pays their wage; PREV and NEXT show the rest."
                        : "PREV and NEXT show the lance; the Mech Lab assigns mechs.";
  if (b.company) v.backdrop = kCompanyBackdrop;
  v.blocks = {at_place("PLANET", {upper(c.planet)}),
              at_place("HOUSE", {"HOUSE " + upper(kHouses[c.planet_house].name) + (b.company ? " \xB7 THE COMPANY" : " \xB7 THE BAR")}),
              at_place("PLACE", {thousands(c.funds) + " C-BILLS"}),
              at_place("PLACE_SUB", {"LANCE " + std::to_string(c.lance.size()) + " OF " + std::to_string(kLanceMax) + " \xB7 WAGES " +
                                     thousands(monthly_wages(c)) + " A MONTH"}),
              at_place("STATUS_WORLD", {upper(c.planet)}),
              at_place("STATUS", {status}),
              at_place("MENU_TITLE", {menu ? "THE BAR" : ""})};
  v.panels.assign(kBarPanelCount, false);
  v.pictures.assign(kBarPanelCount, "");
  for (int32_t p : {kBarPanelPlaque, kBarPanelPlaque2, kBarPanelStatus}) v.panels[static_cast<size_t>(p)] = true;
  v.panels[kBarPanelMenu] = menu;

  // RECRUIT CREW: the chosen recruit, their portrait, record and own words (the player's MW_MAIN.EXE; none without it).
  std::vector<std::string> r_title, r_lines, r_pitch;
  if (recruits) {
    if (c.recruits.empty()) {
      r_title = {"RECRUITS"};
      r_lines = {"Nobody here is looking for work."};
    } else {
      const int32_t id = c.recruits[static_cast<size_t>(b.recruit)];
      r_title = {upper(pilot_title(id))};
      r_lines = {"Recruit " + std::to_string(b.recruit + 1) + " of " + std::to_string(c.recruits.size()), "",
                 std::string("Gunnery  ") + kRatingNames[gunnery(c, id)], std::string("Piloting ") + kRatingNames[piloting(c, id)],
                 "Wage     " + thousands(kWages[std::clamp(gunnery(c, id), 0, 3)]) + " a month"};
      r_pitch = journal::quoted(pitch(id), kPitchColumns);  // FIXES 95: one quote, not each exe line in its own
      v.panels[kBarPanelFrame] = v.panels[kBarPanelPortrait] = true;
      v.pictures[kBarPanelPortrait] = portrait_image(id, false);
    }
    v.panels[kBarPanelTerminal] = true;
  }
  v.blocks.push_back(at_place("R_TITLE", r_title));
  v.blocks.push_back(at_place("R_LINES", r_lines));
  v.blocks.push_back(at_place("R_PITCH", r_pitch));

  // YOUR LANCE: MW1's grid, you first, then each lancemate; the chosen one's record toward promotion beside.
  std::vector<std::string> info;
  for (int32_t col = 0; col < kBarColumns; ++col) {
    std::vector<std::string> lines;
    const size_t i = static_cast<size_t>(col);
    if (lance && col == 0) {
      const std::string you = commander_name();
      lines = {column_line(you.empty() ? "YOU" : upper(you)), "Commander", "", "", column_line(mech_title(c.mech))};
      v.pictures[kBarPanelColumnPortrait + i] = commander_image(true);
      v.pictures[kBarPanelMech + i] = mech_image(c.mech);
    } else if (lance && col - 1 < static_cast<int32_t>(c.lance.size())) {
      const Lancemate& m = c.lance[static_cast<size_t>(col - 1)];
      const int32_t h = mech_of(c, col - 1);
      lines = {column_line(upper(pilot_title(m.id))), std::string("Gun ") + kRatingNames[gunnery(c, m.id)],
               std::string("Pil ") + kRatingNames[piloting(c, m.id)], "Wage " + thousands(kWages[std::clamp(m.wage_tier, 0, 3)]),
               h >= 0 ? column_line(mech_title(c.hangar[static_cast<size_t>(h)].mech)) : std::string("No mech")};
      v.pictures[kBarPanelColumnPortrait + i] = portrait_image(m.id, true);
      if (h >= 0) v.pictures[kBarPanelMech + i] = mech_image(c.hangar[static_cast<size_t>(h)].mech);
      v.panels[kBarPanelMech + i] = h >= 0;
      if (col - 1 == b.mate) {
        const int32_t g = gunnery(c, m.id);
        info = {upper(pilot_title(m.id)), g >= 3 ? std::string("At the top: Excellent")
                                                 : "Missions: " + std::to_string(c.pilots[m.id].missions) + " of " +
                                                       std::to_string(kPromotionMissions[g]),
                g >= 3 ? std::string("") : "to promotion", "", "To refit their mech:", "PILOT it in the MARKET,",
                "fit it in the lab and", "fly, then PILOT yours."};
      }
    }
    if (lance && !lines.empty()) {
      v.panels[kBarPanelColumnFrame + i] = v.panels[kBarPanelColumnPortrait + i] = v.panels[kBarPanelMechFrame + i] = true;
      if (col == 0) v.panels[kBarPanelMech + i] = true;
      v.pictures[kBarPanelColumnFrame + i] = frame_name(104, 136, col - 1 == b.mate && col > 0);
    }
    v.blocks.push_back(at_place(kColumnPlaces[i], lines));
  }
  if (lance) {
    v.panels[kBarPanelGrid] = true;
    if (c.lance.empty()) info = {"No lancemates yet:", "RECRUIT CREW at the", "Bar."};
    v.panels[kBarPanelInfo] = true;
  }
  v.blocks.push_back(at_place("INFO", info));

  v.buttons.resize(kBarButtonCount);
  for (ButtonState& s : v.buttons) s.there = false;
  auto set = [&](int32_t id, bool there, bool on) { v.buttons[static_cast<size_t>(id - 1)].there = there, v.buttons[static_cast<size_t>(id - 1)].on = on; };
  set(kBarDrink, menu, story);
  set(kBarNews, menu, story);
  set(kBarCrew, menu, true);
  set(kBarLance, false, true);  // the hub's COMPANY now
  set(kBarBack, menu, true);
  set(kBarHire, recruits, !c.recruits.empty());
  set(kBarRecruitUp, recruits, b.recruit > 0);
  set(kBarRecruitDown, recruits, b.recruit + 1 < static_cast<int32_t>(c.recruits.size()));
  set(kBarFire, lance, !c.lance.empty());
  set(kBarLanceUp, lance, b.mate > 0);
  set(kBarLanceDown, lance, b.mate + 1 < static_cast<int32_t>(c.lance.size()));
  set(kBarDone, !menu, true);
  if (b.company) v.buttons[kBarDone - 1].words = "LEAVE";
  return v;
}

BarState::BarState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Bar State", kBarFile, kBarDialog, kBarButtons, kBarBack, std::move(pressed)) {}

}  // namespace mw1::bar_ui
