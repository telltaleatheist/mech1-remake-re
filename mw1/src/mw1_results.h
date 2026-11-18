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

// ---- AFTER ACTION's picture and message (FIXES.md items 31 and 38: MW1's beginDebrief, the port's
// MW_MAIN_081B_STARTUP.c, read as reference) ----

// The picture: a win (and an uneventful garrison tour) the commander with a big gun (MW1's 0x4C); a failure with
// nobody lost, a retreat, kicking a rock (0x4E); a failure with someone lost (a lancemate KILLED or MISSING, or the
// commander's mech destroyed) the wrecked cockpit (0x48). Owen's picks (art: end_win, end_retreat, end_lost).
enum class Ending { kWin, kRetreat, kLost };
Ending ending_of(Outcome o, bool anyone_lost);
// Its image: "mw1_end_win", or the large one ("_l") the 16:9 page shows.
std::string ending_image(Ending e, bool large);
// The House's crest (the world's House, as MW1 draws it under the picture): "mw1_icon_house_kurita".
std::string crest_image(int32_t house);

// The line after a win paid short (settlementPayoutPercent below 100): the supply overrun, the "disappointment" or
// the House transports' abandonment (MW1 picks one of the three, MwRandomUpTo(2): `pick` 0..2); no pay at all, the
// breach of contract. None otherwise.
enum class Deduction { kNone, kSupply, kPerformance, kTransport, kNoPay };
Deduction deduction_of(Outcome o, int32_t payout_percent, int32_t pick);
// The message, from the player's MW_MAIN.EXE (the --mw1-game folder; never kept in the repo), each a paragraph (the
// exe's 40-column lines joined): the outcome's (the garrison tour's, else the House's success or failure line;
// a retreat is a failure), then the deduction's. Empty without the exe (or another exe): the screen shows none.
// `garrison`: a deployment resolved without combat (MW1's deploymentResolvedWithoutCombat; MW1: Critical Hit has
// none yet, so it is false today).
std::vector<std::string> outcome_message(int32_t house, Outcome o, bool garrison, Deduction d);
// Paragraphs as lines of at most `width` letters, broken at spaces (a word longer than the width alone on its line).
std::vector<std::string> wrap(const std::vector<std::string>& paragraphs, size_t width);

// The screen's state: pushed over the hub by the loop when a mission is back. Escape is CONTINUE.
class ResultsState : public Mw1State {
 public:
  ResultsState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::results

namespace mw1 {
using namespace results;
}  // namespace mw1
