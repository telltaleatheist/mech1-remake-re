// test_debug_keys.py / test_pm_debug_keys.py: the key switch's debug commands (mech3/app_keys, 0x0040dba0, PM's
// 0x0040ba30) against the original.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zutil/ztimer.h"
#include "mech3/app_keys.h"
#include "mech3/dev_text.h"
#include "mech3/message_panel.h"

namespace {
std::string g_log;
int32_t g_list_answer, g_seq[2][64], g_seq_at[2];
float g_bias;
void log(const char* s) { g_log += s, g_log += ';'; }
void logi(const char* s, int32_t v) {
  char b[64];
  std::snprintf(b, sizeof b, "%s %d;", s, v);
  g_log += b;
}
uint32_t fbits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}
}  // namespace

extern "C" {

// cmds: the commands; list: 0x432c60's answer; cursor/mip: ShowCursor's and 0x557f70's answers in turn; bias: what
// 0x535ef0 gives; zone: zModel's zone mode first. Out: per command its calls, then the state.
int probe_debug_keys(const int32_t* cmds, int n, int32_t list, const int32_t* cursor, const int32_t* mip, float bias,
                     int32_t zone, char* out, int cap) {
  mech3::AppKeyCalls& k = mech3::app_key_calls();
  const mech3::AppKeyCalls saved = k;
  k = mech3::AppKeyCalls{};
  g_list_answer = list, g_bias = bias, g_seq_at[0] = g_seq_at[1] = 0;
  std::memcpy(g_seq[0], cursor, sizeof g_seq[0]);
  std::memcpy(g_seq[1], mip, sizeof g_seq[1]);
  k.cycle_camera = [] { log("cycle"); };
  k.vmode_cycle = [] { log("vmode"); };
  k.message_toggle = [] { log("msgtoggle"); };
  k.mech_debug_bit = [] { log("bit"); };
  k.mech_list_dialog = [] { log("list"); return g_list_answer; };
  k.mech_remake = [](int32_t r) { logi("remake", r); };
  k.next_view_mech = [] { log("next"); };
  k.vehicle_dialog = [] { log("vehdlg"); };
  k.camera_mode = [](int32_t id) { logi("cam", id); };
  k.camera_dialog = [] { log("camdlg"); };
  k.show_cursor = [](int32_t s) { logi("cursor", s); return g_seq[0][g_seq_at[0]++ & 63]; };
  k.marker = [] { log("marker"); };
  k.teleport = [] { log("teleport"); };
  k.soft_mip_toggle = [] { log("mip"); return g_seq[1][g_seq_at[1]++ & 63]; };
  k.edit_vehicles = [] { log("editveh"); };
  k.soft_mip_bias = [] { return g_bias; };
  k.set_soft_mip_bias = [](float b) { logi("bias", static_cast<int32_t>(fbits(b))); };
  k.interp_console = [] { log("console"); };
  k.twod_through_3d = [](int32_t on) { logi("twod", on); };  // PM's 0xaf
  mech3::app_key_state() = mech3::AppKeyState{};
  zmod::zones().mode = zone;
  ztimer::State& ts = ztimer::state();
  const ztimer::State ts0 = ts;
  ts.max_dt = 0.125f, ts.clamp_enabled = 1;
  mech3::fps_text().hide();
  mech3::dev_lines().set_active(0);
  mech3::message_panel_create();
  mech3::MessagePanel& panel = *mech3::message_panel();
  g_log.clear();
  for (int i = 0; i < n; ++i) {
    mech3::message_panel_clear();
    mech3::app_key(cmds[i]);
    for (size_t j = 0; j < panel.lines.size(); ++j)
      if (panel.lines[j].text[0]) g_log += std::string("msg:") + panel.lines[j].text + ";";
    const mech3::AppKeyState& st = mech3::app_key_state();
    char b[200];
    std::snprintf(b, sizeof b, "| %d %d %d %d %08x %d %08x %d %d %d\n", st.dev_text, st.readout, st.clamp, st.cursor,
                  fbits(st.max_rate), zmod::zones().mode, fbits(ts.max_dt), ts.clamp_enabled,
                  (mech3::fps_text().flags & 0x10) ? 1 : 0, mech3::dev_lines().active);
    g_log += b;
  }
  mech3::dev_readout(0);
  mech3::message_panel_destroy();
  ts = ts0;
  zmod::zones().mode = 1;
  k = saved;
  if (static_cast<int>(g_log.size()) + 1 > cap) return -1;
  std::memcpy(out, g_log.c_str(), g_log.size() + 1);
  return static_cast<int>(g_log.size());
}

}  // extern "C"

#include <vector>

#include "mech3/debug_keys.h"

namespace {
uint8_t g_kinds[64];
}  // namespace

extern "C" {

// zEntity's list: kinds 0 a null entry, 1 not a vehicle, 2 a vehicle not a mech, 3 a mech; `target` its index, -1 none,
// -2 one not in the list. Returns the index picked, -1 none, -2 the outside one.
int probe_next_view_mech(const uint8_t* kinds, int n, int target) {
  static uint8_t outside;
  std::vector<void*> list;
  std::memcpy(g_kinds, kinds, static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) list.push_back(kinds[i] ? &g_kinds[i] : nullptr);
  void* t = target >= 0 ? list[static_cast<size_t>(target)] : target == -2 ? &outside : nullptr;
  void* m = mech3::next_view_mech(list, t, [](void* e) -> void* { return *static_cast<uint8_t*>(e) == 3 ? e : nullptr; });
  if (!m) return -1;
  if (m == &outside) return -2;
  return static_cast<int>(static_cast<uint8_t*>(m) - g_kinds);
}

}  // extern "C"

#include "mech3/veh_stealth.h"

namespace {
const float* g_pos;
struct ReadoutHooks : mech3::VehBasicHooks {
  const char* node_name(void* node) override { return static_cast<const char*>(node); }
  const float* entity_position(mech3::Vehicle&) override { return g_pos; }
};
}  // namespace

extern "C" {

// The stealth overlay's readout (0x004a56b0) on a stealth named `name` with or without controls (its motion mode and
// yaw) at `pos`, the overlay `active` or not. Out: the developer lines 0..2, each its hidden flag and text.
int probe_stealth_readout(int32_t mode, int has_controls, float yaw, const float* pos, const char* name, int active,
                          char* out, int cap) {
  ReadoutHooks hooks;
  mech3::VehBasicHooks& keep = mech3::veh_basic_hooks();
  mech3::set_veh_basic_hooks(&hooks);
  g_pos = pos;
  mech3::DevLines& lines = mech3::dev_lines();
  for (mech3::DevText& r : lines.rows) r.set_text(""), r.hide();
  {
    mech3::Stealth s;
    mech3::StealthControls c;
    c.motion.mode = mode;
    c.angles.y = yaw;
    s.controls = has_controls ? &c : nullptr;
    s.node = const_cast<char*>(name);
    mech3::StealthLayer layer(&s);
    layer.active = active;
    layer.update(0.0f);
    s.controls = nullptr;
  }
  std::string o;
  for (int i = 0; i < 3; ++i) {
    o += (lines.rows[i].flags & 0x10) ? '1' : '0';
    o += lines.rows[i].text;
    o += '\n';
  }
  mech3::set_veh_basic_hooks(&keep);
  if (static_cast<int>(o.size()) + 1 > cap) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}

}  // extern "C"
