#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gamez/zimage/zimg_shot.h"

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

// The screenshot's calls out (0x004abbc0, 0x00519cc0).
struct ShotHooks {
  zimg::Texture* (*grab)(int32_t buffer) = nullptr;  // 0x00579500: the screen as a new 16-bit image
  void (*free_image)(zimg::Texture* image) = nullptr;  // 0x00517cc0 (null: zimg::free_texture)
  bool (*lobby)() = nullptr;                          // 0x00586ce0: a DirectPlay lobby session
  bool (*hardware)() = nullptr;                       // [0x0072f348]: the Direct3D path
  void (*gdi_surface)() = nullptr;                    // 0x00577340: the GDI surface flipped to, for the dialog
  int32_t (*show_cursor)(int32_t show) = nullptr;     // ShowCursor
  // GetSaveFileNameA, "Save Captured Image" over "Bitmap Images (*.bmp)" (".bmp" by default): the path, null when
  // cancelled.
  const char* (*save_dialog)() = nullptr;
  bool (*exists)(const char* path) = nullptr;         // _access(path, 0) == 0, for _mktemp
  uint32_t (*process_id)() = nullptr;                 // the id _mktemp numbers names with
  void (*keys_reset)() = nullptr;                     // 0x0051a300
};
// Mech3.exe 1.2 @ 0x004abbc0 (command 0x7e, registered by 0x0040db60 but no key reaches it; docs/screenshots.md): the
// screen grabbed and saved as a .bmp, through the Save dialog (0x00519cc0), or in a lobby session under a fresh name
// mw3_?NNNNN.bmp (_mktemp); the keys reset after either. The name is returned for the tests ("" none).
std::string debug_screenshot(const ShotHooks& h);
uint32_t current_process_id();  // ours: _getpid / getpid
// Pirate's Moon 1.0 @ 0x004af600 (not a Ghidra function; CMD_SNAPSHOT 0x86, Ctrl+P by default): PM's own, no
// dialog and no lobby test: the screen to mw3NNNNNNNN.bmp, NNNNNNNN the session's count in hex ([0x0062f6d0], from
// 0), stepped past every name that exists (0x00596ea0, `exists`), then the keys reset. The name returned ("" none).
std::string pm_screenshot(const ShotHooks& h);
int32_t& pm_screenshot_count();  // [0x0062f6d0]
// Mech3.exe 1.2 @ 0x00519cc0: the dialog's path written (0x00519c20), the cursor shown around it.
bool screenshot_save_dialog(zimg::ScreenShot& shot, const ShotHooks& h, std::string* name);

}  // namespace mech3
