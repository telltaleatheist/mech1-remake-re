#include "mw1_settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "gamez/zutil/zfix.h"
#include "mech3/app_hooks.h"
#include "mech3/controls_cfg.h"
#include "mech3/options.h"
#include "mech3/options_menu.h"
#include "mw1_chrome.h"
#include "mw1_music.h"
#include "upgrades/settings/upgrades.h"

namespace mw1::settings {
namespace {

// PM's ObjectLOD and EffectsLevel run 0 (most) .. 2; shown LOW .. HIGH (options_menu.cpp's level_of).
int32_t level_of(int32_t v) { return v == 0 ? 2 : v == 1 ? 1 : 0; }

// LIGHTING's values in its list's order (options_menu.cpp's light_maps_list), PM's names.
constexpr int32_t kLighting[5] = {-1, 0, 3, 1, 2};
constexpr int32_t kLightingDefault = 2;  // LIGHT MAPS (3): the card's choice, PM's default

// MOUSE SENSITIVITY: PM's slider's range (kSensitivityLow..High) in tenths; 1.3 the original's.
constexpr int32_t kSensSteps = static_cast<int32_t>((mech3::kSensitivityHigh - mech3::kSensitivityLow) * 10.0f + 0.5f) + 1;
constexpr int32_t kSensDefault = static_cast<int32_t>((1.3f - mech3::kSensitivityLow) * 10.0f + 0.5f);
float sens_of(int32_t i) {  // the nearest tenth, as the slider's
  return std::round(mech3::kSensitivityLow * 10.0f + static_cast<float>(std::clamp(i, 0, kSensSteps - 1))) / 10.0f;
}
int32_t index_of_sens(float s) {
  if (s == 0.0f) s = 1.3f;  // an old registry's zero, as PM's slider takes it
  return std::clamp(static_cast<int32_t>(std::lround((std::fabs(s) - mech3::kSensitivityLow) * 10.0f)), 0, kSensSteps - 1);
}

// RESOLUTION's list: the device's modes (options_menu.cpp's ResolutionCycle), each "W x H".
struct Mode {
  int32_t mode;
  std::string name;
};
std::vector<Mode> modes(mech3::Engine& e) {
  int32_t lo = e.hw_card_dev(), hi = 0;
  e.video_mode_range(e.hw_card_dev(), &lo, &hi);
  std::vector<Mode> v;
  auto add = [&](int32_t mode) {
    if (mode < lo || mode > hi) return;
    int32_t w = 0, h = 0;
    if (mode == 3) w = 320, h = 240;  // drawn 320x240, doubled (PM's message 0x41 names it so)
    else if (!mech3::vmode_size(mode, &w, &h)) return;
    char s[32];
    std::snprintf(s, sizeof s, mode == mech3::kCustomVMode ? "%d x %d CUSTOM" : "%d x %d", w, h);
    v.push_back({mode, s});
  };
  for (int32_t m : {3, 5, 6, 7, 8, 9, 10}) add(m);
  for (int32_t m = 11; zfix::options().any_resolution && m <= mech3::kLastVMode; ++m) add(m);
  return v;
}

bool lighting_there(mech3::Engine& e) { return e.hw_card_flag() && e.install_options(0xff); }

void save_switch(const char* key, bool on) {
  if (auto& save = mech3::options_upgrades_saver()) save(key, on);
}

// A row: what it shows and how it reads and writes its setting.
struct Item {
  Row row;
  std::function<int32_t(mech3::Engine&)> get;
  std::function<void(mech3::Engine&, int32_t)> put;  // applied and saved
  int32_t def = -1;                                  // DEFAULTS' value (-1: kept)
};

Item heading(const char* label, bool alert = false) {
  Item it;
  it.row.label = label, it.row.kind = Kind::kHeading, it.row.alert_heading = alert;
  return it;
}
Item choice(const char* label, std::vector<std::string> names, const char* hint, int32_t def,
            std::function<int32_t(mech3::Engine&)> get, std::function<void(mech3::Engine&, int32_t)> put, int32_t alert = -1) {
  Item it;
  it.row.label = label, it.row.kind = Kind::kChoice, it.row.names = std::move(names), it.row.hint = hint, it.row.alert = alert;
  it.get = std::move(get), it.put = std::move(put), it.def = def;
  return it;
}
Item on_off(const char* label, const char* hint, bool def, std::function<int32_t(mech3::Engine&)> get,
            std::function<void(mech3::Engine&, int32_t)> put, bool cheat = false) {
  return choice(label, {"OFF", "ON"}, hint, def ? 1 : 0, std::move(get), std::move(put), cheat ? 1 : -1);
}
// PM's settings: applied, then written (zreg, as its ACCEPT).
auto pm(std::function<void(mech3::Engine&, int32_t)> set) {
  return [set = std::move(set)](mech3::Engine& e, int32_t v) {
    set(e, v);
    e.settings_write();
  };
}
// MW1's sound settings (mw1.cfg, through mw1_start's read_cfg and write_cfg).
auto sound(std::function<void(music::Settings&, int32_t)> set) {
  return [set = std::move(set)](mech3::Engine&, int32_t v) {
    music::Settings m = music::settings();
    set(m, v);
    music::set_settings(m);
  };
}
// The mouse: PM's two accelerations (X the sensitivity, Y's sign the invert), kept for the settings and written.
void put_mouse(mech3::Engine& e, float sens, bool invert) {
  e.set_mouse_accel_x(sens);
  e.set_mouse_accel_y(invert ? -sens : sens);
  mech3::settings_keep_mouse();
  e.settings_write();
}

std::vector<Item> items(mech3::Engine& e, Tab t) {
  std::vector<Item> v;
  switch (t) {
    case Tab::kGame:
      v.push_back(choice("DIFFICULTY", {"EASY", "NORMAL", "HARD"}, "DIFFICULTY: how hard the enemy fights.", 1,
                         [](mech3::Engine& e) { return std::clamp(e.game_intensity(), 0, 2); },
                         pm([](mech3::Engine& e, int32_t v) { e.set_game_intensity(v); })));
      v.push_back(choice("CAMERA", {"COCKPIT", "CHASE HIGH", "CHASE LOW"}, "CAMERA: the view a battle starts in.", 0,
                         [](mech3::Engine& e) { const int32_t c = e.camera_mode(); return c == 1 ? 0 : c == 0 ? 1 : 2; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_camera_mode(v == 0 ? 1 : v == 1 ? 0 : 2); })));
      v.push_back(on_off("TORSO RE-CENTRES", "TORSO RE-CENTRES: the torso turns back to the legs by itself.", false,
                         [](mech3::Engine& e) { return e.torso_auto_return() != 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_torso_auto_return(v); })));
      v.push_back(on_off("THROTTLE STAYS SET", "THROTTLE STAYS SET: speed holds when you let go of the key.", false,
                         [](mech3::Engine& e) { return e.throttle_const() != 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_throttle_const(v); })));
      v.push_back(on_off("JUMP ASSIST", "JUMP ASSIST: OFF flies the jump jets as MechWarrior 2 did.", true,
                         [](mech3::Engine& e) { return e.mw2_jump_jets() != 0 ? 0 : 1; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_mw2_jump_jets(v ? 0 : 1); })));
      v.push_back(heading("CHEATS", true));
      v.push_back(on_off("INVINCIBLE", "INVINCIBLE: a cheat. Your mech takes no damage.", false,
                         [](mech3::Engine& e) { return e.invincible() != 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_invincible(v); }), true));
      v.push_back(on_off("UNLIMITED AMMO", "UNLIMITED AMMO: a cheat. Ammunition never runs out.", false,
                         [](mech3::Engine& e) { return e.infinite_ammo() != 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_infinite_ammo(v); }), true));
      v.push_back(choice("HEAT", {"REAL", "NONE"}, "HEAT: NONE is a cheat. Weapons and jets make no heat.", 0,
                         [](mech3::Engine& e) { return e.heat() != 0 ? 0 : 1; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_heat(v == 0 ? 1 : 0); }), 1));
      break;
    case Tab::kDisplay: {
      std::vector<std::string> names;
      for (const Mode& m : modes(e)) names.push_back(m.name);
      Item res;
      res.row.label = "RESOLUTION", res.row.kind = Kind::kCycle, res.row.names = names;
      res.row.hint = "RESOLUTION: the size battles are drawn at.";
      res.get = [](mech3::Engine& e) {
        const std::vector<Mode> ms = modes(e);
        for (size_t i = 0; i < ms.size(); ++i)
          if (ms[i].mode == e.in_game_vmode()) return static_cast<int32_t>(i);
        return 0;
      };
      res.put = pm([](mech3::Engine& e, int32_t v) {
        const std::vector<Mode> ms = modes(e);
        if (v >= 0 && static_cast<size_t>(v) < ms.size()) e.set_in_game_vmode(ms[static_cast<size_t>(v)].mode);
      });
      res.row.available = !names.empty(), res.row.unavailable = "SET BY THE WINDOW";
      v.push_back(std::move(res));
      v.push_back(heading("QUALITY"));
      v.push_back(choice("DETAIL", {"LOW", "MEDIUM", "HIGH"}, "DETAIL: how much shape models keep at a distance.", 2,
                         [](mech3::Engine& e) { return level_of(std::clamp(e.object_lod(), 0, 2)); },
                         pm([](mech3::Engine& e, int32_t v) { e.set_object_lod(level_of(v)); })));
      v.push_back(choice("EFFECTS", {"LOW", "MEDIUM", "HIGH"}, "EFFECTS: smoke, fire and explosions in full or fewer.", 2,
                         [](mech3::Engine& e) { return level_of(std::clamp(e.effects_level(), 0, 2)); },
                         pm([](mech3::Engine& e, int32_t v) { e.set_effects_level(level_of(v)); })));
      Item light;
      light.row.label = "LIGHTING", light.row.kind = Kind::kCycle;
      light.row.names = {"OFF", "ON", "LIGHT MAPS", "LIGHT MAPS A", "LIGHT MAPS B"};
      light.row.hint = "LIGHTING: LIGHT MAPS is the full look; A and B are lighter.";
      light.get = [](mech3::Engine& e) {
        const int32_t c = mech3::light_maps_choice(static_cast<uint32_t>(e.gfx_flags()));
        for (int32_t i = 0; i < 5; ++i)
          if (kLighting[i] == c) return i;
        return kLightingDefault;
      };
      light.put = pm([](mech3::Engine& e, int32_t v) {
        e.set_gfx_flags(static_cast<int32_t>(mech3::light_maps_flags(kLighting[std::clamp(v, 0, 4)], static_cast<uint32_t>(e.gfx_flags()))));
      });
      light.def = kLightingDefault;
      light.row.available = lighting_there(e), light.row.unavailable = "NEEDS A 3D CARD";
      v.push_back(std::move(light));
      Item shadows = on_off("SHADOWS", "SHADOWS: mechs and buildings cast shadows on the ground.", true,
                            [](mech3::Engine& e) { return e.shadow() != 0 ? 1 : 0; },
                            pm([](mech3::Engine& e, int32_t v) { e.set_shadow(v); }));
      shadows.row.available = lighting_there(e) && e.video_has_cap(3), shadows.row.unavailable = "NOT ON THIS CARD";
      v.push_back(std::move(shadows));
      Item self = on_off("SELF SHADOWS", "SELF SHADOWS: a mech's own parts shade each other.", false,
                         [](mech3::Engine& e) { return e.shadow_parts() != 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_shadow_parts(v); }));
      self.row.available = lighting_there(e), self.row.unavailable = "NEEDS A 3D CARD";
      v.push_back(std::move(self));
      v.push_back(heading("MENUS"));
      // Saved and taken at the next start: the menus' shape changed now would rebuild the stack under MW1's screens.
      v.push_back(on_off("WIDE MENUS", "WIDE MENUS: 16:9 menus on a wide screen, from the next start.", true,
                         [](mech3::Engine&) { return zfix::options().wide_menus ? 1 : 0; },
                         [](mech3::Engine&, int32_t v) {
                           zfix::options().wide_menus = v != 0;
                           save_switch("widemenus", v != 0);
                         }));
      break;
    }
    case Tab::kSound: {
      v.push_back(on_off("MUSIC", "MUSIC: the menu theme and the combat music.", true,
                         [](mech3::Engine&) { return music::settings().on ? 1 : 0; },
                         sound([](music::Settings& m, int32_t v) { m.on = v != 0; })));
      Item mv;
      mv.row.label = "MUSIC VOLUME", mv.row.kind = Kind::kVolume, mv.row.hint = "MUSIC VOLUME: click a bar; the top lit one again goes lower.";
      mv.get = [](mech3::Engine&) { return music::settings().music; };
      mv.put = sound([](music::Settings& m, int32_t v) { m.music = v; });
      mv.def = 50;
      v.push_back(std::move(mv));
      Item fx;
      fx.row.label = "EFFECTS VOLUME", fx.row.kind = Kind::kVolume, fx.row.hint = "EFFECTS VOLUME: weapons, footsteps and the rest of a battle.";
      fx.get = [](mech3::Engine&) { return music::settings().effects; };
      fx.put = sound([](music::Settings& m, int32_t v) { m.effects = v; });
      fx.def = 50;
      v.push_back(std::move(fx));
      v.push_back(choice("SOUND QUALITY", {"LOW", "HIGH"}, "SOUND QUALITY: LOW plays fewer sounds at once.", 1,
                         [](mech3::Engine& e) { return e.sound_lod() == 0 ? 1 : 0; },
                         pm([](mech3::Engine& e, int32_t v) { e.set_sound_lod(v ? 0 : 2); })));
      break;
    }
    case Tab::kControls: {
      Item sens;
      sens.row.label = "MOUSE SENSITIVITY", sens.row.kind = Kind::kCycle;
      for (int32_t i = 0; i < kSensSteps; ++i) {
        char s[8];
        std::snprintf(s, sizeof s, "%.1f", static_cast<double>(sens_of(i)));
        sens.row.names.push_back(s);
      }
      sens.row.hint = "MOUSE SENSITIVITY: how far the torso turns for the mouse.";
      sens.get = [](mech3::Engine& e) { return index_of_sens(e.mouse_accel_x()); };
      sens.put = [](mech3::Engine& e, int32_t v) { put_mouse(e, sens_of(v), e.mouse_accel_y() < 0.0f); };
      sens.def = kSensDefault;
      v.push_back(std::move(sens));
      // mw3.cfg's rawmouse=, at once (a battle's held mouse too), as MW3's game options (its docs/input.md section 14).
      v.push_back(on_off("RAW MOUSE", "RAW MOUSE: the mouse's own movement. OFF: your system's speed.", zfix::kRawMouseDefault,
                         [](mech3::Engine&) { return zfix::options().raw_mouse ? 1 : 0; },
                         [](mech3::Engine&, int32_t v) {
                           zfix::options().raw_mouse = v != 0;
                           save_switch("rawmouse", v != 0);
                         }));
      v.push_back(on_off("INVERT MOUSE", "INVERT MOUSE: the mouse forward looks down.", false,
                         [](mech3::Engine& e) { return e.mouse_accel_y() < 0.0f ? 1 : 0; },
                         [](mech3::Engine& e, int32_t v) { put_mouse(e, sens_of(index_of_sens(e.mouse_accel_x())), v != 0); }));
      Item keys;
      keys.row.label = "KEY BINDINGS", keys.row.kind = Kind::kAction, keys.row.hint = "KEY BINDINGS: the keys, joystick and mouse buttons.";
      v.push_back(std::move(keys));
      break;
    }
    case Tab::kEnhancements: {
      // Ours (MW3 Rebuild's UPGRADES, mw3.cfg's upgrade_ keys), MFB REGROUP left out: MW1 fields no MFB.
      struct Up {
        const char* key;
        const char* label;
        const char* hint;
      };
      static const Up kUps[] = {
          {"improved_ai", "IMPROVED AI", "IMPROVED AI: lancemates find their way round walls and rivers."},
          {"artemis", "ARTEMIS TRACKING", "ARTEMIS TRACKING: Artemis FCS missiles track as they should."},
          {"water", "WATER MOVEMENT", "WATER MOVEMENT: deep water slows a mech smoothly."},
          {"small_fixes", "SMALL FIXES", "SMALL FIXES: the fixes listed below. OFF plays as it shipped."},
      };
      for (const Up& u : kUps) {
        bool zfix::Options::*field = nullptr;
        for (const upgrades::Switch& s : upgrades::switches())
          if (std::string(s.key) == u.key) field = s.field;
        if (!field) continue;
        Item it = on_off(u.label, u.hint, true, [field](mech3::Engine&) { return zfix::options().*field ? 1 : 0; },
                         [field, key = std::string("upgrade_") + u.key](mech3::Engine&, int32_t v) {
                           if (upgrades::hosts()) return;  // a joiner plays on the host's (never MW1's: no network)
                           zfix::options().*field = v != 0;
                           save_switch(key.c_str(), v != 0);
                         });
        it.row.available = !upgrades::hosts(), it.row.unavailable = "THE HOST'S";
        v.push_back(std::move(it));
      }
      v.push_back(heading("SMALL FIXES"));
      break;
    }
  }
  return v;
}

