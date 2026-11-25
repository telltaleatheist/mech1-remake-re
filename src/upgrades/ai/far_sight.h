// Ours: enemies that come from however far off (MW1's rule, asked for by the program; MW3 and PM never ask). The exes'
// pilots know only what their sensors hold, 1000 m (docs/ai.md section 15): past that they patrol, and on most maps
// the patrol's net brings them. A pilot whose patrol can't come near the company (no step it can walk to within
// kNear) is given the attack from any distance: mode 1 (PM's may attack), a range of kFarEngage, no break-offs; the
// rest keep their pilots as shipped. Attacking at once is not the rule for all: the attack's drive goes straight and
// jams on ground a patrol's net goes round.
#pragma once
#include "mech3/zai_target.h"

namespace mech3::ai {
class Brain;
class BrainControls;
inline constexpr float kFarEngage = 20000.0f;  // metres, squared in the params (+0x20); no shipped record passes 10 km

void set_far_sight(bool on);  // off at start
bool far_sight_on();
// A pilot's pick when its sensors have none: the nearest of the player and his lancemates under orders (AiTarget +0x80,
// kept at any distance), for a pilot sent by far_sight_run; else null.
Contact* far_pick(const Brain& b, zgame::Handle vehicle);
void far_sight_run(Brain& b, BrainControls& c);  // each frame, before the orders run
}  // namespace mech3::ai
