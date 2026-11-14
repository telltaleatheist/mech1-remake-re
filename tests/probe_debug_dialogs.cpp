// Probes for test_debug_dialogs.py: the debug menu's handlers, the Select Mech and Vehicle Select lists and the
// Camera Properties dialog (mech3/debug_dialogs), each call outside logged as the test logs the exe's.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "mech3/app.h"
#include "mech3/debug_dialogs.h"
#include "mech3/options.h"
#include "mech3/player_view.h"
#include "probe_app.h"

namespace {

std::string g_log;
std::vector<std::string> g_names;
int32_t g_hw = 0, g_dev = 0, g_count = 0, g_vmode = 0, g_pick = -1;
bool g_picked = false;
std::string g_text;

void put(const std::string& s) {
  if (!g_picked) g_log += s + ";";
}

struct DdEngine : mech3::Engine {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    put(std::string("unported ") + key);
    return 0;
  }
  int32_t hw_card_flag() override { return g_hw; }
  int32_t hw_card_dev() override { return g_dev; }
  uint32_t video_device_count() override { return static_cast<uint32_t>(g_count); }
  const char* video_device_name(uint32_t i) override { return i < g_names.size() ? g_names[i].c_str() : nullptr; }
  int32_t vmode() override { return g_vmode; }
  void set_vmode(int32_t m) override { put("vmode " + std::to_string(m)); }
  void set_use_joystick(int32_t on) override {
    put("joy " + std::to_string(on));
    mech3::set_use_joystick(on);
  }
  const char* message(int32_t id) override { return (g_text = "m" + std::to_string(id)).c_str(); }
  const char* message_format(int32_t id, const char* arg) override {
    return (g_text = "f" + std::to_string(id) + ":" + (arg ? arg : "(null)")).c_str();
  }
  // restore_video(hw, true): software and an install, so (hw false) selects the software path at once and (hw
  // true) asks for the device; what follows the ask is not this test's.
  bool software_renderer() override { return true; }
  uint32_t install_options(uint32_t) override { return 1; }
  void select_hw(int32_t) override { put("restore 0 1"); }
  int32_t pick_video_device(int32_t, int32_t) override {
    put("restore 1 1");
    g_picked = true;
    return -1;
  }
  void set_hw_card_dev(int32_t) override {}
};

struct Ui : mech3::MenuItemUI {
  void enable(bool on) override { put(std::string("E") + (on ? "1" : "0")); }
  void check(int32_t on) override { put("C" + std::to_string(on)); }
  void text(const char* s) override { put(std::string("T") + s); }
};

DdEngine* g_engine = nullptr;
mech3::Mech3App* g_app = nullptr;

int32_t pick() { return g_pick; }

int32_t out(const std::string& s, char* buf, int32_t cap) {
  const int32_t n = static_cast<int32_t>(s.size()) < cap ? static_cast<int32_t>(s.size()) : cap;
  std::memcpy(buf, s.data(), static_cast<size_t>(n));
  return n;
}

int32_t g_modal = 0, g_sel = 0;
std::string g_rows;
int32_t mech_modal(const std::vector<mech3::ListRow>& rows, int32_t& sel) {
  for (const auto& r : rows) g_rows += r.name + "=" + std::to_string(r.data) + ";";
  g_rows += "sel " + std::to_string(sel) + ";";
  if (g_sel != -2) sel = g_sel;
  return g_modal;
}
int32_t vehicle_modal(const std::vector<mech3::VehicleRow>& rows, int32_t& sel) {
  for (const auto& r : rows) g_rows += r.name + "=" + std::to_string(reinterpret_cast<uintptr_t>(r.entity)) + ";";
  g_rows += "sel " + std::to_string(sel) + ";";
  if (g_sel != -2) sel = g_sel;
  return g_modal;
}
// The fake entities: [kind, name index]; kind 0 none, 1 a vehicle not a mech, 2 a mech.
const int32_t* g_ents = nullptr;
void* vehicle_of(void* e) { return reinterpret_cast<const int32_t*>(e)[0] ? e : nullptr; }
bool is_mech(void* v) { return reinterpret_cast<const int32_t*>(v)[0] == 2; }
const char* ent_name(void* e) { return g_names[static_cast<size_t>(reinterpret_cast<const int32_t*>(e)[1])].c_str(); }

}  // namespace