const char* const kTabNames[kTabCount] = {"GAME", "DISPLAY", "SOUND", "CONTROLS", "ENHANCEMENTS"};
const char* const kTabHints[kTabCount] = {
    "GAME: difficulty, camera, handling and the cheats.",
    "DISPLAY: resolution, detail, lighting and shadows.",
    "SOUND: music on or off, the volumes, sound quality.",
    "CONTROLS: the mouse and the key bindings.",
    "ENHANCEMENTS: MW1 Critical Hit's own improvements.",
};
// A row's text places in the layout: L (label), H (heading), U (unavailable), V (a cycle's value), N (its count),
// P (a volume's percent), then the row's number. Kept for the program's life (TextBlock::at holds the pointer).
const char* place_name(char kind, int32_t r) {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> v;
    for (char k : std::string("LHUVNP"))
      for (int32_t i = 0; i < kRowSlots; ++i) v.push_back(k + std::to_string(i));
    return v;
  }();
  const size_t k = std::string("LHUVNP").find(kind);
  return names[k * kRowSlots + static_cast<size_t>(r)].c_str();
}
constexpr const char* kIdleHint = "Changes apply and are saved at once.";
constexpr const char* kDefaultsHint = "DEFAULTS: this section as a new player's.";
constexpr const char* kDoneHint = "DONE: back to where you were.";

}  // namespace

