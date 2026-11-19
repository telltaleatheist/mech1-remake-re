#include "mw1_map_image.h"

#include <algorithm>
#include <cctype>
#include <cmath>

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
    // No holes: space shut in by territory (a gap between worlds' reaches, which drew as a stray outlined sliver)
    // goes to the nearest world's House. Space is what the map's edge reaches.
    std::vector<char> open(m.size(), 0);
    std::vector<int32_t> todo;
    for (int32_t y = 0; y < kMapHeight; ++y)
      for (int32_t x = 0; x < kMapWidth; ++x)
        if ((x == 0 || y == 0 || x == kMapWidth - 1 || y == kMapHeight - 1) && m[static_cast<size_t>(y * kMapWidth + x)] < 0)
          open[static_cast<size_t>(y * kMapWidth + x)] = 1, todo.push_back(y * kMapWidth + x);
    while (!todo.empty()) {
      const int32_t c = todo.back(), x = c % kMapWidth, y = c / kMapWidth;
      todo.pop_back();
      const int32_t next[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
      for (const auto& q : next) {
        if (q[0] < 0 || q[1] < 0 || q[0] >= kMapWidth || q[1] >= kMapHeight) continue;
        const size_t k = static_cast<size_t>(q[1] * kMapWidth + q[0]);
        if (!open[k] && m[k] < 0) open[k] = 1, todo.push_back(static_cast<int32_t>(k));
      }
    }
    for (int32_t y = 0; y < kMapHeight; ++y)
      for (int32_t x = 0; x < kMapWidth; ++x) {
        const size_t k = static_cast<size_t>(y * kMapWidth + x);
        if (m[k] >= 0 || open[k]) continue;
        int32_t best = 0, best_d = -1;
        for (int32_t i = 0; i < kPlanetCount; ++i) {
          const int32_t dx = at[i].x - (kMapLeft + x), dy = at[i].y - (kMapTop + y), d = dx * dx + dy * dy;
          if (best_d < 0 || d < best_d) best = i, best_d = d;
        }
        m[k] = static_cast<int8_t>(kPlanets[best].house);
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

std::vector<uint8_t> backdrop_rgba(int32_t lit, int32_t scale) {
  const int32_t sc = std::max(1, scale), fw = kMapWidth * sc, fh = kMapHeight * sc;
  const float inv = 1.0f / static_cast<float>(sc);
  // Each fine pixel's House: the canvas pixel's inside a territory, found again on a border.
  std::vector<int8_t> fine(static_cast<size_t>(fw) * fh, -1);
  Point at[kPlanetCount];
  for (int32_t i = 0; i < kPlanetCount; ++i) at[i] = on_screen(i);
  const float reach2 = static_cast<float>(kTerritoryReach * kTerritoryReach);
  std::vector<int32_t> near;
  for (int32_t cy = 0; cy < kMapHeight; ++cy)
    for (int32_t cx = 0; cx < kMapWidth; ++cx) {
      const int32_t h = house_at(cx, cy);
      bool border = false, by_space = false;
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t n = house_at(cx + dx, cy + dy);
          border |= n != h, by_space |= n < 0;
        }
      if (!border) {
        for (int32_t y = cy * sc; y < (cy + 1) * sc; ++y)
          std::fill_n(fine.begin() + static_cast<ptrdiff_t>(y) * fw + cx * sc, sc, static_cast<int8_t>(h));
        continue;
      }
      near.clear();  // the worlds that can own a point of this pixel
      for (int32_t i = 0; i < kPlanetCount; ++i) {
        const int32_t dx = at[i].x - (kMapLeft + cx), dy = at[i].y - (kMapTop + cy);
        if (dx * dx + dy * dy <= (kTerritoryReach + 2) * (kTerritoryReach + 2)) near.push_back(i);
      }
      for (int32_t y = cy * sc; y < (cy + 1) * sc; ++y)
        for (int32_t x = cx * sc; x < (cx + 1) * sc; ++x) {
          const float px = kMapLeft + (x + 0.5f) * inv - 0.5f, py = kMapTop + (y + 0.5f) * inv - 0.5f;
          int32_t best = -1;
          float best_d = reach2 + 1e-3f;
          for (int32_t i : near) {
            const float dx = at[i].x - px, dy = at[i].y - py, d = dx * dx + dy * dy;
            if (d < best_d) best = i, best_d = d;
          }
          // Beyond every reach: space where the canvas pixel touches space, else a hole the canvas filled.
          const int32_t got = best >= 0 ? kPlanets[best].house : (by_space || h < 0 ? -1 : h);
          fine[static_cast<size_t>(y) * fw + x] = static_cast<int8_t>(got);
        }
    }
  auto fine_at = [&](int32_t x, int32_t y) {
    return x < 0 || y < 0 || x >= fw || y >= fh ? int8_t{-1} : fine[static_cast<size_t>(y) * fw + x];
  };
  std::vector<float> rgb(static_cast<size_t>(fw) * fh * 3, 0.0f);
  if (kStarfield) {  // the canvas's stars, each a soft dot about a canvas pixel across
    uint32_t s = kStarSeed;
    auto next = [&] { return s = s * 1664525u + 1013904223u, s >> 8; };
    const float r = 0.55f * sc;
    for (int32_t i = 0; i < kStars; ++i) {
      const int32_t x = static_cast<int32_t>(next() % kMapWidth), y = static_cast<int32_t>(next() % kMapHeight);
      const float v = static_cast<float>(40 + next() % 70);
      const float cxf = (x + 0.5f) * sc, cyf = (y + 0.5f) * sc;
      for (int32_t fy = static_cast<int32_t>(cyf - r - 1); fy <= static_cast<int32_t>(cyf + r + 1); ++fy)
        for (int32_t fx = static_cast<int32_t>(cxf - r - 1); fx <= static_cast<int32_t>(cxf + r + 1); ++fx) {
          if (fx < 0 || fy < 0 || fx >= fw || fy >= fh) continue;
          const float d = std::sqrt((fx + 0.5f - cxf) * (fx + 0.5f - cxf) + (fy + 0.5f - cyf) * (fy + 0.5f - cyf));
          const float a = std::clamp(r + 0.5f - d, 0.0f, 1.0f);
          float* p = &rgb[(static_cast<size_t>(fy) * fw + fx) * 3];
          p[0] = std::max(p[0], v * a), p[1] = std::max(p[1], v * a), p[2] = std::max(p[2], std::min(255.0f, v + 20) * a);
        }
    }
  }
  const int32_t t = std::max(1, (sc + 1) / 2);  // the outline's half-width, fine pixels: a canvas pixel in all
  std::vector<uint8_t> out(static_cast<size_t>(fw) * fh * 4, 255);
  for (int32_t y = 0; y < fh; ++y)
    for (int32_t x = 0; x < fw; ++x) {
      float* p = &rgb[(static_cast<size_t>(y) * fw + x) * 3];
      const int8_t h = fine[static_cast<size_t>(y) * fw + x];
      if (kTerritories && h >= 0) {
        bool edge = false;
        for (int32_t k = 1; k <= t && !edge; ++k)
          edge = fine_at(x + k, y) != h || fine_at(x - k, y) != h || fine_at(x, y + k) != h || fine_at(x, y - k) != h;
        const int32_t dim = lit >= 0 && h != lit ? kDimmedPercent : 100;
        const Rgb c = kHouseColours[h];
        const float pc = (edge ? kOutlinePercent : kFillPercent) * dim / 10000.0f;
        const float base = edge ? 0.0f : 1.0f;  // an outline over black, a fill over the stars
        p[0] = p[0] * base * (1 - pc) + c.r * pc, p[1] = p[1] * base * (1 - pc) + c.g * pc;
        p[2] = p[2] * base * (1 - pc) + c.b * pc;
      }
      uint8_t* o = &out[(static_cast<size_t>(y) * fw + x) * 4];
      for (int32_t i = 0; i < 3; ++i) o[i] = static_cast<uint8_t>(std::clamp(p[i] + 0.5f, 0.0f, 255.0f));
    }
  return out;
}

std::vector<images::Image> backdrops() {
  std::vector<images::Image> v = {backdrop(-1)};
  for (int32_t h = 0; h < kHouseCount; ++h) v.push_back(backdrop(h));
  return v;
}

}  // namespace mw1::map_image
