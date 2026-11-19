// The travel map's backdrops (src/mw1_map_image.h): every world inside its own House's territory, the Houses' worlds
// in their own colours on the image, the six images packed, and the same pixels every build. MW1_MAP_PREVIEW=DIR
// writes them as BMPs there to look at.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.h"
#include "mw1_images.h"
#include "mw1_map_image.h"
#include "mw1_marks.h"
#include "mw1_travel.h"
#include "mw1_travel_map.h"

namespace {
uint32_t fnv(const std::vector<uint16_t>& px) {
  uint32_t h = 2166136261u;
  for (uint16_t p : px) h = ((h ^ (p & 0xff)) * 16777619u), h = ((h ^ (p >> 8)) * 16777619u);
  return h;
}
void write_bmp(const std::string& path, const mw1::images::Image& im) {  // 24-bit, bottom-up
  const int32_t row = (im.width * 3 + 3) & ~3, size = 54 + row * im.height;
  std::string b(static_cast<size_t>(size), '\0');
  auto put = [&](size_t at, uint32_t v, int n) {
    for (int i = 0; i < n; ++i) b[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
  };
  b[0] = 'B', b[1] = 'M';
  put(2, static_cast<uint32_t>(size), 4), put(10, 54, 4), put(14, 40, 4), put(18, static_cast<uint32_t>(im.width), 4);
  put(22, static_cast<uint32_t>(im.height), 4), put(26, 1, 2), put(28, 24, 2);
  for (int32_t y = 0; y < im.height; ++y)
    for (int32_t x = 0; x < im.width; ++x) {
      const uint16_t p = im.pixels[static_cast<size_t>(y * im.width + x)];
      const size_t at = 54 + static_cast<size_t>((im.height - 1 - y) * row + x * 3);
      b[at] = static_cast<char>((p & 31) << 3), b[at + 1] = static_cast<char>(((p >> 5) & 63) << 2), b[at + 2] = static_cast<char>((p >> 11) << 3);
    }
  std::ofstream(path, std::ios::binary).write(b.data(), static_cast<std::streamsize>(b.size()));
}
}  // namespace

TEST(mw1_map_image_every_world_in_its_houses_territory) {
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    EXPECT(mw1::territory_at(p.x, p.y) == mw1::kPlanets[i].house);
  }
  EXPECT(mw1::territory_at(mw1::kMapLeft, mw1::kMapTop) == -1);  // the corner: space
  // Each House's territory one piece around its worlds: its worlds' places all lit in its own image, dimmed in another's.
  const mw1::images::Image all = mw1::backdrop(-1), kurita = mw1::backdrop(0);
  EXPECT(all.name == "mw1_map" && kurita.name == "mw1_map_kurita" && mw1::house_image(4) == "mw1_map_davion");
  EXPECT(all.width == mw1::kMapWidth && all.height == mw1::kMapHeight && all.pixels.size() == static_cast<size_t>(all.width * all.height));
  int32_t brighter = 0, darker = 0;
  auto luma = [](uint16_t p) { return (p >> 11) * 2 + ((p >> 5) & 63) + (p & 31) * 2; };
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    const size_t at = static_cast<size_t>((p.y - mw1::kMapTop + 2) * mw1::kMapWidth + (p.x - mw1::kMapLeft + 2));
    if (mw1::kPlanets[i].house != 0 && luma(kurita.pixels[at]) < luma(all.pixels[at])) ++darker;
    if (mw1::kPlanets[i].house == 0 && kurita.pixels[at] == all.pixels[at]) ++brighter;
  }
  std::printf("  Kurita's image: %d of 30 of its worlds as the plain one's, %d of 115 others dimmed\n", brighter, darker);
  EXPECT(brighter == 30 && darker > 100);
}

TEST(mw1_map_image_six_packed_the_same_every_time) {
  const std::vector<mw1::images::Image> v = mw1::backdrops();
  EXPECT(v.size() == 6);
  std::string why;
  EXPECT(!mw1::images::pack(v, &why).empty() && why.empty());
  EXPECT(fnv(mw1::backdrop(-1).pixels) == fnv(v[0].pixels) && fnv(mw1::backdrop(3).pixels) == fnv(v[4].pixels));
  std::printf("  the plain map's pixels %08x\n", fnv(v[0].pixels));
  if (const char* dir = std::getenv("MW1_MAP_PREVIEW"))
    for (const mw1::images::Image& im : v) write_bmp(std::string(dir) + "/" + im.name + ".bmp", im);
}

