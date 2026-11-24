#pragma once
// The Company screen's mech pictures (FIXES.md item 23): Pirate's Moon's own line-art chassis icons, the mech lab's
// mech slots' (icon_<mech>_yellow, 41x50, black clear), in MW1's amber, doubled pixel for pixel and centred in the
// screen's 96x128 mech slot. Made at the start from the player's Pirate's Moon (never changed) into an image archive
// in MW1's write dir (<kIconLibrary>.zbd at its root, rewritten when it differs), registered beside mw1image.zbd.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mw1_images.h"
#include "mw1_outline.h"

namespace zimg { struct Texture; }

namespace mw1 {

inline constexpr const char* kIconLibrary = "mw1icons";
inline constexpr int32_t kIconSlotW = 96, kIconSlotH = 128;  // mw1bar.zrd's mech panels (in their 104x136 frames)
inline constexpr int32_t kIconScale = 2;

// The picture's name for a Pirate's Moon mech ("mw1_icon_" and its name, lower case).
std::string mech_icon_image(int32_t pm_mech);
// PM's icon picture's name for a mech (mech_icon's colour 2, yellow).
std::string pm_mech_icon(int32_t pm_mech);
// PM's icon for a mech in the first colour PM has it in: yellow, then green, red, grey (six chassis, the Atlas,
// Awesome, Centurion, Clint, Masakari and Ryoken, have no yellow one). Null when PM has none; free it with
// zimg::free_texture.
inline constexpr const char* kPmIconColours[] = {"yellow", "green", "red", "grey"};
zimg::Texture* fetch_pm_mech_icon(int32_t pm_mech);
// The icon in amber: each lit pixel (any channel) amber at its brightness, black clear; scaled by kIconScale and
// centred on a clear kIconSlotW x kIconSlotH. An empty image's name when the texture is not 16-bit.
images::Image amber_icon(const std::string& name, const zimg::Texture& pm_icon);
// The lab's mech slots (FIXES.md item 26): every PM mech's icon at its own size (41x50) in amber, three looks for the
// slot's states: dim (not chosen), lit (chosen) and off (greyed). PM's MechButton asks for icon_<mech>_<colour> at
// run time; MW1 puts these in its place on the lab's slots (mw1_game.cpp, lab_look_frame).
enum class SlotLook { kDim, kLit, kOff };
std::string slot_icon_image(int32_t pm_mech, SlotLook look);  // "mw1_slot_<mech>", _lit, _off
images::Image slot_icon(const std::string& name, const zimg::Texture& pm_icon, SlotLook look);
// FIXES 69, 96: a lancemate's slot with no mech: MW3's placeholder 'Mech (icon_notmech, from PM's or MW3's libraries)
// in the look's colour, its outline drawn as the other slots'; only without it, an empty dashed frame of a PM icon's
// size (kBlankSlotW x kBlankSlotH, blank_slot). "mw1_slot_none", _lit, _off.
inline constexpr int32_t kBlankSlotW = 41, kBlankSlotH = 50;
std::string blank_slot_image(SlotLook look);
images::Image blank_slot(SlotLook look);
outline::Colour slot_colour(SlotLook look);  // each look's colour
// Every chassis that fields as an enemy (the roster) and every PM mech's slot looks, from PM's libraries; the archive written under `write_dir` when it
// differs from what is there. The count made (0: none of PM's icons found).
int32_t make_mech_icons(const std::string& write_dir);
// research/hires-2d.md B2 item 1 (the owner 2026-11-18): an icon made this run, as its outline redrawn (mw1_outline.h),
// its colour, and how its canvas copy places it (`scale` canvas pixels an icon pixel, centred on canvas_w x canvas_h);
// the 2D at the window's scale draws it so (mw1_art.h's set_drawn_art). Null when `name` isn't one.
struct IconOutline {
  std::shared_ptr<const outline::Outline> lines;
  outline::Colour colour{};
  int32_t scale = 1, canvas_w = 0, canvas_h = 0;
  bool fit = false;  // fitted into canvas_w x canvas_h (outline::draw_fit, lines at least kRowLineMin) instead
};
// The market's rows (the owner 2026-11-18): each mech's outline beside its name, kRowIconW x kRowIconH canvas pixels (a
// row's height: the lists' GAP), lit on the chosen row, dim on the rest (SlotLook's colours). Its lines at least
// kRowLineMin device pixels wide.
inline constexpr int32_t kRowIconW = 12, kRowIconH = 15;
inline constexpr double kRowLineMin = 1.2;
std::string row_icon_image(int32_t pm_mech, bool lit);  // "mw1_row_<mech>", _lit
const IconOutline* icon_outline(const std::string& name);
// FIXES.md item 12 (the 4K sweep, 2026-11-19): PM's own icons by PM's names ("icon_<Mech>_<colour>", kPmIconColours),
// which PM's screens show as they are (the lab's chassis at its top): their outlines in the icon's own colour (its
// brightest lit pixel's), placed as the icon (scale 1, the icon's size), read from PM's libraries on first ask. Null
// when `name` isn't one of PM's mech icons or PM's has no lines.
const IconOutline* pm_icon_outline(const std::string& name);

}  // namespace mw1
