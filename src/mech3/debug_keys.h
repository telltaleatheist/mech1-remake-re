#pragma once
#include <cstdint>
#include <vector>

// The debug commands' own pieces (app_key's 0x89 and 0x94; keybindings.md). Original file unknown: between player.cpp
// and turret.cpp, beside the teleport 0x004a37a0 (teleport.h). No key reaches them in the release; ported for
// preservation.
namespace mech3 {

// Mech3.exe 1.2 @ 0x004a3220, Pirate's Moon 1.0 @ 0x004a65a0 (PM's command 0x9c), PM's own: "Marker #  n" and
// the view target's place (yaw 0) to stdout, n from 0 ([0x0062db94]); when the target is a mech with a HUD, its
// reticle's aim point (`aim`, 0x00430e30's +0x1d68) to stdout and the message panel, and into `keep` (the script
// object's +0xc4). MW3's printed the place alone.
void debug_marker(const float pos[3], const float* aim, float* keep);
int32_t& debug_marker_count();  // [0x00609a24] / [0x0062db94]

// Mech3.exe 1.2 @ 0x004a33f0, Pirate's Moon 1.0 @ 0x004a6810 (command 0x91; PM's slots vt+0x48 and vt+0x114): the developer lines emptied; then, in zEntity's list ([0x0073655c]),
// the mech after `target`, wrapping to the list's first mech (the first too when `target` is not after it). `mech_of`
// is an entity's vt+0x2c, then that one's vt+0x40 (null: none). The caller runs the answer's vt+0x108 (the view
// onto it); none when it is `target`.
void* next_view_mech(const std::vector<void*>& entities, void* target, void* (*mech_of)(void*));

}  // namespace mech3
