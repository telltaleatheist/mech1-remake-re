#include "mw1_map_image.h"

#include <algorithm>
#include <cctype>

namespace mw1::map_image {
namespace {

// The nearest world's House for every pixel of the map area, -1 beyond the reach (computed once).
const std::vector<int8_t>& territories() {
  static const std::vector<int8_t> t = [] {
    std::vector<int8_t> m(static_cast<size_t>(kMapWidth) * kMapHeight, -1);
    Point at[kPlanetCount];
    for (int32_t i = 0; i < kPlanetCount; ++i) at[i] = on_screen(i);
    const int32_t reach2 = kTerritoryReach * kTerritoryReach;
    for (int32_t y = 0; y < kMapHeight; ++y)
      for (int32_t x = 0; x < kMapWidth; ++x) {
        int32_t best = -1, best_d = reach2 + 1;
        for (int32_t i = 0; i < kPlanetCount; ++i) {
          const int32_t dx = at[i].x - (kMapLeft + x), dy = at[i].y - (kMapTop + y), d = dx * dx + dy * dy;
          if (d < best_d) best = i, best_d = d;
        }
        if (best >= 0) m[static_cast<size_t>(y * kMapWidth + x)] = static_cast<int8_t>(kPlanets[best].house);
      }
    return m;
  }();
  return t;
}

int32_t house_at(int32_t x, int32_t y) {  // map-area pixels
  if (x < 0 || y < 0 || x >= kMapWidth || y >= kMapHeight) return -1;
  return territories()[static_cast<size_t>(y * kMapWidth + x)];
}

Rgb mix(Rgb under, Rgb over, int32_t percent) {
  auto m = [&](uint8_t a, uint8_t b) { return static_cast<uint8_t>((a * (100 - percent) + b * percent) / 100); };
  return {m(under.r, over.r), m(under.g, over.g), m(under.b, over.b)};
}

}  // namespace

std::string house_image(int32_t house) {
  std::string n = kHouses[house].name;
  for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return std::string(kMapImage) + "_" + n;
}

int32_t territory_at(int32_t x, int32_t y) { return house_at(x - kMapLeft, y - kMapTop); }

images::Image backdrop(int32_t lit) {
  images::Image im;
  im.name = lit < 0 ? kMapImage : house_image(lit);
  im.width = kMapWidth, im.height = kMapHeight;
  std::vector<Rgb> px(static_cast<size_t>(kMapWidth) * kMapHeight, Rgb{0, 0, 0});
  if (kStarfield) {
    uint32_t s = kStarSeed;
    auto next = [&] { return s = s * 1664525u + 1013904223u, s >> 8; };
    for (int32_t i = 0; i < kStars; ++i) {
      const int32_t x = static_cast<int32_t>(next() % kMapWidth), y = static_cast<int32_t>(next() % kMapHeight);
      const uint8_t v = static_cast<uint8_t>(40 + next() % 70);  // faint
      px[static_cast<size_t>(y * kMapWidth + x)] = {v, v, static_cast<uint8_t>(std::min(255, v + 20))};
    }
  }
  if (kTerritories)
    for (int32_t y = 0; y < kMapHeight; ++y)
      for (int32_t x = 0; x < kMapWidth; ++x) {
        const int32_t h = house_at(x, y);
        if (h < 0) continue;
        const int32_t dim = lit >= 0 && h != lit ? kDimmedPercent : 100;
        const Rgb colour = kHouseColours[h];
        Rgb& p = px[static_cast<size_t>(y * kMapWidth + x)];
        // The outline: where the next pixel right or down (or left or up) is another House's or space.
        const bool edge = house_at(x + 1, y) != h || house_at(x, y + 1) != h || house_at(x - 1, y) != h || house_at(x, y - 1) != h;
        p = edge ? mix(Rgb{0, 0, 0}, colour, kOutlinePercent * dim / 100) : mix(p, colour, kFillPercent * dim / 100);
      }
  im.pixels.resize(px.size());
  for (size_t i = 0; i < px.size(); ++i) im.pixels[i] = images::rgb565(px[i].r, px[i].g, px[i].b);
  return im;
}

std::vector<images::Image> backdrops() {
  std::vector<images::Image> v = {backdrop(-1)};
  for (int32_t h = 0; h < kHouseCount; ++h) v.push_back(backdrop(h));
  return v;
}

}  // namespace mw1::map_image
