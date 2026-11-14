#include "mech3/debug_dialogs.h"

#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "mech3/app.h"
#include "mech3/app_hooks.h"
#include "mech3/mech_names.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/player_view.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x00483170, Pirate's Moon 1.0 @ 0x00485b20: on only with a 3D card that is the primary display driver.
void debug_menu_fullscreen_ui(Engine& e, Mech3App& app, MenuItemUI& ui) {
  bool on = false;
  if (e.hw_card_flag()) {
    const char* name = e.video_device_name(static_cast<uint32_t>(e.hw_card_dev()));
    on = name && std::strcmp(name, "Primary Display Driver") == 0;  // FIX (A): a nameless device (the exe read null)
  }
  ui.enable(on);
  ui.check(app.fullscreen());  // [0x00608e4f]
}

// Mech3.exe 1.2 @ 0x00483200, Pirate's Moon 1.0 @ 0x00485ba0
void debug_menu_joystick(Engine& e) { e.set_use_joystick(1 - (use_joystick() == 1)); }

// Mech3.exe 1.2 @ 0x00483230, Pirate's Moon 1.0 @ 0x00485bd0
void debug_menu_joystick_ui(MenuItemUI& ui) { ui.check(use_joystick() == 1); }

// Mech3.exe 1.2 @ 0x00483330, Pirate's Moon 1.0 @ 0x00485c60 (its +0x1ae)
void debug_menu_enable_ai_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b2)); }  // [0x00608e3a]

// Mech3.exe 1.2 @ 0x00483570, Pirate's Moon 1.0 @ 0x00485e10 (its +0x1af)
void debug_menu_third_person_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b3)); }  // [0x00608e3b]

// Mech3.exe 1.2 @ 0x00483150
// Mech3.exe 1.2 @ 0x00483310
// Mech3.exe 1.2 @ 0x00483550
// Mech3.exe 1.2 @ 0x004835a0
// Pirate's Moon 1.0 @ 0x00485b10, 0x00485c50, 0x00485e00, 0x00485e30 (its flags 4 bytes lower, full screen 2)
void debug_menu_toggle(Mech3App& app, int32_t off) { app.byte_at(off) = app.byte_at(off) == 0; }

// Mech3.exe 1.2 @ 0x004835f0
void debug_menu_reset_options(Mech3App& app) { app.set_defaults(); }

// Mech3.exe 1.2 @ 0x004835c0, Pirate's Moon 1.0 @ 0x00485e40 (its +0x1b4)
void debug_menu_invincible_ui(Mech3App& app, MenuItemUI& ui) { ui.check(app.byte_at(0x1b8)); }

// Mech3.exe 1.2 @ 0x00483600, Pirate's Moon 1.0 @ 0x00485e60
void debug_menu_play_cd_ui(MenuItemUI& ui) {
  ui.check(1);
  ui.enable(false);
}

// Mech3.exe 1.2 @ 0x00483620, Pirate's Moon 1.0 @ 0x00485e80
void debug_menu_d3d(Engine& e, Mech3App& app) { app.restore_video(e.hw_card_flag() == 0, true); }

// Mech3.exe 1.2 @ 0x00483640, Pirate's Moon 1.0 @ 0x00485ea0 (the device count, 0x00579aa0)
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
// Mech3.exe 1.2 @ 0x00483500
// Pirate's Moon 1.0 @ 0x00485d50, 0x00485db0
void debug_menu_vmode(Engine& e, int32_t mode) { e.set_vmode(mode); }

// Mech3.exe 1.2 @ 0x00483510
// Mech3.exe 1.2 @ 0x004834b0, 0x00483510; Pirate's Moon 1.0 @ 0x00485d60, 0x00485dc0: with a 3D card, off (the 320's also unchecked); else checked in its mode.
void debug_menu_vmode_ui(Engine& e, int32_t mode, MenuItemUI& ui) {
  if (e.hw_card_flag()) {
    if (mode == 3) ui.check(0);
    ui.enable(false);
    return;
  }
  ui.enable(true);
  ui.check(e.vmode() == mode);
}