const char* tab_name(Tab t) {
  const int32_t i = static_cast<int32_t>(t);
  return i >= 0 && i < kTabCount ? kTabNames[i] : "";
}

std::vector<Row> rows(mech3::Engine& e, Tab t) {
  std::vector<Row> out;
  for (Item& it : items(e, t)) {
    if (it.get) it.row.index = it.get(e);
    if (it.row.kind == Kind::kCycle && !it.row.names.empty())
      it.row.index = std::clamp(it.row.index, 0, static_cast<int32_t>(it.row.names.size()) - 1);
    out.push_back(std::move(it.row));
  }
  return out;
}

void set(mech3::Engine& e, Tab t, size_t row, int32_t value) {
  std::vector<Item> v = items(e, t);
  if (row >= v.size() || !v[row].put || !v[row].row.available) return;
  Item& it = v[row];
  switch (it.row.kind) {
    case Kind::kChoice:
    case Kind::kCycle:
      if (it.row.names.empty()) return;
      value = std::clamp(value, 0, static_cast<int32_t>(it.row.names.size()) - 1);
      break;
    case Kind::kVolume: value = std::clamp(value, 0, 100); break;
    default: return;
  }
  it.put(e, value);
}

void defaults(mech3::Engine& e, Tab t) {
  std::vector<Item> v = items(e, t);
  for (Item& it : v)
    if (it.put && it.def >= 0 && it.row.available) it.put(e, it.def);
}

