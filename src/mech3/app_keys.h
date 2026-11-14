#pragma once
#include <cstdint>

// The app's own keys (between briefing.cpp and mech.cpp, after the options;
// the file is not known): the commands whose keys reach the app, not the
// mech (docs/keybindings.md).
namespace mech3 {

// Where the app's keys go; null calls are skipped.
struct AppKeyCalls {
  void (*cycle_camera)() = nullptr;    // 0x0040aed0 on the player's view [0x005f4c8c]
  void (*vmode_cycle)() = nullptr;     // 0x004ab910: the next screen mode, saved as InGameVMode
  void (*message_toggle)() = nullptr;  // 0x0040f070: the message panel
  void (*debug_7e)() = nullptr;        // PM 0x004af600 (CMD_SNAPSHOT 0x86, Ctrl+P); MW3 0x004abbc0 (0x7e, no key)
  // Ours: --debug-camera / debugcamera=1: Shift+F12 to command 0x9a, the
  // game editor's (docs/editor.md S4; the exe binds no key to it).
  bool debug_camera = false;
  // The debug commands' targets (PM's numbers, MW3's plus 8; no key or file binds them in the release;
  // keybindings.md). Each: MW3's function, then PM's.
  void (*mech_debug_bit)() = nullptr;           // 0x75: 0x0041ca90 / 0x0041d8f0 on the pilot's mech
  int32_t (*mech_list_dialog)() = nullptr;      // 0x8c: 0x00432c60 / 0x00433770 on its roster (an MFC list; -1 none)
  void (*mech_remake)(int32_t roster) = nullptr;  // 0x8c: 0x004783c0 / 0x0047a6e0 on the pilot
  void (*next_view_mech)() = nullptr;           // 0x91: 0x004a33f0 / 0x004a6810
  void (*vehicle_dialog)() = nullptr;           // 0x92: 0x004a3550 / 0x004a6940 (an MFC list)
  void (*camera_mode)(int32_t id) = nullptr;    // 0x93, 0x94, 0x96: 0x0040ae70 / 0x00408a70 on the view
  void (*camera_dialog)() = nullptr;            // 0x97: 0x0040bb00 / 0x00409830 on the view's camera (+0x58; MFC)
  int32_t (*show_cursor)(int32_t show) = nullptr;  // 0x9b: ShowCursor
  void (*marker)() = nullptr;                   // 0x9c: 0x004a3220 / PM's own 0x004a65a0
  void (*teleport)() = nullptr;                 // 0x9d: 0x004a37a0 / 0x004a8450
  int32_t (*soft_mip_toggle)() = nullptr;       // 0x9f: 0x00557f70 / 0x00579ef0 (the software renderer's): on after
  void (*edit_vehicles)() = nullptr;            // 0xa5: 0x004b86e0 / 0x004bec10
  float (*soft_mip_bias)() = nullptr;           // 0xa6, 0xa7: 0x00535ef0 / 0x0054fe90 (the software renderer's)
  void (*set_soft_mip_bias)(float bias) = nullptr;  // 0x00535ed0 / 0x0054fe70
  void (*interp_console)() = nullptr;           // 0xab: 0x004810d0 / 0x00483d50 (0x004832a0) on the app
  void (*twod_through_3d)(int32_t on) = nullptr;  // PM's 0xaf: 0x005c0340, the 2D-through-Direct3D flag (MW3's 0xa4: the mech lab)
};
// The debug commands' own state (MW3's globals / PM's).
struct AppKeyState {
  float max_rate = 8.0f;  // [0x005e0c98] / [0x00602c90] the clamp's lowest frame rate, 0x8f and 0x90 five apart
  int32_t clamp = 1;      // [0x005e0c9c] / [0x00602c94] 0x8e
  int32_t cursor = 1;     // [0x005e0ca0] / [0x00602c98] 0x9b: the last ShowCursor's count was below 0
  int32_t dev_text = 0;   // [0x005f4d6c] / [0x0061881c] 0x8a: the frame rate and the developer lines' layer
  int32_t readout = 0;    // [0x005f4d70] / [0x00618820] 0x8b
  int32_t twod_3d = 0;    // PM's [0x00618824] 0xaf
};
AppKeyState& app_key_state();
AppKeyCalls& app_key_calls();

// Mech3.exe 1.2 @ 0x0040db60 (from the mission's load): CYCLE_CAM (0x69),
// MESSAGE_TOGGLE (0x78) and VMODE_TOGGLE (0x74) to 0x0040dba0, CMD_SNAPSHOT (0x86, Pirate's Moon's) to 0x004abbc0.
void register_app_keys();

// Mech3.exe 1.2 @ 0x0040dba0, Pirate's Moon 1.0 @ 0x0040ba30 (MW3: ECX the command; PM: on the stack; Ghidra
// made no function of either): the commands bound to it and the debug commands no key or file binds (PM's numbers,
// MW3's plus 8: 0x75, 0x8a..0xab, and 0xaf in place of MW3's 0xa4; ported for preservation; keybindings.md).
void app_key(int32_t cmd);

}  // namespace mech3