// Mech3.exe 1.2 @ 0x00483390 (+0x1ac), 0x004833a0 (+0x1ae), 0x004833b0 (+0x1b0), 0x004833c0 (+0x1ad),
// 0x004833d0 (+0x1b1), 0x004833e0 (+0x1af)
// Mech3.exe 1.2 @ 0x004833a0
// Mech3.exe 1.2 @ 0x004833b0
// Mech3.exe 1.2 @ 0x004833c0
// Mech3.exe 1.2 @ 0x004833d0
// Mech3.exe 1.2 @ 0x004833e0
// Pirate's Moon 1.0 @ 0x00485cb0, 0x00485cc0, 0x00485cd0, 0x00485ce0, 0x00485cf0, 0x00485d00
void debug_menu_clear_byte(Mech3App& app, int32_t off) { app.byte_at(off) = 0; }

// Mech3.exe 1.2 @ 0x004833f0, Pirate's Moon 1.0 @ 0x00485d10
void debug_menu_checked_ui(MenuItemUI& ui) { ui.check(1); }

// Mech3.exe 1.2 @ 0x00483400, Pirate's Moon 1.0 @ 0x00485d20: the dword [0x00608e34] and the word [0x00608e38], the six above at once.
void debug_menu_clear_all(Mech3App& app) {
  for (int off = 0x1ac; off < 0x1b2; ++off) app.byte_at(off) = 0;
}

// Mech3.exe 1.2 @ 0x00483410, Pirate's Moon 1.0 @ 0x00485d30
void debug_menu_clear_all_ui(Engine& e, MenuItemUI& ui) { ui.text(e.message(0x274)); }

// Mech3.exe 1.2 @ 0x00483430
void debug_menu_pick_mech(Mech3App& app, int32_t (*pick)()) {
  (void)mech_index(app.mech_.c_str());  // 0x004845f0, passed to the list and unread there
  const int32_t m = pick ? pick() : -1;
  if (m != -1) app.mech_ = mech_name(m);  // 0x00484650
}

// Mech3.exe 1.2 @ 0x00483460
void debug_menu_mech_ui(Engine& e, Mech3App& app, MenuItemUI& ui) { ui.text(e.message_format(0x276, app.mech_.c_str())); }

// Mech3.exe 1.2 @ 0x00432d30, Pirate's Moon 1.0 @ 0x00433840
std::vector<ListRow> mech_list_rows() {
  std::vector<ListRow> rows;
  for (int32_t i = 0; i < kMechCount; ++i)  // 0x13; Pirate's Moon's 0x19
    if (const char* n = mech_name(i)) rows.push_back({n, i});  // LB_ADDSTRING, LB_SETITEMDATA
  return rows;
}

// Mech3.exe 1.2 @ 0x00432c60 (OK: 0x00432da0), Pirate's Moon 1.0 @ 0x00433770 (OK: 0x004fc4d0, shared)
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

// Mech3.exe 1.2 @ 0x004af000, Pirate's Moon 1.0 @ 0x004b2ec0 (its mech test vt+0x48)
std::vector<VehicleRow> vehicle_list_rows(const std::vector<void*>& entities, void* (*vehicle_of)(void*),
                                          bool (*is_mech)(void*), const char* (*name)(void*)) {
  std::vector<VehicleRow> rows;
  for (void* e : entities) {
    void* v = e ? vehicle_of(e) : nullptr;
    if (v && is_mech(v)) rows.push_back({name(e), e});  // CB_ADDSTRING, CB_SETITEMDATA
  }
  return rows;
}

// Mech3.exe 1.2 @ 0x004a3550 (the close: 0x004aefb0), Pirate's Moon 1.0 @ 0x004a6940 (0x004b2e70; its take
// vt+0x114)
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

// Mech3.exe 1.2 @ 0x0040bb00, Pirate's Moon 1.0 @ 0x00409830 (the mode on the stack)
bool camera_dialog(CameraMode& m, bool (*edit)(CameraFields& f)) {
  CameraFields f = camera_fields(m);  // the dialog 0x0040bd00 (its box clear)
  if (!edit || !edit(f)) return false;  // 0x0040bea0: DoModal, IDOK
  camera_apply(m, f);
  return true;
}

