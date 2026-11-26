#pragma once
#include <cstdint>
// Ours: the mouse wheel scrolls the menus' lists (FileList, LabItemList: the Mech Lab's store, its weapons and its
// parts, the pilots, the saved games, the lobby's) under the pointer, a notch up as a click on the list's UP arrow and
// a notch down as one on DOWN, only while that arrow is shown and enabled, so the wheel stops at both ends as the
// arrows do; on the controls screen it steps the commands as NEXT and PREV. MW3 and Pirate's Moon: under SMALL FIXES
// (mech3::list_wheel_on, issue #20); MW1's Remake sets it on (its FIXES 102).
namespace platform {

namespace list_wheel_detail {
inline bool& on() {
  static bool v = false;
  return v;
}
}  // namespace list_wheel_detail

inline void set_list_wheel(bool on) { list_wheel_detail::on() = on; }
inline bool list_wheel() { return list_wheel_detail::on(); }

// A list's notch this frame from the wheel's position `z` (rising as it turns up): +1 up, -1 down, 0 none. `last` and
// `read` are the list's own; the first read (and the first after the list was hidden: read false) only takes z.
inline int32_t list_wheel_notch(int32_t z, int32_t& last, bool& read) {
  const int32_t was = last;
  const bool had = read;
  last = z, read = true;
  if (!had || z == was) return 0;
  return z > was ? 1 : -1;
}

}  // namespace platform