int32_t default_of(mech3::Engine& e, Tab t, size_t row) {
  const std::vector<Item> v = items(e, t);
  return row < v.size() && v[row].put ? v[row].def : -1;
}

int32_t largest_textures(mech3::Engine& e) {
  // PM's TextureCycle (options_menu.cpp, Mech3.exe 1.2 @ 0x0043a310): the last it offers for the device.
  const int32_t dev = e.hw_card_dev();
  if (dev == -1) return e.install_options(0x400) ? 0 : e.install_options(0x200) ? 5 : 6;
  uint32_t total = 0, mb = 2;
  if (e.video_card_memory(dev, &total)) mb = total >> 20;
  int32_t sel = static_cast<int32_t>(mb);
  if (e.install_options(1) && mb >= 2) sel = 6;
  if (e.install_options(2) && mb >= 4) sel = 5;
  if (e.install_options(4) && mb >= 8) sel = 3;
  return sel;
}

void at_start(mech3::Engine& e) {
  const int32_t t = largest_textures(e);
  if (e.texture_memory() == t) return;
  e.set_texture_memory(t);
  e.settings_write();
}

std::vector<ScreenButton> buttons() {
  // The names the generator (tools/mw1settings_zrd.py) gives them, in its order.
  static std::vector<std::string> names;
  if (names.empty()) {
    for (int32_t i = 0; i < kTabCount; ++i) names.push_back(std::string("TAB_") + kTabNames[i]);
    names.push_back("DEFAULTS"), names.push_back("DONE"), names.push_back("KEYS");
    for (int32_t r = 0; r < kRowSlots; ++r) {
      const std::string n = std::to_string(r);
      for (int32_t k = 0; k < kSegs; ++k) names.push_back("SEG" + n + "_" + std::to_string(k));
      names.push_back("LEFT" + n), names.push_back("RIGHT" + n);
      for (int32_t k = 0; k < kCells; ++k) names.push_back("CELL" + n + "_" + std::to_string(k));
    }
  }
  std::vector<ScreenButton> v;
  for (size_t i = 0; i < names.size(); ++i) {
    const int32_t id = static_cast<int32_t>(i) + 1;
    ScreenButton b{names[i].c_str(), ""};
    if (id >= kTab0 && id < kTab0 + kTabCount) b.words = kTabNames[id - kTab0];
    if (id == kDefaults) b.words = "DEFAULTS";
    if (id == kDone) b.words = "DONE";
    if (id == kKeys) b.words = "CHANGE";
    if (id >= kRow0) {
      const int32_t k = (id - kRow0) % kPerRow;
      b.repeat = k == kSegs || k == kSegs + 1;  // the arrows step on while held
    }
    v.push_back(b);
  }
  return v;
}

