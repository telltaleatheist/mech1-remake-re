#pragma once
// MW1's screen template (VISION.md, "Screens": one reusable screen for the
// hub, the market and the rest): a zTwoD dialog from one of MW1's own layouts
// (a .zrd in MW1's data folder) with its buttons bound by name, each with
// MW1's words, and blocks of text it is given. Built as MW3's briefing screen
// is. The art is Pirate's Moon's, named in the layout and read from the
// player's install: a BACKGROUND primitive (else the main menu's backdrop,
// borrowed as its sub-screens do), PANEL images behind the texts, the buttons'
// bitmaps (DISABLE's when greyed).
//
// The screen only shows what it is given (ScreenView) and says which button
// was pressed; the game loop (mw1_game.cpp) decides everything.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gamez/ztwod/twod.h"
#include "mech3/app_states.h"
#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"

namespace mw1::screens {

// A button: its name in the layout's BUTTONS (its id is its place in the list, from 1) and its words.
struct ScreenButton {
  const char* name;
  const char* words;
};

// The fonts (PM's fonts.zrd, by name).
inline constexpr const char* kPlaqueFont = "main_red";
inline constexpr const char* kPlaqueSmallFont = "main_red_small";
inline constexpr const char* kTitleFont = "main_green";
inline constexpr const char* kLineFont = "brief_green";
inline constexpr const char* kNoteFont = "brief_yellow";
inline constexpr int32_t kLineGap = 16;

// Lines of text at a place, in a font, one under another.
struct TextBlock {
  int32_t x = 0, y = 0;
  const char* font = kLineFont;
  std::vector<std::string> lines;
  int32_t gap = kLineGap;  // from one line to the next
};
struct ButtonState {
  bool there = true;  // shown
  bool on = true;     // enabled (else greyed: its DISABLE bitmap and grey words)
  std::string words;  // its words now (empty: its own, ScreenButton's)
};
// A shape drawn over the panels and under the texts (the travel map's worlds and markers): a ring (a circle's
// outline, `size` its radius; radius 1 reads as a dot), a line to (x2, y2), or a square's outline `size` from its
// centre. Its colour as red, green, blue.
struct Mark {
  enum class Kind { kRing, kLine, kSquare };
  Kind kind = Kind::kRing;
  int32_t x = 0, y = 0, x2 = 0, y2 = 0, size = 1;
  uint8_t r = 255, g = 255, b = 255;
};
struct ScreenView {
  std::vector<Mark> marks;
  std::vector<TextBlock> blocks;
  std::vector<ButtonState> buttons;  // by id - 1; missing ones shown and enabled
  std::vector<bool> panels;          // the layout's PANEL, PANEL2, ... shown; missing ones shown
};

class Mw1Screen : public mech3::MenuScreen {
 public:
  // The layout `dialog` read from `file`; `pressed` hears each button pressed (a shown, enabled one).
  Mw1Screen(mech3::Engine& e, zgame::Handle state, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
            std::function<void(int32_t)> pressed);
  ~Mw1Screen() override;
  bool loaded() const { return loaded_; }  // the layout read and every button bound
  void show(const ScreenView& v);
  void on_button(int32_t id, int32_t a, int32_t b) override;
  // For the tests: every line of text shown now (the blocks in order, then the buttons' words), and a button's state.
  std::vector<std::string> shown() const;
  bool visible(int32_t id) const;
  bool enabled(int32_t id) const;
  bool panel_shown(size_t i) const;  // the layout's i-th PANEL
  int32_t button_count() const { return static_cast<int32_t>(buttons_.size()); }
  // A part of the screen that hears the mouse (the travel map): `heard` with the mouse's place on each move over it
  // (click false) and on a left click there (true). point() is the same from a test.
  void set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard);
  void point(int32_t x, int32_t y, bool click);
  const std::vector<Mark>& marks() const { return marks_now_; }  // for the tests: the shapes shown now

 private:
  ztwod::Text* text(std::vector<std::unique_ptr<ztwod::Text>>& pool, size_t i, int32_t x, int32_t y, const char* font);
  std::vector<ScreenButton> names_;
  std::function<void(int32_t)> pressed_;
  bool loaded_ = false;
  std::vector<std::unique_ptr<mech3::HelpButton>> buttons_;   // by id - 1
  std::vector<std::unique_ptr<ztwod::Text>> labels_;          // the buttons' words, by id - 1
  std::vector<std::vector<std::unique_ptr<ztwod::Text>>> blocks_;
  std::vector<std::unique_ptr<ztwod::Image>> panels_;         // the layout's PANEL images, behind the texts
  std::vector<std::unique_ptr<ztwod::Frame>> rings_;           // the marks, by kind, reused show to show
  std::vector<std::unique_ptr<ztwod::Line2D>> lines_;
  std::vector<std::unique_ptr<ztwod::PolyLine>> squares_;
  std::vector<Mark> marks_now_;
  std::unique_ptr<ztwod::Object2D> pointer_;
  std::function<void(int32_t, int32_t, bool)> heard_;
};

// A screen's state: pushed by the loop, its screen made on entering
// (Engine::adopt_screen) and gone on leaving, as the engine's own menus are.
// Escape presses `escape_id`.
class Mw1State : public mech3::MenuState {
 public:
  Mw1State(zgame::Handle h, const char* name, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
           int32_t escape_id, std::function<void(int32_t)> pressed);
  bool enter() override;
  void leave() override;
  void escape() override;
  void show(const ScreenView& v);  // now, and on each entering
  Mw1Screen* screen() { return screen_ptr_; }
  // The mouse over a part of the screen (Mw1Screen::set_pointer), set on each entering; and the keys: each frame's
  // first plain key (its DirectInput code) to `keys`, Escape aside (escape_id's button).
  void set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard);
  void set_keys(std::function<void(uint32_t)> keys) { keys_ = std::move(keys); }
  void key(uint32_t code) {  // a key as pressed (tests)
    if (keys_) keys_(code);
  }
  bool update() override;

 private:
  const char* file_;
  const char* dialog_;
  std::vector<ScreenButton> buttons_;
  int32_t escape_id_;
  std::function<void(int32_t)> pressed_;
  ScreenView view_;
  Mw1Screen* screen_ptr_ = nullptr;
  int32_t pointer_rect_[4] = {};
  std::function<void(int32_t, int32_t, bool)> pointer_;
  std::function<void(uint32_t)> keys_;
};

// True when MW1's data has `file` with `dialog` in it (the loop falls back to message boxes when not).
bool layout_there(const char* file, const char* dialog);

}  // namespace mw1::screens

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace screens;
}  // namespace mw1
