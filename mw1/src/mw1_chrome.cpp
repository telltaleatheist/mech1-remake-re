#include "mw1_chrome.h"

#include <algorithm>
#include <cmath>

namespace mw1::chrome {
namespace {

struct Rgba {
  int32_t r = 0, g = 0, b = 0, a = 0;
};

// A canvas of RGBA, then the archive's 565 and alpha.
struct Canvas {
  int32_t w, h;
  std::vector<Rgba> px;
  Canvas(int32_t w_, int32_t h_) : w(w_), h(h_), px(static_cast<size_t>(w_ * h_)) {}
  Rgba& at(int32_t x, int32_t y) { return px[static_cast<size_t>(y * w + x)]; }
  bool in(int32_t x, int32_t y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  void set(int32_t x, int32_t y, Rgba c) {
    if (in(x, y)) at(x, y) = c;
  }
  // `c` over what's there, by its alpha.
  void over(int32_t x, int32_t y, Rgba c) {
    if (!in(x, y)) return;
    Rgba& d = at(x, y);
    const int32_t a = c.a, ia = 255 - a, oa = a + d.a * ia / 255;
    if (oa == 0) return;
    d.r = (c.r * a + d.r * d.a * ia / 255) / oa, d.g = (c.g * a + d.g * d.a * ia / 255) / oa,
    d.b = (c.b * a + d.b * d.a * ia / 255) / oa, d.a = oa;
  }
  images::Image image(const std::string& name) const {
    images::Image im;
    im.name = name, im.width = w, im.height = h;
    im.pixels.resize(px.size()), im.alpha.resize(px.size());
    for (size_t i = 0; i < px.size(); ++i) {
      const Rgba& c = px[i];
      auto cl = [](int32_t v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); };
      im.pixels[i] = images::rgb565(cl(c.r), cl(c.g), cl(c.b));
      im.alpha[i] = cl(c.a);
    }
    return im;
  }
};

Rgba grey(int32_t v, int32_t a = 255) { return {v, v, v + v / 16, a}; }  // gunmetal: a touch of blue

// The distance in from a chamfered box's edge (corners cut `k` at 45 degrees), and which way that edge faces: lit
// (the top, the left, the top-left cut) or not. Negative outside.
struct Edge {
  int32_t d;
  bool lit;
};
Edge edge_of(int32_t x, int32_t y, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t k) {
  const int32_t dl = x - x0, dt = y - y0, dr = x1 - 1 - x, db = y1 - 1 - y;
  Edge e = {dl, true};
  auto take = [&](int32_t d, bool lit) {
    if (d < e.d) e = {d, lit};
  };
  take(dt, true), take(dr, false), take(db, false);
  if (k > 0) {
    // The cuts: a pixel's distance from each diagonal, in pixels (x 0.7), the top-left and bottom-right by their
    // sides' light, the other two half and half (the top-right lit like the top, the bottom-left dark like the foot).
    take((dl + dt - k) * 7 / 10, true), take((dr + db - k) * 7 / 10, false);
    take((dr + dt - k) * 7 / 10, true), take((dl + db - k) * 7 / 10, false);
  }
  return e;
}

// A steel plate filling (x0, y0)-(x1, y1) exclusive, its corners cut `k`: a vertical gradient (light at the top), a
// fine brushed grain, a double bevel (an outer one `bevel` wide, lit top and left; a thin reversed one inside it, so
// the face sits proud), a black outline. Pressed: the outer bevel reversed. `tint` mixed in by its alpha (greyed off).
void steel(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t bevel, int32_t k = 0, bool pressed = false, Rgba tint = {}) {
  const int32_t hgt = std::max(1, y1 - y0);
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const Edge e = edge_of(x, y, x0, y0, x1, y1, k);
      if (e.d < 0) continue;  // cut away: left clear
      int32_t v = 158 - 70 * (y - y0) / hgt;                                     // 158 at the top to 88 at the foot
      v += ((x * 7 + y * 13) % 5 == 0) ? 7 : ((x * 3 + y) % 7 == 0 ? -6 : 0);  // grain, the same every build
      if (e.d == 0) v = 14;                                                     // the outline
      else if (e.d <= bevel) v = (e.lit != pressed) ? 225 - 18 * (e.d - 1) : 52 + 10 * (e.d - 1);
      else if (e.d == bevel + 1) v = (e.lit != pressed) ? 70 : 190;           // the inner, reversed line
      Rgba col = grey(v);
      if (tint.a) col = {col.r + (tint.r - col.r) * tint.a / 255, col.g + (tint.g - col.g) * tint.a / 255, col.b + (tint.b - col.b) * tint.a / 255, 255};
      c.set(x, y, col);
    }
}

// A recess: black, with a dark rim and a 1-pixel highlight at its bottom and right (cut into the steel).
void recess(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t alpha = 255) {
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const bool rim = x == x0 || y == y0, light = x == x1 - 1 || y == y1 - 1;
      c.set(x, y, light ? grey(175) : rim ? grey(40) : Rgba{6, 8, 10, alpha});
    }
}

