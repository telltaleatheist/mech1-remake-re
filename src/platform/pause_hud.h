#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 83, the lead, 2026-11-19): the pause shows over a grab of the mission's
// last frame, the HUD in it; its panel is see-through, and the HUD's marker line ("OP: <name> [62m]", the radar's)
// read across PAUSED's title. With this on, Escape in a mission draws that frame once more (the world not moved) with
// the marker line left out, so the grab hasn't it; the line is back with the mission's next frame. Off (MW3's and
// PM's programs): the pause as ever, its grab the frame shown before Escape.
namespace platform {

namespace pause_hud_detail {
inline bool& on() {
  static bool v = false;
  return v;
}
}  // namespace pause_hud_detail

inline void set_pause_hides_hud_marker(bool on) { pause_hud_detail::on() = on; }
inline bool pause_hides_hud_marker() { return pause_hud_detail::on(); }

}  // namespace platform
