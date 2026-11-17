#pragma once
// MW1's planet hub (VISION.md, the home base): the screen the company sits in
// between missions, built as MW3's briefing screen is (a zTwoD dialog of a
// .zrd's buttons and images) from MW1's own layout, mw1hub.zrd in MW1's data
// folder. Its art is Pirate's Moon's, named there and read from the player's
// install: the main menu's backdrop (borrowed as its sub-screens do), the
// briefing's icons (launch, abort, replay) and the round button, with MW1's
// own words; MW1's own art takes their names' places later.
//
// The screen only shows what it is given (HubView) and says which button was
// pressed; the game loop (mw1_game.cpp) decides everything.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gamez/ztwod/twod.h"
#include "mech3/app_states.h"
#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"

namespace mw1 {

inline constexpr const char* kHubFile = "mw1hub.zrd";     // MW1's data folder
inline constexpr const char* kHubDialog = "MW1HUBDIALOG";  // its dialog

// The hub's buttons, by their ids (and their names in mw1hub.zrd's BUTTONS).
enum HubButton : int32_t { kHubAccept = 1, kHubDecline, kHubOk, kHubReplay, kHubMarket, kHubBar, kHubTravel, kHubQuit };
inline constexpr int32_t kHubButtonCount = 8;
const char* hub_button_name(int32_t id);  // "ACCEPT", ... ("" for no button)

// What the hub shows: the planet's plaque, the company's panel and the middle
// panel, the contract on offer or a mission's AFTER ACTION.
struct HubView {
  std::string planet, house;          // the plaque: "PLANET", "HOUSE DAVION"
  std::vector<std::string> company;   // the company's panel's lines
  std::string title;                  // the middle panel's: "CONTRACT ON OFFER", "AFTER ACTION"
  std::vector<std::string> panel;     // its lines
  bool after_action = false;          // OK (and REPLAY when `replay`) in place of ACCEPT and DECLINE
  bool replay = false;
};

class HubScreen : public mech3::MenuScreen {
 public:
  // The layout read from kHubFile; `pressed` hears each button pressed (an enabled one).
  HubScreen(mech3::Engine& e, zgame::Handle state, std::function<void(int32_t)> pressed);
  ~HubScreen() override;
  bool loaded() const { return loaded_; }  // the layout read and every button bound
  void show(const HubView& v);
  void on_button(int32_t id, int32_t a, int32_t b) override;
  // For the tests: every line of text shown now, top to bottom (the plaque, the panels, the buttons' words), and a
  // button's state.
  std::vector<std::string> shown() const;
  bool visible(int32_t id) const;
  bool enabled(int32_t id) const;

 private:
  ztwod::Text* text(std::vector<std::unique_ptr<ztwod::Text>>& pool, size_t i, int32_t x, int32_t y, const char* font);
  std::function<void(int32_t)> pressed_;
  bool loaded_ = false;
  std::vector<std::unique_ptr<mech3::HelpButton>> buttons_;   // by id - 1
  std::vector<std::unique_ptr<ztwod::Text>> labels_;          // the buttons' words, by id - 1
  std::vector<std::unique_ptr<ztwod::Text>> plaque_, company_, title_, panel_;
  int32_t label_x_[kHubButtonCount] = {}, label_y_[kHubButtonCount] = {};
};

// The hub's state: pushed over the main menu by the loop (ProgramFlow::main_entered), its screen made on entering
// (Engine::adopt_screen) and gone on leaving, as the engine's own menus are. Escape is QUIT.
class HubState : public mech3::MenuState {
 public:
  HubState(zgame::Handle h, std::function<void(int32_t)> pressed);
  bool enter() override;
  void leave() override;
  void escape() override;
  void show(const HubView& v);       // now, and on each entering
  HubScreen* screen() { return screen_ptr_; }
  const HubView& view() const { return view_; }

 private:
  std::function<void(int32_t)> pressed_;
  HubView view_;
  HubScreen* screen_ptr_ = nullptr;
};

}  // namespace mw1
