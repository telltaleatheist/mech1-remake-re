#pragma once
// MW1's screen chrome (FIXES.md item 3): the plaques, the picture tiles' frames (and each tile's four looks, its
// picture framed), the buttons, the menu and terminal panels and the status line's band, drawn by hand in code (not
// generated) and packed at build time into mw1image.zbd (mw1imgpack) beside the art (data-src/art/). Each piece
// carries an alpha plane (the engine's kFullAlpha, blended over the scenery): a frame's window clear, a panel's body
// dark and part see-through.
//
// The look: the owner's hub mockup's (mockups/mw1-hub.html): flat thin gunmetal frames (a lit top lip, a black outline)
// round dark glass, a rivet at each end of a plaque, MW1's amber for a lit (hovered) edge, the terminal's green.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1::chrome {

// Ours (research/hires-2d.md B2b): a piece drawn at scale s for the 2D at the window's resolution: (canvas_w * s) x
// (canvas_h * s) RGBA bytes, straight alpha (0..3 as 0, 0xfc and up as 255, as the canvas blit's edges).
struct Drawn {
  std::string name;
  int32_t canvas_w = 0, canvas_h = 0, s = 1;
  std::vector<uint8_t> rgba;
};

// The pieces, by name in the archive. The look follows the owner's hub mockup (mockups/mw1-hub.html): flat, thin frames.
inline constexpr const char* kPlaque = "mw1_plaque";            // the hub's world and date plaques
inline constexpr int32_t kPlaqueW = 300, kPlaqueH = 60;  // two side by side on 4:3's 640
// The hub's (FIXES.md item 16, the owner): on 4:3's 640 the world's and the date's plaques narrower, the company's strip
// (C-BILLS, PAYROLL, KILLS) between them; the wide canvas keeps kPlaqueW's, the strip in the gap.
inline constexpr const char* kPlaqueNarrow = "mw1_plaque_narrow";
inline constexpr int32_t kPlaqueNarrowW = 208;
inline constexpr const char* kStrip = "mw1_strip";
inline constexpr int32_t kStripW = 192;
inline constexpr int32_t kTile = 100;                           // a picture tile's picture (data-src/art/icon_*)
inline constexpr int32_t kTileFrame = 4;                        // the frame's width round it
inline constexpr int32_t kTileBand = 20;                        // the dark band inside its foot, for the label
inline constexpr const char* kTileFrames[3] = {"mw1_tile", "mw1_tile_lit", "mw1_tile_down"};  // still, hover, pressed
inline constexpr int32_t kTinyButtonW = 58;  // the load box's OK and NO, framed (in PM's 84 wide circlebtn_tiny)
inline constexpr int32_t kButtonW = 168, kButtonH = 30;         // a button (MECH LAB, LAUNCH, NEW, ...)
inline constexpr const char* kButtons[4] = {"mw1_button", "mw1_button_lit", "mw1_button_down", "mw1_button_off"};
// A screen's primary action (the office's ACCEPT, FIXES.md item 56): a button framed in amber when still; lit, pressed
// and off it looks as any (kButtons' 1..3).
inline constexpr const char* kPrimaryButton = "mw1_button_primary";
// A button a tile wide (the hub's STORY under its right column, FIXES.md item 47).
inline constexpr int32_t kTileButtonW = kTile + 2 * kTileFrame;
inline constexpr const char* kTileButtons[4] = {"mw1_button_tile", "mw1_button_tile_lit", "mw1_button_tile_down", "mw1_button_tile_off"};
inline constexpr const char* kDialog = "mw1_dialog";            // every message box's frame (mw1dialog.zrd)
inline constexpr int32_t kDialogW = 420, kDialogH = 200, kDialogRule = 38;  // the rule under the title
inline constexpr const char* kCursor = "mw1_cursor";            // MW1's mouse pointer (every layout's CURSOR)
inline constexpr int32_t kCursorSize = 31;
// The first run's cover over the mech render pass (FIXES.md item 14): a bar of cells under a plaque, one lit per mech
// done (mw1_progress, _lit).
inline constexpr const char* kPlaqueCover = "mw1_plaque_cover";  // its plaque, wide enough for its words
inline constexpr int32_t kPlaqueCoverW = 400;
inline constexpr const char* kProgressCell[2] = {"mw1_progress", "mw1_progress_lit"};
inline constexpr int32_t kProgressCellW = 34, kProgressCellH = 10, kProgressGap = 4;  // 8 across kPlaqueW
inline constexpr const char* kDim = "mw1_dim";                  // the screen dimmed behind a box
inline constexpr int32_t kDimAlpha = 150;
inline constexpr const char* kStatusBar = "mw1_statusbar";      // the status line's band, the wide canvas across
inline constexpr int32_t kStatusH = 26;
// The status line's room at 4:3 (FIXES 3+5's sweep): "mw1_status" is Share Tech Mono at 13, 8 pixels a letter, from 132
// to the 640 canvas's edge less a margin; every status text keeps to this many letters (the soak test checks them all).
inline constexpr size_t kStatusLetters = 62;
// The hub's scenery darkened (percent kept) and its scanlines (every other line, percent of that kept).
inline constexpr int32_t kSceneLight = 74, kScanline = 86;

