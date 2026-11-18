#pragma once
// MW1's loading screen (FIXES.md item 18), its layout mw1load.zrd in MW1's data folder: DEPLOY goes straight into
// the mission, Pirate's Moon's briefing never shown (mech3/program_flow.h's skip_briefing). While the mission loads:
// the battle world's scenery dimmed, the world and the employer on the plaque, the contract (its title, objective,
// map and enemy) on the green terminal, the main enemy's picture framed beside it, LOADING under it. No buttons and
// no pointer.
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
  std::string enemy;     // the main enemy's picture (its render, else its amber icon); empty: none, the frame hidden
};
// The layout's panels (PANEL, PANEL2, ...): the enemy's frame and picture.
enum LoadingPanelId : int32_t { kLoadClear, kLoadPlaque, kLoadTerminal, kLoadEnemyFrame, kLoadEnemy, kLoadPanelCount };

// The terminal's lines fit to it (FIXES.md item 46's long map names, Rolling Hills' and the like, ran past its right
// edge): mw1_term_line is monospace, kLoadColumns letters (its 412 pixels from LINES to the terminal's edge, about 8.1
// a letter) a line. A longer line wraps at a space, the rest indented kLoadIndent, a highlight open at the break
// closed there and opened again; a word longer than a line cut. At most kLoadLines (the terminal's height from LINES,
// 16 apart), the last ending "..." when there were more.
inline constexpr size_t kLoadColumns = 48, kLoadIndent = 2, kLoadLines = 8;
std::vector<std::string> terminal_lines(const std::vector<std::string>& lines);

// The screen's texts (pure: the tests read it), the lines fit (terminal_lines).
ScreenView loading_view(const LoadingPanel& p);

}  // namespace mw1::loading

namespace mw1 {
using namespace loading;
}  // namespace mw1
