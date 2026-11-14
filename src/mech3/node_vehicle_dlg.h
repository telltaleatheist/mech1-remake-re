#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// The net editor's Add veh to node in the release (editor.md, Add veh to node): template 141, "Node Edit - Vehicle"
// (0x0049d350..0x0049d9e0; the original file is not known: it sits between mission.cpp and player.cpp), and the
// row's own step 0x00403820. Ported for preservation; the editor runs ours (upgrades/editor/add_vehicle), which
// does what this was meant to. A team here is the 5-bit packed number (zai_target.h, team_number_of); the sides
// are the player's mech's friend (+0xc4) and foe (+0xc8) masks.
namespace mech3 {

namespace ai {
class AiParams;
}

struct NodeVehicleDlg {
  const char* type = nullptr;  // +0x60  the picked row's data: a type's name, null for "None"
  float x = 0, y = 0;          // +0xa4, +0xa8
  float yaw = 0;               // +0xac  degrees in the form, radians once closed
  float z = 0;                 // +0xb0
  int32_t team = 0;            // +0xb4
  int32_t side = 0;            // +0xb8  0 Friend, 1 Foe, 2 Other
};

// 0x0049d350: the dialog's type list, made once ([0x006099f8]): the mechs' names (0x00484650, a null skipped), then
// each record name of vehicle.zrd (its pairs' keys); the dialog's fields zero, its team set from 0 (0x0049d810).
const std::vector<std::string>& node_vehicle_types();
// 0x0049d810 (the constructor's with 0): the team as its mask, packed again (0x004d71a0), and its side.
void node_vehicle_set_team(NodeVehicleDlg& d, uint32_t team, uint32_t friends, uint32_t foes);
// 0x0049d8d0 (Friend), 0x0049d920 (Foe), 0x0049d970 (Other): the side clicked, the team the first of it (the lowest
// bit of the mask; Other the lowest in neither), -1 with none.
void node_vehicle_pick_side(NodeVehicleDlg& d, int32_t side, uint32_t friends, uint32_t foes);
// 0x0049d9e0: the team typed in, its side again.
void node_vehicle_team_typed(NodeVehicleDlg& d, uint32_t friends, uint32_t foes);
// 0x0049d710: the combo's rows (each type, its name its data), "None" inserted first with no data, the first chosen.
std::vector<std::pair<std::string, const char*>> node_vehicle_rows(const std::vector<const char*>& types);
// 0x0049d7b0: on close, the chosen row's data and the yaw in radians.
void node_vehicle_close(NodeVehicleDlg& d, const char* chosen);
// 0x004df6a0, as 0x004e0540 calls it: net, x, y, z, yaw and team over a fresh record's defaults (its +0xdc 0).
void node_vehicle_record(ai::AiParams& r, int32_t net, const NodeVehicleDlg& d);

// 0x00403820's calls out, in its order.
class NodeVehicleHooks {
 public:
  virtual ~NodeVehicleHooks() = default;
  virtual bool editor_on() = 0;                              // [0x005f4c6c]
  virtual bool first_pick(float pos[3]) = 0;                 // 0x00403770 then the node's vt+0x1c
  virtual void box(const char* text) = 0;                    // 0x00402f10(0x30), captioned "CaiEdit"
  virtual int32_t modal(NodeVehicleDlg& d) = 0;              // 0x0049d8b0: DoModal, the keys reset
  virtual bool net_ready() = 0;                              // the editor's +0x14 vt+0x34
  virtual bool net_rebuilt() = 0;                            // 0x004bd340, 0x004be670 (version 106 only)
  virtual int32_t net_number() = 0;                          // the net's vt+0x18, get_id (0x0040a850)
  // 0x004671d0: the record under the type's name into the net's entry; it returns 0 in the release.
  virtual bool add_entry(std::string* name, const char* type, const ai::AiParams& r) = 0;
  virtual void made(const std::string& name, int32_t net, const NodeVehicleDlg& d) = 0;  // what follows a true
};
// 0x004e0540: the record built (0x004df6a0) and handed to 0x004671d0.
bool node_vehicle_entry(NodeVehicleHooks& h, std::string* name, int32_t net, const NodeVehicleDlg& d);
// 0x00403820: nothing without the editor; the box without a pick; then the dialog at the node (yaw 0), and on any
// result but 0 with a type picked (Cancel too) the record built and handed to 0x004671d0.
void node_vehicle_add(NodeVehicleHooks& h);

}  // namespace mech3
