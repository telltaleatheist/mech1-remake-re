#pragma once
// MW1's planet hub (VISION.md, the home base): the screen the company sits in
// between missions, on MW1's screen template (mw1_screen.h) with MW1's own
// layout, mw1hub.zrd in MW1's data folder. Its art is Pirate's Moon's, named
// there and read from the player's install: the main menu's backdrop
// (borrowed as its sub-screens do), the popup box as the panels behind the
// texts, the briefing's icons (launch, abort, replay) and the round button
// with MW1's own words (greyed as PM greys it); MW1's own art takes their
// names' places later.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_screen.h"

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
  std::string planet, house;          // the plaque: "ROBINSON", "HOUSE DAVION"
  std::vector<std::string> company;   // the company's panel's lines
  std::string title;                  // the middle panel's: "CONTRACT ON OFFER", "AFTER ACTION"
  std::vector<std::string> panel;     // its lines
  bool after_action = false;          // OK (and REPLAY when `replay`) in place of ACCEPT and DECLINE
  bool replay = false;
};

using HubScreen = Mw1Screen;

// The hub's state: pushed over the main menu by the loop (ProgramFlow::main_entered). Escape is QUIT.
class HubState : public Mw1State {
 public:
  HubState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show(const HubView& v);  // now, and on each entering
};

}  // namespace mw1