// A clear window (a tile's picture shows through), its edge a dark rim and a bright lip.
void window(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  for (int32_t y = y0 - 1; y <= y1; ++y)
    for (int32_t x = x0 - 1; x <= x1; ++x) {
      const bool inside = x >= x0 && y >= y0 && x < x1 && y < y1;
      if (inside) c.set(x, y, {0, 0, 0, 0});
      else c.set(x, y, (x == x1 || y == y1) ? grey(190) : grey(25));
    }
}

// A round rivet of radius r at (cx, cy): domed, lit from the top left, a dark ring.
void rivet(Canvas& c, int32_t cx, int32_t cy, int32_t r) {
  for (int32_t y = -r - 1; y <= r + 1; ++y)
    for (int32_t x = -r - 1; x <= r + 1; ++x) {
      const double d = std::sqrt(static_cast<double>(x * x + y * y));
      if (d > r + 0.5) continue;
      if (d > r - 0.5) {
        c.set(cx + x, cy + y, grey(30));
        continue;
      }
      // The dome's light: brighter towards the top left.
      const double l = std::clamp(0.55 - 0.45 * (x + y) / (1.4 * r), 0.0, 1.0);
      c.set(cx + x, cy + y, grey(static_cast<int32_t>(70 + 170 * l)));
    }
  c.set(cx - r / 2, cy - r / 2, grey(250));  // the glint
}

// MW1's amber, lit round the inside of a window or recess (the mouse on it): a solid line, fading out over 3 pixels.
void glow(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  for (int32_t i = 0; i < 4; ++i) {
    const Rgba amber = {255, 176, 32, i == 0 ? 255 : 150 - 45 * i};
    for (int32_t x = x0 + i; x < x1 - i; ++x) c.over(x, y0 + i, amber), c.over(x, y1 - 1 - i, amber);
    for (int32_t y = y0 + i + 1; y < y1 - i - 1; ++y) c.over(x0 + i, y, amber), c.over(x1 - 1 - i, y, amber);
  }
}

// A round lamp at (cx, cy): amber and bright when on, a dark lens when off, a steel ring round it.
void lamp(Canvas& c, int32_t cx, int32_t cy, int32_t r, bool on) {
  for (int32_t y = -r - 1; y <= r + 1; ++y)
    for (int32_t x = -r - 1; x <= r + 1; ++x) {
      const double d = std::sqrt(static_cast<double>(x * x + y * y));
      if (d > r + 0.5) continue;
      if (d > r - 0.5) {
        c.set(cx + x, cy + y, grey(30));
        continue;
      }
      const double l = std::clamp(1.0 - d / r * 0.6 - (x + y) * 0.08, 0.2, 1.0);
      c.set(cx + x, cy + y, on ? Rgba{255, static_cast<int32_t>(120 + 100 * l), static_cast<int32_t>(20 + 60 * l), 255}
                               : Rgba{static_cast<int32_t>(50 * l), static_cast<int32_t>(35 * l), 10, 255});
    }
}

}  // namespace

std::string panel_name(int32_t w, int32_t h) { return "mw1_panel_" + std::to_string(w) + "x" + std::to_string(h); }

