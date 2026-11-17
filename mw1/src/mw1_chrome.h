#pragma once
// MW1's screen chrome (FIXES.md item 3): the plaques, the picture tiles' frames (and each tile's four looks, its
// picture framed), the buttons, the menu and terminal panels and the status line's band, drawn by hand in code (not
// generated) and packed at build time into mw1image.zbd (mw1imgpack) beside the art (data-src/art/). Each piece
// carries an alpha plane (the engine's kFullAlpha, blended over the scenery): a frame's window clear, a panel's body
// dark and part see-through.
//
// The look: Owen's hub mockup's (mockups/mw1-hub.html): flat thin gunmetal frames (a lit top lip, a black outline)
// round dark glass, a rivet at each end of a plaque, MW1's amber for a lit (hovered) edge, the terminal's green.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1::chrome {

// The pieces, by name in the archive. The look follows Owen's hub mockup (mockups/mw1-hub.html): flat, thin frames.
inline constexpr const char* kPlaque = "mw1_plaque";            // the hub's world and date plaques
inline constexpr int32_t kPlaqueW = 300, kPlaqueH = 60;  // two side by side on 4:3's 640
inline constexpr int32_t kTile = 100;                           // a picture tile's picture (data-src/art/icon_*)
inline constexpr int32_t kTileFrame = 4;                        // the frame's width round it
inline constexpr int32_t kTileBand = 20;                        // the dark band inside its foot, for the label
inline constexpr const char* kTileFrames[3] = {"mw1_tile", "mw1_tile_lit", "mw1_tile_down"};  // still, hover, pressed
inline constexpr int32_t kButtonW = 168, kButtonH = 30;         // a button (MECH LAB, LAUNCH, NEW, ...)
inline constexpr const char* kButtons[4] = {"mw1_button", "mw1_button_lit", "mw1_button_down", "mw1_button_off"};
inline constexpr const char* kDialog = "mw1_dialog";            // every message box's frame (mw1dialog.zrd)
inline constexpr int32_t kDialogW = 420, kDialogH = 200, kDialogRule = 38;  // the rule under the title
inline constexpr const char* kCursor = "mw1_cursor";            // MW1's mouse pointer (every layout's CURSOR)
inline constexpr int32_t kCursorSize = 31;
inline constexpr const char* kDim = "mw1_dim";                  // the screen dimmed behind a box
inline constexpr int32_t kDimAlpha = 150;
inline constexpr const char* kStatusBar = "mw1_statusbar";      // the status line's band, the wide canvas across
inline constexpr int32_t kStatusH = 26;
// The hub's scenery darkened (percent kept) and its scanlines (every other line, percent of that kept).
inline constexpr int32_t kSceneLight = 74, kScanline = 86;

// A menu panel's frame and a terminal panel, any size (the screens name theirs: mw1_panel_<w>x<h>,
// mw1_term_<w>x<h>): dark glass, part see-through.
struct PanelSize {
  int32_t w, h;
};
std::string panel_name(int32_t w, int32_t h);
std::string terminal_name(int32_t w, int32_t h);
inline const std::vector<PanelSize>& panel_sizes() {
  // The ones the screens use (each screen's layout names its own; add one here when a layout needs it).
  static const std::vector<PanelSize> k = {{232, 120}, {300, 200}, {400, 300}, {560, 360}, {280, 420}, {376, 214}, {456, 214},
                                           {232, 244}, {176, 156}, {436, 370}, {637, 370}, {436, 190}, {637, 190}};
  return k;
}
inline const std::vector<PanelSize>& terminal_sizes() {
  static const std::vector<PanelSize> k = {{232, 150}, {186, 328}, {377, 328}, {436, 172}, {637, 172}};
  return k;
}
// A picture's frame (a portrait's, a mech's): mw1_frame_<w>x<h> and its lit look (_lit, the chosen one's), its
// inside clear; the picture (w - 2 kFrame by h - 2 kFrame) goes in it.
inline constexpr int32_t kFrame = 4;
std::string frame_name(int32_t w, int32_t h, bool lit = false);
inline const std::vector<PanelSize>& frame_sizes() {
  static const std::vector<PanelSize> k = {{248, 328}, {104, 136}};  // a portrait (240x320), a small one (96x128)
  return k;
}
// The up and down arrows (a list's, a term's): mw1_up and mw1_dn, each with _lit, _down and _off.
inline constexpr int32_t kArrowW = 28, kArrowH = 22;

images::Image plaque();
images::Image tile_frame(int32_t state);  // 0 still, 1 lit, 2 pressed
// A picture tile's four looks, its picture (kTile square) in the frame: `base`, `base`_lit, `base`_down, `base`_off.
std::vector<images::Image> tiles(const images::Image& picture, const std::string& base);
images::Image button(int32_t state);      // 0 still, 1 lit, 2 pressed, 3 off
images::Image panel(int32_t w, int32_t h);
images::Image terminal(int32_t w, int32_t h);
images::Image status_bar();
images::Image dialog_frame();
images::Image dim();
images::Image cursor();
images::Image frame(int32_t w, int32_t h, bool lit);
images::Image arrow(bool up, int32_t state);  // 0 still, 1 lit, 2 pressed, 3 off
void darken_scene(images::Image& im);    // the hub's scenery treatment (kSceneLight, kScanline)
std::vector<images::Image> all();  // every piece, for the archive (the tiles come with their pictures, mw1imgpack)

}  // namespace mw1::chrome

namespace mw1 {
using namespace chrome;
}  // namespace mw1
