// Pirate's Moon's AI, the port's side of tests/differential/test_pm_ai_*.py.
#include <cstdint>
#include <cstring>
#include <new>

#include "gamez/zclass/cls_node.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_inout.h"

namespace ai = mech3::ai;

// test_pm_ai_start.py: the mission-start listeners (0x004944e0, 0x004e4b00).
// In: which valves the data named and their values; out: their values after, and
// whether "bapon" exists then.
extern "C" void probe_pm_ai_start(int32_t named_start, int32_t start, int32_t named_bap, int32_t bap, int32_t player_bap,
                                  int32_t* out) {
  ai::valves_clear();
  if (named_start) ai::valve_get("on_mission_start")->set(start);
  if (named_bap) ai::valve_get("bapon")->set(bap);
  ai::valves_mission_started(player_bap);
  const ai::Valve* s = ai::valve_find("on_mission_start");
  const ai::Valve* b = ai::valve_find("bapon");
  out[0] = s ? s->value->get() : -1;
  out[1] = b ? b->value->get() : -1;
  ai::valves_clear();
}

// test_pm_ai_inout.py: the boundary's variant (0x004cdb30) and child match
// (0x004cdce0). Out: index, count, game, prefix, numbered, valid, the match; name.
namespace {
int32_t g_inout_type = 0, g_inout_mission = 0;
}
extern "C" void probe_pm_inout_variant(int32_t type, int32_t mission, const char* child, int32_t* out, char* name) {
  g_inout_type = type, g_inout_mission = mission;
  ai::InOutHooks& h = ai::inout_hooks();
  h = ai::InOutHooks{};
  h.have_game = [] { return true; };
  h.game_type = [] { return g_inout_type; };
  h.mission_number = [] { return g_inout_mission; };
  const ai::InOutVariant v = ai::inout_variant();
  out[0] = v.index, out[1] = v.count, out[2] = v.game, out[3] = v.prefix, out[4] = v.numbered, out[5] = v.valid;
  out[6] = v.match(child);
  std::memcpy(name, v.name, sizeof v.name);
  h = ai::InOutHooks{};
}

// PM's in/out record (0x54 bytes): its constructor 0x4c7f30 or reset 0x4c8010 on `bytes` with the frame
// counter at `frame` (test_pm_inout_record.py).
extern "C" void probe_pm_inout_record(int reset, const uint8_t* bytes, uint32_t frame, uint8_t* out) {
  alignas(mech3::ai::InOutRecordPm) uint8_t raw[sizeof(mech3::ai::InOutRecordPm)];
  std::memcpy(raw, bytes, sizeof raw);
  auto* r = reinterpret_cast<mech3::ai::InOutRecordPm*>(raw);
  const uint32_t frame_was = zcls::frames().frame;
  zcls::frames().frame = frame;
  if (reset) r->reset();
  else new (raw) mech3::ai::InOutRecordPm();
  zcls::frames().frame = frame_was;
  std::memcpy(out, raw, sizeof raw);
}
