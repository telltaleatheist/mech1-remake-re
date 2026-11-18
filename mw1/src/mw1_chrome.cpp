#include "mw1_chrome.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>

namespace mw1::chrome {
namespace {

struct Rgba {
  int32_t r = 0, g = 0, b = 0, a = 0;
};

// B2b: the scale a piece is drawn at when its canvas isn't given one (piece_at sets it), and where image() hands the
// piece over at a scale above 1.
int32_t g_piece_scale = 1;
Drawn* g_capture = nullptr;

// A canvas of RGBA, then the archive's 565 and alpha.
struct Canvas {
  // w x h canvas pixels, each s x s device pixels (s 1: the archive's pieces; above 1, research/hires-2d.md B2b: the
  // 2D's at the window's scale). set and over take canvas pixels and fill the device block; set_d and over_d take one
  // device pixel, for what is drawn finer (a rivet's circle, the hangar's detail).
  int32_t w, h, s;
  std::vector<Rgba> px;
  Canvas(int32_t w_, int32_t h_, int32_t s_ = 0)
      : w(w_), h(h_), s(std::max(1, s_ > 0 ? s_ : g_piece_scale)), px(static_cast<size_t>(w_ * s * h_ * s)) {}
  int32_t dw() const { return w * s; }
  int32_t dh() const { return h * s; }
  Rgba& at_d(int32_t x, int32_t y) { return px[static_cast<size_t>(y) * dw() + x]; }
  Rgba& at(int32_t x, int32_t y) { return at_d(x * s, y * s); }
  bool in(int32_t x, int32_t y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  bool in_d(int32_t x, int32_t y) const { return x >= 0 && y >= 0 && x < dw() && y < dh(); }
  void set_d(int32_t x, int32_t y, Rgba c) {
    if (in_d(x, y)) at_d(x, y) = c;
  }
  void set(int32_t x, int32_t y, Rgba c) {
    if (!in(x, y)) return;
    for (int32_t j = 0; j < s; ++j)
      for (int32_t i = 0; i < s; ++i) at_d(x * s + i, y * s + j) = c;
  }
  // `c` over what's there, by its alpha.
  void over_d(int32_t x, int32_t y, Rgba c) {
    if (!in_d(x, y)) return;
    Rgba& d = at_d(x, y);
    const int32_t a = c.a, ia = 255 - a, oa = a + d.a * ia / 255;
    if (oa == 0) return;
    d.r = (c.r * a + d.r * d.a * ia / 255) / oa, d.g = (c.g * a + d.g * d.a * ia / 255) / oa,
    d.b = (c.b * a + d.b * d.a * ia / 255) / oa, d.a = oa;
  }
  void over(int32_t x, int32_t y, Rgba c) {
    if (!in(x, y)) return;
    for (int32_t j = 0; j < s; ++j)
      for (int32_t i = 0; i < s; ++i) over_d(x * s + i, y * s + j, c);
  }
  images::Image image(const std::string& name) const {  // at s 1; above, handed to piece_at (the image named only)
    images::Image im;
    if (s > 1) {
      if (g_capture) *g_capture = drawn(name, false);
      im.name = name;
      return im;
    }
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
  // The device pixels as RGBA bytes, straight alpha (B2b); `opaque`: alpha 255 throughout.
  Drawn drawn(const std::string& name, bool opaque) const {
    Drawn d;
    d.name = name, d.canvas_w = w, d.canvas_h = h, d.s = s;
    d.rgba.resize(px.size() * 4);
    auto cl = [](int32_t v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); };
    for (size_t i = 0; i < px.size(); ++i) {
      const Rgba& c = px[i];
      d.rgba[i * 4] = cl(c.r), d.rgba[i * 4 + 1] = cl(c.g), d.rgba[i * 4 + 2] = cl(c.b);
      // B1's edges as the canvas blit's (kFullAlpha): 0..3 clear, 0xfc and up solid.
      const int32_t a = opaque ? 255 : c.a;
      d.rgba[i * 4 + 3] = a <= 3 ? 0 : a >= 0xfc ? 255 : cl(a);
    }
    return d;
  }
};

Rgba grey(int32_t v, int32_t a = 255) { return {v, v, v + v / 16, a}; }  // gunmetal: a touch of blue

// A round rivet of radius r at (cx, cy): domed, lit from the top left, a dark ring.
void rivet(Canvas& c, int32_t cx, int32_t cy, int32_t r) {
  // In device pixels: the centre of canvas pixel (cx, cy), radius r canvas pixels (at s 1, exactly the canvas's).
  const int32_t s = c.s;
  const double ccx = (cx + 0.5) * s - 0.5, ccy = (cy + 0.5) * s - 0.5, rr = static_cast<double>(r) * s;
  const int32_t reach = (r + 1) * s;
  for (int32_t y = -reach; y <= reach; ++y)
    for (int32_t x = -reach; x <= reach; ++x) {
      const int32_t px = static_cast<int32_t>(std::lround(ccx)) + x, py = static_cast<int32_t>(std::lround(ccy)) + y;
      const double dx = px - ccx, dy = py - ccy, d = std::sqrt(dx * dx + dy * dy);
      if (d > rr + 0.5 * s) continue;
      if (d > rr - 0.5 * s) {
        c.set_d(px, py, grey(30));
        continue;
      }
      // The dome's light: brighter towards the top left.
      const double l = std::clamp(0.55 - 0.45 * (dx + dy) / (1.4 * rr), 0.0, 1.0);
      c.set_d(px, py, grey(static_cast<int32_t>(70 + 170 * l)));
    }
  // The glint: one canvas pixel up and left of the middle (at s 1, as it always was); above, a small bright disc there.
  if (s == 1) {
    c.set(cx - r / 2, cy - r / 2, grey(250));
    return;
  }
  const double gx = ccx - (r / 2) * s - (r < 2 ? 0.25 * rr : 0.0), gy = ccy - (r / 2) * s - (r < 2 ? 0.25 * rr : 0.0);
  const double gr = std::max(0.75, 0.3 * rr);
  for (int32_t y = static_cast<int32_t>(gy - gr) - 1; y <= gy + gr + 1; ++y)
    for (int32_t x = static_cast<int32_t>(gx - gr) - 1; x <= gx + gr + 1; ++x)
      if (std::hypot(x - gx, y - gy) <= gr) c.set_d(x, y, grey(250));
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
  // In device pixels, as rivet (at s 1 exactly the canvas's).
  const int32_t s = c.s;
  const double ccx = (cx + 0.5) * s - 0.5, ccy = (cy + 0.5) * s - 0.5, rr = static_cast<double>(r) * s;
  const int32_t reach = (r + 1) * s;
  for (int32_t y = -reach; y <= reach; ++y)
    for (int32_t x = -reach; x <= reach; ++x) {
      const int32_t px = static_cast<int32_t>(std::lround(ccx)) + x, py = static_cast<int32_t>(std::lround(ccy)) + y;
      const double dx = px - ccx, dy = py - ccy, d = std::sqrt(dx * dx + dy * dy);
      if (d > rr + 0.5 * s) continue;
      if (d > rr - 0.5 * s) {
        c.set_d(px, py, grey(30));
        continue;
      }
      const double l = std::clamp(1.0 - d / rr * 0.6 - (dx + dy) / s * 0.08, 0.2, 1.0);
      c.set_d(px, py, on ? Rgba{255, static_cast<int32_t>(120 + 100 * l), static_cast<int32_t>(20 + 60 * l), 255}
                         : Rgba{static_cast<int32_t>(50 * l), static_cast<int32_t>(35 * l), 10, 255});
    }
}

}  // namespace

std::string panel_name(int32_t w, int32_t h) { return "mw1_panel_" + std::to_string(w) + "x" + std::to_string(h); }
std::string terminal_name(int32_t w, int32_t h) { return "mw1_term_" + std::to_string(w) + "x" + std::to_string(h); }
std::string frame_name(int32_t w, int32_t h, bool lit) {
  return "mw1_frame_" + std::to_string(w) + "x" + std::to_string(h) + (lit ? "_lit" : "");
}

namespace {
// A thin flat gunmetal frame `t` wide round (x0, y0)-(x1, y1): a light top edge, a dark foot, a black outline;
// the inside left as it is.
void thin_frame(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t t, Rgba tint = {}) {
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t d = std::min({x - x0, y - y0, x1 - 1 - x, y1 - 1 - y});
      if (d >= t) continue;
      int32_t v = 128 - 40 * (y - y0) / std::max(1, y1 - y0);  // 128 at the top to 88 at the foot
      if (d == 0) v = 16;                                         // the outline
      else if (d == 1) v = (y - y0 <= 1 || x - x0 <= 1) ? 190 : 60;  // the lit lip, the shadowed one
      else if (d == t - 1) v = 30;                                 // the inner shadow line
      Rgba col = grey(v);
      if (tint.a) col = {col.r + (tint.r - col.r) * tint.a / 255, col.g + (tint.g - col.g) * tint.a / 255, col.b + (tint.b - col.b) * tint.a / 255, 255};
      c.set(x, y, col);
    }
}
// Dark glass inside (x0, y0)-(x1, y1): near black, a faint lift towards the top, part see-through.
void glass(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t alpha) {
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t lift = 14 - 12 * (y - y0) / std::max(1, y1 - y0);
      c.set(x, y, {8 + lift, 10 + lift, 13 + lift, alpha});
    }
}
}  // namespace

