#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 100, 2026-11-19): with this on, a --nosound or --hidden run plays no music
// aloud. The game gets a platform::SilentMusicOutput (silent_sound.h): the CD calls are kept (the track asked for,
// its volume), no audio device is opened. MW1 turns it on (its menu theme ships with it, and its tests and shots
// run hidden while Owen plays); off (MW3's and PM's programs), --nosound silences the effects only, as before.
// MW3_QUIET_MUSIC=1 turns it on for any run too.
#include <cstdlib>
#include <cstring>

namespace platform {

namespace quiet_music_detail {
inline bool& on() {
  static bool v = [] {
    const char* e = std::getenv("MW3_QUIET_MUSIC");
    return e && *e && std::strcmp(e, "0") != 0;
  }();
  return v;
}
}  // namespace quiet_music_detail

inline void set_quiet_music(bool on) { quiet_music_detail::on() = on; }
inline bool quiet_music() { return quiet_music_detail::on(); }

}  // namespace platform
