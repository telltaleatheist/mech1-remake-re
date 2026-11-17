#include "mw1_chrome.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
std::string terminal_name(int32_t w, int32_t h) { return "mw1_term_" + std::to_string(w) + "x" + std::to_string(h); }

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

images::Image plaque() {
  // The mockup's: dark glass in a thin gunmetal frame, a rivet at each end, half over the frame.
  Canvas c(kPlaqueW, kPlaqueH);
  glass(c, 4, 4, kPlaqueW - 4, kPlaqueH - 4, 228);
  thin_frame(c, 0, 0, kPlaqueW, kPlaqueH, 4);
  rivet(c, 6, kPlaqueH / 2, 3), rivet(c, kPlaqueW - 7, kPlaqueH / 2, 3);
  return c.image(kPlaque);
}

images::Image tile_frame(int32_t state) {
  // A thin frame round the picture, a dark band inside its foot for the label (fading up into the picture). Lit:
  // the frame amber and a glow inside; pressed: the frame darker.
  const int32_t n = kTile + 2 * kTileFrame;
  Canvas c(n, n);
  const Rgba amber_tint = {255, 170, 40, 200}, dark_tint = {20, 20, 22, 120};
  thin_frame(c, 0, 0, n, n, kTileFrame, state == 1 ? amber_tint : state == 2 ? dark_tint : Rgba{});
  for (int32_t y = 0; y < kTileBand + 10; ++y) {
    const int32_t yy = n - kTileFrame - 1 - y;
    const int32_t a = y < kTileBand ? 200 : 200 * (kTileBand + 10 - y) / 10;  // solid under the label, then fading
    for (int32_t x = kTileFrame; x < n - kTileFrame; ++x) c.set(x, yy, {6, 8, 10, a});
  }
  if (state == 1) glow(c, kTileFrame, kTileFrame, n - kTileFrame, n - kTileFrame);
  return c.image(kTileFrames[std::clamp(state, 0, 2)]);
}

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

images::Image button(int32_t state) {
  // Dark glass in a thin frame, a lamp at the left end: amber when lit or pressed; greyed when off.
  Canvas c(kButtonW, kButtonH);
  const Rgba amber_tint = {255, 170, 40, 200}, off_tint = {40, 40, 44, 150};
  glass(c, 3, 3, kButtonW - 3, kButtonH - 3, state == 2 ? 250 : 230);
  thin_frame(c, 0, 0, kButtonW, kButtonH, 3, state == 1 ? amber_tint : state == 3 ? off_tint : Rgba{});
  lamp(c, 13, kButtonH / 2, 4, state == 1 || state == 2);
  if (state == 1) glow(c, 3, 3, kButtonW - 3, kButtonH - 3);
  return c.image(kButtons[std::clamp(state, 0, 3)]);
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

std::vector<images::Image> all() {
  std::vector<images::Image> v = {plaque(), status_bar(), dialog_frame(), dim(), cursor()};
  for (int32_t s = 0; s < 3; ++s) v.push_back(tile_frame(s));
  for (int32_t s = 0; s < 4; ++s) v.push_back(button(s));
  for (const PanelSize& p : panel_sizes()) v.push_back(panel(p.w, p.h));
  for (const PanelSize& p : terminal_sizes()) v.push_back(terminal(p.w, p.h));
  return v;
}

}  // namespace mw1::chrome
