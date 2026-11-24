#pragma once
// FIXES.md item 100 (the owner, 2026-11-19): MW1's menu music and its sound settings.
//
// The theme (data/music/menu-theme.ogg, ours: a YuE2 track the owner picked, its last 5 s faded out) plays through the
// engine's CD music (zsnd_cd's extra track, outside the disc's 1..99, so PM's mission music never picks it). It starts
// with the game (the title and the intro), plays on every screen out of a mission, and loops; it isn't restarted by
// a screen change. DEPLOY fades it out (the mission loads behind MW1's loading screen); back at the results it fades
// in again. Pirate's Moon's own menu music doesn't play over MW1's screens.
//
// The settings (the hub's OPTIONS): music on or off, its volume and the effects' volume, in mw1.cfg (mw1_start.h's
// SoundCfg) and applied at once to PM's own two (its options' CD volume and sound volume, GRAPHICS & SOUND's
// sliders), so the mission's music and sounds follow and the two screens never disagree. A run no player hears
// (--nosound, --hidden) plays nothing aloud (the engine's own rule: platform::SilentMusicOutput).
//
// FIXES.md item 116 (the owner, 2026-11-20): the missions' music is ours too. Three combat tracks (data/music/combat-*.ogg,
// YuE2 seeds the owner picked: 7106, the soundtrack's, and 7101 and 7102), one picked at each launch (a replay's too),
// never the one the last launch played. It plays through the same extra track as the theme (its file swapped), from
// the launch, looped, at the music volume (PM's, as OPTIONS sets it), paused with the game, until the mission's end
// stops the CD (as it stopped PM's music); the theme fades in at the results. Pirate's Moon's own disc music never plays in MW1: MW1's CD has no disc tracks
// (num_tracks 0, so PM's mission start picks none), and none of PM's player's music is read.
#include <cstdint>
#include <string>

namespace mw1::music {

inline constexpr const char* kThemeFile = "music/menu-theme.ogg";  // through the mounts (mw1data)
inline constexpr int32_t kFadeFrames = 60;                          // a second, either way
inline constexpr int32_t kCombatCount = 3;
inline constexpr const char* kCombatFiles[kCombatCount] = {"music/combat-7106.ogg", "music/combat-7101.ogg",
                                                           "music/combat-7102.ogg"};

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

// Where the game is, each frame shown: MW1's screens; DEPLOY's load (and the briefing), before the launch; the mission;
// the mission ended (its outcome latched: PM's end screen, its fade, Instant Action's end, until MW1's screens).
enum class Where { kMenus, kLoading, kMission, kEnded };
// Each frame shown. Menus: the theme started (from silence) when it isn't playing, faded in (a combat track still
// playing stopped first). Loading: the theme faded out, then stopped, PM's music volume given back. Mission: the launch's combat
// track, playing (started again if it stopped: music turned off and on), at the music volume. Ended: silence, the
// launch's track stopped if it still plays and never started again (FIXES.md item 116b: the mission's end stops the CD,
// and the mission's own frames until the results started it again from the top).
void frame(Where where);
// Each launch (a replay's too): a combat track picked for it, never the last one played.
void launched();

// The pick: any of the three at first (last < 0), else one of the two others. `roll` is any number (rand's).
int32_t pick_combat(int32_t last, uint32_t roll);

// Tests: what the music is doing.
struct Seen {
  bool playing = false;  // the theme is the CD's track now
  float level = 0;       // its fade, 0..1
  int32_t starts = 0;    // times it was started
  int32_t volume = 0;    // the last volume given the CD (0..0xFFFF)
  int32_t combat = -1;   // the combat track playing now (kCombatFiles' index), -1 none
  int32_t picked = -1;   // the last launch's pick
  int32_t launches = 0;  // launches seen
  int32_t combat_starts = 0;  // times a combat track was started (FIXES 116b: one a launch)
};
Seen seen();
void set_last_combat_for_tests(int32_t last);  // as if the last launch played this one

}  // namespace mw1::music
