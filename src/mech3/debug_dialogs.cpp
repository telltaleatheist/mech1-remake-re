#include "mech3/debug_dialogs.h"

#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "mech3/app.h"
#include "mech3/app_hooks.h"
#include "mech3/mech_names.h"
#include "mech3/options.h"
#include "mech3/player_view.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x00483170: on only with a 3D card that is the primary display driver.
void debug_menu_fullscreen_ui(Engine& e, Mech3App& app, MenuItemUI& ui) {
  bool on = false;
  if (e.hw_card_flag()) {
    const char* name = e.video_device_name(static_cast<uint32_t>(e.hw_card_dev()));
    on = name && std::strcmp(name, "Primary Display Driver") == 0;  // FIX (A): a nameless device (the exe read null)
  }
  ui.enable(on);
  ui.check(app.fullscreen());  // [0x00608e4f]
}

// Mech3.exe 1.2 @ 0x00483200
void debug_menu_joystick(Engine& e) { e.set_use_joystick(1 - (use_joystick() == 1)); }

// Mech3.exe 1.2 @ 0x00483230
void debug_menu_joystick_ui(MenuItemUI& ui) { ui.check(use_joystick() == 1); }

// Mech3.exe 1.2 @ 0x00483330
void debug_menu_enable_ai_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b2)); }  // [0x00608e3a]

// Mech3.exe 1.2 @ 0x00483570
void debug_menu_third_person_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b3)); }  // [0x00608e3b]

// Mech3.exe 1.2 @ 0x00483150, 0x00483310, 0x00483550, 0x004835a0
void debug_menu_toggle(Mech3App& app, int32_t off) { app.byte_at(off) = app.byte_at(off) == 0; }

// Mech3.exe 1.2 @ 0x004835f0
void debug_menu_reset_options(Mech3App& app) { app.set_defaults(); }

// Mech3.exe 1.2 @ 0x004835c0
void debug_menu_invincible_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b8)); }

// Mech3.exe 1.2 @ 0x00483600
void debug_menu_play_cd_ui(MenuItemUI& ui) {
  ui.check(1);
  ui.enable(false);
}

// Mech3.exe 1.2 @ 0x00483620
void debug_menu_d3d(Engine& e, Mech3App& app) { app.restore_video(e.hw_card_flag() == 0, true); }

// Mech3.exe 1.2 @ 0x00483640 (the device count, 0x00579aa0)
void debug_menu_d3d_ui(Engine& e, MenuItemUI& ui) {
  if (e.video_device_count() == 0) {
    ui.enable(false);
    return;
  }
  ui.enable(true);
  ui.check(e.hw_card_flag() != 0);
  if (e.hw_card_flag()) {
    ui.text(e.message_format(0x277, e.video_device_name(static_cast<uint32_t>(e.hw_card_dev()))));
    return;
  }
  ui.text(e.message(0x278));
}

// Mech3.exe 1.2 @ 0x004834a0, 0x00483500 (0x0047fc10, a thunk to 0x0040d810)
void debug_menu_vmode(Engine& e, int32_t mode) { e.set_vmode(mode); }

// Mech3.exe 1.2 @ 0x004834b0, 0x00483510: with a 3D card, off (the 320's also unchecked); else checked in its mode.
void debug_menu_vmode_ui(Engine& e, int32_t mode, MenuItemUI& ui) {
  if (e.hw_card_flag()) {
    if (mode == 3) ui.check(0);
    ui.enable(false);
    return;
  }
  ui.enable(true);
  ui.check(e.vmode() == mode);
}

// Mech3.exe 1.2 @ 0x00483390, 0x004833a0, 0x004833b0, 0x004833c0, 0x004833d0, 0x004833e0
void debug_menu_clear_byte(Mech3App& app, int32_t off) { app.byte_at(off) = 0; }

// Mech3.exe 1.2 @ 0x004833f0
void debug_menu_checked_ui(MenuItemUI& ui) { ui.check(1); }

// Mech3.exe 1.2 @ 0x00483400: the dword [0x00608e34] and the word [0x00608e38], the six above at once.
void debug_menu_clear_all(Mech3App& app) {
  for (int off = 0x1ac; off < 0x1b2; ++off) app.byte_at(off) = 0;
}

// Mech3.exe 1.2 @ 0x00483410
void debug_menu_clear_all_ui(Engine& e, MenuItemUI& ui) { ui.text(e.message(0x274)); }

// Mech3.exe 1.2 @ 0x00483430
void debug_menu_pick_mech(Mech3App& app, int32_t (*pick)()) {
  (void)mech_index(app.mech_.c_str());  // 0x004845f0, passed to the list and unread there
  const int32_t m = pick ? pick() : -1;
  if (m != -1) app.mech_ = mech_name(m);  // 0x00484650
}

