#pragma once
// MW1's main menu and the New Campaign intro (VISION.md, "Starting the game"), on MW1's screen template
// (mw1_screen.h) with MW1's own layouts, mw1menu.zrd and mw1intro.zrd in MW1's data folder.
//
// The menu (the title screen, FIXES.md item 3: title_bg, MW1's NEW / LOAD / QUIT down the left) sits where the hub
// sat, over PM's main menu (ProgramFlow::main_entered), which stays under it unseen: the state a mission's end comes
// back to, and the maker of the Options state the hub's OPTIONS pushes. NEW plays the intro, then a new company in
// the hub; LOAD (kMenuContinue) opens the hub on the save; QUIT is PM's own quit. kMenuOptions stays, never shown.
//
// The intro is MW1's (the port's MW_MAIN_081B_STARTUP.c, the campaign's entry): the narration on black, fading in,
// held while the opening music plays, then the opening picture; any key or a click skips it. MW1's narration is read
// from the player's MW_MAIN.EXE at run time (DS:129C, ten lines), never kept here. MW1's title picture (its first
// step) waits for art; its credit panels move to a Credits screen later (VISION).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_screen.h"

namespace mw1::menu {

inline constexpr const char* kMenuFile = "mw1menu.zrd";
inline constexpr const char* kMenuDialog = "MW1MENUDIALOG";
enum MenuButton : int32_t { kMenuNew = 1, kMenuContinue, kMenuOptions, kMenuQuit };
inline constexpr int32_t kMenuButtonCount = 4;
const char* menu_button_name(int32_t id);  // "NEW_CAMPAIGN", ... ("" for none)

// The menu's state: Escape does nothing (QUIT leaves). `continue_on`: a save there to continue.
class MenuState : public Mw1State {
 public:
  MenuState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show_menu(bool continue_on, const std::string& save_line);
};

// ---- The intro ----
inline constexpr const char* kIntroFile = "mw1intro.zrd";
inline constexpr const char* kIntroDialog = "MW1INTRODIALOG";

// MW1's narration, the ten lines at MW_MAIN's DS:129C (MwDrawIntroNarrationLines), from the player's exe
// (mw1_pilots.h's mw_main_data_segment); none without it.
inline constexpr int32_t kNarrationAt = 0x129C, kNarrationLines = 10;
std::vector<std::string> intro_narration();

// MW1's times, in its timer's ticks (18.2 a second): the lines drawn black then grey (2 + 2 ticks: a fade in), held
// white 0x24 ticks before the music and 0x19E after it, then the picture held 0x6C (its seven credit panels, 7 x
// 0x84 + 0xCC more, gone with them).
inline constexpr float kTicksPerSecond = 18.2f;
inline constexpr int32_t kFadeTicks = 4, kQuietTicks = 0x24, kNarrationTicks = 0x19E, kPictureTicks = 0x6C;
enum class IntroPhase { kGrey, kWhite, kPicture, kDone };
IntroPhase intro_phase(float seconds);  // where the intro is `seconds` in
float intro_length();                   // seconds, unskipped

// The intro's state. `done` is heard once: at the end, or at a skip (Escape, any key, a click).
class IntroState : public Mw1State {
 public:
  IntroState(zgame::Handle h, std::vector<std::string> lines, std::function<void()> done);
  bool enter() override;
  bool update() override;
  void escape() override { finish(); }
  IntroPhase phase() const { return intro_phase(seconds_); }
  float seconds() const { return seconds_; }
  bool finished() const { return finished_; }
  void start_at(float seconds) { start_ = seconds; }  // development: the intro entered this far in (shots)

