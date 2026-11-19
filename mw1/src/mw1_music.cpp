#include "mw1_music.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "gamez/zsound/zsnd_cd.h"
#include "mech3/music.h"
#include "mech3/options.h"
#include "mw1_start.h"
#include "platform/quiet_music.h"

namespace mw1::music {
namespace {
std::string g_dir;  // the write dir (mw1.cfg's)
SoundCfg g_cfg;     // mw1.cfg's sound lines (-1 unset)
float g_level = 0;
int32_t g_starts = 0, g_volume = 0;
bool g_faded_out = true;  // in a mission, the theme gone and PM's volume given back

bool cd_open() { return (zsnd_cd::state().flags & 2u) != 0; }
bool theme_playing() {
  const zsnd_cd::State& s = zsnd_cd::state();
  return cd_open() && s.track == zsnd_cd::kExtraTrack && s.mode != zsnd_cd::Stopped && s.mode != zsnd_cd::Paused;
}
int32_t full_volume() { return static_cast<int32_t>(std::clamp(mech3::cd_volume(), 0.0f, 1.0f) * 65535.0); }
void volume(int32_t v) {
  g_volume = std::clamp(v, 0, 0xFFFF);
  zsnd_cd::set_volume(static_cast<uint16_t>(g_volume), static_cast<uint16_t>(g_volume));
}
int32_t percent(float v) { return static_cast<int32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 100.0f)); }
}  // namespace

void install(const std::string& write_dir) {
  g_dir = write_dir;
  g_level = 0, g_starts = 0, g_volume = 0, g_faded_out = true;
  g_cfg = read_cfg(write_dir).sound;
  platform::set_quiet_music(true);
  zsnd_cd::set_extra_track(kThemeFile);
}

// Running, PM's own two are what applies (GRAPHICS & SOUND's sliders may have moved them); off keeps the volume to
// come back to.
Settings settings() {
  Settings s;
  s.on = g_cfg.music != 0;
  s.music = s.on ? percent(mech3::cd_volume()) : (g_cfg.music_volume >= 0 ? g_cfg.music_volume : 50);
  s.effects = percent(mech3::sound_frequency());
  return s;
}

void apply() {
  // mw1.cfg's over PM's (each line it has). PM's music volume below 0x800 counts as off (mech3_music): off is 0.
  if (g_cfg.music == 0) mech3::set_cd_volume(0.0f);
  else if (g_cfg.music_volume >= 0) mech3::set_cd_volume(std::max(g_cfg.music_volume, 1) / 100.0f);
  if (g_cfg.effects_volume >= 0) mech3::set_sound_frequency(g_cfg.effects_volume / 100.0f);
  if (theme_playing()) volume(static_cast<int32_t>(g_level * static_cast<float>(full_volume())));
}

void sync() {
  const Settings s = settings();
  const SoundCfg now{s.on ? 1 : 0, s.music, s.effects};
  if (now == g_cfg) return;
  set_settings(s);
}

void set_settings(const Settings& s) {
  g_cfg.music = s.on ? 1 : 0;
  g_cfg.music_volume = std::clamp(s.music, 0, 100);
  g_cfg.effects_volume = std::clamp(s.effects, 0, 100);
  if (!s.on) mech3::set_cd_volume(0.0f);  // off: PM's music stopped too (the theme, the mission's)
  else mech3::set_cd_volume(std::max(g_cfg.music_volume, 1) / 100.0f);
  mech3::set_sound_frequency(g_cfg.effects_volume / 100.0f);
  if (theme_playing()) volume(static_cast<int32_t>(g_level * static_cast<float>(full_volume())));
  if (!g_dir.empty()) {
    Installs in = read_cfg(g_dir);
    in.sound = g_cfg;
    if (!write_cfg(g_dir, in)) std::printf("mw1: music: couldn't save %s\n", kCfg);
  }
  std::printf("mw1: music: %s, music %d%%, effects %d%%\n", s.on ? "on" : "off", g_cfg.music_volume, g_cfg.effects_volume);
}

void frame(bool menus) {
  if (!cd_open()) return;
  const bool want = menus && settings().on && mech3_music::enabled();
  const float step = 1.0f / static_cast<float>(kFadeFrames);
  if (want) {
    if (!theme_playing()) {  // from silence: at the boot, back from a mission, turned on, or PM's music in its place
      g_level = 0;
      volume(0);
      if (!zsnd_cd::play_track(zsnd_cd::kExtraTrack, zsnd_cd::Loop)) return;
      ++g_starts;
      std::printf("mw1: music: the menu theme\n");
    }
    g_faded_out = false;
    g_level = std::min(1.0f, g_level + step);
    volume(static_cast<int32_t>(g_level * static_cast<float>(full_volume())));
    return;
  }
  if (theme_playing()) {
    g_level = std::max(0.0f, g_level - step);
    if (g_level > 0) return volume(static_cast<int32_t>(g_level * static_cast<float>(full_volume())));
    zsnd_cd::stop();
  }
  if (!g_faded_out) {  // the theme gone (faded, or PM's mission music took its place): PM's own volume back
    g_faded_out = true, g_level = 0;
    volume(full_volume());
  }
}

Seen seen() { return {theme_playing(), g_level, g_starts, g_volume}; }

}  // namespace mw1::music