SettingsState::SettingsState(zgame::Handle h, std::function<void()> keys)
    : Mw1State(h, "MW1 Settings State", kSettingsFile, kSettingsDialog, buttons(), kDone, [this](int32_t id) { press(id); }),
      keys_(std::move(keys)) {}

bool SettingsState::enter() {
  // As PM's Game Options state (0x00438c90): the menus' screen mode first (from the pause, the battle's is up).
  mech3::Engine& en = e();
  en.set_screen_mode(en.menu_vmode());
  const bool r = Mw1State::enter();
  set_hover([this](int32_t id) { hovered(id); });
  set_greyed([this](int32_t id) { hovered(id); });
  status_.clear();
  refresh();
  return r;
}

void SettingsState::show_tab(Tab t) {
  tab_ = t;
  status_.clear();
  refresh();
}

void SettingsState::hovered(int32_t id) {
  std::string s;
  if (id >= kTab0 && id < kTab0 + kTabCount) s = kTabHints[id - kTab0];
  else if (id == kDefaults) s = kDefaultsHint;
  else if (id == kDone) s = kDoneHint;
  else if (id >= kKeys) {
    const std::vector<Row> rs = rows(e(), tab_);
    for (size_t r = 0; r < rs.size(); ++r)
      if (id == kKeys ? rs[r].kind == Kind::kAction : r == static_cast<size_t>((id - kRow0) / kPerRow)) s = rs[r].hint;
  }
  if (s == status_) return;
  status_ = s;
  refresh();
}