// No stray bits in the territories: every piece of a House's territory (and every hole of space inside one) holds a
// world, or is space reaching the map's edge. A piece with none is the generator's (a sliver between worlds' reaches).
TEST(mw1_map_image_no_slivers) {
  const int32_t w = mw1::kMapWidth, h = mw1::kMapHeight;
  std::vector<int32_t> label(static_cast<size_t>(w * h)), piece(static_cast<size_t>(w * h), -1);
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) label[static_cast<size_t>(y * w + x)] = mw1::territory_at(mw1::kMapLeft + x, mw1::kMapTop + y);
  std::vector<char> world(static_cast<size_t>(w * h), 0);
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    world[static_cast<size_t>((p.y - mw1::kMapTop) * w + p.x - mw1::kMapLeft)] = 1;
  }
  int32_t pieces = 0, stray = 0;
  for (int32_t s = 0; s < w * h; ++s) {
    if (piece[static_cast<size_t>(s)] >= 0) continue;
    std::vector<int32_t> todo = {s};
    piece[static_cast<size_t>(s)] = pieces;
    int32_t size = 0, x0 = w, y0 = h;
    bool has_world = false, edge = false;
    while (!todo.empty()) {
      const int32_t c = todo.back();
      todo.pop_back();
      ++size;
      const int32_t x = c % w, y = c / w;
      has_world = has_world || world[static_cast<size_t>(c)];
      edge = edge || x == 0 || y == 0 || x == w - 1 || y == h - 1;
      x0 = std::min(x0, x), y0 = std::min(y0, y);
      const int32_t next[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
      for (const auto& n : next) {
        if (n[0] < 0 || n[1] < 0 || n[0] >= w || n[1] >= h) continue;
        const int32_t k = n[1] * w + n[0];
        if (piece[static_cast<size_t>(k)] < 0 && label[static_cast<size_t>(k)] == label[static_cast<size_t>(c)])
          piece[static_cast<size_t>(k)] = pieces, todo.push_back(k);
      }
    }
    const bool ok = has_world || (label[static_cast<size_t>(s)] < 0 && edge);
    if (!ok) {
      ++stray;
      std::printf("  stray: House %d, %d pixels at screen (%d, %d)\n", label[static_cast<size_t>(s)], size, x0 + mw1::kMapLeft, y0 + mw1::kMapTop);
    }
    ++pieces;
  }
  std::printf("  %d pieces, %d stray\n", pieces, stray);
  EXPECT(stray == 0);
}

// FIXES.md item 12: the backdrop drawn again at the window's scale is the canvas's picture, finer. At scale 1 it is
// the canvas's own (the borders found again at each pixel's own place, which is the canvas's); at 5 (4K's) each
// canvas pixel's fine pixels average near its colour inside a territory, a border runs through fine pixels, not
// whole canvas pixels, and it is quick enough to make as the screen opens.
TEST(mw1_map_backdrop_at_the_windows_scale) {
  for (int32_t lit : {-1, 2}) {
    const mw1::images::Image canvas = mw1::backdrop(lit);
    const std::vector<uint8_t> one = mw1::backdrop_rgba(lit, 1);
    EXPECT(one.size() == static_cast<size_t>(mw1::kMapWidth) * mw1::kMapHeight * 4);
    int32_t differ = 0;
    for (size_t i = 0; i < canvas.pixels.size(); ++i) {
      const uint16_t p = mw1::images::rgb565(one[i * 4], one[i * 4 + 1], one[i * 4 + 2]);
      const int32_t dr = std::abs((p >> 11) - (canvas.pixels[i] >> 11)), dg = std::abs(((p >> 5) & 63) - ((canvas.pixels[i] >> 5) & 63)),
                    db = std::abs((p & 31) - (canvas.pixels[i] & 31));
      differ += std::max({dr, dg / 2, db}) > 1;
    }
    std::printf("  lit %d, scale 1: %d of %zu pixels differ\n", lit, differ, canvas.pixels.size());
    EXPECT(differ * 200 < static_cast<int32_t>(canvas.pixels.size()));  // under 0.5% (the stars' soft edges)
  }
  const int32_t s = 5;
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<uint8_t> big = mw1::backdrop_rgba(-1, s);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::printf("  scale %d: %dx%d in %.0f ms\n", s, mw1::kMapWidth * s, mw1::kMapHeight * s, ms);
  EXPECT(big.size() == static_cast<size_t>(mw1::kMapWidth * s) * mw1::kMapHeight * s * 4);
  EXPECT(ms < 1500);
  const mw1::images::Image canvas = mw1::backdrop(-1);
  int32_t inside = 0, near = 0, split = 0;
  for (int32_t cy = 1; cy + 1 < mw1::kMapHeight; ++cy)
    for (int32_t cx = 1; cx + 1 < mw1::kMapWidth; ++cx) {
      const int32_t h = mw1::territory_at(mw1::kMapLeft + cx, mw1::kMapTop + cy);
      bool same = h >= 0;
      for (int32_t d = -2; d <= 2 && same; ++d)
        same = mw1::territory_at(mw1::kMapLeft + cx + d, mw1::kMapTop + cy) == h && mw1::territory_at(mw1::kMapLeft + cx, mw1::kMapTop + cy + d) == h;
      int32_t sum[3] = {0, 0, 0}, lo = 255, hi = 0;
      for (int32_t y = cy * s; y < (cy + 1) * s; ++y)
        for (int32_t x = cx * s; x < (cx + 1) * s; ++x) {
          const uint8_t* p = &big[(static_cast<size_t>(y) * mw1::kMapWidth * s + x) * 4];
          for (int32_t i = 0; i < 3; ++i) sum[i] += p[i];
          lo = std::min<int32_t>(lo, p[0] + p[1] + p[2]), hi = std::max<int32_t>(hi, p[0] + p[1] + p[2]);
        }
      if (same) {  // a territory's inside, starless: the canvas's fill
        const uint16_t c = canvas.pixels[static_cast<size_t>(cy) * mw1::kMapWidth + cx];
        const int32_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
        ++inside;
        near += std::abs(sum[0] / (s * s) - r) <= 12 && std::abs(sum[1] / (s * s) - g) <= 12 && std::abs(sum[2] / (s * s) - b) <= 12;
      } else if (h >= 0 && hi - lo > 60) {
        ++split;  // a border inside one canvas pixel: finer than the canvas
      }
    }
  std::printf("  %d inside pixels, %d near the canvas's colour; %d border pixels split finer\n", inside, near, split);
  EXPECT(near * 100 >= inside * 98);
  EXPECT(split > 500);
}