images::Image plaque(int32_t w, const char* name) {
  // The mockup's: dark glass in a thin gunmetal frame, a rivet at each end, half over the frame.
  Canvas c(w, kPlaqueH);
  glass(c, 4, 4, w - 4, kPlaqueH - 4, 228);
  thin_frame(c, 0, 0, w, kPlaqueH, 4);
  rivet(c, 6, kPlaqueH / 2, 3), rivet(c, w - 7, kPlaqueH / 2, 3);
  return c.image(name);
}

namespace {
Canvas tile_frame_canvas(int32_t state, int32_t scale) {
  // A thin frame round the picture, a dark band inside its foot for the label (fading up into the picture). Lit:
  // the frame amber and a glow inside; pressed: the frame darker.
  const int32_t n = kTile + 2 * kTileFrame;
  Canvas c(n, n, scale);
  const Rgba amber_tint = {255, 170, 40, 200}, dark_tint = {20, 20, 22, 120};
  thin_frame(c, 0, 0, n, n, kTileFrame, state == 1 ? amber_tint : state == 2 ? dark_tint : Rgba{});
  for (int32_t y = 0; y < kTileBand + 10; ++y) {
    const int32_t yy = n - kTileFrame - 1 - y;
    const int32_t a = y < kTileBand ? 200 : 200 * (kTileBand + 10 - y) / 10;  // solid under the label, then fading
    for (int32_t x = kTileFrame; x < n - kTileFrame; ++x) c.set(x, yy, {6, 8, 10, a});
  }
  if (state == 1) glow(c, kTileFrame, kTileFrame, n - kTileFrame, n - kTileFrame);
  return c;
}
}  // namespace

images::Image tile_frame(int32_t state) { return tile_frame_canvas(state, 0).image(kTileFrames[std::clamp(state, 0, 2)]); }

