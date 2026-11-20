#include "mw1_music.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "gamez/zsound/zsnd_cd.h"
#include "mech3/music.h"
#include "mech3/options.h"
#include "mw1_start.h"

namespace mw1::music {
namespace {
std::string g_dir;  // the write dir (mw1.cfg's)
SoundCfg g_cfg;     // mw1.cfg's sound lines (-1 unset)
float g_level = 0;
int32_t g_starts = 0, g_volume = 0;
bool g_faded_out = true;  // in a mission, the theme gone and PM's volume given back
// FIXES.md item 116: the extra track's file now (-1 the theme, else kCombatFiles' index); the launch's pick (-1 none
// yet); the last pick played; a new launch's track not started yet.
int32_t g_extra = -1, g_pick = -1, g_last = -1, g_launches = 0, g_combat_starts = 0;
bool g_new_launch = false, g_combat_failed = false;  // the launch's track wouldn't play: not tried every frame
std::minstd_rand g_rng{std::random_device{}()};

bool cd_open() { return (zsnd_cd::state().flags & 2u) != 0; }
bool extra_playing() {
  const zsnd_cd::State& s = zsnd_cd::state();
  return cd_open() && s.track == zsnd_cd::kExtraTrack && s.mode != zsnd_cd::Stopped && s.mode != zsnd_cd::Paused;
}
bool theme_playing() { return g_extra < 0 && extra_playing(); }
// A combat track on: playing, or paused with the game (the pause's cd_pause; its resume plays on).
int32_t combat_on() {
  const zsnd_cd::State& s = zsnd_cd::state();
  return cd_open() && g_extra >= 0 && s.track == zsnd_cd::kExtraTrack && s.mode != zsnd_cd::Stopped ? g_extra : -1;
}
void use_extra(int32_t which) {
  g_extra = which;
  zsnd_cd::set_extra_track(which < 0 ? kThemeFile : kCombatFiles[which]);
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
  g_pick = -1, g_last = -1, g_launches = 0, g_combat_starts = 0, g_new_launch = g_combat_failed = false;
  g_cfg = read_cfg(write_dir).sound;
  use_extra(-1);
}

int32_t pick_combat(int32_t last, uint32_t roll) {
  if (last < 0 || last >= kCombatCount) return static_cast<int32_t>(roll % kCombatCount);
  return (last + 1 + static_cast<int32_t>(roll % (kCombatCount - 1))) % kCombatCount;  // never `last`
}

void launched() {
  g_pick = pick_combat(g_last, static_cast<uint32_t>(g_rng()));
  g_last = g_pick, g_new_launch = true, g_combat_failed = false, ++g_launches;
}

void set_last_combat_for_tests(int32_t last) { g_last = last; }

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
  else if (combat_on() >= 0) volume(full_volume());
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
  else if (combat_on() >= 0) volume(full_volume());
  if (!g_dir.empty()) {
    Installs in = read_cfg(g_dir);
    in.sound = g_cfg;
    if (!write_cfg(g_dir, in)) std::printf("mw1: music: couldn't save %s\n", kCfg);
  }
  std::printf("mw1: music: %s, music %d%%, effects %d%%\n", s.on ? "on" : "off", g_cfg.music_volume, g_cfg.effects_volume);
}

void frame(Where where) {
  if (!cd_open()) return;
  // FIXES.md item 116: MW1's CD has no disc tracks. PM's mission start picks its music among them (none: no track), so
  // its disc music never plays in MW1, whatever music/ holds. Set each frame, as the CD may be opened again.
  zsnd_cd::state().num_tracks = 0;
  const bool on = settings().on && mech3_music::enabled();
  const float step = 1.0f / static_cast<float>(kFadeFrames);
  // FIXES.md item 116b: the mission ended. Its end stops the CD (as it stopped PM's music); the launch's track is never
  // started again (the mission's frames until the results did, from the top), and stopped if it plays on. Silence until
  // MW1's screens, where the theme fades in.
  if (where == Where::kEnded) {
    if (combat_on() >= 0) zsnd_cd::stop(), g_level = 0;
    return;
  }
  if (where == Where::kMission) {
    // The theme cut if DEPLOY's fade hadn't ended (a fast load); the launch's pick started, or started again.
    if (theme_playing()) zsnd_cd::stop(), g_level = 0;
    const zsnd_cd::State& s = zsnd_cd::state();
    if (s.mode != zsnd_cd::Stopped && s.track != zsnd_cd::kExtraTrack) zsnd_cd::stop();  // never anything else
    if (g_pick < 0) launched();  // in a mission without a launch seen (a test's own)
    if (g_new_launch && combat_on() >= 0 && combat_on() != g_pick) zsnd_cd::stop();  // a replay: its own pick
    g_new_launch = false;
    if (!on || g_combat_failed) return;
    if (combat_on() < 0) {
      use_extra(g_pick);
      volume(full_volume());
      if (!zsnd_cd::play_track(zsnd_cd::kExtraTrack, zsnd_cd::Loop)) {
        std::printf("mw1: music: no %s\n", kCombatFiles[g_pick]);
        g_combat_failed = true;
        return use_extra(-1);
      }
      ++g_combat_starts;
      std::printf("mw1: music: combat %s\n", kCombatFiles[g_pick]);
    }
    g_faded_out = false, g_level = 1;
    volume(full_volume());
    return;
  }
  // Out of the mission: the combat track already stopped there (the mission's end stops the CD, as it did PM's own
  // music), else stopped now; the extra track the theme's again.
  if (combat_on() >= 0) zsnd_cd::stop(), g_level = 0;
  if (g_extra >= 0) use_extra(-1);
  const bool want = where == Where::kMenus && on;
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

Seen seen() { return {theme_playing(), g_level, g_starts, g_volume, combat_on(), g_pick, g_launches, g_combat_starts}; }

}  // namespace mw1::music
