// Probe for tests/differential/test_pm_mp_stations.py: Pirate's Moon's multiplayer stations 0x004938e0 (the port's
// program_mp_stations with the table's mp_stations_mark) on an MCB waynet of dock points, against the exe's.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgame/zgame_table.h"
#include "mech3/app_vehicle.h"
#include "mech3/mech_waynet.h"
#include "mech3/mission_world.h"
#include "pm/pm_game.h"

namespace {
void retire(zcls::Node* n) {  // out of the next case's name lookups
  if (n) zcls::node_set_name(n, "probe_pm_mp_stations_done");
}
}  // namespace

extern "C" {
// in: n points, each its place (3 floats) and +0x0d; have_station: the world's mfb_station01 exists. out, per point:
// +0x0d after, its station's number from its label (-1 none), the station's place, whether it is shown, whether it
// hangs under the world. Returns the stations made.
int probe_pm_mp_stations(int n, const float* pos, const uint8_t* marks, int have_station, uint8_t* out_marks,
                         int32_t* out_number, float* out_at, int32_t* out_shown, int32_t* out_in_world) {
  zgame::set_game(pm::table());
  zcls::Node* const was_world = mech3::world_hooks().world;
  zcls::Node* world = zcls::object3d_create();
  mech3::world_hooks().world = world;
  zcls::Node* station = nullptr;
  if (have_station) {
    station = zcls::object3d_create();
    zcls::node_set_name(station, "mfb_station01");
  }
  auto* w = new mech3::McbWaynet(1, "mcbwaynet");  // left alive: its destructor lets go of the waynet icons
  for (int i = 0; i < n; ++i) {
    char name[16];
    std::snprintf(name, sizeof name, "dock%d", i);
    auto* p = new mech3::WaynetPoint(name, nullptr, pos + 3 * i, nullptr, 0, 1, 0, 0, 0, 0);
    p->static_mfb = marks[i];
    w->add(p);
  }
  const int made = mech3::program_mp_stations(static_cast<mech3::Waynet*>(w));
  for (int i = 0; i < n; ++i) {
    const mech3::WaynetPoint* p = mech3::as_point(w->elem(i + 1));  // the port's element 0 is the default behaviour
    out_marks[i] = p ? p->static_mfb : 0xff;
    out_number[i] = -1, out_shown[i] = 0, out_in_world[i] = 0;
    out_at[3 * i] = out_at[3 * i + 1] = out_at[3 * i + 2] = 0.0f;
    int k = -1;
    if (!p || !p->has_label || std::sscanf(p->label.c_str(), "mfb_station_%d", &k) != 1) continue;
    out_number[i] = k;
    zcls::Node* s = zcls::list_find_by_name(zcls::kAllNodes, p->label.c_str());
    if (!s) continue;
    zcls::object3d_get_translation(s, &out_at[3 * i], &out_at[3 * i + 1], &out_at[3 * i + 2]);
    out_shown[i] = (s->flags() & 4) != 0;
    out_in_world[i] = s->parent_count() == 1 && s->parents && s->parents[0] == world;
    retire(s);
  }
  retire(station);
  retire(world);
  mech3::world_hooks().world = was_world;
  return made;
}
}  // extern "C"
