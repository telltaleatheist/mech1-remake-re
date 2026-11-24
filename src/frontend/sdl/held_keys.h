#pragma once
// Ours (2026-11-21): Shift held while the game starts opens the find
// screen with the files in use offered again, so a player can choose other
// game files without a terminal (docs/launcher.md, Choosing again).
namespace sdlfe {

// Shift down right now, asked of the system (no window needed): Windows'
// key state, the Mac's event flags, X11's keymap (Linux; a Wayland-only
// session without X has no answer: false). MW3_SHIFT_HELD=1 says yes (checks).
bool shift_held_now();

}  // namespace sdlfe
