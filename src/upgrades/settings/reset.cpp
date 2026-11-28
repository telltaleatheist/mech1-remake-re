#include "upgrades/settings/reset.h"

#include <cstdio>
#include <cstring>

#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_reg.h"
#include "mech3/app_hooks.h"
#include "mech3/controls_cfg.h"
#include "mech3/options.h"
#include "upgrades/settings/rumble.h"
#include "upgrades/settings/upgrades.h"

namespace upgrades::reset {

// The loader's defaults (mech3/options.cpp load_options), set through the same setters, for the entries it saves.
bool options_and_controls(mech3::Engine& e, std::string* why) {
  using namespace mech3;
  Options& o = options();
  zrdr::Node* root = options_hooks().reader_load("detail.zrd");
  auto d = [&](const char* name, int32_t def) { return detail_value(root, name, def); };
  const int32_t hw = o.hw;  // the _SW/_HW choice (select_hw's): each pair's two set, then it again
  o.hw = 0, set_texture_memory(d("TextureMemory_SW", 0));
  o.hw = 1, set_texture_memory(d("TextureMemory_HW", 0));
  for (int h = 0; h < 2; ++h) {
    int32_t f = (o.machine.flags & 1) ? 0x04 : 0;  // MMX
    o.hw = h;
    if (d("Transparency", 1)) f |= 0x02;
    if (d("Lighting", 1)) f |= 0x01;
    if (d("Perspective", 1)) f |= 0x08;
    if (d(h ? "GlobalLight_HW" : "GlobalLight_SW", h)) f |= 0x10;
    if (d("AllVideoBuffer", 0)) f |= 0x10000;
    set_gfx_flags(f);
  }
  set_sound_lod(d("SoundLOD", 0));
  o.hw = 0, set_object_lod(d("ObjectLOD_SW", 0));
  o.hw = 1, set_object_lod(d("ObjectLOD_HW", 0));
  o.hw = 0, set_effects_level(d("EffectsLevel_SW", 1));
  o.hw = 1, set_effects_level(d("EffectsLevel_HW", 0));
  o.hw = 0, set_shadow_parts(d("ShadowParts_SW", 0));
  o.hw = 1, set_shadow_parts(d("ShadowParts_HW", 0));
  o.hw = hw;
  set_shadow(d("Shadow", 1));
  set_game_options(0);
  set_camera_mode(d("CameraMode", 1));
  set_invincible(d("Invincible", 0));
  set_infinite_ammo(d("InfiniteAmmo", 0));
  set_heat(d("HeatMgmt", 1));
  set_torso_auto_return(d("TorsoAutoRtn", 0));
  set_throttle_const(d("ThrottleConst", 0));
  set_use_joystick(d("UseJoystick", 1));
  set_mw2_jump_jets(d("MW2JJets", 0));
  set_fast_menu(d("FastMenu", 0));
  o.mouse_accel[0] = o.mouse_accel[1] = 1.3f;
  o.joy_accel[0] = o.joy_accel[1] = 1.3f;
  set_game_intensity(1);
  o.mouse_mode = 2;
  set_sound_frequency(0.5f);
  set_cd_volume(0.5f);
  o.in_game_vmode = d("InGameVMode", 5);
  if (zfix::options().any_resolution) {  // FIX (B): a first start's mode 16 at the desktop's size (resolution.md)
    o.screen_w = 1920, o.screen_h = 1080;
    int32_t w = 0, h = 0;
    if (options_hooks().first_start_screen(&w, &h)) o.in_game_vmode = kCustomVMode, o.screen_w = w, o.screen_h = h;
  }
  // The controls: the controls screen's RESET JOY with a stick, RESET MOUSE without (controls_menu.cpp).
  AxisTable& t = axis_table();
  e.controls_defaults();
  e.input_map_refresh();
  const bool stick = e.joystick_present() != 0;
  if (stick) {
    t.axes[0] = {3, -0.5f, 0.5f, 0.05f};
    t.axes[1] = {6, -1.0f, 0.0f, 0.0f};
    t.axes[2] = {2, 1.0f, 0.0f, 0.0f};
    t.axes[3] = {1, -1.0f, 0.0f, 0.0f};
  } else {
    t.axes[2] = {10, 1.0f, 0.0f, 0.0f};
    t.axes[1] = {9, 1.0f, 0.0f, 0.0f};
    t.axes[0].source = 0;
    t.axes[3].source = 0;
  }
  t.ready = 1;
  char path[0x104];
  std::snprintf(path, sizeof path, "%s\\%s.zrd", "keys", e.message(0xfc0));  // keys\DefaultCtlConfig.zrd
  e.controls_write(path);
  std::strncpy(e.active_ctl_cfg(), path, 0x104);
  controls_keep_saved();
  e.set_use_joystick(stick ? 1 : 0);
  e.options_apply_all();
  if (!controls_write_settings()) {
    if (why) *why = "the settings could not be written to settings.reg";
    return false;
  }
  return true;
}

const std::vector<std::string>& config_keys() {
  static const std::vector<std::string> k = [] {
    std::vector<std::string> v = {"bugfixes", "widemenus", zfix::raw_mouse_key(false), zfix::raw_mouse_key(true),
                                  rumble::kKey, "throttle_reverse", "joystick", "joystick_axes", "windowed"};
    for (const Switch& s : switches()) v.push_back(std::string("upgrade_") + s.key);
    return v;
  }();
  return k;
}

void config_defaults() {
  if (!hosts()) apply(Config{});  // the five on
  zfix::options().wide_menus = true;
  zfix::options().raw_mouse = zfix::kRawMouseDefault;
  rumble::set(rumble::kDefault);
  zfix::options().throttle_reverse = true;
}

}  // namespace upgrades::reset
