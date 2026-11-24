#pragma once
// FIXES.md items 100, 116 and 142: MW1's music and its sound settings.
//
// The soundtrack (FIXES 142, a project decision): MW1's five songs and six combat songs (data/music/). They all play
// through the engine's CD music as zsnd_cd's extra track (outside the disc's 1..99, so PM's mission music never picks
// it), its file swapped for each.
//
// The theme (theme.ogg, MW1's MECH) starts with the game (the title and the intro), plays on every screen out of a
// mission, and loops; a screen change never restarts it. DEPLOY fades it out (the mission loads behind MW1's loading
// screen); after a mission, or after a cue, it fades in again from the top.
//
// The cues, each where MW1 played its song (mech1-re's 081B campaign code; docs/AUDIO.md there), once and never
// looped, as MW1's sequencer played every song: the theme cut, the cue from its start at the music volume.
//   - bar.ogg (MECHBAR): the Bar entered from the hub; its own pages (drink, the crew, the news) keep it playing.
//   - debrief.ogg (LAUNCH, despite MW1's name): the results of a contract or story battle won (not the Dark Wing's),
//     lancemates lost or not.
//   - death.ogg (DEATH): the results of a mission not won that lost a lancemate (KILLED or MISSING); and MW1's death
//     events (its restart, which ours never does: FIXES 33), over the page until it's read.
//   - ending.ogg (VICTORY): the Dark Wing beaten, over the victory's page and the credits.
// MW1 stopped them all on entering the hub, so the hub's screen coming up ends a cue. A cue that ends on its own (MW1
// went silent there) gives way to the theme. MW1 played nothing in its missions and over none of its other screens.
//
// The settings (the hub's OPTIONS): music on or off, its volume and the effects' volume, in mw1.cfg (mw1_start.h's
// SoundCfg) and applied at once to PM's own two (its options' CD volume and sound volume, GRAPHICS & SOUND's
// sliders), so the mission's music and sounds follow and the two screens never disagree. A run no player hears
// (--nosound, --hidden) plays nothing aloud (the engine's own rule: platform::SilentMusicOutput).
//
// The missions' music (FIXES 116, 142): the six combat songs from a shuffle bag (CombatBag: none again until all six
// have played, never the same twice running). Each launch (a replay's too) starts the bag's next; when a song ends the
// bag's next follows it at once (the owner's ruling, 2026-11-23: one song never loops). They play at the music volume
// (PM's, as OPTIONS sets it), paused with the game, until the mission's end stops the CD (as it stopped PM's music);
// the theme fades in at the results. A song drawn to follow and not reached by the mission's end is the next launch's.
// Pirate's Moon's own disc music never plays in MW1: MW1's CD has no disc tracks (num_tracks 0, so PM's mission start
// picks none), and none of PM's player's music is read.
#include <cstdint>
#include <functional>
#include <string>

namespace mw1::music {

inline constexpr const char* kThemeFile = "music/theme.ogg";  // through the mounts (mw1data)
inline constexpr int32_t kFadeFrames = 60;                     // a second, either way
inline constexpr int32_t kCombatCount = 6;
inline constexpr const char* kCombatFiles[kCombatCount] = {
    "music/combat-iron-stride.ogg", "music/combat-cold-front.ogg", "music/combat-blackwind.ogg",
    "music/combat-firestorm.ogg",   "music/combat-long-range.ogg", "music/combat-last-stand.ogg"};

enum class Cue { kBar, kDebrief, kDeath, kEnding };
inline constexpr int32_t kCueCount = 4;
inline constexpr const char* kCueFiles[kCueCount] = {"music/bar.ogg", "music/debrief.ogg", "music/death.ogg",
                                                     "music/ending.ogg"};
// Each cue's length in seconds (the file's last granule over 48 kHz; mw1tests checks them against the files). The
// CD plays a track once and never says it ended, so a cue's own clock does.
inline constexpr double kCueSeconds[kCueCount] = {115.399, 29.839, 76.279, 103.079};

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
// Each frame shown. Menus: a cue asked for started (the theme or a combat track still playing stopped first) and played
// to its end; else the theme started (from silence) when it isn't playing, faded in. Loading: a cue stopped, the theme
// faded out, then stopped, PM's music volume given back. Mission: the launch's combat track, playing (started again if
// it stopped: music turned off and on), at the music volume; a song ended, the bag's next. Ended: silence, the
// launch's track stopped if it still plays and never started again (FIXES.md item 116b: the mission's end stops the CD,
// and the mission's own frames until the results started it again from the top).
void frame(Where where);
// Each launch (a replay's too): the bag's next combat track for it (the one drawn to follow, if it never played).
void launched();

// A cue at its moment: played from its start at the next frame on MW1's screens (again from the start if it plays
// already: the Bar entered again). Music off: none.
void cue(Cue c);
// Its stop: the hub's screen coming up (MW1 stopped its music entering the hub), a death page read. The theme fades
// in. A cue asked for and not started yet stays asked.
void cue_end();

// FIXES 142: the combat songs' shuffle bag. Each launch takes the next song of a shuffled order of all six; an empty
// bag is filled again in a new order whose first isn't the song just played.
struct CombatBag {
  int32_t order[kCombatCount] = {};
  int32_t at = kCombatCount;  // the next one's place; kCombatCount: empty
  int32_t last = -1;          // the last launch's song, -1 none
};
// The next launch's song. `roll`: any random numbers (the game's rand).
int32_t next_combat(CombatBag& bag, const std::function<uint32_t()>& roll);

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
  int32_t cue = -1;           // the cue playing now (Cue's value), -1 none
  int32_t cue_starts = 0;     // times a cue was started
  int32_t cue_asked = -1;     // a cue asked for, not started yet, -1 none
  int32_t queued = -1;        // the combat song drawn to follow the one playing, -1 none
  int32_t followed = 0;       // combat songs that followed another (a song's end, not a launch)
};
Seen seen();
void set_last_combat_for_tests(int32_t last);  // as if the last launch played this one, the bag empty
void end_cue_for_tests();                      // as if the cue playing reached its end

}  // namespace mw1::music