void SettingsState::press(int32_t id) {
  mech3::Engine& en = e();
  if (id >= kTab0 && id < kTab0 + kTabCount) return show_tab(static_cast<Tab>(id - kTab0));
  if (id == kDone) return escape_done();
  if (id == kDefaults) {
    defaults(en, tab_);
    std::printf("mw1: settings: %s defaults\n", tab_name(tab_));
    return refresh();
  }
  if (id == kKeys) {
    if (keys_) keys_();
    return;
  }
  if (id < kRow0 || id > kSettingsButtonCount) return;
  const size_t r = static_cast<size_t>((id - kRow0) / kPerRow);
  const int32_t k = (id - kRow0) % kPerRow;
  const std::vector<Row> rs = rows(en, tab_);
  if (r >= rs.size()) return;
  const Row& row = rs[r];
  int32_t value = row.index;
  if (k < kSegs) value = k;
  else if (k == kSegs) value = row.index - 1;
  else if (k == kSegs + 1) value = row.index + 1;
  else {
    // A volume's cell: up to it; the top lit cell again, one lower (so 0 can be had).
    const int32_t to = (k - kSegs - 2 + 1) * 10;
    value = row.index == to ? to - 10 : to;
  }
  set(en, tab_, r, value);
  std::printf("mw1: settings: %s %s %d\n", tab_name(tab_), row.label.c_str(), static_cast<int>(value));
  refresh();
}

