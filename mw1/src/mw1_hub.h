#pragma once
// MW1's planet hub (VISION.md, the home base): the screen the company sits in
// between missions, on MW1's screen template (mw1_screen.h) with MW1's own
// layout, mw1hub.zrd in MW1's data folder, and MW1's own art (FIXES.md item 3:
// The owner's hub mockup in MW1's layout): the world's scenery, the plaques, the
// services as picture tiles down both sides where MW1's hub tables put them
// (FIXES.md item 11, MW_MAIN.EXE DS:1083 and 10D7), the middle panel only for
// AFTER ACTION and messages, dismissed with OK (item 16), the company's strip
// between the plaques (C-BILLS, PAYROLL, KILLS; the owner on item 16: MW1's hub had
// no company box) and the status line (what to do, a tile's description on
// hover, the contract taken). Contracts come from the House office only
// (item 7): the hub shows the one accepted and its DEPLOY.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_screen.h"

namespace mw1::hub {

inline constexpr const char* kHubFile = "mw1hub.zrd";     // MW1's data folder
inline constexpr const char* kHubDialog = "MW1HUBDIALOG";  // its dialog

// The hub's buttons, by their ids (and their names in mw1hub.zrd's BUTTONS). MW1's tiles: COMPANY, MECH COMPLEX and
// HOUSE OFFICE down the left, STARPORT, BAR and OPTIONS down the right; with a contract accepted, DEPLOY in the
// office's place and the right column gone (DS:10D7).
// FIXES.md item 47: STORY (the journal, mw1_journal.h), a small button under the right column once the story has begun.
// FIXES.md items 62, 63 (mw1_log.h): READ in the box's foot when its messages have news (their stories), and LOG, a
// small button under the left column, always (a contract taken too).
enum HubButton : int32_t {
  kHubOk = 1, kHubReplay, kHubMarket, kHubBar, kHubTravel, kHubOffice, kHubOptions, kHubCompany, kHubDeploy, kHubStory, kHubRead, kHubLog,
  kHubQuit  // FIXES.md item 100: at the foot between LOG and STORY; PM's quit box (the main menu's) asks first
};
inline constexpr int32_t kHubButtonCount = 13;
// The middle panel's width in its line font's columns (4:3's 376: 48 fit); a longer line wraps there, indented.
inline constexpr int32_t kHubPanelColumns = 48;
const char* hub_button_name(int32_t id);  // "ACCEPT", ... ("" for no button)

// What the hub shows: the plaques, the company's strip, the middle panel (AFTER
// ACTION or the messages), the status line.
struct HubView {
  std::string scenery;                // the backdrop's picture (mw1_scenery.h's scenery_image)
  std::string office;                 // the House office tile's look: "mw1_tile_house_davion"
  std::string planet, house;          // the left plaque: "ROBINSON", "HOUSE DAVION"
  std::string date, day;              // the right one: "APRIL 3024", "DAY 14 \xB7 PAYDAY IN 16"
  std::vector<std::string> labels, values;  // the company strip's rows: "C-BILLS", "412,750"
  std::string status;                 // the status line: what to do, a hovered tile's description, the contract
  // The middle panel (FIXES.md item 16, MW1's notifications box): there only for something to say, AFTER ACTION or
  // `message`, its OK under it; otherwise the hub is the scenery and the tiles.
  std::string title;                  // its title: "AFTER ACTION", "MESSAGES"
  std::vector<std::string> panel;     // its lines
  bool message = false;               // messages (the log's unheard entries, mw1_log.h): OK dismisses them
  bool read = false;                  // the messages have news: READ opens their stories
  bool after_action = false;          // OK (and REPLAY when `replay`) under the panel, the services off
  bool replay = false;
  bool contract = false;              // a contract accepted: DEPLOY in the office's place, the right column gone
  bool office_open = true;            // the world has a House office: its tile (empty on a world with none, MW1's)
  bool story = false;                 // the story has begun (its first event seen, MW1's exe read): STORY
  size_t page = 0;                    // the box's page (FIXES.md item 42: kBoxLinesMax lines a page, OK the next)
};
inline constexpr int32_t kHubBoxWidth = 376;  // the box between the tile columns, at 4:3 and 16:9 (mw1_box_376_<n>)
// The box's pages for the panel's lines (wrapped as shown): one at the least.
size_t hub_pages(const std::vector<std::string>& panel);

// The hub's OPTIONS (MW1's options hold the game's own: ours, PM's settings and the main menu).
inline constexpr const char* kHubOptionsDialog = "MW1HUBOPTIONS";  // in mw1hub.zrd
// SETTINGS (MW1's, FIXES 126: mw1_settings.h; the sound settings of FIXES.md item 100 are its SOUND now), SAVE GAME and
// LOAD GAME (FIXES 125: the save slots' screen), MAIN MENU, BACK.
enum HubOptionsButton : int32_t { kHubOptSettings = 1, kHubOptMainMenu, kHubOptBack, kHubOptSaveGame, kHubOptLoadGame };
inline constexpr int32_t kHubOptionsButtonCount = 5;
const char* hub_options_button_name(int32_t id);

using HubScreen = Mw1Screen;

// The hub's state: pushed over the main menu by the loop (ProgramFlow::main_entered). Escape is QUIT.
class HubState : public Mw1State {
 public:
  HubState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show(const HubView& v);  // now, and on each entering
  // Development (MW1_SCREEN=quit): Pirate's Moon's quit question over the hub once it has drawn (--shot the box).
  bool update() override;

 private:
  int32_t frames_ = 0;
};

// The OPTIONS menu over the hub's scenery: SETTINGS, SAVE GAME, LOAD GAME, MAIN MENU, BACK.
// Escape is BACK.
class HubOptionsState : public Mw1State {
 public:
  HubOptionsState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show(const std::string& scenery, const std::string& planet);
};

}  // namespace mw1::hub

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace hub;
}  // namespace mw1