extern "C" {

// One handler by its address. s: hw, dev, count, vmode, joystick, pick, then the app's bytes +0x1ac..+0x1b8 and
// +0x1c7 (14); names: the devices, '\n' between; mech: the default mech's name. Out: the log, then the bytes and mech.
int32_t probe_dd_menu(uint32_t addr, const int32_t* s, const char* names, const char* mech, char* buf, int32_t cap) {
  probe_app::Mute quiet;
  if (!g_engine) {
    g_engine = new DdEngine;
    g_app = new mech3::Mech3App(g_engine);
  }
  mech3::set_app(g_app);
  g_hw = s[0], g_dev = s[1], g_count = s[2], g_vmode = s[3], g_pick = s[5];
  mech3::set_use_joystick(s[4]);
  for (int i = 0; i < 0x1c; ++i) g_app->byte_at(0x1ac + i) = i < 13 ? static_cast<uint8_t>(s[6 + i]) : 0;
  g_app->fullscreen() = static_cast<uint8_t>(s[19]);
  g_app->mech_ = mech;
  g_names.clear();
  for (const char* p = names; *p;) {
    const char* e = std::strchr(p, '\n');
    g_names.emplace_back(p, e ? static_cast<size_t>(e - p) : std::strlen(p));
    if (!e) break;
    p = e + 1;
  }
  g_log.clear();
  g_picked = false;
  Ui ui;
  DdEngine& e = *g_engine;
  mech3::Mech3App& app = *g_app;
  switch (addr) {
    case 0x483170: mech3::debug_menu_fullscreen_ui(e, app, ui); break;
    case 0x483200: mech3::debug_menu_joystick(e); break;
    case 0x483230: mech3::debug_menu_joystick_ui(ui); break;
    case 0x483330: mech3::debug_menu_enable_ai_ui(app, ui); break;
    case 0x483390: mech3::debug_menu_clear_byte(app, 0x1ac); break;
    case 0x4833a0: mech3::debug_menu_clear_byte(app, 0x1ae); break;
    case 0x4833b0: mech3::debug_menu_clear_byte(app, 0x1b0); break;
    case 0x4833c0: mech3::debug_menu_clear_byte(app, 0x1ad); break;
    case 0x4833d0: mech3::debug_menu_clear_byte(app, 0x1b1); break;
    case 0x4833e0: mech3::debug_menu_clear_byte(app, 0x1af); break;
    case 0x4833f0: mech3::debug_menu_checked_ui(ui); break;
    case 0x483400: mech3::debug_menu_clear_all(app); break;
    case 0x483410: mech3::debug_menu_clear_all_ui(e, ui); break;
    case 0x483430: mech3::debug_menu_pick_mech(app, pick); break;
    case 0x483460: mech3::debug_menu_mech_ui(e, app, ui); break;
    case 0x4834a0: mech3::debug_menu_vmode(e, 3); break;
    case 0x4834b0: mech3::debug_menu_vmode_ui(e, 3, ui); break;
    case 0x483500: mech3::debug_menu_vmode(e, 5); break;
    case 0x483510: mech3::debug_menu_vmode_ui(e, 5, ui); break;
    case 0x483570: mech3::debug_menu_third_person_ui(app, ui); break;
    case 0x483150: mech3::debug_menu_toggle(app, 0x1c7); break;
    case 0x483310: mech3::debug_menu_toggle(app, 0x1b2); break;
    case 0x483550: mech3::debug_menu_toggle(app, 0x1b3); break;
    case 0x4835a0: mech3::debug_menu_toggle(app, 0x1b8); break;
    case 0x4835f0: mech3::debug_menu_reset_options(app); break;
    case 0x4835c0: mech3::debug_menu_invincible_ui(app, ui); break;
    case 0x483600: mech3::debug_menu_play_cd_ui(ui); break;
    case 0x483620: mech3::debug_menu_d3d(e, app); break;
    case 0x483640: mech3::debug_menu_d3d_ui(e, ui); break;
    default: return -1;
  }
  g_picked = false;
  char tail[64];
  std::string o = g_log + "|";
  for (int i = 0; i < 0x1c; ++i) {  // +0x1ac..+0x1c7
    std::snprintf(tail, sizeof tail, "%02x", g_app->byte_at(0x1ac + i));
    o += tail;
  }
  o += "|joy " + std::to_string(mech3::use_joystick()) + "|" + g_app->mech_;
  return out(o, buf, cap);
}

// The Select Mech list: the rows shown and the pick (modal: DoModal's result; sel: the row chosen, -2 none).
int32_t probe_dd_mech_list(int32_t modal, int32_t sel, char* buf, int32_t cap) {
  g_rows.clear();
  g_modal = modal, g_sel = sel;
  static std::string ev;
  ev.clear();
  const int32_t r = mech3::mech_list_pick([] { ev += "gdi;"; }, mech_modal, [] { ev += "keys;"; });
  return out(g_rows + "|" + ev + std::to_string(r), buf, cap);
}

// Vehicle Select over n fake entities (ents: kind, name index pairs; names '\n' separated): the rows, the gdi call,
// the entity taken (its index, or -1).
int32_t probe_dd_vehicle(const int32_t* ents, int32_t n, const char* names, int32_t modal, int32_t sel, char* buf,
                         int32_t cap) {
  g_names.clear();
  for (const char* p = names; *p;) {
    const char* e = std::strchr(p, '\n');
    g_names.emplace_back(p, e ? static_cast<size_t>(e - p) : std::strlen(p));
    if (!e) break;
    p = e + 1;
  }
  g_ents = ents;
  std::vector<void*> list;
  for (int32_t i = 0; i < n; ++i) list.push_back(ents[2 * i] < 0 ? nullptr : const_cast<int32_t*>(ents + 2 * i));
  g_rows.clear();
  g_modal = modal, g_sel = sel;
  static std::string gdi;
  gdi.clear();
  const auto rows = mech3::vehicle_list_rows(list, vehicle_of, is_mech, ent_name);
  // the row's data printed as the entity's index
  void* got = mech3::vehicle_pick(rows, [] { gdi = "gdi;"; }, vehicle_modal);
  std::string o;
  for (const auto& r : rows) o += r.name + "=" + std::to_string((static_cast<const int32_t*>(r.entity) - ents) / 2) + ";";
  o += "sel " + std::string(g_rows.substr(g_rows.rfind("sel ") + 4));
  const int32_t idx = got ? static_cast<int32_t>((static_cast<const int32_t*>(got) - ents) / 2) : -1;
  return out(gdi + o + "|" + std::to_string(idx), buf, cap);
}

// Camera Properties: m = the mode's delta_rot (4), desired_pos (3), dist, factor, vary, catch_up; edit (10, the
// dialog's order: dist, pos 3, angles 3, catch_up, vary, factor) written when `ok`, the box from `norm`. Out (21):
// the shown 10, then the mode's 11 after; the result returned.
int32_t probe_dd_camera(const float* m, int32_t ok, const float* edit, int32_t norm, float* o) {
  mech3::CameraMode mode(0);
  std::memcpy(mode.delta_rot, m, 16);
  std::memcpy(mode.desired_pos, m + 4, 12);
  mode.dist = m[7], mode.dist_factor = m[8], mode.dist_vary = m[9], mode.dist_catch_up = m[10];
  static float* shown;
  static const float* ed;
  static int32_t gok, gnorm;
  shown = o, ed = edit, gok = ok, gnorm = norm;
  const bool r = mech3::camera_dialog(mode, [](mech3::CameraFields& f) {
    const float v[10] = {f.dist, f.pos[0], f.pos[1], f.pos[2], f.angles[0], f.angles[1], f.angles[2], f.catch_up, f.vary, f.factor};
    std::memcpy(shown, v, sizeof v);
    if (!gok) return false;
    f.dist = ed[0], f.pos[0] = ed[1], f.pos[1] = ed[2], f.pos[2] = ed[3];
    f.angles[0] = ed[4], f.angles[1] = ed[5], f.angles[2] = ed[6];
    f.catch_up = ed[7], f.vary = ed[8], f.factor = ed[9];
    f.normalize = gnorm != 0;
    return true;
  });
  std::memcpy(o + 10, mode.delta_rot, 16);
  std::memcpy(o + 14, mode.desired_pos, 12);
  o[17] = mode.dist, o[18] = mode.dist_factor, o[19] = mode.dist_vary, o[20] = mode.dist_catch_up;
  return r ? 1 : 0;
}

}  // extern "C"