// FIXES.md item 12: the travel map's marks as pictures (mw1_marks.h), drawn from one geometry at the canvas's size
// and the window's scale.
TEST(mw1_marks_named_drawn_and_scaled) {
  mw1::marks::Spec dot;  // a world's: the ring of 2, the square of 1, the middle pixel, its office's ring of 4
  dot.r = 255, dot.g = 85, dot.b = 255;
  dot.parts = {{mw1::marks::Shape::kRing, 2, 0, 0}, {mw1::marks::Shape::kSquare, 1, 0, 0},
               {mw1::marks::Shape::kLine, 0, 0, 0}, {mw1::marks::Shape::kRing, 4, 0, 0}};
  const std::string name = mw1::marks::image_name(dot);
  EXPECT(name == "mw1_mark_ff55ff_r2_s1_l0,0_r4");
  mw1::marks::Spec back;
  EXPECT(mw1::marks::parse(name, &back) && mw1::marks::image_name(back) == name && back.parts.size() == 4);
  EXPECT(!mw1::marks::is_mark("mw1_map") && !mw1::marks::is_mark("mw1_mark_ff55ff") && !mw1::marks::is_mark("mw1_mark_ff55ff_r2_q1") &&
         !mw1::marks::is_mark("mw1_mark_ff55ff_r02"));  // one spelling a picture
  int32_t w, h, ax, ay;
  mw1::marks::extent(dot, &w, &h, &ax, &ay);
  EXPECT(w == 11 && h == 11 && ax == 5 && ay == 5);
  // The canvas: the dot's middle 5x5 filled but its corners (a disc 5 across), the ring of 4 round it, nothing
  // between.
  const mw1::images::Image c = mw1::marks::canvas(dot);
  auto solid = [&](int32_t dx, int32_t dy) { return c.alpha[static_cast<size_t>((ay + dy) * w + ax + dx)] == 255; };
  int32_t filled = 0;
  for (int32_t dy = -1; dy <= 1; ++dy)
    for (int32_t dx = -1; dx <= 1; ++dx) filled += solid(dx, dy);
  EXPECT(filled == 9 && solid(2, 0) && solid(0, -2) && solid(2, 1) && !solid(2, 2) && !solid(3, 0) && solid(4, 0) && solid(0, 4));
  EXPECT(c.pixels[0] == mw1::images::rgb565(255, 85, 255));
  // A line: Bresenham's pixels from the anchor to its end, one a column.
  mw1::marks::Spec line;
  line.parts = {{mw1::marks::Shape::kLine, 0, -6, 3}};
  mw1::marks::extent(line, &w, &h, &ax, &ay);
  const mw1::images::Image l = mw1::marks::canvas(line);
  int32_t lit = 0;
  for (uint8_t a : l.alpha) lit += a == 255;
  EXPECT(lit == 7 && l.alpha[static_cast<size_t>(ay * w + ax)] == 255 && l.alpha[static_cast<size_t>((ay + 3) * w + ax - 6)] == 255);
  // At 5x: the ring's band anti-aliased (partial coverage on its edges), the middle solid, the gap clear.
  const int32_t s = 5;
  mw1::marks::extent(dot, &w, &h, &ax, &ay);
  const std::vector<uint8_t> big = mw1::marks::rgba(dot, s);
  EXPECT(big.size() == static_cast<size_t>(w * s) * (h * s) * 4);
  auto alpha = [&](float cx, float cy) {  // canvas units from the anchor's centre
    const int32_t x = static_cast<int32_t>((ax + 0.5f + cx) * s), y = static_cast<int32_t>((ay + 0.5f + cy) * s);
    return big[(static_cast<size_t>(y) * w * s + x) * 4 + 3];
  };
  int32_t partial = 0;
  for (size_t i = 3; i < big.size(); i += 4) partial += big[i] > 0 && big[i] < 255;
  EXPECT(alpha(0, 0) == 255 && alpha(1.2f, 0) == 255 && alpha(3.0f, 0) == 0 && alpha(4.0f, 0) == 255 && alpha(0, -4.0f) == 255);
  EXPECT(partial > 40);
}
