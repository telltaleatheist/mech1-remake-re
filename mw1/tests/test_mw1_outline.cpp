// PM's mech outlines redrawn as lines (research/hires-2d.md B2 item 1, mw1_outline.h), on synthetic icons in PM's
// three values: a box keeps its square corners, a pixel stair becomes one straight line, a one-pixel notch stays,
// the stroke is one icon pixel wide at any scale, the colours are the look's, PM's near-black is clear, and the
// drawing sits where the canvas copy puts it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.h"
#include "mw1_images.h"
#include "mw1_outline.h"

namespace O = mw1::outline;

namespace {
const uint16_t kLine = mw1::images::rgb565(255, 215, 0), kFill = mw1::images::rgb565(57, 48, 0);

struct Icon {
  int32_t w, h;
  std::vector<uint16_t> px;
  Icon(int32_t w_, int32_t h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_, 0) {}
  void set(int32_t x, int32_t y, uint16_t c) { px[static_cast<size_t>(y) * w + x] = c; }
};

uint8_t alpha_at(const std::vector<uint8_t>& rgba, int32_t W, int32_t x, int32_t y) {
  return rgba[(static_cast<size_t>(y) * W + x) * 4 + 3];
}
}  // namespace

TEST(mw1_outline_a_box_keeps_its_corners) {
  Icon i(12, 12);  // a 10x10 ring of line at (1,1)-(10,10), filled inside
  for (int32_t y = 1; y <= 10; ++y)
    for (int32_t x = 1; x <= 10; ++x) i.set(x, y, (x == 1 || y == 1 || x == 10 || y == 10) ? kLine : kFill);
  const O::Outline o = O::vectorise(i.px, i.w, i.h);
  EXPECT(o.lines.size() == 4 && o.dots.empty());
  EXPECT(std::fabs(o.fill_level - 48.0f / 215.0f) < 0.03f);  // brightness: max channel (57 is red; 565 rounds)
  const int32_t s = 8;
  const std::vector<uint8_t> d = O::draw(o, {255, 176, 64}, 1, 12, 12, s);
  EXPECT(d.size() == static_cast<size_t>(12 * s) * (12 * s) * 4);
  // The corner pixel's outer corner solid (its edge anti-aliased): square, not rounded.
  EXPECT(alpha_at(d, 12 * s, 1 * s, 1 * s) > 200 && alpha_at(d, 12 * s, 11 * s - 1, 11 * s - 1) > 200);
  // Nothing outside the ring (but the edge's last fraction); the line amber; the middle PM's fill.
  EXPECT(alpha_at(d, 12 * s, 1 * s - 1, 1 * s - 1) < 50 && alpha_at(d, 12 * s, 11 * s + 1, 5 * s) == 0);
  const size_t line = (static_cast<size_t>(5 * s + s / 2) * 12 * s + 1 * s + s / 2) * 4;
  EXPECT(d[line] == 255 && d[line + 1] == 176 && d[line + 2] == 64);
  const size_t mid = (static_cast<size_t>(6 * s) * 12 * s + 6 * s) * 4;
  EXPECT(d[mid + 3] == 255 && std::abs(d[mid] - static_cast<int>(255 * o.fill_level)) <= 1);
}

TEST(mw1_outline_a_stair_becomes_a_line) {
  Icon a(12, 12), b(12, 12);
  for (int32_t k = 0; k < 10; ++k) a.set(k + 1, k + 1, kLine);  // corner to corner
  for (int32_t k = 0; k < 6; ++k) b.set(k + 1, k + 1, kLine), b.set(k + 2, k + 1, kLine);  // side to side
  const O::Outline oa = O::vectorise(a.px, a.w, a.h), ob = O::vectorise(b.px, b.w, b.h);
  EXPECT(oa.lines.size() == 1 && ob.lines.size() == 1);
  // A long line with a one-pixel notch up in its middle: kept (1.0 off the line, over kFitTolerance).
  Icon n(24, 8);
  for (int32_t x = 1; x <= 22; ++x) n.set(x, x == 12 ? 3 : 4, kLine);
  const O::Outline on = O::vectorise(n.px, n.w, n.h);
  bool notch = false;
  for (const O::Segment& g : on.lines) notch = notch || (g.x0 == 12 && g.y0 == 3) || (g.x1 == 12 && g.y1 == 3);
  EXPECT(notch);
}

TEST(mw1_outline_the_stroke_is_one_icon_pixel) {
  Icon i(24, 8);
  for (int32_t x = 2; x <= 21; ++x) i.set(x, 4, kLine);
  const O::Outline o = O::vectorise(i.px, i.w, i.h);
  for (int32_t s : {3, 5, 10}) {
    const std::vector<uint8_t> d = O::draw(o, {255, 176, 64}, 2, 48, 16, s);  // 2 canvas pixels an icon pixel
    int32_t rows = 0;
    for (int32_t y = 0; y < 16 * s; ++y) rows += alpha_at(d, 48 * s, 24 * s, y) > 127;
    EXPECT(std::abs(rows - 2 * s) <= 1);
  }
}

TEST(mw1_outline_clear_and_placement) {
  Icon i(8, 10);
  for (int32_t y = 0; y < 10; ++y) i.set(0, y, kLine);
  i.set(4, 4, mw1::images::rgb565(0, 0, 8));  // PM's cockpit black: clear
  const O::Outline o = O::vectorise(i.px, i.w, i.h);
  EXPECT(o.lines.size() == 1 && std::count(o.fill.begin(), o.fill.end(), 1) == 0);
  // Placed as the canvas copy: 8x10 at 2 on a 96x128, centred (40, 54); at s = 3 the line's left edge at 40 * 3.
  const std::vector<uint8_t> d = O::draw(o, {255, 176, 64}, 2, 96, 128, 3);
  const int32_t W = 96 * 3, y = (54 + 10) * 3;
  EXPECT(alpha_at(d, W, 40 * 3, y) > 200 && alpha_at(d, W, 40 * 3 - 1, y) == 0 && alpha_at(d, W, 42 * 3 + 1, y) == 0);
  bool clear_out = true;
  for (int32_t yy = 0; yy < 128 * 3; ++yy)
    for (int32_t x = 0; x < W; ++x)
      if (x < 40 * 3 || x >= 56 * 3 || yy < 54 * 3 || yy >= 74 * 3) clear_out = clear_out && alpha_at(d, W, x, yy) == 0;
  EXPECT(clear_out);
  EXPECT(O::vectorise(std::vector<uint16_t>(80, 0), 8, 10).empty());  // all clear: nothing
}
