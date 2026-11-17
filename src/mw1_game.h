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
// Every new contract a battle (destroy all) on this world's Instant Action map (by folder: "mw3c2", 1): a test
// override. Empty: the contracts' own (mw1_missions.h).
void set_game_loop_world(const std::string& world, int32_t ia);
// Every new contract on this row of the map table (mw1_missions.h), whatever was rolled (tests: the map survey,
// each objective's win).
namespace missions {
struct MapRow;
}
void set_game_loop_row(const missions::MapRow& row);
// What the mission's objectives were built as: "" (destroy all), "reach", "attack" or "defend"; `fell_back` when
// the contract's structure wasn't on the map (tests).
std::string game_loop_objective_built(bool* fell_back = nullptr);
// Taken off the engine again, and the company dropped (tests).
void uninstall_game_loop();
struct Company;
struct Contract;
// The loop's company, null before the first hub (tests).
const Company* game_loop_company();
// The contract on offer, or accepted while its mission runs; null before the first hub (tests).
const Contract* game_loop_contract();
struct AfterAction;
namespace hub {
class HubState;
}
using namespace hub;
// A mission back and not settled (the hub's AFTER ACTION), else null (tests).
const AfterAction* game_loop_pending();
// The hub's state, once the loop has made it (tests: its screen shows what the player sees); a press of one of
// its buttons (mw1_hub.h's HubButton), as the screen's own.
HubState* game_loop_hub();
namespace market_ui {
class MarketState;
}
using namespace market_ui;
// The market's state once the hub's MARKET has made it (tests); the company to change in a test (funds, the
// market's stock) and the market's screen shown again after such a change.
MarketState* game_loop_market();
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
// The lancemates the last launch offered (tests): who, the map record's aim error and the one set.
struct AllyLaunch {
  int32_t id = 0;
  float record_error = 0, error = 0;
};
const std::vector<AllyLaunch>& game_loop_ally_launches();
void game_loop_market_changed();
void game_loop_press(int32_t button);

}  // namespace mw1