std::vector<images::Image> tiles(const images::Image& picture, const std::string& base) {
  std::vector<images::Image> out;
  const int32_t n = kTile + 2 * kTileFrame;
  for (int32_t state = 0; state < 4; ++state) {
    const images::Image f = tile_frame(state == 3 ? 0 : state);
    images::Image im;
    im.name = base + (state == 1 ? "_lit" : state == 2 ? "_down" : state == 3 ? "_off" : "");
    im.width = im.height = n;
    im.pixels.assign(static_cast<size_t>(n * n), 0);
    for (int32_t y = 0; y < n; ++y)
      for (int32_t x = 0; x < n; ++x) {
        const size_t i = static_cast<size_t>(y * n + x);
        // The picture in the window (black where it's smaller), then the frame over it by its alpha.
        int32_t r = 0, g = 0, b = 0;
        const int32_t px = x - kTileFrame, py = y - kTileFrame;
        if (px >= 0 && py >= 0 && px < picture.width && py < picture.height) {
          const uint16_t p = picture.pixels[static_cast<size_t>(py * picture.width + px)];
          r = (p >> 11) << 3, g = ((p >> 5) & 63) << 2, b = (p & 31) << 3;
          if (state == 3) r = g = b = (r * 30 + g * 59 + b * 11) / 100 / 3;  // off: grey and dark
          if (state == 2) r = r * 3 / 4, g = g * 3 / 4, b = b * 3 / 4;       // pressed: a shade darker
        }
        const uint16_t fp = f.pixels[i];
        const int32_t a = f.alpha[i];
        r = (r * (255 - a) + ((fp >> 11) << 3) * a) / 255, g = (g * (255 - a) + (((fp >> 5) & 63) << 2) * a) / 255,
        b = (b * (255 - a) + ((fp & 31) << 3) * a) / 255;
        im.pixels[i] = images::rgb565(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
      }
    out.push_back(std::move(im));
  }
  return out;
}

images::Image button(int32_t state) { return button(state, kButtonW, kButtonH, kButtons[std::clamp(state, 0, 3)]); }

images::Image button(int32_t state, int32_t w, int32_t h, const std::string& name, int32_t drawn) {
  // Dark glass in a thin frame, a lamp at the left end: amber when lit or pressed; greyed when off. `drawn` wide
  // from the left (0: all of it), the rest clear.
  Canvas c(w, h);
  const int32_t dw = drawn > 0 ? std::min(drawn, w) : w;
  const Rgba amber_tint = {255, 170, 40, 200}, off_tint = {40, 40, 44, 150};
  glass(c, 3, 3, dw - 3, h - 3, state == 2 ? 250 : 230);
  thin_frame(c, 0, 0, dw, h, 3, state == 1 ? amber_tint : state == 3 ? off_tint : Rgba{});
  lamp(c, 13, h / 2, 4, state == 1 || state == 2);
  if (state == 1) glow(c, 3, 3, dw - 3, h - 3);
  return c.image(name);
}

images::Image panel(int32_t w, int32_t h) {
  // A steel menu panel (the Bar's menu, a contract): dark glass in a thin frame, a rivet at each corner.
  Canvas c(w, h);
  glass(c, 4, 4, w - 4, h - 4, 215);
  thin_frame(c, 0, 0, w, h, 4);
  for (int32_t x : {5, w - 6})
    for (int32_t y : {5, h - 6}) rivet(c, x, y, 2);
  return c.image(panel_name(w, h));
}

images::Image terminal(int32_t w, int32_t h) {
  // The mockup's green terminal: a thin green border (and a dim line inside it), the body near-black green.
  Canvas c(w, h);
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const int32_t d = std::min({x, y, w - 1 - x, h - 1 - y});
      c.set(x, y, d < 2 ? Rgba{70, 190, 105, 255} : d == 3 ? Rgba{30, 80, 45, 230} : Rgba{4, 14, 8, 214});
    }
  return c.image(terminal_name(w, h));
}

images::Image status_bar() {
  // Across the screen's foot: a thin amber rule, then a dark band fading down to more solid.
  Canvas c(images::kWideW, kStatusH);
  for (int32_t y = 0; y < kStatusH; ++y)
    for (int32_t x = 0; x < images::kWideW; ++x)
      c.set(x, y, y == 0 ? Rgba{200, 130, 40, 210} : Rgba{4, 5, 7, 150 + 60 * y / kStatusH});
  return c.image(kStatusBar);
}