namespace {
DebugStartHooks g_default_start;
DebugStartHooks* g_start = &g_default_start;

// The campaign jumps: the operation, the mission, the campaign game selected again (40090 only), mission 3-5's
// extra weapon (40097), and the mechs put in the store (0x004836e0's list, to -1).
struct CampaignJump {
  int32_t item, campaign, mission;
  bool select;
  bool weapon;
  std::vector<int32_t> mechs;
};
const std::vector<CampaignJump>& campaign_jumps() {
  static const std::vector<CampaignJump> t = {
      {40085, 1, 1, false, false, {3}},  // Mech3.exe 1.2 @ 0x00483720
      {40086, 1, 2, false, false, {3, 11}},  // Mech3.exe 1.2 @ 0x00483770
      {40087, 1, 3, false, false, {3, 11}},  // Mech3.exe 1.2 @ 0x004837c0
      {40088, 1, 4, false, false, {3, 8, 14, 11}},  // Mech3.exe 1.2 @ 0x00483810
      {40089, 2, 1, false, false, {3, 13, 11, 2}},  // Mech3.exe 1.2 @ 0x00483860
      {40090, 2, 2, true, false, {3, 13, 11, 2}},  // Mech3.exe 1.2 @ 0x004838b0
      {40091, 2, 3, false, false, {3, 13, 11, 2}},  // Mech3.exe 1.2 @ 0x00483910
      {40092, 2, 4, false, false, {3, 13, 11, 2}},  // Mech3.exe 1.2 @ 0x00483960
      {40093, 3, 1, false, false, {3, 13, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x004839b0
      {40094, 3, 2, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483a10
      {40095, 3, 3, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483a70
      {40096, 3, 4, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483ac0
      {40097, 3, 5, false, true, {3, 8, 14, 10, 12}},  // Mech3.exe 1.2 @ 0x00483b20
      {40098, 3, 6, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483b90
      {40099, 4, 1, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483bf0
      {40100, 4, 2, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483c50
      {40101, 4, 3, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483cb0
      {40102, 4, 4, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483d10
      {40103, 4, 5, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483d60
      {40104, 4, 6, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483dc0
      {40114, 4, 7, false, false, {3, 8, 14, 10, 11, 2, 12}},  // Mech3.exe 1.2 @ 0x00483e20, in no menu
  };
  return t;
}
}  // namespace

DebugStartHooks& debug_start_hooks() { return *g_start; }
void set_debug_start_hooks(DebugStartHooks* h) { g_start = h ? h : &g_default_start; }

bool DebugStartHooks::install_check() { return mech3::install_check(app().engine()); }
Pilot* DebugStartHooks::start_campaign(int32_t c, int32_t m) { return debug_start_campaign(c, m); }
Pilot* DebugStartHooks::create_pilot(const char* name) { return mech3::create_pilot(name); }
void DebugStartHooks::set_pilot(Pilot* p) { current_pilot() = p; }
void DebugStartHooks::select_game(Pilot* p, int32_t type) { p->select_game(type); }
const char* DebugStartHooks::message(int32_t id) { return app().engine().message(id); }
void DebugStartHooks::add_mech(Pilot* p, int32_t mech, const char* config) {
  pilot_hooks().store_add_mech(*p->store(), mech, config);
}
void DebugStartHooks::add_weapon(Pilot* p, int32_t weapon, int32_t tech) { p->store()->add_weapon(weapon, tech); }
void DebugStartHooks::set_training(Pilot* p, int32_t mission) { p->training_game()->mission = mission; }
void DebugStartHooks::set_lab_level(Pilot* p, int32_t level) { static_cast<LabGame*>(p->current)->level = level; }
int32_t DebugStartHooks::default_mech() { return mech_index(app().mech_.c_str()); }
void DebugStartHooks::set_slot_mech(Pilot* p, int32_t mech) { p->current->slot.mech = mech; }
void DebugStartHooks::post_intro(bool lobby) { app().pick_post_intro_state(lobby); }
void DebugStartHooks::start() { app().start(app().main_window()); }
void DebugStartHooks::restart_in_lab() { app().restart_in_lab(); }
void DebugStartHooks::network_set(int32_t on) { app().engine().network_set(on); }
void DebugStartHooks::reopen_instant_action(uint8_t a, uint8_t b) { app().set_reopen_instant_action(a, b); }

// Mech3.exe 1.2 @ 0x00480420, Pirate's Moon 1.0 @ 0x00482b40
bool install_check(Engine& e) {
  const bool hw = e.hw_card_flag() != 0;
  if (e.install_options(hw ? 0xff : 0xff00) != 0) return true;
  const std::string caption = e.message(0x1eb);  // copied to the stack first
  const char* text = e.message(hw ? 0xf42 : 0xf41);
  const uint32_t hwnd = e.focus_window();
  e.message_box_text(hwnd, text, caption.c_str(), 0x30);
  return false;
}

// Mech3.exe 1.2 @ 0x00483100, Pirate's Moon 1.0 @ 0x00485ac0
void debug_game_start() {
  DebugStartHooks& h = *g_start;
  if (!h.install_check()) return;
  h.post_intro(false);
  h.start();
}

// Mech3.exe 1.2 @ 0x00483720 (the rest by campaign_jumps(); their mechs through 0x004836e0)
void debug_campaign_jump(int32_t item) {
  const CampaignJump* j = nullptr;
  for (const CampaignJump& c : campaign_jumps())
    if (c.item == item) j = &c;
  DebugStartHooks& h = *g_start;
  if (!j || !h.install_check()) return;
  Pilot* p = h.start_campaign(j->campaign, j->mission);
  h.set_pilot(p);
  if (j->select) h.select_game(p, kCampaign);
  // Mech3.exe 1.2 @ 0x004836e0: each mech with message 0xf20's configuration
  for (int32_t m : j->mechs) h.add_mech(p, m, h.message(0xf20));
  if (j->weapon) h.add_weapon(p, 0x20, 3);
  h.post_intro(true);
  h.start();
}

// Mech3.exe 1.2 @ 0x00483e80
// Mech3.exe 1.2 @ 0x00483ed0
// Mech3.exe 1.2 @ 0x00483f20
// Mech3.exe 1.2 @ 0x00483f70
void debug_training_jump(int32_t mission) {
  DebugStartHooks& h = *g_start;
  if (!h.install_check()) return;
  Pilot* p = h.create_pilot("Default Player");  // 0x005e5efc
  h.set_pilot(p);
  h.select_game(p, kTraining);
  h.set_training(p, mission);
  h.post_intro(true);
  h.start();
}

// Mech3.exe 1.2 @ 0x00483fc0
// Mech3.exe 1.2 @ 0x00484040
// Mech3.exe 1.2 @ 0x004840c0
void debug_lab_jump(int32_t level) {
  DebugStartHooks& h = *g_start;
  if (!h.install_check()) return;
  Pilot* p = h.create_pilot("Default Player");
  h.set_pilot(p);
  h.select_game(p, kMechLab);
  h.set_lab_level(p, level);
  h.set_slot_mech(p, level == 6 ? 3 : h.default_mech());
  h.post_intro(true);
  if (level == 6) h.restart_in_lab();
  else h.start();
}

// Mech3.exe 1.2 @ 0x00484190, Pirate's Moon 1.0 @ 0x00486290
void debug_network_start() {
  debug_game_start();
  g_start->network_set(1);
}

// Mech3.exe 1.2 @ 0x004841b0, Pirate's Moon 1.0 @ 0x004862b0
void debug_instant_action_start() {
  debug_game_start();
  g_start->reopen_instant_action(1, 0);
}

// Pirate's Moon 1.0 @ 0x00485f40, 0x00485f80, 0x00485fb0, 0x00485fe0, 0x00486010, 0x00486040
// Pirate's Moon 1.0 @ 0x00486070, 0x004860a0, 0x004860d0, 0x00486100, 0x00486300, 0x00486330
// Missions 1..12 in that order: the mission by its number (0x0047ad20, clamped to 10 there), no mechs added; only
// the first asks the install check.
void pm_debug_campaign_jump(int32_t mission) {
  DebugStartHooks& h = *g_start;
  if (mission == 1 && !h.install_check()) return;
  Pilot* p = h.start_campaign(0, mission);
  h.set_pilot(p);
  h.post_intro(true);
  h.start();
}

// Pirate's Moon 1.0 @ 0x00486130, 0x00486190, 0x004861f0
// Levels 7, 8 and 6 (6 checked, the lab through 0x00482620): the "DefaultPlayer" pilot (no space, as the
// campaign's), mech 5 in the slot whatever the default.
void pm_debug_lab_jump(int32_t level) {
  DebugStartHooks& h = *g_start;
  if (level == 6 && !h.install_check()) return;
  Pilot* p = h.create_pilot("DefaultPlayer");  // 0x00607aec
  h.set_pilot(p);
  h.select_game(p, kMechLab);
  h.set_lab_level(p, level);
  h.set_slot_mech(p, 5);
  h.post_intro(true);
  if (level == 6) h.restart_in_lab();
  else h.start();
}

}  // namespace mech3
