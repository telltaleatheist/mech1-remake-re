#pragma once
// Ours: mw1 (engine, ours), MW1: Critical Hit's FIXES.md item 89. A recorded 2D line (hires_lists.h's kLine: a line,
// a line list, a circle) drawn at the frame's scale, anti-aliased: what the canvas drew a pixel wide, as wide as a
// canvas pixel at that scale, its edges blended over a frame pixel. Pure (no SDL): hires_draw.cpp makes its texture
// from it; the tests read it.
#include <cstdint>
#include <vector>

#include "platform/hires_lists.h"

namespace platform::hires {

// `c`'s rect at sx x sy frame pixels a canvas pixel: w x h (each the rect's side scaled, rounded up), RGBA
// premultiplied, in `rgba`'s colour (0xRRGGBB). Empty when `c` isn't a line or has no points.
std::vector<uint8_t> line_rgba(const Command& c, float sx, float sy, uint32_t rgb, int32_t* w, int32_t* h);
// The canvas pixel's colour as 0xRRGGBB.
uint32_t canvas_rgb(uint16_t pixel, bool rgb565);

}  // namespace platform::hires
