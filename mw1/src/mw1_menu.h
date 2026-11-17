#pragma once
// MW1's main menu and the New Campaign intro (VISION.md, "Starting the game"), on MW1's screen template
// (mw1_screen.h) with MW1's own layouts, mw1menu.zrd and mw1intro.zrd in MW1's data folder.
//
// The menu sits where the hub sat, over PM's main menu (ProgramFlow::main_entered), which stays under it unseen: the
// state a mission's end comes back to, and the maker of the Options state MW1's OPTIONS pushes. NEW CAMPAIGN plays
// the intro, then a new company in the hub; CONTINUE opens the hub on the save; QUIT is PM's own quit.
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

}  // namespace mw1::menu

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace menu;
}  // namespace mw1
