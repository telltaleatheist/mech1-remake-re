#include "mw1_results.h"

#include <cstdio>

#include "mw1_campaign.h"

namespace mw1::results {
namespace {
const std::vector<ScreenButton> kResultsButtons = {{"CONTINUE", "CONTINUE"}, {"REPLAY", "REPLAY"}};

// PM's lance bars (mech_lance_armor's order): the torso's six facings 0..5, the head 6, the arms 7 and 8, the legs 9
// and 10. By location (kLocationNames): the bars it is made of.
const std::vector<std::vector<int>> kLocationBars = {{6}, {0, 1, 2, 3, 4, 5}, {7}, {8}, {9}, {10}};

int32_t percent_left(const float points[11], const float left[11], const std::vector<int>& bars) {
  float all = 0, kept = 0;
  for (int k : bars) all += points[k], kept += points[k] * left[k];
  if (all <= 0) return -1;
  const int32_t p = static_cast<int32_t>(kept * 100.0f / all + 0.5f);
  return p < 0 ? 0 : p > 100 ? 100 : p;
}
}  // namespace

static_assert(sizeof kLocationNames / sizeof kLocationNames[0] == kMechLocations, "a name for every location");

std::vector<std::string> fit_lines(std::vector<std::string> lines) {
  if (lines.size() <= kTerminalLines) return lines;
  const size_t more = lines.size() - (kTerminalLines - 1);
  lines.resize(kTerminalLines - 1);
  lines.push_back("... and " + std::to_string(more) + " more");
  return lines;
}

std::string time_words(int32_t seconds) {
  if (seconds < 0) return "unknown";
  char b[32];
  std::snprintf(b, sizeof b, "%d:%02d", seconds / 60, seconds % 60);
  return b;
}

void location_left(const Bars& b, std::vector<int32_t>* armor, std::vector<int32_t>* structure) {
  armor->clear(), structure->clear();
  for (const std::vector<int>& bars : kLocationBars) {
    armor->push_back(percent_left(b.armor_points, b.armor_left, bars));
    structure->push_back(percent_left(b.structure_points, b.structure_left, bars));
  }
}

std::vector<std::string> mech_lines(const std::vector<int32_t>& armor, const std::vector<int32_t>& structure, bool destroyed) {
  std::vector<std::string> l;
  if (destroyed) l.push_back("Destroyed in the battle");
  if (armor.size() != kMechLocations || structure.size() != kMechLocations) {
    if (!destroyed) l.push_back("Its damage wasn't read");
    return l;
  }
  auto pc = [](int32_t v) { return v < 0 ? std::string("  -") : (v < 10 ? "  " : v < 100 ? " " : "") + std::to_string(v) + "%"; };
  l.push_back("            ARMOUR STRUCTURE");
  for (int32_t i = 0; i < kMechLocations; ++i) {
    std::string name = kLocationNames[i];
    name.resize(12, ' ');
    l.push_back(name + "  " + pc(armor[static_cast<size_t>(i)]) + "     " + pc(structure[static_cast<size_t>(i)]));
  }
  return l;
}

ScreenView results_view(const ResultsPanel& p) {
  ScreenView v;
  v.backdrop = p.scene;
  v.blocks = {at_place("PLANET", {p.planet}),
              at_place("PLACE", {p.employer}),
              at_place("OUTCOME", {p.outcome}),
              at_place("RESULTS_TITLE", {"RESULTS"}),
              at_place("RESULTS", fit_lines(p.results)),
              at_place("PAY_TITLE", {"PAY"}),
              at_place("PAY", fit_lines(p.pay)),
              at_place("SALVAGE_TITLE", {"SALVAGE TO STORES"}),
              at_place("SALVAGE", fit_lines(p.salvage)),
              at_place("MECH_TITLE", {p.mech_title}),
              at_place("MECH", fit_lines(p.mech))};
  v.buttons.assign(kResultsButtonCount, ButtonState{});
  v.buttons[kResultsReplay - 1].there = p.replay;
  return v;
}

ResultsState::ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Results State", kResultsFile, kResultsDialog, kResultsButtons, kResultsContinue, std::move(pressed)) {}

}  // namespace mw1::results
