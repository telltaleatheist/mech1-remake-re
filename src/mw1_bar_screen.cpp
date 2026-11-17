#include "mw1_bar_screen.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "mw1_market.h"
#include "mw1_pilots.h"

namespace mw1::bar_ui {
namespace {
const std::vector<ScreenButton> kBarButtons = {{"HIRE", "HIRE"},       {"DECLINE", "DECLINE"}, {"FIRE", "FIRE"},
                                               {"ASSIGN", "ASSIGN"},   {"BACK", "BACK"},       {"RECRUIT_UP", ""},
                                               {"RECRUIT_DOWN", ""},   {"LANCE_UP", ""},       {"LANCE_DOWN", ""},
                                               {"DRINK", "DRINK"},     {"NEWS", "NEWS"}};
constexpr size_t kNameWidth = 11;                                       // the records' names
const char* const kShortRatings[4] = {"Poor", "Avg", "Good", "Exc"};  // the lists' columns

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string padded(std::string s, size_t n) {
  if (s.size() < n) s.append(n - s.size(), ' ');
  return s;
}
// "> Elli Gujar  Poor Poor    400": chosen or not, the name, Gunnery, Piloting, the wage a month.
std::string row(bool chosen, const Company& c, int32_t id, int32_t wage_tier) {
  std::string w = thousands(kWages[std::clamp(wage_tier, 0, 3)]);
  if (w.size() < 6) w.insert(0, 6 - w.size(), ' ');
  return std::string(chosen ? "> " : "  ") + padded(pilot_title(id), kNameWidth) + "  " + padded(kShortRatings[gunnery(c, id)], 5) +
         padded(kShortRatings[piloting(c, id)], 4) + w;
}
}  // namespace

const char* bar_button_name(int32_t id) { return id >= 1 && id <= kBarButtonCount ? kBarButtons[id - 1].name : ""; }

void clamp_choice(const Company& c, BarChoice& b) {
  const int32_t r = static_cast<int32_t>(c.recruits.size()), l = static_cast<int32_t>(c.lance.size());
  b.recruit = r == 0 ? 0 : std::clamp(b.recruit, 0, r - 1);
  b.mate = l == 0 ? 0 : std::clamp(b.mate, 0, l - 1);
}

ScreenView bar_view(const Company& c, const BarChoice& b, bool story) {
  ScreenView v;
  std::vector<std::string> recruits = {"  Name         Gun  Pil   Wage"}, lance = {"  You, the commander: " + mech_title(c.mech)};
  for (size_t i = 0; i < c.recruits.size(); ++i)
    recruits.push_back(row(static_cast<int32_t>(i) == b.recruit, c, c.recruits[i], gunnery(c, c.recruits[i])));
  if (c.recruits.empty()) recruits = {"Nobody here is looking for work."};
  for (size_t i = 0; i < c.lance.size(); ++i) lance.push_back(row(static_cast<int32_t>(i) == b.mate, c, c.lance[i].id, c.lance[i].wage_tier));
  // The chosen recruit's pitch, in their own words (the player's MW_MAIN.EXE; none without it).
  std::vector<std::string> pitch_lines;
  if (!c.recruits.empty()) {
    const int32_t id = c.recruits[static_cast<size_t>(b.recruit)];
    pitch_lines.push_back(pilot_title(id) + ":");
    for (const std::string& l : pitch(id)) pitch_lines.push_back("\"" + l + "\"");
  }
  // The chosen lancemate: their mech, their record toward promotion.
  std::vector<std::string> mate;
  if (!c.lance.empty()) {
    const Lancemate& m = c.lance[static_cast<size_t>(b.mate)];
    const int32_t h = mech_of(c, b.mate), g = gunnery(c, m.id);
    mate.push_back(pilot_title(m.id) + ": " + (h >= 0 ? "the " + mech_title(c.hangar[static_cast<size_t>(h)].mech) : std::string("no mech")));
    mate.push_back("Gunnery " + std::string(kRatingNames[g]) + ", Piloting " + kRatingNames[piloting(c, m.id)]);
    mate.push_back(g >= 3 ? std::string("At the top: Excellent")
                          : "Missions: " + std::to_string(c.pilots[m.id].missions) + " of " + std::to_string(kPromotionMissions[g]) +
                                " to promotion");
  }
  std::vector<std::string> terms = {
      "Funds: " + thousands(c.funds) + " C-bills. Lance: " + std::to_string(c.lance.size()) + " of " + std::to_string(kLanceMax) +
          ", wages " + thousands(monthly_wages(c)) + " a month",
      "ASSIGN gives the chosen lancemate the next free mech in the hangar.",
      "A lancemate flies their mech as last fitted. To refit it: PILOT it",
      "in the MARKET, fit it in the lab and fly, then PILOT yours back."};
  if (c.wage_warned) terms.push_back("Last month's wages are unpaid: short again and the lance leaves.");
  v.blocks = {{24, 14, kPlaqueFont, {upper(c.planet)}},
              {24, 32, kPlaqueSmallFont, {"HOUSE " + upper(kHouses[c.planet_house].name) + ": THE BAR"}},
              {30, 72, kTitleFont, {"RECRUITS"}},
              {30, 96, kLineFont, recruits},
              {30, 244, kLineFont, pitch_lines},  // up to 5 lines (64 characters), inside the panel
              {398, 72, kTitleFont, {"YOUR LANCE"}},
              {398, 96, kLineFont, lance},
              {398, 196, kLineFont, mate},
              {30, 350, kLineFont, terms},
              {30, 428, kNoteFont, {b.note}}};
  v.buttons.resize(kBarButtonCount);
  v.buttons[kBarHire - 1].on = v.buttons[kBarDecline - 1].on = !c.recruits.empty();
  v.buttons[kBarFire - 1].on = v.buttons[kBarAssign - 1].on = !c.lance.empty();
  v.buttons[kBarRecruitUp - 1].on = b.recruit > 0;
  v.buttons[kBarRecruitDown - 1].on = b.recruit + 1 < static_cast<int32_t>(c.recruits.size());
  v.buttons[kBarLanceUp - 1].on = b.mate > 0;
  v.buttons[kBarLanceDown - 1].on = b.mate + 1 < static_cast<int32_t>(c.lance.size());
  v.buttons[kBarDrink - 1].on = v.buttons[kBarNews - 1].on = story;
  return v;
}

BarState::BarState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Bar State", kBarFile, kBarDialog, kBarButtons, kBarBack, std::move(pressed)) {}

}  // namespace mw1::bar_ui