void SettingsState::escape_done() {
  std::printf("mw1: settings: done\n");
  e().pop_state(7);  // as PM's Game Options (0x004399e0): the pop with 7
}

void SettingsState::refresh() {
  mech3::Engine& en = e();
  const std::vector<Row> rs = rows(en, tab_);
  ScreenView s;
  s.buttons.resize(static_cast<size_t>(kSettingsButtonCount));
  auto b = [&](int32_t id) -> ButtonState& { return s.buttons[static_cast<size_t>(id - 1)]; };
  for (int32_t i = 0; i < kTabCount; ++i) b(kTab0 + i).on = static_cast<int32_t>(tab_) != i;  // the chosen one lit (its DISABLE look)
  b(kKeys).there = false;
  for (int32_t r = 0; r < kRowSlots; ++r) {
    for (int32_t k = 0; k < kSegs; ++k) b(seg_id(r, k)).there = false;
    b(left_id(r)).there = b(right_id(r)).there = false;
    for (int32_t k = 0; k < kCells; ++k) b(cell_id(r, k)).there = false;
  }
  s.blocks.push_back(at_place("TITLE", {tab_name(tab_)}));
  s.blocks.push_back(at_place("STATUS_WORLD", {"SETTINGS"}));
  s.blocks.push_back(at_place("STATUS", {status_.empty() ? kIdleHint : status_}));
  for (size_t i = 0; i < rs.size() && i < static_cast<size_t>(kRowSlots); ++i) {
    const Row& row = rs[i];
    const int32_t r = static_cast<int32_t>(i);
    auto place = [&](char p) { return place_name(p, r); };
    if (row.kind == Kind::kHeading) {
      s.blocks.push_back(at_place(place('H'), {row.alert_heading ? highlighted(row.label) : row.label}));
      continue;
    }
    s.blocks.push_back(at_place(place('L'), {row.label}));
    if (!row.available) {
      s.blocks.push_back(at_place(place('U'), {row.unavailable}));
      continue;
    }
    switch (row.kind) {
      case Kind::kChoice:
        for (int32_t k = 0; k < kSegs && k < static_cast<int32_t>(row.names.size()); ++k) {
          ButtonState& sb = b(seg_id(r, k));
          sb.there = true, sb.words = row.names[static_cast<size_t>(k)];
          sb.on = k != row.index;  // the chosen one lit (its DISABLE look), the others to click
          sb.look = k == row.index && k == row.alert ? kSegAlertLook : kSegLook;
        }
        break;
      case Kind::kCycle: {
        const int32_t count = static_cast<int32_t>(row.names.size());
        b(left_id(r)).there = b(right_id(r)).there = true;
        b(left_id(r)).on = row.index > 0, b(right_id(r)).on = row.index < count - 1;
        s.blocks.push_back(at_place(place('V'), {row.names.empty() ? "" : row.names[static_cast<size_t>(row.index)]}));
        s.blocks.push_back(at_place(place('N'), {std::to_string(row.index + 1) + " of " + std::to_string(count)}));
        break;
      }
      case Kind::kVolume:
        for (int32_t k = 0; k < kCells; ++k) {
          ButtonState& sb = b(cell_id(r, k));
          sb.there = true, sb.look = (k + 1) * 10 <= row.index ? kVolOnLook : kVolLook;
        }
        s.blocks.push_back(at_place(place('P'), {std::to_string(row.index) + "%"}));
        break;
      case Kind::kAction: b(kKeys).there = true; break;
      default: break;
    }
  }
  if (tab_ == Tab::kEnhancements) {  // the small fixes' list under the switches (upgrades::small_fixes_list)
    std::vector<std::string> lines;
    for (const char* l : upgrades::small_fixes_list()) lines.push_back(l);
    s.blocks.push_back(at_place("LIST", lines));
  }
  if (tab_ == Tab::kDisplay) s.blocks.push_back(at_place("NOTE", {"Texture memory is set to your card's largest."}));
  Mw1State::show(s);
}

}  // namespace mw1::settings
