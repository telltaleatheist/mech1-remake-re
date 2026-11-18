#pragma once
// The mech pictures rendered from the game's own models (FIXES.md item 14): each mech MW1 uses (kMechKinds' stand-ins)
// drawn by the mech lab's renderer, from the front on black, then cropped round the mech, toned to MW1's amber on a
// dark warm backdrop and kept at each place's size. The models are the player's install's, so the pictures are made
// at the first run into an image archive in MW1's write dir (<kRenderLibrary>.zbd at its root, as mw1icons), with a
// stamp beside it (<kRenderLibrary>.txt: kRenderVersion and the mechs); a stamp that differs makes them again.
//
// The pass itself (mw1_game.cpp, render_pass_frame) opens the lab with its 2D hidden, shows each mech in a square
// window over the screen, reads the frame back (platform::Render3D::read_back) and hands it here.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1 {

inline constexpr const char* kRenderLibrary = "mw1renders";
// Raised whenever the camera, the grade or a size changes: every player's pictures are made again.
// 2: FIXES 14's first runs stamped the lab's 2D as the pictures (the pool the world conversion left up); made again.
// 3: FIXES 12 B2b, the 4x set (kRenderHiLibrary) beside the canvas's.
inline constexpr int32_t kRenderVersion = 3;
// The 2D's set at the window's resolution (research/hires-2d.md B2b): each picture graded again at kRenderHiScale
// times its place's size, RGBA, in mw1_art.h's archive format beside the canvas's archive, under the same stamp; the
// 2D finds it by the canvas picture's name (art::Store::add).
inline constexpr const char* kRenderHiLibrary = "mw1renders_hi";
inline constexpr int32_t kRenderHiScale = 4;
// The lab's camera moved for the pass (its own plus this): lower and further back, a tall mech's feet in frame.
inline constexpr float kRenderCamera[3] = {0.0f, -0.5f, -2.5f};

// Where a picture shows, and its size (the item 12 resolution to come: raise kRenderVersion with them).
enum class RenderUse { kSlot, kPanel };
inline constexpr int32_t kRenderSlotW = 96, kRenderSlotH = 128;    // the crew grid's mech slot (mw1bar.zrd)
inline constexpr int32_t kRenderPanelW = 160, kRenderPanelH = 128;  // the Mech Complex's chosen mech (mw1market.zrd)

// The picture's name: "mw1_rnd_" and the PM mech's name, lower case, then "_slot" or "_panel".
std::string render_image(int32_t pm_mech, RenderUse use);
// The PM mechs MW1 pictures (kMechKinds' stand-ins, each once, in their order).
std::vector<int32_t> render_mechs();

// One picture from a frame (0xAARRGGBB, w x h, top row first) with the mech on black: the mech's bounds (any channel
// over kRenderBlack) with a margin, widened to out_w:out_h round its middle; each pixel toned (65% toward MW1's amber
// by its brightness, dark brown in the shadows, 35% of its own colour kept) over a dark warm vignette, the area under
// each out pixel averaged. An empty image when the frame shows nothing.
inline constexpr int32_t kRenderBlack = 8;
images::Image graded_render(const std::string& name, const uint32_t* argb, int32_t w, int32_t h, int32_t out_w,
                            int32_t out_h);
// The capture's sanity check, before any picture is made of it: the mech alone on the lab's black. The frame's outer
// band (kRenderBorder of each side) at least kRenderBorderBlack unlit, and under kRenderLitMost of it lit at all; a
// frame of the lab's 2D (FIXES 14's bad first runs) is lit edge to edge.
inline constexpr float kRenderBorder = 0.04f, kRenderBorderBlack = 0.97f, kRenderLitMost = 0.6f;
bool frame_mech_alone(const uint32_t* argb, int32_t w, int32_t h);
// Every use's picture of `pm_mech` from its frame (empty when the frame shows nothing, or not the mech alone).
std::vector<images::Image> renders_from_frame(int32_t pm_mech, const uint32_t* argb, int32_t w, int32_t h);
// The same at kRenderHiScale, RGBA (no 565): the grade's own values.
struct HiRender {
  std::string name;
  int32_t w = 0, h = 0, canvas_w = 0, canvas_h = 0;
  std::vector<uint8_t> rgba;
};
HiRender graded_render_hi(const std::string& name, const uint32_t* argb, int32_t w, int32_t h, int32_t out_w, int32_t out_h);
std::vector<HiRender> renders_hi_from_frame(int32_t pm_mech, const uint32_t* argb, int32_t w, int32_t h);

// The stamp the pictures are made under: the version, then each mech's name.
std::string render_stamp(const std::vector<int32_t>& mechs);
// True when `write_dir` holds both archives with this stamp beside them (whatever frame they were made in).
bool renders_current(const std::string& write_dir, const std::vector<int32_t>& mechs);
// The frame's height the pictures were graded from (the stamp's "frame" line), 0 when it doesn't say. The 4x set's
// detail is that frame's: the pass runs again when a later frame is enough bigger (renders_outgrown).
int32_t renders_frame_height(const std::string& write_dir);
// From kRenderFrameEnough rows the 4x set has its detail (the mech about 1:1 in the 640x512 panel at 720 rows, with a
// margin); below it, a frame kRenderFrameGrowth times taller makes the pictures again.
inline constexpr int32_t kRenderFrameEnough = 1080;
inline constexpr float kRenderFrameGrowth = 1.25f;
bool renders_outgrown(int32_t made_height, int32_t frame_height);
// Both archives and their stamp written under `write_dir`; false on a failure (nothing half kept: the stamp goes last).
bool write_renders(const std::string& write_dir, const std::vector<images::Image>& made, const std::vector<HiRender>& hi,
                   const std::vector<int32_t>& mechs, int32_t frame_height = 0);
std::string hi_renders_path(const std::string& write_dir);  // the 4x archive's (for art::Store::add)

// Whether the archive is registered this run (the screens show the renders, else the amber icons).
bool renders_ready();
void set_renders_ready(bool ready);

}  // namespace mw1
