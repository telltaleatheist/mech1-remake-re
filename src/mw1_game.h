#pragma once
// MW1's loop over Pirate's Moon's game (VISION.md, "The first milestone"):
// the hub between missions (for now PM's message box), one contract, the
// launch into a PM Instant Action map, and the settlement when it comes back.
// It sits on mw3-re's extension points for programs built on the engine
// (mech3/program_flow.h, ObjectivesExtension in mech3/mech_objective.h).
#include <cstdint>

namespace mw1 {

// Set the loop on the engine before PM's game main runs. A seed of 0 rolls
// the company at random; another gives the same company and contracts (tests).
void install_game_loop(uint32_t seed);
// Taken off the engine again, and the company dropped (tests).
void uninstall_game_loop();
struct Company;
// The loop's company, null before the first hub (tests).
const Company* game_loop_company();

}  // namespace mw1