void darken_scene(images::Image& im) {
  // The hub's scenery as the mockup shows it: darker, and faint scanlines (every other line a shade darker).
  for (int32_t y = 0; y < im.height; ++y)
    for (int32_t x = 0; x < im.width; ++x) {
      uint16_t& p = im.pixels[static_cast<size_t>(y * im.width + x)];
      const int32_t k = kSceneLight * (y % 2 ? kScanline : 100) / 100;  // percent kept
      const int32_t r = ((p >> 11) << 3) * k / 100, g = (((p >> 5) & 63) << 2) * k / 100, b = ((p & 31) << 3) * k / 100;
      p = images::rgb565(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    }
}

images::Image dialog_frame() {
  // Every message box's (mw1dialog.zrd): dark glass in a thin frame, a rivet at each corner, an amber rule under
  // the title's band.
  Canvas c(kDialogW, kDialogH);
  glass(c, 4, 4, kDialogW - 4, kDialogH - 4, 255);  // solid: a box is the one thing to read
  thin_frame(c, 0, 0, kDialogW, kDialogH, 4);
  for (int32_t x = 14; x < kDialogW - 14; ++x) c.set(x, kDialogRule, {200, 130, 40, 230});
  for (int32_t x : {5, kDialogW - 6})
    for (int32_t y : {5, kDialogH - 6}) rivet(c, x, y, 2);
  return c.image(kDialog);
}

images::Image dim() {
  // Over the screen behind a box: black, part see-through.
  Canvas c(images::kWideW, images::kScreenH);
  for (Rgba& p : c.px) p = {0, 0, 0, kDimAlpha};
  return c.image(kDim);
}

images::Image cursor() {
  // MW1's pointer (every layout's CURSOR, centred on the mouse): an amber ring, four ticks pointing in, a dot, each
  // with a dark edge so it reads on a light scene.
  const int32_t n = kCursorSize, c0 = n / 2;
  Canvas c(n, n);
  const Rgba amber = {255, 176, 64, 255}, edge = {20, 12, 4, 200};
  auto mark = [&](int32_t x, int32_t y) {
    for (int32_t dy = -1; dy <= 1; ++dy)
      for (int32_t dx = -1; dx <= 1; ++dx)
        if (c.in(x + dx, y + dy) && c.at(x + dx, y + dy).a == 0) c.set(x + dx, y + dy, edge);
    c.set(x, y, amber);
  };
  for (int32_t y = 0; y < n; ++y)
    for (int32_t x = 0; x < n; ++x) {
      const int32_t dx = x - c0, dy = y - c0, d2 = dx * dx + dy * dy;
      if (d2 >= 7 * 7 && d2 <= 8 * 8) mark(x, y);                             // the ring
      if ((dx == 0 && std::abs(dy) >= 10 && std::abs(dy) <= 14) || (dy == 0 && std::abs(dx) >= 10 && std::abs(dx) <= 14))
        mark(x, y);                                                            // the ticks
      if (d2 <= 1) mark(x, y);                                                 // the dot
    }
  return c.image(kCursor);
}

images::Image frame(int32_t w, int32_t h, bool lit) {
  // A thin gunmetal frame, its inside clear (the picture shows through); amber when it is the chosen one's.
  Canvas c(w, h);
  thin_frame(c, 0, 0, w, h, kFrame, lit ? Rgba{255, 170, 40, 220} : Rgba{});
  return c.image(frame_name(w, h, lit));
}

images::Image arrow(bool up, int32_t state) {
  // A small button: dark glass in a thin frame, a solid triangle pointing up or down; amber lit, grey off.
  Canvas c(kArrowW, kArrowH);
  glass(c, 2, 2, kArrowW - 2, kArrowH - 2, state == 2 ? 250 : 230);
  thin_frame(c, 0, 0, kArrowW, kArrowH, 2, state == 1 ? Rgba{255, 170, 40, 200} : state == 3 ? Rgba{40, 40, 44, 150} : Rgba{});
  const Rgba ink = state == 3 ? Rgba{80, 84, 88, 255} : state == 0 ? Rgba{217, 221, 224, 255} : Rgba{255, 176, 64, 255};
  const int32_t cx = kArrowW / 2, top = 6, rows = kArrowH - 12;
  for (int32_t r = 0; r < rows; ++r) {
    const int32_t half = up ? r : rows - 1 - r;  // up: the point at the top
    for (int32_t x = cx - half - 1; x <= cx + half; ++x) c.set(x, top + r, ink);
  }
  static const char* const kLooks[4] = {"", "_lit", "_down", "_off"};
  return c.image(std::string(up ? "mw1_up" : "mw1_dn") + kLooks[std::clamp(state, 0, 3)]);
}

images::Image side_arrow(bool right, int32_t state) {
  // arrow()'s look on its side: the point to the left or the right.
  Canvas c(kSideArrowW, kSideArrowH);
  glass(c, 2, 2, kSideArrowW - 2, kSideArrowH - 2, state == 2 ? 250 : 230);
  thin_frame(c, 0, 0, kSideArrowW, kSideArrowH, 2, state == 1 ? Rgba{255, 170, 40, 200} : state == 3 ? Rgba{40, 40, 44, 150} : Rgba{});
  const Rgba ink = state == 3 ? Rgba{80, 84, 88, 255} : state == 0 ? Rgba{217, 221, 224, 255} : Rgba{255, 176, 64, 255};
  const int32_t cy = kSideArrowH / 2, left = 6, cols = kSideArrowW - 12;
  for (int32_t col = 0; col < cols; ++col) {
    const int32_t half = right ? cols - 1 - col : col;
    for (int32_t y = cy - half - 1; y <= cy + half; ++y) c.set(left + col, y, ink);
  }
  static const char* const kLooks[4] = {"", "_lit", "_down", "_off"};
  return c.image(std::string(right ? "mw1_rt" : "mw1_lt") + kLooks[std::clamp(state, 0, 3)]);
}

images::Image slider(bool fill) {
  // A slider's groove (dark glass in a thin frame) or the fill drawn over it (amber, lit at the top).
  Canvas c(kSliderW, kSliderH);
  if (!fill) {
    glass(c, 2, 2, kSliderW - 2, kSliderH - 2, 235);
    thin_frame(c, 0, 0, kSliderW, kSliderH, 2);
  } else {
    for (int32_t y = 2; y < kSliderH - 2; ++y)
      for (int32_t x = 2; x < kSliderW - 2; ++x) c.set(x, y, y == 2 ? Rgba{255, 214, 140, 255} : Rgba{230, 150, 40, 255});
  }
  return c.image(fill ? kSliderFill : kSlider);
}

images::Image check(bool on) {
  // A check box: the button's glass and frame, an amber square inside when checked.
  Canvas c(kCheckSize, kCheckSize);
  glass(c, 2, 2, kCheckSize - 2, kCheckSize - 2, 235);
  thin_frame(c, 0, 0, kCheckSize, kCheckSize, 2);
  if (on)
    for (int32_t y = 5; y < kCheckSize - 5; ++y)
      for (int32_t x = 5; x < kCheckSize - 5; ++x) c.set(x, y, {255, 176, 64, 255});
  return c.image(on ? kChecked : kCheck);
}

images::Image cell(int32_t state) {
  // A control table's cell (a key's, the row shown): dark glass, a thin frame (its lit lip), amber when lit, an amber
  // fill when chosen.
  Canvas c(kCellW, kCellH);
  glass(c, 1, 1, kCellW - 1, kCellH - 1, state == 2 ? 250 : 200);
  if (state == 2)
    for (int32_t y = 2; y < kCellH - 2; ++y)
      for (int32_t x = 2; x < kCellW - 2; ++x) c.set(x, y, {90, 56, 14, 240});
  thin_frame(c, 0, 0, kCellW, kCellH, 2, state >= 1 ? Rgba{255, 170, 40, 220} : Rgba{});
  return c.image(kCells[std::clamp(state, 0, 2)]);
}

images::Image progress_cell(bool lit) {
  // A cell of the cover's progress bar: dark glass with a thin frame; lit, an amber fill.
  Canvas c(kProgressCellW, kProgressCellH);
  glass(c, 1, 1, kProgressCellW - 1, kProgressCellH - 1, 230);
  for (int32_t y = 2; lit && y < kProgressCellH - 2; ++y)
    for (int32_t x = 2; x < kProgressCellW - 2; ++x) c.set(x, y, {255, 176, 64, 255});
  thin_frame(c, 0, 0, kProgressCellW, kProgressCellH, 1, lit ? Rgba{255, 170, 40, 220} : Rgba{});
  return c.image(kProgressCell[lit ? 1 : 0]);
}

images::Image dialog_tall() {
  // dialog_frame()'s look, taller (the file popup's).
  Canvas c(kDialogW, kDialogTallH);
  glass(c, 4, 4, kDialogW - 4, kDialogTallH - 4, 255);
  thin_frame(c, 0, 0, kDialogW, kDialogTallH, 4);
  for (int32_t x = 14; x < kDialogW - 14; ++x) c.set(x, kDialogRule, {200, 130, 40, 230});
  for (int32_t x : {5, kDialogW - 6})
    for (int32_t y : {5, kDialogTallH - 6}) rivet(c, x, y, 2);
  return c.image(kDialogTall);
}

images::Image clear() {
  Canvas c(4, 4);
  return c.image(kClear);
}

// The lab's reskin.
const std::vector<Reskin>& lab_pictures() {
  static const std::vector<Reskin> k = {
      // The panels (lab_panels): PM's left and right, single and double, the centre, and the backdrop under them
      // (PM has none: it shows, widened, at the wide canvas's sides).
      {"lab_background", 640, 480},
      {"lab_panel_leftsingle", 220, 480}, {"lab_panel_leftdouble", 220, 480}, {"lab_panel_rightsingle", 220, 480},
      {"lab_panel_rightdouble", 220, 480}, {"lab_panel_center", 200, 480},
      // The page icons and ACCEPT, each still (green), pressed (red) and off (grey); one size (FIXES.md item 29: PM's
      // were 51x58, 55x49, 65x48 and 60x49), the button's hit area (Control::hit: its picture's).
      {"lab_icon1_green", kLabTabW, kLabTabH}, {"lab_icon1_red", kLabTabW, kLabTabH}, {"lab_icon1_grey", kLabTabW, kLabTabH},
      {"lab_icon4_green", kLabTabW, kLabTabH}, {"lab_icon4_red", kLabTabW, kLabTabH}, {"lab_icon4_grey", kLabTabW, kLabTabH},
      {"lab_icon5_green", kLabTabW, kLabTabH}, {"lab_icon5_red", kLabTabW, kLabTabH}, {"lab_icon5_grey", kLabTabW, kLabTabH},
      {"lab_accept_green", kLabTabW, kLabTabH}, {"lab_accept_red", kLabTabW, kLabTabH}, {"lab_accept_grey", kLabTabW, kLabTabH},
      // The glows behind the chosen icon and mech, and the variant being renamed.
      {"lab_icon_glow", 127, 63}, {"lab_player_glow", 127, 63}, {"lab_text_glow", 207, 45},
      // The arrows: the lists' (up, down), the armour's (left, right), the toggles'.
      {"lab_arrowup", 28, 9}, {"lab_arrowdn", 28, 9}, {"lab_arrowupD", 28, 9}, {"lab_arrowdnD", 28, 9}, {"lab_arrow_left", 7, 13}, {"lab_arrow_right", 7, 13},
      {"lab_togglel_green", 5, 9}, {"lab_togglel_red", 5, 9}, {"lab_toggler_green", 5, 9}, {"lab_toggler_red", 5, 9},
  };
  return k;
}
const std::vector<Reskin>& filebox_pictures() {
  static const std::vector<Reskin> k = {
      {"load_box", 240, 280}, {"textbox_small", 140, 21}, {"textbox_small_r", 140, 21}, {"circlebtn_tiny", 84, 30},
      {"arrowupmp_rollover", 9, 5}, {"arrowupmp_d", 9, 5}, {"arrowdnmp_normal", 9, 5}, {"arrowdnmp_rollover", 9, 5},
      {"arrowdnmp_d", 9, 5},
  };
  return k;
}
std::string reskin_name(const char* game) { return std::string("mw1_") + game; }
bool is_lab_panel(const Reskin& r) {
  return std::string(r.game).rfind("lab_panel_", 0) == 0 || std::string(r.game) == "lab_background";
}

namespace {
bool has(const char* s, const char* part) { return std::strstr(s, part) != nullptr; }
void fill(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, Rgba ink) {
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) c.set(x, y, ink);
}
// A line `t` thick from (x0, y0) to (x1, y1).
void line(Canvas& c, double x0, double y0, double x1, double y1, double t, Rgba ink) {
  const double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
  for (int32_t y = 0; y < c.h; ++y)
    for (int32_t x = 0; x < c.w; ++x) {
      const double u = std::clamp(((x - x0) * dx + (y - y0) * dy) / l2, 0.0, 1.0);
      const double ex = x0 + u * dx - x, ey = y0 + u * dy - y;
      if (ex * ex + ey * ey <= t * t / 4) c.set(x, y, ink);
    }
}
// A solid triangle in (x0, y0)-(x1, y1) pointing `dir` (0 up, 1 down, 2 left, 3 right).
void triangle(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t dir, Rgba ink) {
  const double w = x1 - x0, h = y1 - y0;
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const double u = (x - x0 + 0.5) / w, v = (y - y0 + 0.5) / h;  // 0..1 across, down
      const double along = dir == 0 ? v : dir == 1 ? 1 - v : dir == 2 ? u : 1 - u;  // 0 at the point
      const double side = dir <= 1 ? u : v;
      if (std::abs(side - 0.5) <= along / 2 + 0.06) c.set(x, y, ink);
    }
}
// A soft amber glow filling the canvas (an ellipse), at most `peak` opaque.
images::Image glow_piece(const Reskin& r, int32_t peak) {
  Canvas c(r.w, r.h);
  for (int32_t y = 0; y < r.h; ++y)
    for (int32_t x = 0; x < r.w; ++x) {
      const double u = (x + 0.5) / r.w * 2 - 1, v = (y + 0.5) / r.h * 2 - 1, d = u * u + v * v;
      if (d < 1) c.set(x, y, {255, 170, 40, static_cast<int32_t>(peak * (1 - d) * (1 - d))});
    }
  return c.image(reskin_name(r.game));
}
// The chosen row's mark in the lab's lists (FIXES.md item 17: our amber bar, not a glow): PM places it 17 left of and
// 15 above the row's words (its large font, 16 high), so the bar spans the row and its left edge is solid amber.
images::Image bar_piece(const Reskin& r) {
  Canvas c(r.w, r.h);
  const int32_t top = kBarTop, bottom = kBarBottom, left = kBarLeft, right = std::min(kBarRight, r.w);  // the row's span
  for (int32_t y = top; y < bottom; ++y)
    for (int32_t x = left; x < right; ++x) c.set(x, y, x < left + kBarEdge ? Rgba{255, 176, 64, 235} : Rgba{255, 150, 30, 64});
  for (int32_t x = left + kBarEdge; x < right; ++x) c.set(x, top, {255, 176, 64, 120}), c.set(x, bottom - 1, {255, 176, 64, 120});
  return c.image(reskin_name(r.game));
}
// A page icon: the button's glass in a thin frame (amber pressed, dim off) and its glyph.
images::Image icon_piece(const Reskin& r) {
  const int32_t state = has(r.game, "_red") ? 2 : has(r.game, "_grey") ? 3 : 0;
  Canvas c(r.w, r.h);
  glass(c, 2, 2, r.w - 2, r.h - 2, state == 2 ? 250 : 230);
  thin_frame(c, 0, 0, r.w, r.h, 2, state == 2 ? Rgba{255, 170, 40, 220} : state == 3 ? Rgba{40, 40, 44, 150} : Rgba{});
  for (int32_t x : {4, r.w - 5})
    for (int32_t y : {4, r.h - 5}) rivet(c, x, y, 1);
  const Rgba ink = state == 3 ? Rgba{80, 84, 88, 255} : state == 0 ? Rgba{217, 221, 224, 255} : Rgba{255, 176, 64, 255};
  // The glyph's box: square, centred (the same on every icon, item 29).
  const int32_t w = std::min(r.w, r.h) - 18, h = w, x0 = (r.w - w) / 2, y0 = (r.h - h) / 2;
  auto box = [&](double a, double b, double cc, double d) {  // fractions of the glyph's box
    fill(c, x0 + static_cast<int32_t>(a * w), y0 + static_cast<int32_t>(b * h), x0 + static_cast<int32_t>(cc * w),
         y0 + static_cast<int32_t>(d * h), ink);
  };
  if (has(r.game, "icon1")) {  // the chassis: a mech, front on
    box(0.38, 0.0, 0.62, 0.16);                          // the cockpit
    box(0.22, 0.2, 0.78, 0.52);                          // the torso
    box(0.02, 0.22, 0.18, 0.62), box(0.82, 0.22, 0.98, 0.62);  // the arms
    box(0.26, 0.56, 0.42, 1.0), box(0.58, 0.56, 0.74, 1.0);    // the legs
  } else if (has(r.game, "icon4")) {  // the criticals: a grid of slots
    for (int32_t col = 0; col < 3; ++col)
      for (int32_t row = 0; row < 4; ++row)
        box(col * 0.36, row * 0.26, col * 0.36 + 0.28, row * 0.26 + 0.18);
  } else if (has(r.game, "icon5")) {  // the weapon groups: a bracket round three bars
    box(0.0, 0.0, 0.08, 1.0), box(0.0, 0.0, 0.2, 0.1), box(0.0, 0.9, 0.2, 1.0);
    for (int32_t i = 0; i < 3; ++i) box(0.28, 0.08 + i * 0.32, 1.0 - i * 0.18, 0.28 + i * 0.32);
  } else {  // ACCEPT: a check mark
    line(c, x0 + 0.08 * w, y0 + 0.55 * h, x0 + 0.38 * w, y0 + 0.9 * h, 5, ink);
    line(c, x0 + 0.38 * w, y0 + 0.9 * h, x0 + 0.95 * w, y0 + 0.08 * h, 5, ink);
  }
  return c.image(reskin_name(r.game));
}
// A bare arrow (the lab's and the file list's small ones): light grey still, amber lit, grey off.
images::Image arrow_piece(const Reskin& r) {
  const std::string g = r.game;
  const int32_t dir = has(r.game, "up") ? 0 : has(r.game, "dn") ? 1 : has(r.game, "left") || has(r.game, "togglel") ? 2 : 3;
  const bool lit = has(r.game, "_red") || has(r.game, "rollover") || g == "lab_arrow_left" || g == "lab_arrow_right";
  const bool off = (g.size() > 2 && g.compare(g.size() - 2, 2, "_d") == 0) || g.back() == 'D';
  const Rgba ink = off ? Rgba{80, 84, 88, 255}
                   : lit ? Rgba{255, 176, 64, 255} : Rgba{217, 221, 224, 255};
  Canvas c(r.w, r.h);
  if (r.w >= 20) {  // the lists' wide tab: the triangle in its middle
    triangle(c, r.w / 2 - 6, 1, r.w / 2 + 6, r.h - 1, dir, ink);
  } else {
    triangle(c, 0, 0, r.w, r.h, dir, ink);
  }
  return c.image(reskin_name(r.game));
}
}  // namespace

