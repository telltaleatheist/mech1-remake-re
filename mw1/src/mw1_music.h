#pragma once
// FIXES.md item 100 (Owen, 2026-11-19): MW1's menu music and its sound settings.
//
// The theme (data/music/menu-theme.ogg, ours: a YuE2 track Owen picked, its last 5 s faded out) plays through the
// engine's CD music (zsnd_cd's extra track, outside the disc's 1..99, so PM's mission music never picks it). It starts
// with the game (the title and the intro), plays on every screen out of a mission, and loops; it isn't restarted by
// a screen change. DEPLOY fades it out (the mission loads behind MW1's loading screen); back at the results it fades
// in again. Pirate's Moon's own menu music doesn't play over MW1's screens.
//
// The settings (the hub's OPTIONS): music on or off, its volume and the effects' volume, in mw1.cfg (mw1_start.h's
// SoundCfg) and applied at once to PM's own two (its options' CD volume and sound volume, GRAPHICS & SOUND's
// sliders), so the mission's music and sounds follow and the two screens never disagree. A run no player hears
// (--nosound, --hidden) plays nothing aloud (the engine's own rule: platform::SilentMusicOutput).
#include <cstdint>
#include <string>

namespace mw1::music {

inline constexpr const char* kThemeFile = "music/menu-theme.ogg";  // through the mounts (mw1data)
inline constexpr int32_t kFadeFrames = 60;                          // a second, either way

// Before the game main: the theme as the CD's extra track, quiet runs silent, mw1.cfg's settings read (none: PM's).
void install(const std::string& write_dir);

struct Settings {
  bool on = true;
  int32_t music = 50, effects = 50;  // percent
};
Settings settings();  // what applies now: PM's options (their volumes), mw1.cfg's on or off
// Applied at once (PM's options, the theme) and saved to mw1.cfg (the installs' lines kept).
void set_settings(const Settings& s);
void apply();  // mw1.cfg's settings to PM's options (the main state entered: after PM read its own)
void sync();   // PM's options moved (its SETTINGS' sliders): mw1.cfg saved to match (the hub's OPTIONS shown)

// Each frame shown: `menus` (out of a mission: not deployed). The theme started (from silence) when it isn't playing,
// faded in; in a mission, faded out, then stopped, PM's music volume given back.
void frame(bool menus);

// Tests: what the music is doing.
struct Seen {
  bool playing = false;  // the theme is the CD's track now
  float level = 0;       // its fade, 0..1
  int32_t starts = 0;    // times it was started
  int32_t volume = 0;    // the last volume given the CD (0..0xFFFF)
};
Seen seen();

}  // namespace mw1::music
