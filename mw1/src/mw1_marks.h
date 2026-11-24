#pragma once
// The travel map's marks as pictures (FIXES.md item 12, the owner 2026-11-18: "the right way, no shortcuts"): each world's
// dot, its office's ring and its capital's square, the rings round here, the chosen world and the one under the
// mouse, the story's square, the jump's line. They were ztwod's rings, lines and polylines, drawn straight into the
// canvas, so at the window's resolution they stayed the canvas's pixels magnified beside the smooth backdrop. Now
// they are small named images drawn from the same geometry twice: at the canvas's size for the canvas (565 and an
// alpha plane, through MW1's image provider), and at the window's scale for the 2D's high-resolution draw (RGBA,
// anti-aliased, through mw1::art's Store). The marks that share a place and a colour (a world's dot, its ring, its
// square) are one picture: the high-resolution lists hold at most 512 draws a screen, and a shape each was more.
// The name says the whole picture, so each is made once and kept by name; moving the mouse only moves pictures.
//
// The geometry, in canvas pixels, a pixel's centre at its + 0.5, round the anchor pixel: a ring of radius r is the
// band within half a pixel of the circle of radius r round the anchor's centre (r 0, a dot); a square of half-size d
// the band a pixel wide inside the box d pixels out from the anchor pixel's edges; a line a pixel wide from the
// anchor's centre to (dx, dy) pixels on (the canvas: Bresenham's pixels); of no length, the anchor's pixel whole. A
// box (FIXES.md item 87: the travel map's names' backing) is w x h pixels filled from the anchor pixel's
// top left, at its own opacity.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1::marks {

enum class Shape : uint8_t { kRing, kSquare, kLine, kBox };

struct Part {
  Shape shape = Shape::kRing;
  int32_t size = 0;        // a ring's radius, a square's half-size
  int32_t dx = 0, dy = 0;  // a line's end from the anchor; a box's width and height
  uint8_t opacity = 255;   // a box's (the other shapes are solid)
};

struct Spec {
  std::vector<Part> parts;  // drawn in order, one colour
  uint8_t r = 255, g = 255, b = 255;
};

// "mw1_mark_<rrggbb>_<part>[_<part>...]", a part "r<radius>", "s<half-size>", "l<dx>,<dy>" or "b<w>x<h>o<opacity>".
std::string image_name(const Spec& s);
bool parse(const std::string& name, Spec* s);  // false: not a mark's name
bool is_mark(const char* name);

// The image's size, and where in it the anchor pixel is: the image goes at (anchor x - ax, anchor y - ay).
void extent(const Spec& s, int32_t* w, int32_t* h, int32_t* ax, int32_t* ay);

// The canvas's: 565 in the colour where a part covers a pixel's centre (a line's: Bresenham's), alpha 255 there (a
// box's: its opacity), 0 elsewhere.
images::Image canvas(const Spec& s);
// The window's: `scale` times the canvas's size, RGBA with straight alpha, each pixel's coverage (4x4 samples; a
// sample in a box counts its opacity).
std::vector<uint8_t> rgba(const Spec& s, int32_t scale);

// The canvas image as an engine texture with its alpha plane (zimg::Texture*, owned as a library's: free_texture).
void* canvas_texture(const char* name);

}  // namespace mw1::marks
