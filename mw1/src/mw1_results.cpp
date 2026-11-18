#include "mw1_results.h"

#include <cctype>
#include <cstdio>

#include "mw1_campaign.h"
#include "mw1_pilots.h"

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

namespace {
// MW_MAIN's DS 0x1090 strings (the port's MW_MAIN_1090.c import): offset and the largest size.
struct ExeText {
  size_t at, size;
};
constexpr ExeText kGarrison{0x140D, 0x56};
constexpr ExeText kKuritaFailure{0x149B, 0x8D}, kLiaoFailure{0x1528, 0x73}, kGeneralFailure{0x159B, 0x91};
constexpr ExeText kKuritaSuccess{0x1674, 0x62}, kLiaoSuccess{0x16D6, 0x6C}, kGeneralSuccess{0x1742, 0x44};
constexpr ExeText kSupply{0x178C, 0xA8}, kPerformance{0x1834, 0xAC}, kTransport{0x18E0, 0x98}, kNoPay{0x19A2, 0x95};

// The text there as one paragraph (its lines joined with spaces, blank ones dropped); empty when it isn't text (no
// exe, another exe).
std::string exe_text(const ExeText& t) {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  if (d.size() < t.at + t.size) return "";
  std::string out, word;
  size_t n = 0;
  for (size_t i = t.at; i < t.at + t.size && d[i]; ++i, ++n) {
    const uint8_t c = d[i];
    if (c == '\r' || c == '\n') {
      if (!out.empty() && out.back() != ' ') out += ' ';
      continue;
    }
    if (c < 0x20 || c > 0x7e) return "";  // not MW1's text
    out += static_cast<char>(c);
  }
  if (n == t.size || n < 16) return "";  // no end inside its size, or too short: not this text
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}
}  // namespace

Ending ending_of(Outcome o, bool anyone_lost) {
  if (o == Outcome::kSuccess) return Ending::kWin;
  return anyone_lost ? Ending::kLost : Ending::kRetreat;
}

std::string ending_image(Ending e, bool large) {
  const char* n = e == Ending::kWin ? "mw1_end_win" : e == Ending::kRetreat ? "mw1_end_retreat" : "mw1_end_lost";
  return std::string(n) + (large ? "_l" : "");
}

std::string crest_image(int32_t house) {
  if (house < 0 || house >= kHouseCount) return "";
  std::string n = kHouses[house].name;
  for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return "mw1_icon_house_" + n;
}

Deduction deduction_of(Outcome o, int32_t payout_percent, int32_t pick) {
  if (o != Outcome::kSuccess || payout_percent >= 100) return Deduction::kNone;
  if (payout_percent <= 0) return Deduction::kNoPay;
  return pick <= 0 ? Deduction::kSupply : pick == 1 ? Deduction::kPerformance : Deduction::kTransport;
}

std::vector<std::string> outcome_message(int32_t house, Outcome o, bool garrison, Deduction d) {
  std::vector<std::string> p;
  std::string first;
  if (garrison) first = exe_text(kGarrison);
  else if (o == Outcome::kSuccess) first = exe_text(house == kKurita ? kKuritaSuccess : house == kLiao ? kLiaoSuccess : kGeneralSuccess);
  else first = exe_text(house == kKurita ? kKuritaFailure : house == kLiao ? kLiaoFailure : kGeneralFailure);
  if (first.empty()) return p;
  p.push_back(first);
  const ExeText* t = d == Deduction::kSupply ? &kSupply : d == Deduction::kPerformance ? &kPerformance
                     : d == Deduction::kTransport ? &kTransport : d == Deduction::kNoPay ? &kNoPay : nullptr;
  if (t) {
    const std::string s = exe_text(*t);
    if (!s.empty()) p.push_back(s);
  }
  return p;
}

std::vector<std::string> wrap(const std::vector<std::string>& paragraphs, size_t width) {
  std::vector<std::string> lines;
  for (const std::string& para : paragraphs) {
    std::string line;
    size_t i = 0;
    while (i < para.size()) {
      while (i < para.size() && para[i] == ' ') ++i;
      size_t e = i;
      while (e < para.size() && para[e] != ' ') ++e;
      if (e == i) break;
      const std::string w = para.substr(i, e - i);
      if (!line.empty() && line.size() + 1 + w.size() > width) lines.push_back(line), line.clear();
      line += (line.empty() ? "" : " ") + w;
      i = e;
    }
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

ResultsState::ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Results State", kResultsFile, kResultsDialog, kResultsButtons, kResultsContinue, std::move(pressed)) {}

}  // namespace mw1::results