images::Image reskin(const Reskin& r) {
  const std::string g = r.game;
  if (g == "lab_text_glow") return bar_piece(r);
  if (has(r.game, "glow")) return glow_piece(r, 90);
  if (has(r.game, "icon") || has(r.game, "accept")) return icon_piece(r);
  if (has(r.game, "arrow") || has(r.game, "toggle")) return arrow_piece(r);
  Canvas c(r.w, r.h);
  if (g == "load_box") {  // the load/save box: the message boxes' frame
    glass(c, 4, 4, r.w - 4, r.h - 4, 255);
    thin_frame(c, 0, 0, r.w, r.h, 4);
    for (int32_t x : {5, r.w - 6})
      for (int32_t y : {5, r.h - 6}) rivet(c, x, y, 2);
  } else if (g.rfind("textbox_small", 0) == 0) {  // the name's bar: a recess, amber when the mouse is on it
    glass(c, 1, 1, r.w - 1, r.h - 1, 250);
    thin_frame(c, 0, 0, r.w, r.h, 2, g == "textbox_small_r" ? Rgba{255, 170, 40, 220} : Rgba{});
  } else if (g == "circlebtn_tiny") {  // OK and NO: MW1's framed button (FIXES.md item 10), PM's label at 22, 8 in it
    // PM's image is a small circle in 84 clear: NO sits at 372 in the box's 200..440 (item 17), so the frame is
    // kTinyButtonW of it.
    return button(0, r.w, r.h, reskin_name(r.game), kTinyButtonW);
  }
  return c.image(reskin_name(r.game));
}

