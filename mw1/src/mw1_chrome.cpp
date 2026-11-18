#include "mw1_chrome.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

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
      // The page icons and ACCEPT, each still (green), pressed (red) and off (grey).
      {"lab_icon1_green", 51, 58}, {"lab_icon1_red", 51, 58}, {"lab_icon1_grey", 51, 58},
      {"lab_icon4_green", 55, 49}, {"lab_icon4_red", 55, 49}, {"lab_icon4_grey", 55, 49},
      {"lab_icon5_green", 65, 48}, {"lab_icon5_red", 65, 48}, {"lab_icon5_grey", 65, 48},
      {"lab_accept_green", 60, 49}, {"lab_accept_red", 60, 49}, {"lab_accept_grey", 60, 49},
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
  const int32_t top = 13, bottom = 33, left = 12, right = r.w - 16;  // inside the right panels at 453 (PM's lists)
  for (int32_t y = top; y < bottom; ++y)
    for (int32_t x = left; x < right; ++x) c.set(x, y, x < left + 3 ? Rgba{255, 176, 64, 235} : Rgba{255, 150, 30, 64});
  for (int32_t x = left + 3; x < right; ++x) c.set(x, top, {255, 176, 64, 120}), c.set(x, bottom - 1, {255, 176, 64, 120});
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
  const int32_t x0 = 9, y0 = 9, w = r.w - 18, h = r.h - 18;  // the glyph's box
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

std::vector<images::Image> lab_panels(const images::Image& hangar) {
  constexpr int32_t kMid = 640, kLeft = 220, kCentre = 200;
  const int32_t off = (hangar.width - kMid) / 2;
  // A strip of the hangar, darkened to `keep` percent with faint scanlines.
  auto strip = [&](int32_t from, int32_t w, int32_t keep) {
    Canvas c(w, images::kScreenH);
    for (int32_t y = 0; y < c.h && y < hangar.height; ++y)
      for (int32_t x = 0; x < w; ++x) {
        const int32_t sx = std::clamp(off + from + x, 0, hangar.width - 1);
        const uint16_t p = hangar.pixels[static_cast<size_t>(y * hangar.width + sx)];
        const int32_t k = keep * (y % 2 ? 88 : 100) / 100;
        c.set(x, y, {((p >> 11) << 3) * k / 100, (((p >> 5) & 63) << 2) * k / 100, ((p & 31) << 3) * k / 100, 255});
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
    for (int32_t y = 414; y < c.h; ++y)
      for (int32_t x = 0; x < c.w; ++x) c.over(x, y, {8, 10, 13, 190});
    for (int32_t x = 0; x < c.w; ++x) c.set(x, 414, {200, 130, 40, 200});
  };
  std::vector<images::Image> v;
  for (bool right : {false, true})
    for (bool two : {false, true}) {
      Canvas c = strip(right ? kLeft + kCentre : 0, kLeft, 45);
      // The rule under the left's titles (the right's tabs sit where it would be).
      const int32_t rule = right ? 0 : 38;
      if (two) {
        box(c, 6, 6, kLeft - 6, 242, rule);
        box(c, 6, 248, kLeft - 6, 410, 23);  // at 271: the lower lists' first rows start at 275 (PM's layout)
      } else {
        box(c, 6, 6, kLeft - 6, 410, rule);
      }
      tray(c);
      v.push_back(c.image(reskin_name(right ? (two ? "lab_panel_rightdouble" : "lab_panel_rightsingle")
                                            : (two ? "lab_panel_leftdouble" : "lab_panel_leftsingle"))));
    }
  // The centre: the hangar brighter round the mech, a glass box under it for its parts, data, icons and messages.
  Canvas c = strip(kLeft, kCentre, 80);
  box(c, 6, 280, kCentre - 6, 474, 0);
  v.push_back(c.image(reskin_name("lab_panel_center")));
  // The backdrop: the whole middle, as the sides' (on a wide canvas the engine mirrors its edges out).
  Canvas b = strip(0, kMid, 45);
  tray(b);
  v.push_back(b.image(reskin_name("lab_background")));
  for (images::Image& im : v) im.alpha.clear();  // opaque, as PM's (the engine widens an opaque backdrop)
  return v;
}

std::vector<images::Image> all() {
  std::vector<images::Image> v = {plaque(), plaque(kPlaqueNarrowW, kPlaqueNarrow), plaque(kStripW, kStrip), status_bar(), dialog_frame(), dim(), cursor()};
  for (int32_t s = 0; s < 3; ++s) v.push_back(tile_frame(s));
  for (int32_t s = 0; s < 4; ++s) v.push_back(button(s));
  for (const PanelSize& p : panel_sizes()) v.push_back(panel(p.w, p.h));
  for (const PanelSize& p : terminal_sizes()) v.push_back(terminal(p.w, p.h));
  for (const PanelSize& p : frame_sizes())
    for (bool lit : {false, true}) v.push_back(frame(p.w, p.h, lit));
  for (bool up : {true, false})
    for (int32_t s = 0; s < 4; ++s) v.push_back(arrow(up, s));
  for (bool right : {false, true})
    for (int32_t s = 0; s < 4; ++s) v.push_back(side_arrow(right, s));
  for (bool on : {false, true}) v.push_back(slider(on)), v.push_back(check(on));
  for (int32_t s = 0; s < 3; ++s) v.push_back(cell(s));
  v.push_back(dialog_tall());
  for (const std::vector<Reskin>* t : {&lab_pictures(), &filebox_pictures()})
    for (const Reskin& r : *t)
      if (!is_lab_panel(r)) v.push_back(reskin(r));
  v.push_back(clear());
  return v;
}

}  // namespace mw1::chrome
