#pragma once
// Pirate's Moon's mission HUD pictures (src/mech3/hud.cpp fetches them by name: hud_*, and lab_toggle[lr]_* for its
// toggler) drawn from their shapes at the window's scale (FIXES.md item 12, MW1 only: the 2D at high resolution asks
// mw1_art's drawn art for them, MW3 and PM never do). PM ships each only at its canvas size, small flat-coloured
// shapes: the arrows a fill with a one-pixel rim, the radar's blips and the waypoints a shape with PM's drop shadow
// (the shape again one pixel right, down, and both, in its dark colour, under it), the toggles and the weapon marks
// plain triangles. Each is described here as geometry in canvas pixels; at one device pixel a canvas pixel, sampled
// at the pixel's centre, it is PM's picture exactly (the tests hold it to PM's pixels); above, each device pixel
// takes the shape by its coverage. Our own code: nothing of PM's is kept.
#include <cstdint>
#include <string>
#include <vector>

namespace mw1::hud_art {

// The pictures drawn here, by PM's names.
const std::vector<std::string>& names();

// `name`'s canvas size (false when it isn't one of names()).
bool size_of(const std::string& name, int32_t* w, int32_t* h);

// `name` at `s` device pixels a canvas pixel: (w * s) x (h * s) RGBA, straight alpha, top row first; PM's clear
// (its black key) is alpha 0. Empty when `name` isn't one of names() or s < 1.
std::vector<uint8_t> draw(const std::string& name, int32_t s);

}  // namespace mw1::hud_art