// The Game menu's starts (S9b): every call through DebugStartHooks logged as the test logs the exe's; the pilot
// set, the training mission and the slot's mech printed after (the exe writes those to memory).
namespace {
struct StartLog : mech3::DebugStartHooks {
  std::string log, msg;
  int32_t ok = 1, mech = 0, training = -1, slot = -1, pilot_set = 0;
  mech3::Pilot* const P = reinterpret_cast<mech3::Pilot*>(0x100);
  void put(const std::string& s) { log += s + ";"; }
  bool install_check() override { put("check"); return ok != 0; }
  mech3::Pilot* start_campaign(int32_t c, int32_t m) override {
    put("campaign " + std::to_string(c) + " " + std::to_string(m));
    return P;
  }
  mech3::Pilot* create_pilot(const char* name) override { put(std::string("create ") + name); return P; }
  void set_pilot(mech3::Pilot* p) override { pilot_set = p == P; }
  void select_game(mech3::Pilot*, int32_t t) override { put("select " + std::to_string(t)); }
  const char* message(int32_t id) override { return (msg = "m" + std::to_string(id)).c_str(); }
  void add_mech(mech3::Pilot*, int32_t m, const char* cfg) override { put("add " + std::to_string(m) + " " + cfg); }
  void add_weapon(mech3::Pilot*, int32_t w, int32_t t) override {
    put("weapon " + std::to_string(w) + " " + std::to_string(t));
  }
  void set_training(mech3::Pilot*, int32_t n) override { training = n; }
  void set_lab_level(mech3::Pilot*, int32_t l) override { put("level " + std::to_string(l)); }
  int32_t default_mech() override { return mech; }
  void set_slot_mech(mech3::Pilot*, int32_t m) override { slot = m; }
  void post_intro(bool lobby) override { put(std::string("post ") + (lobby ? "1" : "0")); }
  void start() override { put("start"); }
  void restart_in_lab() override { put("lab"); }
  void network_set(int32_t on) override { put("net " + std::to_string(on)); }
  void reopen_instant_action(uint8_t a, uint8_t b) override { put("ia " + std::to_string(a) + " " + std::to_string(b)); }
};

struct InstallEngine : mech3::Engine {
  std::string log, text;
  int32_t hw = 0;
  uint32_t opts = 0;
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    log += std::string("unported ") + key + ";";
    return 0;
  }
  int32_t hw_card_flag() override { return hw; }
  uint32_t install_options(uint32_t mask) override {
    log += "opts " + std::to_string(mask) + ";";
    return opts & mask;
  }
  const char* message(int32_t id) override { return (text = "m" + std::to_string(id)).c_str(); }
  uint32_t focus_window() override {
    log += "focus;";
    return 0x77;
  }
  uint32_t message_box_text(uint32_t hwnd, const char* t, const char* caption, uint32_t type) override {
    log += "box " + std::to_string(hwnd) + " " + t + " " + caption + " " + std::to_string(type) + ";";
    return 1;
  }
};
}  // namespace