// A menu panel's frame and a terminal panel, any size (the screens name theirs: mw1_panel_<w>x<h>,
// mw1_term_<w>x<h>): dark glass, part see-through.
struct PanelSize {
  int32_t w, h;
};
std::string panel_name(int32_t w, int32_t h);
std::string terminal_name(int32_t w, int32_t h);
// A message's box at the screen's foot (FIXES.md item 42, the owner: unobtrusive, the picture above it clear): the
// terminal's look, sized to its text, a title row, 1..kBoxLinesMax lines and a row of buttons inside it. Each is
// drawn at the foot of a kBoxCanvasH high picture, clear above it, so every size shares one place in a layout and a
// box grows upward with its lines (mw1_box_<w>_<lines>).
inline constexpr int32_t kBoxLinesMax = 6;           // then it pages: about a third of the 480-line canvas
inline constexpr int32_t kBoxLineGap = 15;
inline constexpr int32_t kBoxChromeH = 82;           // its frame, the title row and the buttons' row around the lines
inline constexpr int32_t kBoxCanvasH = kBoxChromeH + kBoxLinesMax * kBoxLineGap;  // 172
inline constexpr int32_t box_height(int32_t lines) { return kBoxChromeH + lines * kBoxLineGap; }
inline constexpr int32_t kBoxWidths[] = {560, 376, 760};  // a message's; the hub's (between its tiles); a message's at 16:9
std::string box_name(int32_t w, int32_t lines);
inline const std::vector<PanelSize>& panel_sizes() {
  // The ones the screens use (each screen's layout names its own; add one here when a layout needs it).
  static const std::vector<PanelSize> k = {{232, 120}, {300, 200}, {400, 300}, {560, 360}, {280, 420}, {376, 214}, {456, 214},
                                           {232, 244}, {232, 324}, {176, 156}, {436, 370}, {637, 370}, {436, 152}, {637, 152},
                                           {460, 414}, {232, 256}, {424, 100}, {424, 170}};  // FIXES 125: the save slots' panel, a slot, its confirm;
                                                                            // the hub's OPTIONS with SAVE and LOAD
  return k;
}
inline const std::vector<PanelSize>& terminal_sizes() {
  // The screens' terminals; the file popup's name (220x22); AFTER ACTION's
  // status and message at 16:9 and 4:3 (item 38).
  static const std::vector<PanelSize> k = {{232, 150}, {436, 300}, {637, 300}, {186, 328}, {377, 328}, {436, 172}, {637, 172},
                                           {220, 22},  {304, 168}, {569, 324}, {408, 324}, {721, 96},  {508, 96}};
  return k;
}
// A picture's frame (a portrait's, a mech's): mw1_frame_<w>x<h> and its lit look (_lit, the chosen one's), its
// inside clear; the picture (w - 2 kFrame by h - 2 kFrame) goes in it.
inline constexpr int32_t kFrame = 4;
std::string frame_name(int32_t w, int32_t h, bool lit = false);
inline const std::vector<PanelSize>& frame_sizes() {
  // A portrait (240x320), a small one (96x128), the market's mech (160x128), AFTER ACTION's picture (item 38: 240x210
  // at 16:9, 192x168 at 4:3), the office's liaison (items 50 and 56: 160x278).
  static const std::vector<PanelSize> k = {{248, 328}, {104, 136}, {168, 136}, {248, 218}, {200, 176}, {168, 286}};
  return k;
}
// The up and down arrows (a list's, a term's): mw1_up and mw1_dn, each with _lit, _down and _off; the left and right
// ones (an option's choice): mw1_lt and mw1_rt, the same four looks, kSideArrowW by kSideArrowH.
inline constexpr int32_t kArrowW = 28, kArrowH = 22;
inline constexpr int32_t kSideArrowW = 20, kSideArrowH = 18;
inline constexpr int32_t kSideInkH = 10;  // the side arrow's triangle at its base (FIXES.md item 101)
// The options' pieces (mw1options.zrd): a slider's bar and its fill, a check box and its check, a control table's
// cell (and lit, chosen), the file popup's taller dialog.
inline constexpr const char* kSlider = "mw1_slider";
inline constexpr const char* kSliderFill = "mw1_slider_fill";
inline constexpr int32_t kSliderW = 190, kSliderH = 12;
inline constexpr const char* kCheck = "mw1_check";
inline constexpr const char* kChecked = "mw1_check_on";
inline constexpr int32_t kCheckSize = 16;
inline constexpr const char* kCells[3] = {"mw1_cell", "mw1_cell_lit", "mw1_cell_sel"};
inline constexpr int32_t kCellW = 62, kCellH = 20;
// FIXES 126, MW1's SETTINGS (mw1settings.zrd): a section's tab (a button's looks; _off the chosen one, its lamp lit and
// framed in amber), a choice's value (a segment; _off the chosen one, filled amber; a cheat's chosen ON filled red,
// mw1_segx_off), and a volume's cell (empty, mw1_vol; full, mw1_vol_on), each with the four looks.
inline constexpr const char* kTabLook = "mw1_tab";
inline constexpr const char* kSegLook = "mw1_seg";
inline constexpr const char* kSegAlertLook = "mw1_segx";
inline constexpr int32_t kSegW = 84, kSegH = 20;
inline constexpr const char* kVolLook = "mw1_vol";
inline constexpr const char* kVolOnLook = "mw1_vol_on";
inline constexpr int32_t kVolW = 14, kVolH = 18;
images::Image tab(int32_t state);                       // 0 still, 1 lit, 2 pressed, 3 the chosen one
images::Image segment(int32_t state, bool alert);       // the same; 3 chosen (amber; red when alert)
images::Image volume_cell(int32_t state, bool full);    // 0 still, 1 lit, 2 pressed, 3 off
std::string look_name(const char* base, int32_t state);  // base, _lit, _down, _off
inline constexpr const char* kClear = "mw1_clear";  // nothing to see: a part a screen binds that MW1 doesn't show
inline constexpr const char* kDialogTall = "mw1_dialog_tall";
inline constexpr int32_t kDialogTallH = 300;

