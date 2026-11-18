#pragma once
// MW1's loading screen (FIXES.md item 18), its layout mw1load.zrd in MW1's data folder: DEPLOY goes straight into
// the mission, Pirate's Moon's briefing never shown (mech3/program_flow.h's skip_briefing). While the mission loads:
// the battle world's scenery dimmed, the world and the employer on the plaque, the contract (its title, objective,
// map and enemy) on the green terminal, LOADING under it. No buttons and no pointer.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_screen.h"

namespace mw1::loading {

inline constexpr const char* kLoadFile = "mw1load.zrd";
inline constexpr const char* kLoadDialog = "MW1LOADDIALOG";

// What the screen shows.
struct LoadingPanel {
  std::string planet;    // the battle's world, as the plaque says it
  std::string employer;  // "HOUSE DAVION"
  std::string scene;     // the backdrop (mw1image.zbd's name)
  std::string title;     // the contract's title on the terminal (its mission's, else CONTRACT)
  std::vector<std::string> lines;  // the contract's facts, as the office's offer says them
};

// The screen's texts (pure: the tests read it).
ScreenView loading_view(const LoadingPanel& p);

}  // namespace mw1::loading

namespace mw1 {
using namespace loading;
}  // namespace mw1
