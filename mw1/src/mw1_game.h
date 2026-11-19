#pragma once
// MW1's loop over Pirate's Moon's game (VISION.md, "The first milestone"):
// the hub between missions (mw1_hub.h; PM's message boxes when MW1's layout
// is missing), one contract, the
// launch into a PM Instant Action map, and the settlement when it comes back.
// It sits on mw3-re's extension points for programs built on the engine
// (mech3/program_flow.h, ObjectivesExtension in mech3/mech_objective.h).
#include <cstdint>
#include <vector>
#include <string>

namespace mw1 {

// Set the loop on the engine before PM's game main runs. A seed of 0 rolls
// the company at random; another gives the same company and contracts (tests).
// A structure's node (`reach`, e.g. c2 ia2's "portalgate") makes every
// contract reach-a-structure on that map (a test override: contracts choose
// their own objective and map, mw1_missions.h).
void install_game_loop(uint32_t seed, const char* reach = nullptr);
// The save (mw1_save.h): the company read from `path` at the first hub when it
// is there (unless `new_company`: one made new, over it), and written after
// each settlement and at each new offer. Empty: no save (tests).
void set_game_loop_save(const std::string& path, bool new_company);
// How the game starts (mw1_menu.h): MW1's main menu (the program's), NEW CAMPAIGN at once over the menu without its
// question (--new-company: the intro, the old save kept as mw1save.prev), or the hub at once as before the menu (the
// tests that don't drive the menu).
enum class Start { kMenu, kNewCampaign, kHub };
void set_game_loop_start(Start s);
namespace menu {
class MenuState;
class IntroState;
class CreditsState;
}  // namespace menu
menu::MenuState* game_loop_menu();    // the menu's state once made (tests)
menu::IntroState* game_loop_intro();  // the intro's, once made
menu::CreditsState* game_loop_credits();  // the credits', while they roll
namespace movie {
class MovieState;
}  // namespace movie
movie::MovieState* game_loop_movie();  // the DropShip's launch (FIXES.md item 54), once made
void game_loop_press_menu(int32_t button);  // a menu button, as pressed (tests)
// Every new contract a battle (destroy all) on this world's Instant Action map (by folder: "mw3c2", 1): a test
// override. Empty: the contracts' own (mw1_missions.h).
void set_game_loop_world(const std::string& world, int32_t ia);
// Every new contract on this row of the map table (mw1_missions.h), whatever was rolled (tests: the map survey,
// each objective's win).
namespace missions {
struct MapRow;
}
void set_game_loop_row(const missions::MapRow& row);
// Every new contract's force these MW1 kinds (tests: the ladder's map survey); empty: the contracts' own.
void set_game_loop_enemies(const std::vector<int32_t>& kinds);
// What the mission's objectives were built as: "" (destroy all), "reach", "attack" or "defend"; `fell_back` when
// the contract's structure wasn't on the map (tests).
std::string game_loop_objective_built(bool* fell_back = nullptr);
// FIXES.md item 45 (tests): where the contract's structure is in the mission (its node's place, or its parts' middle
// for a group); false without one.
bool game_loop_structure_point(float out[3]);
// Taken off the engine again, and the company dropped (tests).
void uninstall_game_loop();
struct Company;
struct Contract;
struct Request;
// The loop's company, null before the first hub (tests).
const Company* game_loop_company();
// The contract on offer, or accepted while its mission runs; null before the first hub (tests).
const Contract* game_loop_contract();
const Request* game_loop_office_ask();  // the office's terms asked so far (tests)
struct AfterAction;
namespace hub {
class HubState;
class HubOptionsState;
}
using namespace hub;
// A mission back and not settled (the hub's AFTER ACTION), else null (tests).
const AfterAction* game_loop_pending();
// The hub's state, once the loop has made it (tests: its screen shows what the player sees); a press of one of
// its buttons (mw1_hub.h's HubButton), as the screen's own.
HubState* game_loop_hub();
HubOptionsState* game_loop_hub_options();  // the hub's OPTIONS state once made (tests)
// The results screen's state once made (FIXES.md item 19; tests: its screen shows what the player sees), and the last
// panel it showed.
namespace results {
class ResultsState;
struct ResultsPanel;
}
results::ResultsState* game_loop_results();
const results::ResultsPanel* game_loop_results_shown();
namespace market_ui {
class MarketState;
struct MarketChoice;
}
using namespace market_ui;
// The market's state once the hub's MARKET has made it (tests); the company to change in a test (funds, the
// market's stock) and the market's screen shown again after such a change.
MarketState* game_loop_market();
// FIXES.md item 58 (tests): the Mech Complex's chosen rows, and the 'Mech its live panel draws (-1 none).
const MarketChoice* game_loop_market_choice();
int32_t game_loop_market_panel_mech();
// The mech lab's PILOT line while the Mech Complex's MECH LAB is open (tests), else null.
namespace lab_pilot {
class PilotChooser;
class LabFunds;
}
const lab_pilot::PilotChooser* game_loop_lab_pilot();
const lab_pilot::LabFunds* game_loop_lab_funds();  // the lab's funds once up (tests: FIXES.md item 41)
// The travel screen's state once TRAVEL has made it (tests), else null.
namespace travel_ui {
class TravelState;
}
using namespace travel_ui;
TravelState* game_loop_travel();
Company* game_loop_company_for_tests();
namespace bar_ui {
class BarState;
}
using namespace bar_ui;
BarState* game_loop_bar();  // the Bar's state once made (tests)
namespace office_ui {
class OfficeState;
}
using namespace office_ui;
OfficeState* game_loop_office();  // the House office's state once made (tests)
namespace event_ui {
class EventState;
}
using namespace event_ui;
EventState* game_loop_event();  // the event panel's state once made (tests)
// MW1's story (mw1_story.h) on or off: on in the game; the loop tests that aren't about it turn it off.
void set_game_loop_story(bool on);
// The offer drawn again for the company as a test changed it (game_loop_company_for_tests: its world, standing,
// patience), and the hub shown again.
void game_loop_offer_again();
// The lancemates the last launch offered (tests): who, the map record's aim error and the one set.
struct AllyLaunch {
  int32_t id = 0;
  float record_error = 0, error = 0;
};
const std::vector<AllyLaunch>& game_loop_ally_launches();
// A story battle's enemies' aim errors as they launched (FIXES.md item 47, tests).
const std::vector<float>& game_loop_enemy_aims();
void game_loop_market_changed();
// MW1's loading screen (mw1_loading.h, FIXES.md item 18), as the tests see it: how many loads it has been up for, its
// lines the last time (the screen's texts), and at the launches, the frames PM's briefing drew and whether its script
// played (none, never). Held (tests): once loaded the Load state waits, PM's briefing still unseen, for the test to
// launch or abort it itself (BriefingScreen::launch, abort), as the lab fit and ABORT were made at the briefing before.
struct LoadingSeen {
  int32_t loads = 0, briefing_frames = 0;
  bool briefing_script = false;
  std::vector<std::string> lines;
  std::string scene;  // its backdrop's picture (mw1image.zbd's name)
  std::string enemy;  // the main enemy's picture shown beside the terminal (empty: none)
};
LoadingSeen game_loop_loading_seen();
void set_game_loop_hold_launch(bool on);
void game_loop_press(int32_t button);

}  // namespace mw1