images::Image plaque(int32_t w = kPlaqueW, const char* name = kPlaque);  // kPlaqueH high
images::Image progress_cell(bool lit);
images::Image tile_frame(int32_t state);  // 0 still, 1 lit, 2 pressed
// A picture tile's four looks, its picture (kTile square) in the frame: `base`, `base`_lit, `base`_down, `base`_off.
std::vector<images::Image> tiles(const images::Image& picture, const std::string& base);
// Ours (B2b): the four looks at scale s, the picture from (x0, y0) side_w x side_h of a source picture (RGBA, sw x sh);
// the same names as tiles'; opaque.
std::vector<Drawn> tiles_at(const uint8_t* rgba, int32_t sw, int32_t sh, int32_t x0, int32_t y0, int32_t side_w,
                            int32_t side_h, const std::string& base, int32_t s);
images::Image button(int32_t state);
images::Image button(int32_t state, int32_t w, int32_t h, const std::string& name, int32_t drawn = 0, bool primary = false);  // any size, any name      // 0 still, 1 lit, 2 pressed, 3 off
images::Image panel(int32_t w, int32_t h);
images::Image terminal(int32_t w, int32_t h);
images::Image box(int32_t w, int32_t lines);
images::Image status_bar();
images::Image dialog_frame();
images::Image dim();
images::Image cursor();
images::Image frame(int32_t w, int32_t h, bool lit);
images::Image arrow(bool up, int32_t state);  // 0 still, 1 lit, 2 pressed, 3 off
images::Image side_arrow(bool right, int32_t state);  // the same looks
images::Image slider(bool fill);
images::Image check(bool on);
images::Image cell(int32_t state);  // 0 still, 1 lit, 2 chosen
images::Image dialog_tall();
images::Image clear();