namespace {
// The hangar's colour over (u0, v0)-(u1, v1), in its canvas copy's pixels (854 x 480).
using HangarSample = Rgba (*)(const void* ctx, double u0, double v0, double u1, double v1);
std::vector<std::pair<std::string, Canvas>> lab_panel_canvases(HangarSample sample, const void* ctx, int32_t hangar_w,
                                                               int32_t scale) {
  constexpr int32_t kMid = 640, kLeft = 220, kCentre = 200;
  const int32_t off = (hangar_w - kMid) / 2;
  // A strip of the hangar, darkened to `keep` percent with faint scanlines.
  auto strip = [&](int32_t from, int32_t w, int32_t keep) {
    Canvas c(w, images::kScreenH, scale);
    const double step = 1.0 / c.s;
    for (int32_t y = 0; y < c.dh(); ++y)
      for (int32_t x = 0; x < c.dw(); ++x) {
        const double u = off + from + x * step, v = y * step;
        const Rgba p = sample(ctx, u, v, u + step, v + step);
        const int32_t k = keep * ((y / c.s) % 2 ? 88 : 100) / 100;  // the scanlines a canvas line apart
        c.set_d(x, y, {p.r * k / 100, p.g * k / 100, p.b * k / 100, 255});
      }
    return c;
  };
  // A glass box framed, over the strip (its rule under the title, rivets at its corners).
  auto box = [](Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rule) {
    for (int32_t y = y0 + 2; y < y1 - 2; ++y)
      for (int32_t x = x0 + 2; x < x1 - 2; ++x) c.over(x, y, {8, 10, 13, 205});
    thin_frame(c, x0, y0, x1, y1, 2);
    if (rule)
      for (int32_t x = x0 + 8; x < x1 - 8; ++x) c.set(x, y0 + rule, {200, 130, 40, 230});
    for (int32_t x : {x0 + 4, x1 - 5})
      for (int32_t y : {y0 + 4, y1 - 5}) rivet(c, x, y, 1);
  };
  // The icons' tray across the foot.
  auto tray = [](Canvas& c) {
    for (int32_t y = kLabTrayY; y < c.h; ++y)
      for (int32_t x = 0; x < c.w; ++x) c.over(x, y, {8, 10, 13, 190});
    for (int32_t x = 0; x < c.w; ++x) c.set(x, kLabTrayY, {200, 130, 40, 200});
  };
  std::vector<std::pair<std::string, Canvas>> v;
  for (bool right : {false, true})
    for (bool two : {false, true}) {
      Canvas c = strip(right ? kLeft + kCentre : 0, kLeft, 45);
      // The rule under the left's titles (the right's tabs sit where it would be).
      const int32_t rule = right ? 0 : 38;
      if (two) {
        box(c, kLabSideUpper.x0, kLabSideUpper.y0, kLabSideUpper.x1, kLabSideUpper.y1, rule);
        // the rule at 271: the lower lists' first rows start at 275 (PM's layout)
        box(c, kLabSideLower.x0, kLabSideLower.y0, kLabSideLower.x1, kLabSideLower.y1, 23);
      } else {
        box(c, kLabSideWhole.x0, kLabSideWhole.y0, kLabSideWhole.x1, kLabSideWhole.y1, rule);
      }
      tray(c);
      v.emplace_back(reskin_name(right ? (two ? "lab_panel_rightdouble" : "lab_panel_rightsingle")
                                       : (two ? "lab_panel_leftdouble" : "lab_panel_leftsingle")),
                     std::move(c));
    }
  // The centre: the hangar brighter round the mech, a glass box under it for its parts, data, icons and messages.
  // Above the tray as the sides' (Owen, item 17: it ran down through the tray round the page icon).
  Canvas c = strip(kLeft, kCentre, 80);
  box(c, kLabCentreBox.x0 - kLeft, kLabCentreBox.y0, kLabCentreBox.x1 - kLeft, kLabCentreBox.y1, 0);
  tray(c);
  v.emplace_back(reskin_name("lab_panel_center"), std::move(c));
  // The backdrop: the whole middle, as the sides' (on a wide canvas the engine mirrors its edges out).
  Canvas b = strip(0, kMid, 45);
  tray(b);
  v.emplace_back(reskin_name("lab_background"), std::move(b));
  return v;
}

// The source-size picture's colour over (x0, y0)-(x1, y1) of its own pixels, partial pixels weighed by cover.
struct Source {
  const uint8_t* rgba;
  int32_t w, h;
};
Rgba source_area(const Source& src, double x0, double y0, double x1, double y1) {
  x0 = std::clamp(x0, 0.0, static_cast<double>(src.w)), x1 = std::clamp(x1, 0.0, static_cast<double>(src.w));
  y0 = std::clamp(y0, 0.0, static_cast<double>(src.h)), y1 = std::clamp(y1, 0.0, static_cast<double>(src.h));
  if (x1 - x0 < 1e-9 || y1 - y0 < 1e-9) {  // under a pixel at the edge: the nearest one
    const int32_t x = std::clamp(static_cast<int32_t>(x0), 0, src.w - 1), y = std::clamp(static_cast<int32_t>(y0), 0, src.h - 1);
    const uint8_t* p = src.rgba + (static_cast<size_t>(y) * src.w + x) * 4;
    return {p[0], p[1], p[2], 255};
  }
  double sum[3] = {0, 0, 0}, area = 0;
  for (int32_t y = static_cast<int32_t>(y0); y < y1; ++y) {
    const double wy = std::min<double>(y + 1, y1) - std::max<double>(y, y0);
    for (int32_t x = static_cast<int32_t>(x0); x < x1; ++x) {
      const double wgt = wy * (std::min<double>(x + 1, x1) - std::max<double>(x, x0));
      const uint8_t* p = src.rgba + (static_cast<size_t>(y) * src.w + x) * 4;
      for (int k = 0; k < 3; ++k) sum[k] += wgt * p[k];
      area += wgt;
    }
  }
  return {static_cast<int32_t>(std::lround(sum[0] / area)), static_cast<int32_t>(std::lround(sum[1] / area)),
          static_cast<int32_t>(std::lround(sum[2] / area)), 255};
}
}  // namespace

