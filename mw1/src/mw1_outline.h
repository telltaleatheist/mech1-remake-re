#pragma once
// Pirate's Moon's mech outlines redrawn as lines (FIXES.md item 12, research/hires-2d.md B2 item 1; the owner 2026-11-18:
// the outlines PM shows along the lab's top and on the MFB's salvage screen, drawn at the window's scale). PM ships
// them at 41x50 only (rimage.zbd's icon_<mech>_<colour>), clean line art in three values: a bright line one pixel
// wide, a dark fill, black clear. Each icon is turned once into its lines: the line pixels' centres joined into
// chains, a chain's true corners kept (a turn with runs of two or more on both sides), the pixel stairs between them
// fitted straight (Douglas-Peucker at kFitTolerance: a one-pixel notch, 1.0 off the line, stays). At any scale the
// lines are stroked one icon pixel wide (PM's weight) and anti-aliased by their distance; the fill is PM's fill
// pixels grown to the lines' centres, under them. Our own code; nothing of PM's is kept but the pixels it reads.
#include <cstdint>
#include <vector>

#include "mw1_images.h"

namespace mw1::outline {

inline constexpr double kFitTolerance = 0.75;  // in icon pixels: a 1:1 stair's corners are 0.71 off its line
inline constexpr int32_t kClearMost = 16;      // brightness (0..255) at or under it is PM's clear (a cockpit's 0,0,8)

struct Segment {
  float x0, y0, x1, y1;  // icon pixel centres
};
struct Outline {
  int32_t width = 0, height = 0;
  std::vector<Segment> lines;
  std::vector<std::pair<int32_t, int32_t>> dots;  // a line pixel with no neighbour
  std::vector<uint8_t> fill;                      // per pixel: 1 PM's fill
  float fill_level = 0.0f;                        // the fill's brightness over the line's (PM's 57 / 215)
  bool empty() const { return width <= 0 || height <= 0 || (lines.empty() && dots.empty()); }
};

// The outline of a 565 picture (w x h, PM's black clear). The line is the brightest value's pixels (over half the
// brightest's brightness), the fill the rest that isn't clear.
Outline vectorise(const std::vector<uint16_t>& pixels, int32_t w, int32_t h);

// `o` drawn in `colour` (the line's; the fill at fill_level of it) `scale` canvas pixels an icon pixel, its top-left
// min(w * scale, canvas_w) x min(h * scale, canvas_h) part centred on a clear canvas_w x canvas_h (as the canvas
// copies are placed), at `s` device pixels a canvas pixel: (canvas_w * s) x (canvas_h * s) RGBA, straight alpha.
struct Colour {
  uint8_t r, g, b;
};
std::vector<uint8_t> draw(const Outline& o, Colour colour, int32_t scale, int32_t canvas_w, int32_t canvas_h, int32_t s);
// The same fitted into a box_w x box_h canvas box (its shape kept, centred) at any size, its lines at least
// `min_line` device pixels wide (a small outline still reads: the market's rows).
std::vector<uint8_t> draw_fit(const Outline& o, Colour colour, int32_t box_w, int32_t box_h, int32_t s, double min_line);

}  // namespace mw1::outline