// The lab's reskin (Pirate's Moon's dlgMechLabII.zrd, its layout kept) and its load/save box's (dlgLoadSave.zrd):
// each of PM's pictures the layout names, ours in its place at its size (PM's layout places it;
// ztwod::set_layout_image while that file is read). The sizes are PM's pictures'. Ours is "mw1_" and PM's name.
struct Reskin {
  const char* game;
  int32_t w, h;
};
inline constexpr const char* kLabFile = "dlgMechLabII.zrd";
inline constexpr const char* kFileBoxFile = "dlgLoadSave.zrd";
inline constexpr const char* kLabHangar = "mechcomplex_hangar";  // the art (data-src/art) the lab's panels are cut from
const std::vector<Reskin>& lab_pictures();      // the panels first (lab_panels), then the drawn pieces
const std::vector<Reskin>& filebox_pictures();  // all drawn
std::string reskin_name(const char* game);       // ours: "mw1_" + game
bool is_lab_panel(const Reskin& r);
images::Image reskin(const Reskin& r);           // a drawn piece (not a panel)
// The lab's five panels from the hangar (its middle 640 of 854: left 220, centre 200, right 220), darkened, the
// sides' glass boxes framed (single: one; double: two).
std::vector<images::Image> lab_panels(const images::Image& hangar);
// Ours (research/hires-2d.md B2b): the lab's panels at scale s from the hangar's source picture (RGBA, sw x sh, the
// whole of what its canvas copy shows), the same names; opaque.
std::vector<Drawn> lab_panels_at(const uint8_t* rgba, int32_t sw, int32_t sh, int32_t s);
// The lab's frames in its 640x480 layout (lab_panels draws them, 2 thick; mw1_lab_fit.h checks against their
// insides): the side panels' boxes (the upper and lower of a doubled panel, or the whole), the centre's box under the
// mech, and the icons' tray line across the foot, which every frame stays above (FIXES.md item 17).
struct LabBox { int32_t x0, y0, x1, y1; };
inline constexpr int32_t kLabTrayY = 414;
// The tray's four buttons (FIXES.md item 29): the chassis, criticals and groups pages' icons and ACCEPT (LabScreen's
// tabs 0, 3, 4, 6), one size, kLabTabY down the layout, in pairs at the ends of the panels' span (kLabTabSpan0..1,
// lab_tab_x; FIXES 70); mw1_game.cpp places them, mw1_lab_fit.h checks them.
// Each button's HELP label (its hover words, PM's layout's) centred above it, kLabHelpY down, kLabHelpH high: between
// the tray's line and the button, clear of both (FIXES.md item 37; the buttons 48 high, not item 29's 52, for its room).
inline constexpr int32_t kLabHelpY = kLabTrayY + 2, kLabHelpH = 11;
inline constexpr int32_t kLabTabW = 60, kLabTabH = 48, kLabTabY = kLabHelpY + kLabHelpH + 2;
inline constexpr int32_t kLabTabSpan0 = 6, kLabTabSpan1 = 634;
inline constexpr int32_t kLabTabPages[4] = {0, 3, 4, 6};
// FIXES 70 (the owner, option A): in pairs at the tray's ends, the chassis and criticals buttons at the left, groups and
// ACCEPT at the right, kLabTabEdge in from the span's ends and kLabTabPairGap apart; the middle clear for the warnings.
inline constexpr int32_t kLabTabEdge = 14, kLabTabPairGap = 16;
inline constexpr int32_t lab_tab_x(int32_t i) {
  return i < 2 ? kLabTabSpan0 + kLabTabEdge + i * (kLabTabW + kLabTabPairGap)
               : kLabTabSpan1 - kLabTabEdge - kLabTabW - (3 - i) * (kLabTabW + kLabTabPairGap);
}
// FIXES 70: the lab's warnings (MESSAGE_AREA; PM's place, under the tray's line, put them behind its buttons) in the
// tray's middle, between the pairs, wrapped in a kLabWarnW x kLabWarnH box centred on the layout, from the buttons' top.
inline constexpr int32_t kLabWarnW = 300, kLabWarnH = 39, kLabWarnY = kLabTabY;
inline constexpr LabBox kLabSideUpper = {6, 6, 214, 242}, kLabSideLower = {6, 248, 214, 410}, kLabSideWhole = {6, 6, 214, 410};
inline constexpr LabBox kLabCentreBox = {226, 280, 414, 410};  // in the 640 layout (the centre panel's image at 220)
// The selection bar (lab_text_glow, 207x45, PM's OFFSET -17, -15 from the row): its solid edge kBarEdge wide just
// left of the row's text (2 in from the row's x: a side panel's frame inside is 2 left of PM's columns at 10 and 453),
// then to the end of the row's dark panel (172 from the row's x), 20 high.
inline constexpr int32_t kBarEdge = 2, kBarLeft = 17 - kBarEdge, kBarRight = 17 + 172, kBarTop = 13, kBarBottom = 33;
// FIXES.md item 39: 'MECH LOAD-OUT's and AVAILABLE SUPPLIES' rows across their panels: a side panel's inside
// (kLabSideWhole less its kLabFrame-thick frame) less kLabListMargin each side; and their selection bar
// (lab_text_glow_wide, ours: no PM layout names it), lab_text_glow's look to the end of a row that wide.
inline constexpr int32_t kLabFrame = 2, kLabListMargin = 6;
inline constexpr int32_t kLabListRowW = kLabSideWhole.x1 - kLabSideWhole.x0 - 2 * kLabFrame - 2 * kLabListMargin;  // 192
inline constexpr const char* kLabWideBar = "lab_text_glow_wide";
inline constexpr int32_t kLabWideBarW = 207 + kLabListRowW - 172;
void darken_scene(images::Image& im);    // the hub's scenery treatment (kSceneLight, kScanline)
std::vector<images::Image> all();  // every piece, for the archive (the tiles come with their pictures, mw1imgpack)
// Ours (research/hires-2d.md B2b): the pieces drawn again at scale s (2 and up) for the 2D at the window's
// resolution, by their archive names (all but the clear placeholder); RGBA, straight alpha. The dim (flat black)
// comes at s 1 (its canvas size) whatever the scale.
std::vector<std::string> chrome_names();
bool chrome_at(const std::string& name, int32_t s, Drawn* out);

}  // namespace mw1::chrome

namespace mw1 {
using namespace chrome;
}  // namespace mw1