 private:
  void show_phase();
  void finish();
  std::vector<std::string> lines_;
  std::function<void()> done_;
  float seconds_ = 0.0f, start_ = 0.0f;
  IntroPhase shown_ = IntroPhase::kDone;
  bool finished_ = false;
  bool mouse_was_down_ = true;  // the click that chose NEW CAMPAIGN isn't a skip
  int32_t frames_ = 0;
};

// ---- The credits (FIXES.md item 47, after the Dark Wing's defeat) ----
// MW1's credits roll up slowly over our art, then play goes on at the hub. Any key or a click skips them.
inline constexpr const char* kCreditsFile = "mw1credits.zrd";
inline constexpr const char* kCreditsDialog = "MW1CREDITSDIALOG";

// MW1's credits: the intro's seven panels, two texts each (its left and right), at MW_MAIN's DS. A text is its x and
// y words (MW1's 320x200 screen) and its words, NUL-ended within 0x80, lines split at 0x0D (two in a row: a blank
// line). Read from the player's exe (mw_main_data_segment); none without it, or when a record doesn't read as one.
inline constexpr int32_t kCreditPanels = 7;
inline constexpr uint16_t kCreditTexts[kCreditPanels][2] = {
    {0x1C33, 0x1C57}, {0x1C80, 0x1CA9}, {0x1CCF, 0x1D1F}, {0x1D53, 0x1D6F},
    {0x1D8E, 0x1DD8}, {0x1DF7, 0x1E18}, {0x1E40, 0x1EB8},
};
struct CreditText {
  int32_t x = 0, y = 0;  // MW1's screen's
  std::vector<std::string> lines;
};
struct CreditPanel {
  std::vector<CreditText> texts;
  bool headed = true;  // each text's first line a role (MW1's last panel, its trademark notice, isn't)
};
std::vector<CreditPanel> mw1_credits();
// The hook for our own credits, rolled after MW1's (a text's x picks its column as MW1's do: under 160 the left,
// else the right; a line wider than a column centred across). None yet.
std::vector<CreditPanel> extra_credits();
std::vector<CreditPanel> credits_roll();  // MW1's, then ours

// The roll laid out top-down from 0 in the layout's 640x480: each column's lines centred on it, a headed panel's
// texts' first lines in the heading font (MW1's role over its names), a blank line half a line, the panels one under
// another, a text kept its place against its panel's other (MW1's y offsets, doubled).
inline constexpr int32_t kCreditLeftX = 170, kCreditRightX = 470, kCreditMiddleX = 320, kCreditColumnChars = 24;
inline constexpr int32_t kCreditGap = 22, kCreditPanelGap = 72;
inline constexpr int32_t kCreditsTop = 24, kCreditsBottom = 456;  // the window lines show in
inline constexpr float kCreditsSpeed = 30.0f;                     // pixels a second, up
inline constexpr const char* kCreditHeadFont = "mw1_credit_head";
inline constexpr const char* kCreditFont = "mw1_credit";
struct CreditLine {
  int32_t x = 0, y = 0;
  bool head = false;
  std::string text;
};
std::vector<CreditLine> credits_layout(const std::vector<CreditPanel>& roll);
int32_t credits_height(const std::vector<CreditLine>& lines);  // to the last line's foot
// Seconds to roll them all past the window's top.
float credits_length(const std::vector<CreditLine>& lines);

// The credits' state over our `art` (an image's name; empty or missing: black). `done` is heard once: at the end,
// or at a skip (Escape, any key, a click).
class CreditsState : public Mw1State {
 public:
  CreditsState(zgame::Handle h, std::vector<CreditPanel> roll, std::string art, std::function<void()> done);
  bool enter() override;
  bool update() override;
  void escape() override { finish(); }
  float seconds() const { return seconds_; }
  bool finished() const { return finished_; }
  size_t lines() const { return lines_.size(); }
  size_t lines_shown() const { return shown_count_; }  // in the window now
  void start_at(float seconds) { start_ = seconds; }   // development: entered this far in (shots)

 private:
  void show_roll();
  void finish();
  std::vector<CreditLine> lines_;
  std::string art_;
  std::function<void()> done_;
  float seconds_ = 0.0f, start_ = 0.0f, length_ = 0.0f;
  int32_t shown_at_ = -1;
  size_t shown_count_ = 0;
  bool finished_ = false;
  bool mouse_was_down_ = true;
  int32_t frames_ = 0;
};

}  // namespace mw1::menu

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace menu;
}  // namespace mw1
