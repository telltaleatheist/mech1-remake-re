#pragma once
// MW1's results screen (FIXES.md item 19), its layout mw1results.zrd in MW1's data folder: a mission ends here, never
// on Pirate's Moon's debriefing (mech3/program_flow.h's skip_debriefing). Over the world's scenery dimmed: the plaque
// (the world, the employer) and the outcome (VICTORY, DEFEAT, RETREAT); four terminals: RESULTS (the objective and its
// outcome, the enemy mechs destroyed and by whom, the time, the lancemates' fates), PAY (the fee with any deduction,
// the salvage share of each wreck's value, the total, the new funds), SALVAGE TO STORES (every part added, with its
// count; none on a loss), YOUR MECH (its damage by location). CONTINUE settles it and goes back to the hub; REPLAY (a
// failure) plays the contract again. It replaces the hub's AFTER ACTION panel.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_repairs.h"
#include "mw1_screen.h"

namespace mw1::results {

inline constexpr const char* kResultsFile = "mw1results.zrd";
inline constexpr const char* kResultsDialog = "MW1RESULTSDIALOG";

enum ResultsButton : int32_t { kResultsContinue = 1, kResultsReplay };
inline constexpr int32_t kResultsButtonCount = 2;
inline constexpr size_t kTerminalLines = 8;  // a terminal's lines under its title (304 x 168)

// The locations' names, in AfterAction's kMechLocations order.
inline constexpr const char* kLocationNames[6] = {"Head", "Torso", "Left arm", "Right arm", "Left leg", "Right leg"};

// What the screen shows: each terminal's lines (cut to kTerminalLines, the last saying how many more).
struct ResultsPanel {
  std::string planet, employer, outcome, scene;
  std::vector<std::string> results, pay, salvage, mech;
  std::string mech_title = "YOUR MECH";
  bool replay = false;  // a failure: REPLAY offered
};

// The screen's texts and buttons (pure: the tests read it).
ScreenView results_view(const ResultsPanel& p);
// A terminal's lines cut to kTerminalLines: past it, the last line says "... and N more".
std::vector<std::string> fit_lines(std::vector<std::string> lines);
// A mission's time as the results say it: "12:05" (minutes, seconds); "unknown" below 0.
std::string time_words(int32_t seconds);
// A mech's armour and structure left by location (percent, kMechLocations), from PM's lance bars: the head bar, the
// six torso bars together, the arms, the legs, each weighted by its points; -1 where it has none.
void location_left(const Bars& b, std::vector<int32_t>* armor, std::vector<int32_t>* structure);
// YOUR MECH's lines: each location's armour and structure left; destroyed, a line saying so first. Unknown: one line.
std::vector<std::string> mech_lines(const std::vector<int32_t>& armor, const std::vector<int32_t>& structure, bool destroyed);

// The screen's state: pushed over the hub by the loop when a mission is back. Escape is CONTINUE.
class ResultsState : public Mw1State {
 public:
  ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::results

namespace mw1 {
using namespace results;
}  // namespace mw1
