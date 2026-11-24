#pragma once
// The travel map's backdrop (research/travel-map.md, step 2): drawn by our code from the worlds' data (kPlanets), no
// MW1 pixels, at build time (mw1imgpack) into MW1's image archive (mw1_images.h). A faint starfield, and each House's
// territory: every pixel near a world takes that world's House (the nearest within kTerritoryReach; space shut in
// by territory, too, so there are no holes), filled faintly with a thin outline where it meets another House or space. One plain, and one per House with the others dimmed
// (FILTER's one House). The worlds themselves, the markers and the names are drawn live by the screen.
//
// The look is provisional, each a constant until the owner answers (research/travel-map.md, Q1 to Q3).
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_images.h"
#include "mw1_travel_map.h"

namespace mw1::map_image {

// Q1, the colours: MW1's own, the fills of its map (EGA indices 9, 6, 15, 12, 13 in the default palette), by House
// in kHouses' order: Kurita light blue, Steiner brown, Marik white, Liao light red, Davion light magenta. Or
// BattleTech's usual: Kurita red, Steiner blue, Marik purple, Liao green, Davion gold.
struct Rgb {
  uint8_t r, g, b;
};
inline constexpr bool kMw1Colours = true;
inline constexpr Rgb kMw1HouseColours[kHouseCount] = {{85, 85, 255}, {170, 85, 0}, {255, 255, 255}, {255, 85, 85}, {255, 85, 255}};
inline constexpr Rgb kCanonHouseColours[kHouseCount] = {{220, 40, 40}, {70, 110, 230}, {160, 80, 200}, {60, 170, 80}, {230, 190, 60}};
inline constexpr const Rgb* kHouseColours = kMw1Colours ? kMw1HouseColours : kCanonHouseColours;

// Q2, the territories: filled (this much of the House's colour over the stars), with a thin outline; 0 fill: outlines
// only. Another House's dimmed to this much when one is lit.
inline constexpr bool kTerritories = true;
inline constexpr int32_t kFillPercent = 28, kOutlinePercent = 75, kDimmedPercent = 30;
inline constexpr int32_t kTerritoryReach = 30;  // pixels from a world on our screen

// Q3, the backdrop: a faint starfield (seeded, the same every build), or plain black.
inline constexpr bool kStarfield = true;
inline constexpr int32_t kStars = 700;
inline constexpr uint32_t kStarSeed = 0x4D573130u;

// The images: the plain one and each House's, kMapWidth x kMapHeight, drawn at (kMapLeft, kMapTop).
inline constexpr const char* kMapImage = "mw1_map";
std::string house_image(int32_t house);  // "mw1_map_kurita", ...

// The House whose territory a point of the map area (screen pixels) is in: the nearest world's within the reach,
// -1 space.
int32_t territory_at(int32_t x, int32_t y);

// The backdrop: `lit` -1 every House as itself, else that House's territory bright and the others dimmed.
images::Image backdrop(int32_t lit = -1);
// All six, for the archive.
std::vector<images::Image> backdrops();

// The backdrop at `scale` times the canvas's size, RGBA (opaque), for the 2D drawn at the window's resolution
// (FIXES.md item 12, research/hires-2d.md: the travel map). The same picture as backdrop(lit): each fine pixel's House
// is the canvas pixel's, found again from the worlds at the fine pixel's own place where the canvas pixel is on a
// border (so a border is a smooth curve, not the canvas's steps); the outline about a canvas pixel wide; each star a
// soft dot where the canvas has its pixel.
std::vector<uint8_t> backdrop_rgba(int32_t lit, int32_t scale);

}  // namespace mw1::map_image

namespace mw1 {
using namespace map_image;
}  // namespace mw1
