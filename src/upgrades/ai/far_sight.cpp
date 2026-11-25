#include "upgrades/ai/far_sight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/zai_wire_states_pm.h"

namespace mech3::ai {
namespace {
constexpr int32_t kPlayerTeam = 1;
constexpr float kNear = 500.0f;  // metres: a patrol that comes this near will meet the company
bool g_on = false;
std::unordered_map<const Brain*, bool> g_looked;  // brain, sent
}  // namespace

void set_far_sight(bool on) {
  g_on = on;
  g_looked.clear();
}
bool far_sight_on() { return g_on; }

// The least distance to `p` of the steps the pilot may walk to from `from`.
static float reach(Behaviour* from, const Pilot* pilot, const float p[3]) {
  float best = 1e9f;
  std::unordered_set<const Behaviour*> seen{from};
  std::deque<Behaviour*> next{from};
  while (!next.empty()) {
    Behaviour* s = next.front();
    next.pop_front();
    if (const float* q = s->position()) best = std::min(best, std::hypot(q[0] - p[0], q[2] - p[2]));
    for (int32_t i = 0, n = s->link_count(); i < n; ++i) {
      Behaviour* to = s->link(i) ? s->link(i)->as_behaviour() : nullptr;
      if (to && to->passable(pilot, s) && seen.insert(to).second) next.push_back(to);
    }
  }
  return best;
}

static Contact* company(zgame::Handle v) {
  const AiTarget* t = brain_hooks().targetable(v);
  return !t ? nullptr : t->nearest_lance ? t->nearest_lance : t->player_contact;
}

Contact* far_pick(const Brain& b, zgame::Handle vehicle) {
  if (!g_on || b.params.team() == kPlayerTeam || !(b.params.get<float>(0x20) >= kFarEngage * kFarEngage)) return nullptr;
  return company(vehicle);
}

void far_sight_run(Brain& b, BrainControls& c) {
  if (!g_on || b.params.team() == kPlayerTeam || b.orders.empty() || g_looked.count(&b)) return;
  Order& o = b.current();
  auto* net = o.net ? dynamic_cast<CompiledNet*>(o.net->net()) : nullptr;
  auto* state = dynamic_cast<PmMoveState*>(o.state.get());
  if (!net || !state) return;
  const zgame::Handle v = c.vehicle();
  Contact* who = company(v);
  if (!who) return;
  BrainHooks& h = brain_hooks();
  float p[3];
  h.position(h.contact_vehicle(who), p);
  h.pilot_of(v, &b.path_pilot);
  const float off = reach(state->behaviour(), &b.path_pilot, p);
  const bool way = off <= kNear;
  std::printf("mw1: an enemy %.0f m off, its patrol comes within %.0f m of the company: %s\n", std::sqrt(who->dist2), off,
              way ? "as the map has it" : "comes");
  g_looked[&b] = !way;
  if (way) return;
  b.params.put<int32_t>(0x3c, 1);
  b.params.put<float>(0x20, kFarEngage * kFarEngage);
  b.params.put<uint8_t>(0xcc, static_cast<uint8_t>(b.params.get<uint8_t>(0xcc) & ~3));
  BrainTickPm& t = pm_brain(b).tick;
  t.mode = 1, t.engage = kFarEngage * kFarEngage;
  t.engage_flags = static_cast<decltype(t.engage_flags)>(t.engage_flags & ~3);
}
}  // namespace mech3::ai
