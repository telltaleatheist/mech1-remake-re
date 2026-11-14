#include "mech3/node_vehicle_dlg.h"

#include "gamez/zreader/zreader.h"
#include "mech3/mech_names.h"
#include "mech3/zai_target.h"
#include "mech3/zrdraiv.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x0049d350, Pirate's Moon 1.0 @ 0x004a07a0
const std::vector<std::string>& node_vehicle_types() {
  static std::vector<std::string> types;  // [0x006099ec..0x006099f0]
  static bool made = false;                // [0x006099f8]
  if (made) return types;
  for (int32_t i = 0; i < kMechCount; ++i)  // 0x13; Pirate's Moon's 0x19
    if (const char* n = mech_name(i)) types.emplace_back(n);
  if (zrdr::Node* v = zrdr::load("vehicle.zrd")) {  // 0x005e0378
    const zrdr::Node& pairs = v->list[1];
    for (int32_t k = 0; k < (pairs.list[0].i - 1) / 2; ++k) {
      const zrdr::Node& key = pairs.list[1 + 2 * k];
      types.emplace_back(key.type == zrdr::String ? key.s : "");  // FIX (A): the exe strdup'd any item as text
    }
    zrdr::free_node(v);  // 0x00556820
  }
  made = true;
  return types;
}

// Mech3.exe 1.2 @ 0x0049d810, Pirate's Moon 1.0 @ 0x004a0c00: each 5-bit group of the number a bit of the mask.
void node_vehicle_set_team(NodeVehicleDlg& d, uint32_t team, uint32_t friends, uint32_t foes) {
  uint32_t mask = 1u << (team & 0x1f);
  for (team >>= 5; team; team >>= 5) mask |= 1u << (team & 0x1f);
  d.team = static_cast<int32_t>(ai::team_number_of(mask));  // 0x004d71a0
  d.side = (friends & mask) ? 0 : (foes & mask) ? 1 : 2;
}

namespace {
int32_t first_bit(uint32_t keep) {
  for (int32_t i = 0; i < 0x20; ++i)
    if (keep & (1u << i)) return i;
  return -1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0049d8d0, Pirate's Moon 1.0 @ 0x004a0cb0
// Mech3.exe 1.2 @ 0x0049d920, Pirate's Moon 1.0 @ 0x004a0d00
// Mech3.exe 1.2 @ 0x0049d970, Pirate's Moon 1.0 @ 0x004a0d50
void node_vehicle_pick_side(NodeVehicleDlg& d, int32_t side, uint32_t friends, uint32_t foes) {
  d.side = side;  // UpdateData(TRUE): the radio
  d.team = first_bit(side == 0 ? friends : side == 1 ? foes : ~(friends | foes));
}

// Mech3.exe 1.2 @ 0x0049d9e0, Pirate's Moon 1.0 @ 0x004a0da0
void node_vehicle_team_typed(NodeVehicleDlg& d, uint32_t friends, uint32_t foes) {
  node_vehicle_set_team(d, static_cast<uint32_t>(d.team), friends, foes);
}

// Mech3.exe 1.2 @ 0x0049d710 (the string at 0x005e7cd8), Pirate's Moon 1.0 @ 0x004a0b00
std::vector<std::pair<std::string, const char*>> node_vehicle_rows(const std::vector<const char*>& types) {
  std::vector<std::pair<std::string, const char*>> rows;
  rows.emplace_back("None", nullptr);  // CB_INSERTSTRING 0, its data 0
  for (const char* t : types) rows.emplace_back(t, t);
  return rows;
}

// Mech3.exe 1.2 @ 0x0049d7b0 (x 0.017453292, the double at 0x005bc350), Pirate's Moon 1.0 @ 0x004a0ba0
void node_vehicle_close(NodeVehicleDlg& d, const char* chosen) {
  d.yaw = static_cast<float>(d.yaw * 0.01745329251994);
  d.type = chosen;
}

// Mech3.exe 1.2 @ 0x004df6a0, Pirate's Moon 1.0 @ 0x004f07a0 (its flags at +0xec: the port's +0xcc)
void node_vehicle_record(ai::AiParams& r, int32_t net, const NodeVehicleDlg& d) {
  r.put<int32_t>(0x04, net);
  r.put<float>(0x08, d.x);
  r.put<float>(0x0c, d.y);
  r.put<float>(0x10, d.z);
  r.put<float>(0x14, d.yaw);
  r.put<int32_t>(0x18, d.team);  // and +0xdc (the mech3 tag) 0, the record's
}

// Mech3.exe 1.2 @ 0x004e0540, Pirate's Moon 1.0 @ 0x004f18e0
bool node_vehicle_entry(NodeVehicleHooks& h, std::string* name, int32_t net, const NodeVehicleDlg& d) {
  ai::AiParams r;  // 0x004df6a0 makes the record's own (vtable 0x005cd438) without 0x004e0490's reference on aiv.zrd
  node_vehicle_record(r, net, d);
  return h.add_entry(name, d.type, r);
}

// Mech3.exe 1.2 @ 0x00403820, Pirate's Moon 1.0 @ 0x00403830 (the boxes captioned, the made-path its own)
void node_vehicle_add(NodeVehicleHooks& h) {
  if (!h.editor_on()) return;
  float pos[3];
  if (!h.first_pick(pos)) {
    h.box("Select a node first");  // 0x005e03c0
    return;
  }
  NodeVehicleDlg d;  // 0x0049d350
  d.x = pos[0], d.y = pos[1], d.z = pos[2], d.yaw = 0;
  if (h.modal(d) == 0 || !d.type) return;
  ai::AivRecord r;  // 0x004e0490
  if (!h.net_ready() && !h.net_rebuilt()) {
    h.box("Could not locate the net");  // 0x005e03a4
    return;
  }
  const int32_t net = h.net_number();
  std::string name;
  if (node_vehicle_entry(h, &name, net, d)) h.made(name, net, d);
}

}  // namespace mech3