extern "C" {

// kind 0 Start, 1 a campaign jump (arg its menu id), 2 training (arg 1..4), 3 the lab (arg the level), 4 the network
// start, 5 the Instant Action start; ok: the install check's answer; mech: the default mech's number.
int32_t probe_dd_start(int32_t kind, int32_t arg, int32_t ok, int32_t mech, char* buf, int32_t cap) {
  StartLog h;
  h.ok = ok, h.mech = mech;
  mech3::set_debug_start_hooks(&h);
  switch (kind) {
    case 0: mech3::debug_game_start(); break;
    case 1: mech3::debug_campaign_jump(arg); break;
    case 2: mech3::debug_training_jump(arg); break;
    case 3: mech3::debug_lab_jump(arg); break;
    case 4: mech3::debug_network_start(); break;
    case 5: mech3::debug_instant_action_start(); break;
    case 6: mech3::pm_debug_campaign_jump(arg); break;
    case 7: mech3::pm_debug_lab_jump(arg); break;
    default: break;
  }
  mech3::set_debug_start_hooks(nullptr);
  return out(h.log + "|pilot " + std::to_string(h.pilot_set) + "|training " + std::to_string(h.training) + "|slot " +
                 std::to_string(h.slot),
             buf, cap);
}

// 0x00480420 with a card or not and the InstallOptions value.
int32_t probe_dd_install(int32_t hw, uint32_t opts, char* buf, int32_t cap) {
  probe_app::Mute quiet;
  InstallEngine e;
  e.hw = hw, e.opts = opts;
  const bool r = mech3::install_check(e);
  return out(e.log + "|" + (r ? "1" : "0"), buf, cap);
}

}  // extern "C"
