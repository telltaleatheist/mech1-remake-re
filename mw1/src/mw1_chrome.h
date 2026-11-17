#pragma once
// MW1's screen chrome (FIXES.md item 3): the riveted plaques, the picture tiles' frames, the menu panels' frames and
// the buttons, drawn by hand in code (not generated), sharp, in MW3's gunmetal style, and packed at build time into
// mw1image.zbd (mw1imgpack) beside the art (data-src/art/). Each piece carries an alpha plane (the engine's kFullAlpha,
// blended over the scenery): a frame's window is clear, a panel's body dark and part see-through.
//
// The look: a steel face lit from the top left (a vertical gradient, a bright bevel top and left, a dark one bottom
// and right), a black recess inset for text or a picture, round rivets with a highlight, and MW1's amber for a lit
// (hovered) edge.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1::chrome {

// The pieces, by name in the archive.
inline constexpr const char* kPlaque = "mw1_plaque";            // the hub's world and date plaques
inline constexpr int32_t kPlaqueW = 340, kPlaqueH = 52;         // its text recess: kPlaqueInset in from each side
inline constexpr int32_t kPlaqueInset = 12;
inline constexpr int32_t kTile = 96;                            // a picture tile (data-src/art/icon_*)
inline constexpr int32_t kTileFrame = 8;                        // the frame's width round it
inline constexpr const char* kTileFrames[3] = {"mw1_tile", "mw1_tile_lit", "mw1_tile_down"};  // still, hover, pressed
inline constexpr int32_t kButtonW = 168, kButtonH = 30;         // a button down the left (MECH LAB, LAUNCH, ...)
inline constexpr const char* kButtons[4] = {"mw1_button", "mw1_button_lit", "mw1_button_down", "mw1_button_off"};
inline constexpr int32_t kPanelBorder = 10;                      // a menu panel's frame width

// A menu panel's frame, any size (the screens name theirs: mw1_panel_<w>x<h>): its body dark, 80% opaque.
struct PanelSize {
  int32_t w, h;
};
std::string panel_name(int32_t w, int32_t h);
inline const std::vector<PanelSize>& panel_sizes() {
  // The ones the screens use (each screen's layout names its own; add one here when a layout needs it).
  static const std::vector<PanelSize> k = {{232, 120}, {300, 200}, {400, 300}, {560, 360}, {280, 420}};
  return k;
}

images::Image plaque();
images::Image tile_frame(int32_t state);  // 0 still, 1 lit, 2 pressed
images::Image button(int32_t state);      // 0 still, 1 lit, 2 pressed, 3 off
images::Image panel(int32_t w, int32_t h);
std::vector<images::Image> all();  // every piece, for the archive

}  // namespace mw1::chrome

namespace mw1 {
using namespace chrome;
}  // namespace mw1
