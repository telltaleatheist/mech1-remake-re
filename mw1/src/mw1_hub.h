#pragma once
// MW1's planet hub (VISION.md, the home base): the screen the company sits in
// between missions, on MW1's screen template (mw1_screen.h) with MW1's own
// layout, mw1hub.zrd in MW1's data folder, and MW1's own art (FIXES.md item 3:
// Owen's hub mockup in MW1's layout): the world's scenery, the plaques, the
// services as picture tiles down both sides, the middle panel (the contract,
// AFTER ACTION, the news and the messages), the company's green terminal and
// the status line.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_screen.h"

namespace mw1::hub {

inline constexpr const char* kHubFile = "mw1hub.zrd";     // MW1's data folder
inline constexpr const char* kHubDialog = "MW1HUBDIALOG";  // its dialog

// The hub's buttons, by their ids (and their names in mw1hub.zrd's BUTTONS).
enum HubButton : int32_t { kHubAccept = 1, kHubDecline, kHubOk, kHubReplay, kHubMarket, kHubBar, kHubTravel, kHubQuit, kHubOffice, kHubOptions };
inline constexpr int32_t kHubButtonCount = 10;
const char* hub_button_name(int32_t id);  // "ACCEPT", ... ("" for no button)

// What the hub shows: the plaques, the company's terminal, the middle panel
// (the contract on offer or a mission's AFTER ACTION), the status line.
struct HubView {
  std::string scenery;                // the backdrop's picture (mw1_scenery.h's scenery_image)
  std::string office;                 // the House office tile's look: "mw1_tile_house_davion"
  std::string planet, house;          // the left plaque: "ROBINSON", "HOUSE DAVION"
  std::string date, day;              // the right one: "APRIL 3024", "DAY 14 \xB7 PAYDAY IN 16"
  std::vector<std::string> labels, values;  // the terminal's rows: "C-BILLS", "412,750"
  std::vector<std::string> mechs, kinds;    // its mechs: "Owens", "LIGHT"
  std::string status;                 // the status line's message
  std::string title;                  // the middle panel's: "CONTRACT ON OFFER", "AFTER ACTION"
  std::vector<std::string> panel;     // its lines
  bool after_action = false;          // OK (and REPLAY when `replay`) in place of ACCEPT and DECLINE
  bool replay = false;
  bool offer = true;                  // ACCEPT and DECLINE (none without an offer)
  bool office_open = true;            // the world has a House office: OFFICE opens, an offer or not (MW1's)
};

using HubScreen = Mw1Screen;

// The hub's state: pushed over the main menu by the loop (ProgramFlow::main_entered). Escape is QUIT.
class HubState : public Mw1State {
 public:
  HubState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show(const HubView& v);  // now, and on each entering
};

}  // namespace mw1::hub

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace hub;
}  // namespace mw1