std::vector<images::Image> lab_panels(const images::Image& hangar) {
  // The canvas copy's own pixels (at scale 1 each sample is one pixel).
  const HangarSample at = [](const void* ctx, double u0, double v0, double, double) {
    const images::Image& h = *static_cast<const images::Image*>(ctx);
    const int32_t x = std::clamp(static_cast<int32_t>(u0), 0, h.width - 1), y = std::clamp(static_cast<int32_t>(v0), 0, h.height - 1);
    const uint16_t p = h.pixels[static_cast<size_t>(y * h.width + x)];
    return Rgba{(p >> 11) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3, 255};
  };
  std::vector<images::Image> v;
  for (auto& [name, c] : lab_panel_canvases(at, &hangar, hangar.width, 1)) {
    v.push_back(c.image(name));
    v.back().alpha.clear();  // opaque, as PM's (the engine widens an opaque backdrop)
  }
  return v;
}

std::vector<Drawn> lab_panels_at(const uint8_t* rgba, int32_t sw, int32_t sh, int32_t s) {
  // The hangar's canvas copy is the whole source at 854 x 480: a canvas pixel is sw / 854 by sh / 480 of it.
  struct Ctx { Source src; double fx, fy; } ctx{{rgba, sw, sh}, static_cast<double>(sw) / images::kWideW,
                                               static_cast<double>(sh) / images::kScreenH};
  const HangarSample area = [](const void* p, double u0, double v0, double u1, double v1) {
    const Ctx& c = *static_cast<const Ctx*>(p);
    return source_area(c.src, u0 * c.fx, v0 * c.fy, u1 * c.fx, v1 * c.fy);
  };
  std::vector<Drawn> v;
  if (!rgba || sw <= 0 || sh <= 0) return v;
  for (auto& [name, c] : lab_panel_canvases(area, &ctx, images::kWideW, s)) v.push_back(c.drawn(name, true));
  return v;
}

