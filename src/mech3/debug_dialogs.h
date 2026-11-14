#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The developers' menu and dialogs (editor.md, Which of them code still opens): the frame window's debug menu
// (resource 147, taken off the window at engine start, 0x00480570, so never seen; its handlers 0x00483170..0x00483640
// sit with the app, between mech_waynet.cpp and mission.cpp), the Select Mech list (156, 0x00432c60), the Vehicle
// Select list (143, 0x004af000 for 0x004a3550) and the Camera Properties dialog (106, 0x0040bb00). Their MFC windows
// are not ported; what they show and what they change is, for preservation. None is reachable in the release.
namespace mech3 {

class Engine;
class Mech3App;
class CameraMode;

// MFC's CCmdUI as the menu's update handlers use it: vt+0x00 Enable, +0x04 SetCheck, +0x0c SetText.
struct MenuItemUI {
  virtual ~MenuItemUI() = default;
  virtual void enable(bool on) = 0;
  virtual void check(int32_t on) = 0;
  virtual void text(const char* s) = 0;
};

// The update handlers (each item's state when the menu opens) and the commands, by menu 147's items (editor.md, The
// debug menu); 40037 and 40058..40066 are in no menu the exe has.
void debug_menu_fullscreen_ui(Engine& e, Mech3App& app, MenuItemUI& ui);  // 0x00483170, 40014 Fullscreen
void debug_menu_joystick(Engine& e);                                       // 0x00483200, 40016 Joystick
void debug_menu_joystick_ui(MenuItemUI& ui);                               // 0x00483230
// The four flags the menu flips (`off` the app's): 0x00483150 Fullscreen (+0x1c7), 0x00483310 Enable AI (+0x1b2),
// 0x00483550 Third Person View (+0x1b3), 0x004835a0 Invincible (+0x1b8).
void debug_menu_toggle(Mech3App& app, int32_t off);
void debug_menu_reset_options(Mech3App& app);                              // 0x004835f0, 40051: 0x0047fb80
void debug_menu_enable_ai_ui(Mech3App& app, MenuItemUI& ui);               // 0x00483330, 40039 Enable AI
void debug_menu_third_person_ui(Mech3App& app, MenuItemUI& ui);            // 0x00483570, 40055 Third Person View
void debug_menu_invincible_ui(Mech3App& app, MenuItemUI& ui);              // 0x004835c0, 40070 Invincible
void debug_menu_play_cd_ui(MenuItemUI& ui);                                // 0x00483600, 40063 Play CD Tracks
void debug_menu_d3d(Engine& e, Mech3App& app);                             // 0x00483620, 40071 Direct 3D
void debug_menu_d3d_ui(Engine& e, MenuItemUI& ui);                         // 0x00483640
void debug_menu_vmode(Engine& e, int32_t mode);  // 0x004834a0 (40069 320 x 240: 3), 0x00483500 (40067 640 x 480: 5)
void debug_menu_vmode_ui(Engine& e, int32_t mode, MenuItemUI& ui);  // 0x004834b0 (3), 0x00483510 (5)
// 0x00483390.. (40058..40065): one byte of the app's +0x1ac..+0x1b1 cleared; each item checked (0x004833f0).
void debug_menu_clear_byte(Mech3App& app, int32_t off);
void debug_menu_checked_ui(MenuItemUI& ui);                                // 0x004833f0
void debug_menu_clear_all(Mech3App& app);                                  // 0x00483400, 40066: +0x1ac..+0x1b1
void debug_menu_clear_all_ui(Engine& e, MenuItemUI& ui);                   // 0x00483410: message 0x274
// 0x00483430, 40037: the Select Mech list (`pick`, 0x00432c60; the current mech's index is passed and unread), the
// pick the default mech's name.
void debug_menu_pick_mech(Mech3App& app, int32_t (*pick)());
void debug_menu_mech_ui(Engine& e, Mech3App& app, MenuItemUI& ui);         // 0x00483460: message 0x276 of the mech

// The Select Mech list (0x00432c60): its rows (0x00432d30: the 19 mechs' names, a null name skipped, each row's data
// its number; the first selected), shown by `modal` (DoModal: 1 on OK, `sel` the row chosen) between `gdi`
// (0x00577340) and `keys_reset` (0x0051a300), as 0x00432df0 runs them; on OK the chosen row's number (0x00432da0:
// LB_GETCURSEL, LB_GETITEMDATA), else -1.
struct ListRow {
  std::string name;
  int32_t data;
};
std::vector<ListRow> mech_list_rows();
int32_t mech_list_pick(void (*gdi)(), int32_t (*modal)(const std::vector<ListRow>& rows, int32_t& sel),
                       void (*keys_reset)());

// Vehicle Select (0x004a3550): the rows (0x004af000), zEntity's mechs (vt+0x2c, then vt+0x40) by their name (the
// entity's +0x04), each row's data the entity; the second selected (CB_SETCURSEL 1). The row selected when the
// window closes (0x004aefb0) is taken whenever DoModal returns non-zero, Cancel too; the caller runs its vt+0x108.
// `gdi` is 0x00577340 (the GDI surface flipped to). Returns the entity, or null.
struct VehicleRow {
  std::string name;
  void* entity;
};
std::vector<VehicleRow> vehicle_list_rows(const std::vector<void*>& entities, void* (*vehicle_of)(void*),
                                          bool (*is_mech)(void*), const char* (*name)(void*));
void* vehicle_pick(const std::vector<VehicleRow>& rows, void (*gdi)(),
                   int32_t (*modal)(const std::vector<VehicleRow>& rows, int32_t& sel));

// The Camera Properties dialog (0x0040bb00, over a camera mode, the view's +0x58): its fields filled from the
// mode, the angles in degrees from the offset's rotation (0x00525740, 0x00523bc0); on OK they go back, the place
// normalised when its box is ticked (0x004020d0), the rotation rebuilt from the angles (0x00525340, which takes the
// first two the other way round). `edit` stands for the modal dialog (0x0040bea0): true on OK.
struct CameraFields {
  float dist = 0;          // +0x28
  float pos[3] = {};       // +0x18
  float angles[3] = {};    // degrees: the euler's 0, 1, 2
  float catch_up = 0;      // +0x34
  float vary = 0;          // +0x30
  float factor = 0;        // +0x2c
  bool normalize = false;  // the box, clear when the dialog opens
};
CameraFields camera_fields(const CameraMode& m);
void camera_apply(CameraMode& m, const CameraFields& f);
bool camera_dialog(CameraMode& m, bool (*edit)(CameraFields& f));

}  // namespace mech3
