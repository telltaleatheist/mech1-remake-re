#pragma once
// The lab's reskin (FIXES.md item 5): Pirate's Moon's lab and its load/save box keep PM's layouts (dlgMechLabII.zrd,
// dlgLoadSave.zrd); while each is read, PM's pictures are ours (mw1_chrome.h's lab_pictures, filebox_pictures: at PM's
// sizes) and PM's fonts MW1's (these, by name). Only in those two files: the briefing, the salvage and others use
// some of the same names (ztwod::set_layout_image, set_layout_font with the game's file).
#include <utility>

#include "mw1_chrome.h"

namespace mw1 {

// The lab's fonts: each PM name, ours "mw1_" and the name (mw1fonts.zrd).
inline constexpr const char* kLabFonts[] = {
    "lab_large_green", "lab_large_greenR", "lab_large_red", "lab_large_redR", "lab_large_title", "lab_large_white",
    "lab_large_whiteR", "lab_small_green", "lab_small_greenC", "lab_small_greenR", "lab_small_grey", "lab_small_greyR",
    "lab_small_red", "lab_small_redR", "lab_small_title", "lab_small_white", "lab_small_whiteC", "lab_small_whiteR",
};
// The load/save box's (PM's shared ones there, as the lab's: amber the words, the title's, grey the files not chosen).
inline constexpr std::pair<const char*, const char*> kFileBoxFonts[] = {
    {"ia_small_amber", "mw1_lab_small_red"}, {"main_red_small", "mw1_lab_small_title"}, {"ia_dark_grey", "mw1_lab_small_grey"},
};

// FIXES.md item 22: PM's pointer is ours (mw1_chrome.h's kCursor) on PM's own screens too while MW1 runs. Every
// CURSOR of PM's layouts names this picture, centred; these are the files that have one (ztwod::set_layout_image
// for each, while it is read).
inline constexpr const char* kPmCursor = "cursor_circle_orange";
inline constexpr const char* kPmCursorFiles[] = {
    "briefing.zrd", "dialog.zrd",    "dlgCredits.zrd", "dlgLoadSave.zrd", "dlgMechLabII.zrd", "dlgMessage.zrd",
    "dlgOptions.zrd", "dlgPause.zrd", "dlgSalvage.zrd", "dlgWeapon.zrd", "dlgWin.zrd",
};

}  // namespace mw1