images::Image plaque() {
  Canvas c(kPlaqueW, kPlaqueH);
  // The bar: a long plate between two end caps, the text recess along it; the caps stand taller, chamfered, each with
  // a groove and two rivets (MW1's clamps).
  constexpr int32_t cap = 24;
  steel(c, cap - 4, 4, kPlaqueW - cap + 4, kPlaqueH - 4, 2, 3);
  recess(c, kPlaqueInset + 16, 11, kPlaqueW - kPlaqueInset - 16, kPlaqueH - 11);
  for (int32_t x0 : {0, kPlaqueW - cap}) {
    steel(c, x0, 0, x0 + cap, kPlaqueH, 3, 6);
    for (int32_t y = 10; y < kPlaqueH - 10; ++y) c.set(x0 + cap / 2, y, grey(40)), c.set(x0 + cap / 2 + 1, y, grey(185));
    rivet(c, x0 + cap / 2, 8, 3), rivet(c, x0 + cap / 2, kPlaqueH - 9, 3);
  }
  for (int32_t x = cap + 40; x < kPlaqueW - cap - 30; x += 56) rivet(c, x, 7, 2), rivet(c, x, kPlaqueH - 8, 2);
  return c.image(kPlaque);
}

images::Image tile_frame(int32_t state) {
  const int32_t n = kTile + 2 * kTileFrame;
  Canvas c(n, n);
  steel(c, 0, 0, n, n, 3, 7, state == 2);
  window(c, kTileFrame, kTileFrame, kTileFrame + kTile, kTileFrame + kTile);
  // Corner plates on the cuts' insides: a rivet each.
  for (int32_t x : {kTileFrame / 2 + 1, n - kTileFrame / 2 - 2})
    for (int32_t y : {kTileFrame / 2 + 1, n - kTileFrame / 2 - 2}) rivet(c, x, y, 2);
  if (state == 1) glow(c, kTileFrame, kTileFrame, kTileFrame + kTile, kTileFrame + kTile);
  return c.image(kTileFrames[std::clamp(state, 0, 2)]);
}

images::Image button(int32_t state) {
  Canvas c(kButtonW, kButtonH);
  const Rgba off_tint = {40, 40, 44, 150};
  steel(c, 0, 0, kButtonW, kButtonH, 2, 5, state == 2, state == 3 ? off_tint : Rgba{});
  // The caption's recess, and a lamp at the left end: amber when lit, dark otherwise.
  recess(c, 22, 6, kButtonW - 10, kButtonH - 6);
  lamp(c, 12, kButtonH / 2, 4, state == 1 || state == 2);
  if (state == 1) glow(c, 22, 6, kButtonW - 10, kButtonH - 6);
  return c.image(kButtons[std::clamp(state, 0, 3)]);
}

images::Image panel(int32_t w, int32_t h) {
  Canvas c(w, h);
  // A steel border, its corners cut, a corner plate with a rivet at each, rivets along the sides; the body dark and
  // 80% opaque, a thin steel trim round it.
  steel(c, 0, 0, w, h, 3, 10);
  recess(c, kPanelBorder, kPanelBorder, w - kPanelBorder, h - kPanelBorder, 205);
  for (int32_t x : {9, w - 10})
    for (int32_t y : {9, h - 10}) rivet(c, x, y, 3);
  for (int32_t x = 64; x < w - 48; x += 64) rivet(c, x, 5, 2), rivet(c, x, h - 6, 2);
  for (int32_t y = 64; y < h - 48; y += 64) rivet(c, 5, y, 2), rivet(c, w - 6, y, 2);
  return c.image(panel_name(w, h));
}

std::vector<images::Image> all() {
  std::vector<images::Image> v = {plaque()};
  for (int32_t s = 0; s < 3; ++s) v.push_back(tile_frame(s));
  for (int32_t s = 0; s < 4; ++s) v.push_back(button(s));
  for (const PanelSize& p : panel_sizes()) v.push_back(panel(p.w, p.h));
  return v;
}

}  // namespace mw1::chrome
