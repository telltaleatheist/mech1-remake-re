#pragma once
#include <utility>

// MW1's own layouts for Pirate's Moon's screens that MW1 shows as they are (their code, MW1's look; FIXES.md
// item 3): the pause and the options. Each of PM's (file, dialog) is read from MW1's instead (ztwod::set_layout,
// ours; mw1_game.cpp sets them before Pirate's Moon builds those screens), binding the same names. Each has a 4:3
// layout and a wide one, placed for the canvas (Pirate's Moon's wide-menus regrouping leaves them alone).
namespace mw1 {

struct LayoutSwap {
  const char* game_file;    // Pirate's Moon's, as its code loads it
  const char* game_dialog;
  const char* file;         // MW1's data
  const char* dialog;       // the 4:3 canvas's; on a wide one (854) the same name with _WIDE
};
inline constexpr const char* kWideSuffix = "_WIDE";

inline constexpr const char* kPauseFile = "mw1pause.zrd";
inline constexpr const char* kOptionsFile = "mw1options.zrd";
inline constexpr LayoutSwap kLayoutSwaps[] = {
    {"dlgPause.zrd", "PAUSEDIALOG", kPauseFile, "MW1PAUSE"},
    {"dlgOptions.zrd", "GAME_OPTION_DIALOG", kOptionsFile, "MW1GAMEOPTIONS"},
    {"dlgOptions.zrd", "GFX_AUDIO_OPTIONDIALOG", kOptionsFile, "MW1GFXOPTIONS"},
    {"dlgOptions.zrd", "UPGRADES_DIALOG", kOptionsFile, "MW1UPGRADES"},
    {"dlgOptions.zrd", "CTL_CFG_DIALOG", kOptionsFile, "MW1CONTROLS"},
    {"dlgOptions.zrd", "ASSIGN_AXIS_POPUP", kOptionsFile, "MW1AXIS"},
    {"dlgOptions.zrd", "LOAD_SAVE_POPUP", kOptionsFile, "MW1FILES"},
};

// The fonts Pirate's Moon's code names on those screens (not their layouts), MW1's in their place while they're
// read (ztwod::set_layout_font): the cheats' OFF and ON (AlertCycle, menu_widgets.cpp).
inline constexpr std::pair<const char*, const char*> kLayoutFonts[] = {
    {"ia_small_green", "mw1_value"},
    {"ia_small_red", "mw1_value_alert"},
};

}  // namespace mw1
