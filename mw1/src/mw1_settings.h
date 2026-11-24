#pragma once
// FIXES 126 (the owner, 2026-11-20: "make the options menu more intuitive"): MW1's SETTINGS, one screen in place of
// Pirate's Moon's option pages (GAME OPTIONS, GRAPHICS & SOUND, UPGRADES) and the hub's own sound rows.
//
// The look is the mockups the owner approved (outbox 2026-11-20/mw1-options-126): section tabs down the left (GAME,
// DISPLAY, SOUND, CONTROLS, ENHANCEMENTS), the chosen one lit; DEFAULTS (the section's) and DONE under them; the
// section's rows in the panel, a plain label and its control:
// - a choice of two or three: every value shown, the chosen one lit amber (a cheat's ON in red);
// - a longer list (resolution, lighting, mouse sensitivity): its value between arrows, "n of N" beside;
// - a volume: ten cells, the % beside;
// - KEY BINDINGS: Pirate's Moon's key map, as it is.
// Every change applies at once and is saved (PM's settings, mw1.cfg's sound, mw3.cfg's switches); nothing to ACCEPT.
// The status line says what the row under the mouse does.
//
// The lead's calls (2026-11-20): TEXTURES hidden, set to the device's largest (at_start); MFB REGROUP hidden (MW1
// fields no MFB); the cheats a red-headed group in GAME. RENDERER is not offered: the SDL port has the one device.
// RESOLUTION is the battles' (PM's in-game mode), taken at the next launch, so no keep-or-revert check is needed.
//
// Opened from the hub's OPTIONS (SETTINGS) and the pause's OPTIONS (pause_options_hook, mech3/pause_menu.h).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_screen.h"

namespace mech3 {
class Engine;
class GameEngine;
}  // namespace mech3

namespace mw1::settings {

inline constexpr const char* kSettingsFile = "mw1settings.zrd";
inline constexpr const char* kSettingsDialog = "MW1SETTINGS";

enum class Tab : int32_t { kGame, kDisplay, kSound, kControls, kEnhancements };
inline constexpr int32_t kTabCount = 5;
const char* tab_name(Tab t);  // "GAME", ...

// A row's kind.
enum class Kind { kChoice, kCycle, kVolume, kHeading, kAction };

// A row as shown now.
struct Row {
  std::string label;
  Kind kind = Kind::kChoice;
  std::vector<std::string> names;  // a choice's values; a cycle's list
  int32_t index = 0;               // the chosen value (a choice's, a cycle's); a volume's percent
  int32_t alert = -1;              // a choice's value shown red when chosen (a cheat's ON)
  bool alert_heading = false;      // a heading in red (CHEATS)
  bool available = true;           // false: its control hidden, `unavailable` shown in its place
  std::string unavailable;
  std::string hint;                // the status line's words for it (at most chrome::kStatusLetters)
};
std::vector<Row> rows(mech3::Engine& e, Tab t);

// A change to a row (rows()' order), applied at once and saved: a choice's or a cycle's index (a cycle's clamped,
// not wrapped), a volume's percent. A heading or an action is ignored.
void set(mech3::Engine& e, Tab t, size_t row, int32_t value);
// DEFAULTS: the section's rows as a new player's (RESOLUTION kept).
void defaults(mech3::Engine& e, Tab t);
int32_t default_of(mech3::Engine& e, Tab t, size_t row);  // a row's DEFAULTS value (-1: kept, or no setting)
// MW1's start (after PM's settings are read): TEXTURES the device's largest.
void at_start(mech3::Engine& e);
int32_t largest_textures(mech3::Engine& e);  // TextureMemory's value for it (PM's TextureCycle's last offered)

// The layout's buttons by id: the tabs, DEFAULTS, DONE, KEYS, then each row slot's three choice buttons, its two
// arrows and its ten volume cells.
inline constexpr int32_t kRowSlots = 9;
inline constexpr int32_t kSegs = 3, kCells = 10;
inline constexpr int32_t kPerRow = kSegs + 2 + kCells;
enum SettingsButton : int32_t { kTab0 = 1, kDefaults = kTab0 + kTabCount, kDone, kKeys, kRow0 };
inline constexpr int32_t kSettingsButtonCount = kRow0 - 1 + kRowSlots * kPerRow;
inline constexpr int32_t seg_id(int32_t row, int32_t k) { return kRow0 + row * kPerRow + k; }
inline constexpr int32_t left_id(int32_t row) { return kRow0 + row * kPerRow + kSegs; }
inline constexpr int32_t right_id(int32_t row) { return left_id(row) + 1; }
inline constexpr int32_t cell_id(int32_t row, int32_t k) { return left_id(row) + 2 + k; }
std::vector<screens::ScreenButton> buttons();  // the layout's names and words, by id

// The screen. `keys` opens Pirate's Moon's key map over it (KEY BINDINGS); DONE (and Escape) pops it.
class SettingsState : public screens::Mw1State {
 public:
  SettingsState(zgame::Handle h, std::function<void()> keys);
  bool enter() override;  // the menus' screen mode first (as PM's options from the pause), then the screen
  void show_tab(Tab t);
  void press(int32_t id);
  Tab tab() const { return tab_; }
  std::string status() const { return status_; }  // the status line now (tests)

 private:
  void refresh();
  void hovered(int32_t id);
  void escape_done();  // DONE: popped as PM's Game Options (with 7)
  Tab tab_ = Tab::kGame;
  std::string status_;
  std::function<void()> keys_;
};

}  // namespace mw1::settings

namespace mw1 {
using namespace settings;
}  // namespace mw1