std::vector<Drawn> tiles_at(const uint8_t* rgba, int32_t sw, int32_t sh, int32_t x0, int32_t y0, int32_t side_w,
                            int32_t side_h, const std::string& base, int32_t s) {
  std::vector<Drawn> out;
  if (!rgba || sw <= 0 || sh <= 0 || side_w <= 0 || side_h <= 0) return out;
  const Source src{rgba, sw, sh};
  const int32_t n = kTile + 2 * kTileFrame;
  // The picture once (its window's device pixels), then each look's tint and frame over it, as tiles() does.
  const int32_t pw = kTile * std::max(1, s);
  std::vector<Rgba> pic(static_cast<size_t>(pw) * pw);
  const double fx = static_cast<double>(side_w) / pw, fy = static_cast<double>(side_h) / pw;
  for (int32_t y = 0; y < pw; ++y)
    for (int32_t x = 0; x < pw; ++x)
      pic[static_cast<size_t>(y) * pw + x] = source_area(src, x0 + x * fx, y0 + y * fy, x0 + (x + 1) * fx, y0 + (y + 1) * fy);
  for (int32_t state = 0; state < 4; ++state) {
    const Canvas f = tile_frame_canvas(state == 3 ? 0 : state, s);
    Canvas c(n, n, s);
    const int32_t inset = kTileFrame * c.s;
    for (int32_t y = 0; y < c.dh(); ++y)
      for (int32_t x = 0; x < c.dw(); ++x) {
        Rgba p;
        const int32_t px = x - inset, py = y - inset;
        if (px >= 0 && py >= 0 && px < pw && py < pw) {
          p = pic[static_cast<size_t>(py) * pw + px];
          if (state == 3) p.r = p.g = p.b = (p.r * 30 + p.g * 59 + p.b * 11) / 100 / 3;  // off: grey and dark
          if (state == 2) p.r = p.r * 3 / 4, p.g = p.g * 3 / 4, p.b = p.b * 3 / 4;       // pressed: a shade darker
        }
        const Rgba& fr = f.px[static_cast<size_t>(y) * c.dw() + x];
        const int32_t a = fr.a;
        c.set_d(x, y, {(p.r * (255 - a) + fr.r * a) / 255, (p.g * (255 - a) + fr.g * a) / 255, (p.b * (255 - a) + fr.b * a) / 255, 255});
      }
    out.push_back(c.drawn(base + (state == 1 ? "_lit" : state == 2 ? "_down" : state == 3 ? "_off" : ""), true));
  }
  return out;
}

namespace {
using Maker = std::function<images::Image()>;
// Every piece of the archive, in all()'s order, each made when called (at piece_at's scale, else 1).
std::vector<Maker> makers() {
  std::vector<Maker> v = {[] { return plaque(); }, [] { return plaque(kPlaqueNarrowW, kPlaqueNarrow); },
                         [] { return plaque(kStripW, kStrip); }, status_bar, dialog_frame, dim, cursor};
  for (int32_t s = 0; s < 3; ++s) v.push_back([=] { return tile_frame(s); });
  for (int32_t s = 0; s < 4; ++s) v.push_back([=] { return button(s); });
  for (const PanelSize& p : panel_sizes()) v.push_back([=] { return panel(p.w, p.h); });
  for (const PanelSize& p : terminal_sizes()) v.push_back([=] { return terminal(p.w, p.h); });
  for (const PanelSize& p : frame_sizes())
    for (bool lit : {false, true}) v.push_back([=] { return frame(p.w, p.h, lit); });
  for (bool up : {true, false})
    for (int32_t s = 0; s < 4; ++s) v.push_back([=] { return arrow(up, s); });
  for (bool right : {false, true})
    for (int32_t s = 0; s < 4; ++s) v.push_back([=] { return side_arrow(right, s); });
  for (bool on : {false, true}) v.push_back([=] { return slider(on); }), v.push_back([=] { return check(on); });
  for (int32_t s = 0; s < 3; ++s) v.push_back([=] { return cell(s); });
  v.push_back([=] { return dialog_tall(); });
  for (bool lit : {false, true}) v.push_back([=] { return progress_cell(lit); });
  v.push_back([=] { return plaque(kPlaqueCoverW, kPlaqueCover); });
  for (const std::vector<Reskin>* t : {&lab_pictures(), &filebox_pictures()})
    for (const Reskin& r : *t)
      if (!is_lab_panel(r)) v.push_back([=] { return reskin(r); });
  v.push_back([=] { return clear(); });
  return v;
}
}  // namespace

std::vector<images::Image> all() {
  std::vector<images::Image> v;
  for (const Maker& m : makers()) v.push_back(m());
  return v;
}

namespace {
// The pieces drawn at scale for the 2D (B2b): every piece but those with nothing to gain (the dim, a flat
// see-through black; the cursor, drawn by the host; the clear placeholder), by name to its maker.
const std::map<std::string, size_t>& drawn_index() {
  static const std::map<std::string, size_t> index = [] {
    std::map<std::string, size_t> m;
    const std::vector<Maker> v = makers();
    for (size_t i = 0; i < v.size(); ++i) {
      const std::string n = v[i]().name;
      if (n != kDim && n != kCursor && n != kClear) m.emplace(n, i);
    }
    return m;
  }();
  return index;
}
}  // namespace

std::vector<std::string> chrome_names() {
  std::vector<std::string> v;
  for (const auto& [n, i] : drawn_index()) v.push_back(n);
  return v;
}

bool chrome_at(const std::string& name, int32_t s, Drawn* out) {
  const auto it = drawn_index().find(name);
  if (it == drawn_index().end() || s < 2 || !out) return false;
  const Maker m = makers()[it->second];
  Drawn d;
  const int32_t was = g_piece_scale;
  Drawn* was_capture = g_capture;
  g_piece_scale = s, g_capture = &d;
  m();
  g_piece_scale = was, g_capture = was_capture;
  if (d.rgba.empty() || d.name != name) return false;
  *out = std::move(d);
  return true;
}

}  // namespace mw1::chrome
