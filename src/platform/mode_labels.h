#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 83, the lead, 2026-11-19): the options' RESOLUTION names every mode in one
// format, "W x H" from the mode's own size (vmode_size), where PM's names 3..10 by its messages ("640 x 480") and
// FIX (B)'s 11..16 by "%dx%d". Off (MW3's and PM's programs): the names as ever.
namespace platform {

namespace mode_labels_detail {
inline bool& on() {
  static bool v = false;
  return v;
}
}  // namespace mode_labels_detail

inline void set_uniform_mode_labels(bool on) { mode_labels_detail::on() = on; }
inline bool uniform_mode_labels() { return mode_labels_detail::on(); }

}  // namespace platform
