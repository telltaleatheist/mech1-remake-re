#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 12, mw1/research/hires-2d.md option B): the 2D's text and pictures drawn
// at the window's own resolution, over the 480-line canvas. Opt-in: only MW1's program turns it on (mw1_main.cpp);
// MechWarrior 3's and Pirate's Moon's never do, and with it off the 2D is drawn exactly as before.
//
// hires_2d_debug() (MW1_HIRES_DEBUG=1): the recorded regions tinted, so a shot shows at a glance what is drawn at
// the window's resolution and what fell back to the canvas.
#include <cstdlib>
#include <cstring>

namespace platform {

namespace hires_detail {
inline bool& on() {
  static bool v = false;
  return v;
}
}  // namespace hires_detail

inline void set_hires_2d(bool on) { hires_detail::on() = on; }
inline bool hires_2d() { return hires_detail::on(); }
inline bool hires_2d_debug() {
  static const bool v = [] {
    const char* e = std::getenv("MW1_HIRES_DEBUG");
    return e && *e && std::strcmp(e, "0") != 0;
  }();
  return v;
}

}  // namespace platform