// Mech3.exe 1.2 @ 0x00483460
void debug_menu_mech_ui(Engine& e, Mech3App& app, MenuItemUI& ui) { ui.text(e.message_format(0x276, app.mech_.c_str())); }

// Mech3.exe 1.2 @ 0x00432d30
std::vector<ListRow> mech_list_rows() {
  std::vector<ListRow> rows;
  for (int32_t i = 0; i < 0x13; ++i)
    if (const char* n = mech_name(i)) rows.push_back({n, i});  // LB_ADDSTRING, LB_SETITEMDATA
  return rows;
}

// Mech3.exe 1.2 @ 0x00432c60 (OK: 0x00432da0)
int32_t mech_list_pick(void (*gdi)(), int32_t (*modal)(const std::vector<ListRow>& rows, int32_t& sel),
                       void (*keys_reset)()) {
  const std::vector<ListRow> rows = mech_list_rows();
  int32_t sel = 0;  // LB_SETCURSEL 0
  if (gdi) gdi();  // 0x00432df0
  const int32_t r = modal ? modal(rows, sel) : 0;
  if (keys_reset) keys_reset();
  if (r != 1) return -1;
  return sel >= 0 && sel < static_cast<int32_t>(rows.size()) ? rows[sel].data : -1;  // LB_ERR without one
}

// Mech3.exe 1.2 @ 0x004af000
std::vector<VehicleRow> vehicle_list_rows(const std::vector<void*>& entities, void* (*vehicle_of)(void*),
                                          bool (*is_mech)(void*), const char* (*name)(void*)) {
  std::vector<VehicleRow> rows;
  for (void* e : entities) {
    void* v = e ? vehicle_of(e) : nullptr;
    if (v && is_mech(v)) rows.push_back({name(e), e});  // CB_ADDSTRING, CB_SETITEMDATA
  }
  return rows;
}

// Mech3.exe 1.2 @ 0x004a3550 (the close: 0x004aefb0)
void* vehicle_pick(const std::vector<VehicleRow>& rows, void (*gdi)(),
                   int32_t (*modal)(const std::vector<VehicleRow>& rows, int32_t& sel)) {
  if (gdi) gdi();
  int32_t sel = rows.size() > 1 ? 1 : -1;  // CB_SETCURSEL 1: none when there is no second row
  if (!modal || modal(rows, sel) == 0) return nullptr;
  // FIX (A): with nothing selected the exe keeps the dialog's +0x60 unset and calls through it (fixes.md).
  return sel >= 0 && sel < static_cast<int32_t>(rows.size()) ? rows[sel].entity : nullptr;
}

// Mech3.exe 1.2 @ 0x0040bb00's fill: the angles x 57.29577951308 (a double; 0x005bc348), kept as floats.
CameraFields camera_fields(const CameraMode& m) {
  CameraFields f;
  f.dist = m.dist;
  std::memcpy(f.pos, m.desired_pos, sizeof f.pos);
  float r[9];
  zmth::quat_matrix(m.delta_rot, r);  // 0x00525740
  zmth::Mat34 mat{};
  std::memcpy(mat.m, r, sizeof r);
  float e[3];
  zmth::matrix_to_euler(mat, e);  // 0x00523bc0
  for (int i = 0; i < 3; ++i) f.angles[i] = static_cast<float>(e[i] * 57.29577951308);
  f.catch_up = m.dist_catch_up;
  f.vary = m.dist_vary;
  f.factor = m.dist_factor;
  return f;
}

// Mech3.exe 1.2 @ 0x0040bb00's OK: the angles x 0.01745329251994 (0x005bc350), the euler's 1 passed first.
void camera_apply(CameraMode& m, const CameraFields& f) {
  m.dist = f.dist;
  std::memcpy(m.desired_pos, f.pos, sizeof f.pos);
  if (f.normalize) {
    zmth::Vec3 v{m.desired_pos[0], m.desired_pos[1], m.desired_pos[2]};
    zmth::normalize(v);  // 0x004020d0
    m.desired_pos[0] = v.x, m.desired_pos[1] = v.y, m.desired_pos[2] = v.z;
  }
  zmth::euler_to_quat(static_cast<float>(f.angles[1] * 0.01745329251994), static_cast<float>(f.angles[0] * 0.01745329251994),
                      static_cast<float>(f.angles[2] * 0.01745329251994), m.delta_rot);  // 0x00525340
  m.dist_catch_up = f.catch_up;
  m.dist_vary = f.vary;
  m.dist_factor = f.factor;
}

// Mech3.exe 1.2 @ 0x0040bb00
bool camera_dialog(CameraMode& m, bool (*edit)(CameraFields& f)) {
  CameraFields f = camera_fields(m);  // the dialog 0x0040bd00 (its box clear)
  if (!edit || !edit(f)) return false;  // 0x0040bea0: DoModal, IDOK
  camera_apply(m, f);
  return true;
}

}  // namespace mech3
