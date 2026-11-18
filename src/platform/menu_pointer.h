#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 30): the menus leave the mouse free fullscreen too, as they do in a window
// (sdl_input.h, docs/input.md): the pointer goes out of the window (to a second screen) and the game's cursor follows
// it inside. A mission holds the mouse as before. Opt-in: only MW1's program turns it on (mw1_main.cpp); MechWarrior
// 3's and Pirate's Moon's never do, and with it off the mouse is held fullscreen in the menus as before.

namespace platform {

namespace menu_pointer_detail {
inline bool& on() {
  static bool v = false;
  return v;
}
}  // namespace menu_pointer_detail

inline void set_menus_free_pointer(bool on) { menu_pointer_detail::on() = on; }
inline bool menus_free_pointer() { return menu_pointer_detail::on(); }

}  // namespace platform
