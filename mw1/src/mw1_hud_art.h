#pragma once
// Pirate's Moon's mission HUD pictures (src/mech3/hud.cpp fetches them by name: hud_*, and lab_toggle[lr]_* for its
// toggler) drawn from their shapes at the window's scale (FIXES.md item 12, MW1 only: the 2D at high resolution asks
// mw1_art's drawn art for them, MW3 and PM never do). PM ships each only at its canvas size. Most are small flat-
// coloured shapes: the arrows a fill with a one-pixel rim, the radar's blips and the waypoints a shape with PM's drop shadow
// (the shape again one pixel right, down, and both, in its dark colour, under it), the toggles and the weapon marks
// plain triangles. Each is described here as geometry in canvas pixels; at one device pixel a canvas pixel, sampled
// at the pixel's centre, it is PM's picture exactly (the tests hold it to PM's pixels); above, each device pixel
// takes the shape by its coverage. The line art, the heat bar, the compass frame and the lights are read from PM's
// pictures when drawn (read_from_pm; mw1_hud_art.cpp says how). Our own code: nothing of PM's is kept.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mw1::hud_art {

// The pictures drawn here, by PM's names.
const std::vector<std::string>& names();

// True for the pictures drawn from PM's own at run time (the line art traced, the gradients and the lights read): they
// need PM's libraries loaded (a mission's). The rest are geometry alone.
bool read_from_pm(const std::string& name);

// `name`'s canvas size (false when it isn't one of names()).
bool size_of(const std::string& name, int32_t* w, int32_t* h);

// `name` at `s` device pixels a canvas pixel: (w * s) x (h * s) RGBA, straight alpha, top row first; PM's clear
// (its black key) is alpha 0. Empty when `name` isn't one of names() or s < 1.
std::vector<uint8_t> draw(const std::string& name, int32_t s);

// FIXES.md item 82: the labels in PM's line art (the speed and fuel scales' numbers, the coolant's F and E, the
// compass tape's headings and N E S W) are a pixel font: as squares they are blocks at 4K. Above canvas size, with a
// painter set, each is drawn as text instead, where PM's was and in its colour and drop shadow; PM's glyph pixels left
// out. The painter (the program's: SDL_ttf with PM's HUD face, mw1_hud_labels.cpp) gives `text`'s coverage as w x h
// device pixels, one byte a pixel: its ink exactly h tall, as wide as the face makes it but at most w (narrowed to
// fit), against the box's left (align -1), centre (0) or right (1). Empty: that picture's labels stay PM's pixels.
// None set (the tests, mw1tests): every picture as before. At s 1 the pictures are always PM's own.
using LabelPainter = std::function<std::vector<uint8_t>(const std::string& text, int32_t w, int32_t h, int32_t align)>;
void set_label_painter(LabelPainter paint);

// FIXES.md item 132, tests only: true paints every layer as before the item, sampling every device pixel through the
// shape's std::function and laying each layer over the whole picture (seconds at 4K); false (the default) the fast
// painter. The pictures must be the same bytes either way.
void set_reference_painter(bool on);

// A picture's labels as found in PM's (canvas pixels: the ink's top and height, the box it may take): what draw()
// draws as text. Empty for a picture without labels, or when PM's isn't laid out as expected (its pixels are kept).
struct Label {
  std::string text;
  double x = 0, y = 0, w = 0, h = 0;
  int32_t align = 0;
};
std::vector<Label> labels_of(const std::string& name);

}  // namespace mw1::hud_art
