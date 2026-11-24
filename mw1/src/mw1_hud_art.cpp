#include "mw1_hud_art.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"

namespace mw1::hud_art {
namespace {

struct Rgb {
  uint8_t r, g, b;
};
using Shape = std::function<bool(double x, double y)>;  // canvas pixels, a pixel's centre at its + 0.5
struct Layer {
  Rgb colour;
  Shape in;
};
struct Piece {
  int32_t w = 0, h = 0;
  std::vector<Layer> layers;  // drawn in order, each over the last
};

// ---- shapes ----

struct P {
  double x, y;
};
// Inside a convex polygon given either way round.
Shape polygon(std::vector<P> pts) {
  return [pts = std::move(pts)](double x, double y) {
    int32_t pos = 0, neg = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
      const P& a = pts[i];
      const P& b = pts[(i + 1) % pts.size()];
      const double c = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
      (c > 0 ? pos : neg) += c != 0;
    }
    return pos == 0 || neg == 0;
  };
}
Shape disc(double cx, double cy, double r) {
  return [=](double x, double y) { return (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r; };
}
Shape ring(double cx, double cy, double outer, double inner) {
  return [=](double x, double y) {
    const double d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    return d < outer * outer && d >= inner * inner;
  };
}
Shape rect(double l, double t, double r, double b) {
  return [=](double x, double y) { return x >= l && x < r && y >= t && y < b; };
}
Shape minus(Shape a, Shape b) {
  return [=](double x, double y) { return a(x, y) && !b(x, y); };
}
// A closed path of strokes `width` wide (round at the corners).
Shape strokes(std::vector<P> pts, double width) {
  return [pts = std::move(pts), h = width / 2](double x, double y) {
    for (size_t i = 0; i < pts.size(); ++i) {
      const P& a = pts[i];
      const P& b = pts[(i + 1) % pts.size()];
      const double dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
      const double t = len2 > 0 ? std::clamp(((x - a.x) * dx + (y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
      const double ex = a.x + t * dx - x, ey = a.y + t * dy - y;
      if (ex * ex + ey * ey < h * h) return true;
    }
    return false;
  };
}
// PM's drop shadow: the shape again one pixel right, down, and both.
Shape shadow_of(Shape s) {
  return [=](double x, double y) { return s(x - 1, y) || s(x, y - 1) || s(x - 1, y - 1); };
}
// The shape seen through a change of axes: `to` takes the piece's point to the shape's.
Shape through(Shape s, std::function<P(double, double)> to) {
  return [=](double x, double y) {
    const P p = to(x, y);
    return s(p.x, p.y);
  };
}

// ---- the pictures ----

// An arrow pointing up in a 25 x 14 box: a fill under a one-pixel rim (the rim the whole base row).
Piece arrow(const std::string& dir, Rgb fill, Rgb rim) {
  const Shape outer = polygon({{12.5, 0}, {25, 12.5}, {25, 14}, {0, 14}, {0, 12.5}});
  const Shape inner = polygon({{12.5, 1}, {24.5, 13}, {0.5, 13}});
  std::function<P(double, double)> to = [](double x, double y) { return P{x, y}; };
  Piece p{25, 14, {}};
  if (dir == "down") to = [](double x, double y) { return P{x, 14 - y}; };
  if (dir == "left") to = [](double x, double y) { return P{y, x}; }, p.w = 14, p.h = 25;
  if (dir == "right") to = [](double x, double y) { return P{y, 14 - x}; }, p.w = 14, p.h = 25;
  p.layers = {{rim, through(outer, to)}, {fill, through(inner, to)}};
  return p;
}

// A shape over its drop shadow.
Piece shadowed(int32_t w, int32_t h, Shape s, Rgb bright, Rgb dark) { return {w, h, {{dark, shadow_of(s)}, {bright, s}}}; }

const std::map<std::string, Piece>& pieces() {
  static const std::map<std::string, Piece> all = [] {
    std::map<std::string, Piece> m;
    const Rgb ally{8, 8, 192}, ally_dark{0, 4, 96}, enemy{216, 8, 8}, enemy_dark{88, 4, 0}, other{0, 204, 0},
        other_dark{0, 88, 0};
    const struct {
      const char* kind;
      Rgb fill, rim;
    } arrows[] = {{"ally", ally, ally_dark}, {"en", enemy, enemy_dark}, {"mfb", {104, 108, 104}, {40, 40, 40}},
                  {"nav", {248, 244, 0}, {112, 84, 0}}};
    for (const auto& a : arrows)
      for (const char* d : {"up", "down", "left", "right"})
        m["hud_" + std::string(a.kind) + "arrow_" + d] = arrow(d, a.fill, a.rim);
    // The radar's blips: a 'Mech a square ring, a target and a turret a disc, a vehicle a triangle.
    const struct {
      const char* side;
      Rgb bright, dark;
    } sides[] = {{"ally", ally, ally_dark}, {"enemy", enemy, enemy_dark}, {"other", other, other_dark}};
    for (const auto& s : sides) {
      const std::string side = s.side;
      m["hud_mech_" + side + "_sm"] = shadowed(5, 5, minus(rect(0, 0, 4, 4), rect(1, 1, 3, 3)), s.bright, s.dark);
      m["hud_target_" + side + "_sm"] = shadowed(5, 5, disc(2, 2, 2), s.bright, s.dark);
      m["hud_turret_" + side + "_sm"] = shadowed(5, 5, disc(2, 2, 2), s.bright, s.dark);
      m["hud_veh_" + side + "_sm"] =
          shadowed(6, 4, [tri = polygon({{2.5, -0.25}, {5.75, 3}, {-0.75, 3}})](double x, double y) { return y < 3 && tri(x, y); },
                   s.bright, s.dark);
    }
    // The waypoints: a ring whose shadow falls into its hole only along the hole's top-left edge (the hole, half a
    // pixel down and right, stays clear).
    auto waypoint = [&](Rgb bright, Rgb dark) {
      const Shape r = ring(3, 3, 3.2, 1.85);
      return Piece{7, 7, {{dark, minus(shadow_of(r), disc(3.5, 3.5, 1.7))}, {bright, r}}};
    };
    m["hud_waypoint_icon"] = waypoint({248, 224, 0}, {112, 84, 0});
    m["hud_mfbwaypoint_icon"] = waypoint({120, 124, 120}, {48, 48, 48});
    // The weapon list's marks: the selected group a solid triangle, a chained one its outline.
    const std::vector<P> mark = {{0, 0}, {1.5, 0}, {5, 3.5}, {1.5, 7}, {0, 7}};
    m["hud_weapon_select"] = {5, 7, {{{248, 0, 40}, polygon(mark)}}};
    m["hud_weapon_chain"] = {5, 7, {{{0, 252, 0}, strokes({{0.5, 0.5}, {1.5, 0.5}, {4.5, 3.5}, {1.5, 6.5}, {0.5, 6.5}}, 1)}}};
    // The HUD's toggler (PM's lab art by its own name; MW1's lab draws its reskin, mw1_lab_toggle*).
    const Shape right = polygon({{0, 0}, {5, 4.5}, {0, 9}});
    const Shape left = through(right, [](double x, double y) { return P{5 - x, y}; });
    for (const auto& [look, c] : {std::pair<const char*, Rgb>{"green", {0, 252, 0}}, {"red", {248, 180, 0}}}) {
      m[std::string("lab_toggler_") + look] = {5, 9, {{c, right}}};
      m[std::string("lab_togglel_") + look] = {5, 9, {{c, left}}};
    }
    return m;
  }();
  return all;
}

// ---- the pictures read from PM's at run time ----
// The line art (the frames and their digits, the heat meter's ladder, the zoom's crosshairs, the compass's tape): one-
// pixel strokes in a few colours, each under its drop shadow in a darker one, all of it on the pixel grid but for the
// compass's letters' diagonals: a pixel font whose one-pixel steps are meant (the 3's gaps, the 5's corner), so not
// fitted as the 'Mech outlines' stairs are. Each colour's pixels are drawn as their squares (at canvas size PM's
// exactly), and two that touch only at a corner are joined by a stroke a pixel wide between their centres (the N's
// diagonal a line, not stairs); the darkest colour first, the shadows under the lines. The heat bar: three stacked bars, its colour
// running down them (each row's own, between the rows' centres). The compass frame: a band of PM's dark green whose
// alpha rises from its ends (each column's, between the columns' centres) under the bright centre tick. The lights: a
// disc (its centre and area PM's alpha's) shaded as PM's are (the colour between the pixels' centres). The reticle:
// its ring a circle fitted to PM's (draw_reticle). The damage display's backgrounds (a 'Mech's silhouette over a grid,
// at two alphas): their pixels, as the line art's.
enum class Read { kLines, kHeatBar, kCompassFrame, kLight, kReticle, kAlphaPixels };
struct ReadPiece {
  int32_t w, h;
  Read kind;
};
const std::map<std::string, ReadPiece>& read_pieces() {
  static const std::map<std::string, ReadPiece> m = {
      {"hud_coolant_frame", {20, 40, Read::kLines}}, {"hud_fuel_frame", {27, 38, Read::kLines}},
      {"hud_speed_frame", {28, 78, Read::kLines}},   {"hud_heat_meter", {17, 112, Read::kLines}},
      {"hud_zoomcant", {21, 21, Read::kLines}},      {"hud_zoomoff", {21, 21, Read::kLines}},
      {"hud_zoomon", {21, 21, Read::kLines}},        {"hud_zoomover", {21, 21, Read::kLines}},
      {"hud_compass", {1668, 10, Read::kLines}},     {"hud_heat_bar", {15, 110, Read::kHeatBar}},
      {"hud_compass_frame", {148, 19, Read::kCompassFrame}}, {"hud_light_green", {8, 8, Read::kLight}},
      {"hud_light_red", {8, 8, Read::kLight}},       {"hud_light_yellow", {8, 8, Read::kLight}},
      // The reticle's four looks (hud.cpp; veh_stealth.cpp's ret_off) and the damage display's backgrounds.
      {"ret_off", {40, 48, Read::kReticle}},         {"ret_on", {40, 48, Read::kReticle}},
      {"ret_over", {40, 48, Read::kReticle}},        {"ret_cant", {40, 48, Read::kReticle}},
      {"dmg_bg_blue", {75, 91, Read::kAlphaPixels}}, {"dmg_bg_grn", {75, 91, Read::kAlphaPixels}},
      {"dmg_bg_red", {75, 91, Read::kAlphaPixels}},
      // FIXES.md item 109 (the owner: the damage display showed some parts, an arm alone): the eight parts' pictures in
      // their four states (hud.cpp kParts; PM has no _blk), drawn here as the backgrounds are. Left to the canvas
      // they were under the hires grid, and which showed changed frame to frame.
      {"dmg_head_blue", {15, 10, Read::kAlphaPixels}}, {"dmg_head_grn", {15, 10, Read::kAlphaPixels}}, {"dmg_head_yel", {15, 10, Read::kAlphaPixels}}, {"dmg_head_red", {15, 10, Read::kAlphaPixels}},
      {"dmg_ltorso_blue", {10, 32, Read::kAlphaPixels}}, {"dmg_ltorso_grn", {10, 32, Read::kAlphaPixels}}, {"dmg_ltorso_yel", {10, 32, Read::kAlphaPixels}}, {"dmg_ltorso_red", {10, 32, Read::kAlphaPixels}},
      {"dmg_rtorso_blue", {10, 32, Read::kAlphaPixels}}, {"dmg_rtorso_grn", {10, 32, Read::kAlphaPixels}}, {"dmg_rtorso_yel", {10, 32, Read::kAlphaPixels}}, {"dmg_rtorso_red", {10, 32, Read::kAlphaPixels}},
      {"dmg_ctorso_blue", {13, 33, Read::kAlphaPixels}}, {"dmg_ctorso_grn", {13, 33, Read::kAlphaPixels}}, {"dmg_ctorso_yel", {13, 33, Read::kAlphaPixels}}, {"dmg_ctorso_red", {13, 33, Read::kAlphaPixels}},
      {"dmg_larm_blue", {15, 38, Read::kAlphaPixels}}, {"dmg_larm_grn", {15, 38, Read::kAlphaPixels}}, {"dmg_larm_yel", {15, 38, Read::kAlphaPixels}}, {"dmg_larm_red", {15, 38, Read::kAlphaPixels}},
      {"dmg_rarm_blue", {15, 38, Read::kAlphaPixels}}, {"dmg_rarm_grn", {15, 38, Read::kAlphaPixels}}, {"dmg_rarm_yel", {15, 38, Read::kAlphaPixels}}, {"dmg_rarm_red", {15, 38, Read::kAlphaPixels}},
      {"dmg_lleg_blue", {29, 47, Read::kAlphaPixels}}, {"dmg_lleg_grn", {29, 47, Read::kAlphaPixels}}, {"dmg_lleg_yel", {29, 47, Read::kAlphaPixels}}, {"dmg_lleg_red", {29, 47, Read::kAlphaPixels}},
      {"dmg_rleg_blue", {29, 47, Read::kAlphaPixels}}, {"dmg_rleg_grn", {29, 47, Read::kAlphaPixels}}, {"dmg_rleg_yel", {29, 47, Read::kAlphaPixels}}, {"dmg_rleg_red", {29, 47, Read::kAlphaPixels}}};
  return m;
}

// PM's picture as RGB and alpha (no alpha: black clear), or false when it isn't there at the size expected.
struct Pm {
  int32_t w = 0, h = 0;
  std::vector<Rgb> rgb;
  std::vector<uint8_t> alpha;
};
bool read_pm(const std::string& name, int32_t w, int32_t h, Pm* out) {
  zimg::Texture* t = zimg::fetch_image(name.c_str());
  if (!t) return false;
  const bool ok = t->width == w && t->height == h && (t->flags & zimg::kTwoBytes) && !t->palette_count && t->pixels;
  if (ok) {
    out->w = w, out->h = h;
    const uint16_t* p = static_cast<const uint16_t*>(t->pixels);
    for (int32_t i = 0; i < w * h; ++i) {
      const uint16_t v = p[i];
      out->rgb.push_back({static_cast<uint8_t>((v >> 11) << 3), static_cast<uint8_t>(((v >> 5) & 63) << 2), static_cast<uint8_t>((v & 31) << 3)});
      out->alpha.push_back(t->alpha ? t->alpha[i] : (v ? 255 : 0));
    }
  }
  zimg::free_texture(t);  // fetch_image reads a new copy for every get
  return ok;
}

int32_t luma(Rgb c) { return 299 * c.r + 587 * c.g + 114 * c.b; }

// One RGBA pixel `src` (straight alpha) over `dst`.
inline void over_px(uint8_t* dst, const uint8_t* src) {
  const float a = src[3] / 255.0f, b = dst[3] / 255.0f, oa = a + b * (1 - a);
  if (oa <= 0) return;
  for (size_t k = 0; k < 3; ++k) dst[k] = static_cast<uint8_t>(std::lround((src[k] * a + dst[k] * b * (1 - a)) / oa));
  dst[3] = static_cast<uint8_t>(std::lround(oa * 255.0f));
}
// `src` over `dst`, both RGBA bytes, the same size.
void over(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src) {
  for (size_t i = 0; i + 3 < dst.size() && i + 3 < src.size(); i += 4) over_px(&dst[i], &src[i]);
}

// One colour's pixels as squares, and a stroke one pixel wide joining the centres of two that touch only at a corner.
// A named functor (not a lambda) so paint() can find it in the Shape, test its samples directly and visit only the
// canvas pixels it can reach (FIXES.md item 132: the damage display's backdrop, 102 such layers, took 1.3 s at 4K
// through std::function and a std::map per sample over every pixel). The same test either way: the same pictures.
struct PixelShape {
  int32_t w = 0, h = 0;
  std::shared_ptr<const std::vector<uint8_t>> on;     // w * h: the pixel is the colour's
  std::shared_ptr<const std::vector<uint8_t>> joins;  // w * h: the 2 x 2 cell at its top-left pixel joins 1 down-right, 2 down-left
  int32_t left = 0, top = 0, right = 0, bottom = 0;   // the pixels' box (right and bottom one past); empty when none
  bool at(int32_t x, int32_t y) const { return x >= 0 && y >= 0 && x < w && y < h && (*on)[static_cast<size_t>(y) * w + x]; }
  bool operator()(double x, double y) const {
    const int32_t ix = static_cast<int32_t>(std::floor(x)), iy = static_cast<int32_t>(std::floor(y));
    if (at(ix, iy)) return true;
    for (int32_t cy = iy - 1; cy <= iy; ++cy)
      for (int32_t cx = ix - 1; cx <= ix; ++cx) {
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) continue;
        const int32_t j = (*joins)[static_cast<size_t>(cy) * w + cx];
        if (!j) continue;
        // Distance to the centre-to-centre segment of the cell's diagonal(s), under half a pixel.
        for (int32_t k = 1; k <= 2; ++k) {
          if (!(j & k)) continue;
          const double ax = cx + (k == 1 ? 0.5 : 1.5), ay = cy + 0.5, bx = cx + (k == 1 ? 1.5 : 0.5), by = cy + 1.5;
          const double dx = bx - ax, dy = by - ay, t = std::clamp(((x - ax) * dx + (y - ay) * dy) / 2.0, 0.0, 1.0);
          const double ex = ax + t * dx - x, ey = ay + t * dy - y;
          if (ex * ex + ey * ey < 0.25) return true;
        }
      }
    return false;
  }
};
// A layer coloured point by point: `tone` gives straight RGB and an alpha (0..1) at a canvas point.
using Tone = std::function<void(double x, double y, float* rgba)>;
// The device pixel (i, j)'s `hits` of n x n samples laid over `out` in the layer's tone. Each layer is laid over the
// last pixel by pixel: a pixel no sample of the layer hits is left as it was (over a clear source is no change).
inline void lay(std::vector<uint8_t>& out, int32_t dw, int32_t s, int32_t n, int32_t i, int32_t j, int32_t hits, const Tone& tone) {
  float t[4];
  tone((i + 0.5) / s, (j + 0.5) / s, t);
  uint8_t d[4];
  for (int32_t k = 0; k < 3; ++k) d[k] = static_cast<uint8_t>(std::lround(std::clamp(t[k], 0.0f, 255.0f)));
  d[3] = static_cast<uint8_t>(std::lround(std::clamp(t[3] * hits / (n * n), 0.0f, 1.0f) * 255.0f));
  over_px(&out[(static_cast<size_t>(j) * dw + i) * 4], d);
}
template <typename In>
inline int32_t hits_at(const In& in, int32_t s, int32_t n, int32_t i, int32_t j) {
  int32_t hits = 0;
  for (int32_t b = 0; b < n; ++b)
    for (int32_t a = 0; a < n; ++a) hits += in((i + (a + 0.5) / n) / s, (j + (b + 0.5) / n) / s) ? 1 : 0;
  return hits;
}
bool g_reference = false;  // set_reference_painter
// The painter before FIXES.md item 132, for the tests: every device pixel of every layer sampled, each layer a whole
// picture laid over the last.
std::vector<uint8_t> paint_reference(int32_t w, int32_t h, int32_t s, const std::vector<std::pair<Shape, Tone>>& layers) {
  const int32_t dw = w * s, dh = h * s, n = s == 1 ? 1 : 4;
  std::vector<uint8_t> out(static_cast<size_t>(dw) * dh * 4, 0);
  for (const auto& [in, tone] : layers) {
    std::vector<uint8_t> layer(out.size(), 0);
    for (int32_t j = 0; j < dh; ++j)
      for (int32_t i = 0; i < dw; ++i) {
        int32_t hits = 0;
        for (int32_t b = 0; b < n; ++b)
          for (int32_t a = 0; a < n; ++a) hits += in((i + (a + 0.5) / n) / s, (j + (b + 0.5) / n) / s) ? 1 : 0;
        if (!hits) continue;
        float t[4];
        tone((i + 0.5) / s, (j + 0.5) / s, t);
        uint8_t* d = &layer[(static_cast<size_t>(j) * dw + i) * 4];
        for (int32_t k = 0; k < 3; ++k) d[k] = static_cast<uint8_t>(std::lround(std::clamp(t[k], 0.0f, 255.0f)));
        d[3] = static_cast<uint8_t>(std::lround(std::clamp(t[3] * hits / (n * n), 0.0f, 1.0f) * 255.0f));
      }
    over(out, layer);
  }
  return out;
}
std::vector<uint8_t> paint(int32_t w, int32_t h, int32_t s, const std::vector<std::pair<Shape, Tone>>& layers) {
  if (g_reference) return paint_reference(w, h, s, layers);
  const int32_t dw = w * s, dh = h * s, n = s == 1 ? 1 : 4;
  std::vector<uint8_t> out(static_cast<size_t>(dw) * dh * 4, 0);
  for (const auto& [in, tone] : layers) {
    if (const PixelShape* px = in.target<PixelShape>()) {
      // A device pixel's samples all lie in one canvas pixel (s is whole), (i / s, j / s). A sample there is in only
      // when that pixel is the colour's (then every sample is) or a corner join of the 2 x 2 cells it touches passes
      // near: other canvas pixels are skipped, their device pixels not hit.
      const PixelShape& p = *px;
      for (int32_t y = p.top; y < p.bottom; ++y)
        for (int32_t x = p.left; x < p.right; ++x) {
          const bool full = p.at(x, y);
          bool near = false;
          for (int32_t cy = y - 1; cy <= y && !full && !near; ++cy)
            for (int32_t cx = x - 1; cx <= x && !near; ++cx)
              near = cx >= 0 && cy >= 0 && (*p.joins)[static_cast<size_t>(cy) * p.w + cx] != 0;
          if (!full && !near) continue;
          for (int32_t j = y * s; j < (y + 1) * s; ++j)
            for (int32_t i = x * s; i < (x + 1) * s; ++i) {
              const int32_t hits = full ? n * n : hits_at(p, s, n, i, j);
              if (hits) lay(out, dw, s, n, i, j, hits, tone);
            }
        }
    } else {
      for (int32_t j = 0; j < dh; ++j)
        for (int32_t i = 0; i < dw; ++i)
          if (const int32_t hits = hits_at(in, s, n, i, j)) lay(out, dw, s, n, i, j, hits, tone);
    }
  }
  return out;
}
Tone flat(Rgb c, float alpha = 1) {
  return [=](double, double, float* t) { t[0] = c.r, t[1] = c.g, t[2] = c.b, t[3] = alpha; };
}
// Values at the centres of n cells, read between them at u (cells of size 1 from 0).
float between(const std::vector<float>& v, double u) {
  const double f = std::clamp(u - 0.5, 0.0, static_cast<double>(v.size() - 1));
  const size_t i = static_cast<size_t>(f);
  const double k = f - i;
  return static_cast<float>(i + 1 < v.size() ? v[i] * (1 - k) + v[i + 1] * k : v[i]);
}

// `which` pixels as a PixelShape.
Shape pixels_where(const Pm& pm, const std::function<bool(size_t i)>& which) {
  PixelShape ps;
  ps.w = pm.w, ps.h = pm.h;
  auto on = std::make_shared<std::vector<uint8_t>>(pm.rgb.size(), 0);
  ps.left = pm.w, ps.top = pm.h;
  for (size_t i = 0; i < on->size(); ++i)
    if (((*on)[i] = which(i))) {
      const int32_t x = static_cast<int32_t>(i % pm.w), y = static_cast<int32_t>(i / pm.w);
      ps.left = std::min(ps.left, x), ps.top = std::min(ps.top, y);
      ps.right = std::max(ps.right, x + 1), ps.bottom = std::max(ps.bottom, y + 1);
    }
  if (ps.left >= ps.right) ps.left = ps.top = ps.right = ps.bottom = 0;
  ps.on = on;
  auto joins = std::make_shared<std::vector<uint8_t>>(on->size(), 0);
  for (int32_t y = 0; y + 1 < pm.h; ++y)
    for (int32_t x = 0; x + 1 < pm.w; ++x) {
      const bool a = ps.at(x, y), b = ps.at(x + 1, y), cc = ps.at(x, y + 1), d = ps.at(x + 1, y + 1);
      (*joins)[static_cast<size_t>(y) * pm.w + x] = static_cast<uint8_t>((a && d && !b && !cc ? 1 : 0) | (b && cc && !a && !d ? 2 : 0));
    }
  ps.joins = joins;
  return ps;
}

Shape pixels_of(const Pm& pm, Rgb c) {
  return pixels_where(pm, [&pm, c](size_t i) { return pm.rgb[i].r == c.r && pm.rgb[i].g == c.g && pm.rgb[i].b == c.b; });
}

// Pictures with alpha drawn as their pixels: each colour and alpha its own layer (squares, corner joins), faintest
// first. `skip` leaves pixels out (the reticle's ring, drawn as a circle).
void alpha_layers(const Pm& pm, const std::function<bool(size_t)>& skip, std::vector<std::pair<Shape, Tone>>* layers) {
  std::map<uint64_t, std::pair<Rgb, uint8_t>> groups;
  for (size_t i = 0; i < pm.rgb.size(); ++i) {
    if (!pm.alpha[i] || skip(i)) continue;
    const Rgb& c = pm.rgb[i];
    groups[(static_cast<uint64_t>(pm.alpha[i]) << 24) | (static_cast<uint64_t>(c.r) << 16) | (c.g << 8) | c.b] = {c, pm.alpha[i]};
  }
  for (const auto& [key, g] : groups) {
    const Rgb c = g.first;
    const uint8_t a = g.second;
    layers->push_back({pixels_where(pm, [&pm, &skip, c, a](size_t i) {
                         return pm.alpha[i] == a && !skip(i) && pm.rgb[i].r == c.r && pm.rgb[i].g == c.g && pm.rgb[i].b == c.b;
                       }),
                       flat(c, a / 255.0f)});
  }
}

std::vector<uint8_t> draw_alpha_pixels(const Pm& pm, int32_t s) {
  std::vector<std::pair<Shape, Tone>> layers;
  alpha_layers(pm, [](size_t) { return false; }, &layers);
  return paint(pm.w, pm.h, s, layers);
}

// The reticle: PM's ring (its bright pixels away from the centre dot) fitted as a circle, its centre and radius from
// their alpha; under it the dim backing and over it the centre dot as their pixels.
std::vector<uint8_t> draw_reticle(const Pm& pm, int32_t s) {
  auto bright = [&pm](size_t i) { return pm.alpha[i] > 0 && luma(pm.rgb[i]) > 299 * 40 + 587 * 40 + 114 * 40; };
  double sw = 0, cx = 0, cy = 0;
  for (size_t i = 0; i < pm.rgb.size(); ++i)
    if (bright(i)) {
      const double a = pm.alpha[i] / 255.0;
      sw += a, cx += a * (i % pm.w + 0.5), cy += a * (i / pm.w + 0.5);
    }
  if (sw <= 0) return draw_alpha_pixels(pm, s);
  cx /= sw, cy /= sw;
  auto ring_px = [&](size_t i) {
    const double dx = i % pm.w + 0.5 - cx, dy = i / pm.w + 0.5 - cy;
    return bright(i) && dx * dx + dy * dy > 9;
  };
  // The ring's colour its brightest pixels' and its alpha theirs (the most of them at it); its width keeps PM's light:
  // all its pixels' alpha x brightness over that alpha x brightness, along the circle.
  double rsum = 0, cover = 0, light = 0;
  Rgb colour{0, 0, 0};
  for (size_t i = 0; i < pm.rgb.size(); ++i)
    if (ring_px(i) && luma(pm.rgb[i]) > luma(colour)) colour = pm.rgb[i];
  std::map<uint8_t, int32_t> at;
  for (size_t i = 0; i < pm.rgb.size(); ++i)
    if (ring_px(i)) {
      const double a = pm.alpha[i] / 255.0;
      rsum += a * std::hypot(i % pm.w + 0.5 - cx, i / pm.w + 0.5 - cy), cover += a;
      light += a * luma(pm.rgb[i]);
      if (luma(pm.rgb[i]) == luma(colour)) ++at[pm.alpha[i]];
    }
  uint8_t core = 0;
  for (const auto& [a, n] : at)
    if (!core || n > at[core]) core = a;
  std::vector<std::pair<Shape, Tone>> layers;
  alpha_layers(pm, [&](size_t i) { return bright(i); }, &layers);  // the backing
  if (cover > 0 && core) {
    const double r = rsum / cover, width = light / (core / 255.0 * luma(colour)) / (2 * 3.14159265358979 * r);
    layers.push_back({ring(cx, cy, r + width / 2, r - width / 2), flat(colour, core / 255.0f)});
  }
  alpha_layers(pm, [&](size_t i) { return !bright(i) || ring_px(i); }, &layers);  // the dot
  return paint(pm.w, pm.h, s, layers);
}

std::vector<uint8_t> draw_lines(const Pm& pm, int32_t s) {
  std::map<uint32_t, Rgb> colours;
  for (const Rgb& c : pm.rgb)
    if (c.r || c.g || c.b) colours[(static_cast<uint32_t>(c.r) << 16) | (c.g << 8) | c.b] = c;
  std::vector<Rgb> order;
  for (const auto& [key, c] : colours) order.push_back(c);
  std::stable_sort(order.begin(), order.end(), [](Rgb a, Rgb b) { return luma(a) < luma(b); });  // the shadows first
  std::vector<std::pair<Shape, Tone>> layers;
  for (const Rgb& c : order) layers.push_back({pixels_of(pm, c), flat(c)});
  return paint(pm.w, pm.h, s, layers);
}

std::vector<uint8_t> draw_heat_bar(const Pm& pm, int32_t s) {
  // Each row's colour, its lit pixels' mean.
  std::vector<float> ch[3];
  for (int32_t y = 0; y < pm.h; ++y) {
    double sum[3] = {0, 0, 0};
    int32_t n = 0;
    for (int32_t x = 0; x < pm.w; ++x) {
      const Rgb& c = pm.rgb[static_cast<size_t>(y) * pm.w + x];
      if (!(c.r || c.g || c.b)) continue;
      sum[0] += c.r, sum[1] += c.g, sum[2] += c.b, ++n;
    }
    for (int32_t k = 0; k < 3; ++k) ch[k].push_back(n ? static_cast<float>(sum[k] / n) : (ch[k].empty() ? 0.0f : ch[k].back()));
  }
  const Shape bars = [](double x, double y) {
    return x >= 0 && y >= 0 && ((y < 41 && x < 15) || (y < 61 && x < 11) || (y < 110 && x < 7));
  };
  const Tone run = [&](double, double y, float* t) {
    for (int32_t k = 0; k < 3; ++k) t[k] = between(ch[k], y);
    t[3] = 1;
  };
  return paint(pm.w, pm.h, s, {{bars, run}});
}

std::vector<uint8_t> draw_compass_frame(const Pm& pm, int32_t s) {
  std::vector<float> a;  // each column's alpha over the band's rows (7 to 18)
  for (int32_t x = 0; x < pm.w; ++x) {
    double sum = 0;
    for (int32_t y = 7; y < pm.h; ++y) sum += pm.alpha[static_cast<size_t>(y) * pm.w + x];
    a.push_back(static_cast<float>(sum / (pm.h - 7) / 255.0));
  }
  const Rgb band = pm.rgb[static_cast<size_t>(12) * pm.w + pm.w / 2], tick = pm.rgb[74];
  const Tone fade = [&](double x, double, float* t) { t[0] = band.r, t[1] = band.g, t[2] = band.b, t[3] = between(a, x); };
  return paint(pm.w, pm.h, s, {{rect(0, 7, pm.w, pm.h), fade}, {rect(74, 0, 75, 7), flat(tick)}});
}

std::vector<uint8_t> draw_light(const Pm& pm, int32_t s) {
  double area = 0, cx = 0, cy = 0;
  std::vector<float> ch[3];
  for (auto& c : ch) c.resize(pm.rgb.size());
  for (int32_t y = 0; y < pm.h; ++y)
    for (int32_t x = 0; x < pm.w; ++x) {
      const size_t i = static_cast<size_t>(y) * pm.w + x;
      const double a = pm.alpha[i] / 255.0;
      area += a, cx += a * (x + 0.5), cy += a * (y + 0.5);
      ch[0][i] = pm.rgb[i].r, ch[1][i] = pm.rgb[i].g, ch[2][i] = pm.rgb[i].b;
    }
  if (area <= 0) return std::vector<uint8_t>(static_cast<size_t>(pm.w) * s * pm.h * s * 4, 0);
  cx /= area, cy /= area;
  const double r = std::sqrt(area / 3.14159265358979);
  // The shading between the pixels' centres, from the lit ones only (the clear ones are black).
  const Tone shade = [&, w = pm.w, h = pm.h](double x, double y, float* t) {
    const double fx = std::clamp(x - 0.5, 0.0, w - 1.0), fy = std::clamp(y - 0.5, 0.0, h - 1.0);
    const int32_t x0 = static_cast<int32_t>(fx), y0 = static_cast<int32_t>(fy);
    double sum[3] = {0, 0, 0}, wsum = 0;
    for (int32_t j = 0; j < 2; ++j)
      for (int32_t i = 0; i < 2; ++i) {
        const int32_t xi = std::min(x0 + i, w - 1), yj = std::min(y0 + j, h - 1);
        const size_t k = static_cast<size_t>(yj) * w + xi;
        const double wt = (i ? fx - x0 : 1 - (fx - x0)) * (j ? fy - y0 : 1 - (fy - y0)) * (pm.alpha[k] / 255.0);
        for (int32_t c = 0; c < 3; ++c) sum[c] += wt * ch[c][k];
        wsum += wt;
      }
    for (int32_t c = 0; c < 3; ++c) t[c] = wsum > 0 ? static_cast<float>(sum[c] / wsum) : 0.0f;
    t[3] = 1;
  };
  return paint(pm.w, pm.h, s, {{disc(cx, cy, r), shade}});
}


// ---- the labels (FIXES.md item 82) ----
LabelPainter& painter() {
  static LabelPainter p;
  return p;
}
std::mutex& painter_lock() {
  static std::mutex m;
  return m;
}
bool same(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
bool lit(const Rgb& c) { return c.r || c.g || c.b; }

// The scales' and the coolant's labels: their pixels PM's brightest colour (the lines are a dimmer one), in groups a
// few pixels apart (a number's digits), top to bottom: PM's text for each, in that order.
const std::map<std::string, std::vector<std::string>>& scale_texts() {
  static const std::map<std::string, std::vector<std::string>> m = {
      {"hud_speed_frame", {"100", "50", "0"}}, {"hud_fuel_frame", {"100", "0"}}, {"hud_coolant_frame", {"F", "E"}}};
  return m;
}
// The compass tape: a heading's label every 36 pixels, centred between its major ticks (full height, at 26 + 36 m), on
// 8 + 36 k: 310 degrees first, ten a label; N E S W at 0, 90, 180 and 270 (the full height), the numbers rows 1 to 8.
constexpr int32_t kCompassFirstX = 8, kCompassStep = 36, kCompassFirst = 310, kCompassHalf = 6, kCompassRoom = 15;  // the room: 3 clear of the minor ticks

struct Found {
  Label label;
  std::vector<size_t> pixels;  // PM's: the glyphs' and their shadow's, left out of the line art
  Rgb ink{0, 0, 0}, shadow{0, 0, 0};
  bool has_shadow = false;
};

std::vector<Found> find_scale_labels(const Pm& pm, const std::vector<std::string>& texts) {
  Rgb ink{0, 0, 0};
  for (const Rgb& c : pm.rgb)
    if (luma(c) > luma(ink)) ink = c;
  if (!lit(ink)) return {};
  // The ink's pixels in groups: 8-connected, then any two whose boxes come within 2 pixels across and share rows.
  struct Box {
    int32_t x0, y0, x1, y1;
    std::vector<size_t> px;
  };
  std::vector<Box> boxes;
  std::vector<int32_t> seen(pm.rgb.size(), 0);
  for (size_t i = 0; i < pm.rgb.size(); ++i) {
    if (seen[i] || !same(pm.rgb[i], ink)) continue;
    Box b{pm.w, pm.h, -1, -1, {}};
    std::vector<size_t> todo{i};
    seen[i] = 1;
    while (!todo.empty()) {
      const size_t at = todo.back();
      todo.pop_back();
      const int32_t x = static_cast<int32_t>(at % pm.w), y = static_cast<int32_t>(at / pm.w);
      b.px.push_back(at), b.x0 = std::min(b.x0, x), b.y0 = std::min(b.y0, y), b.x1 = std::max(b.x1, x), b.y1 = std::max(b.y1, y);
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= pm.w || ny >= pm.h) continue;
          const size_t n = static_cast<size_t>(ny) * pm.w + nx;
          if (!seen[n] && same(pm.rgb[n], ink)) seen[n] = 1, todo.push_back(n);
        }
    }
    boxes.push_back(std::move(b));
  }
  for (bool merged = true; merged;) {
    merged = false;
    for (size_t a = 0; a < boxes.size() && !merged; ++a)
      for (size_t b = a + 1; b < boxes.size() && !merged; ++b) {
        Box &p = boxes[a], &q = boxes[b];
        const bool rows = p.y0 <= q.y1 && q.y0 <= p.y1;
        const bool near = q.x0 - p.x1 <= 3 && p.x0 - q.x1 <= 3;
        if (!rows || !near) continue;
        p.x0 = std::min(p.x0, q.x0), p.y0 = std::min(p.y0, q.y0), p.x1 = std::max(p.x1, q.x1), p.y1 = std::max(p.y1, q.y1);
        p.px.insert(p.px.end(), q.px.begin(), q.px.end());
        boxes.erase(boxes.begin() + static_cast<std::ptrdiff_t>(b));
        merged = true;
      }
  }
  if (boxes.size() != texts.size()) return {};  // not PM's as expected: keep its pixels
  std::sort(boxes.begin(), boxes.end(), [](const Box& a, const Box& b) { return a.y0 < b.y0; });
  // The drop shadow: the colour most under the ink one pixel right and down (not ink itself).
  std::map<uint32_t, std::pair<Rgb, int32_t>> under;
  for (const Box& b : boxes)
    for (size_t at : b.px) {
      const int32_t x = static_cast<int32_t>(at % pm.w) + 1, y = static_cast<int32_t>(at / pm.w) + 1;
      if (x >= pm.w || y >= pm.h) continue;
      const Rgb& c = pm.rgb[static_cast<size_t>(y) * pm.w + x];
      if (!lit(c) || same(c, ink)) continue;
      auto& u = under[(static_cast<uint32_t>(c.r) << 16) | (c.g << 8) | c.b];
      u.first = c, ++u.second;
    }
  Rgb shadow{0, 0, 0};
  int32_t most = 0;
  for (const auto& [k, u] : under)
    if (u.second > most) shadow = u.first, most = u.second;
  std::vector<Found> out;
  for (size_t k = 0; k < boxes.size(); ++k) {
    const Box& b = boxes[k];
    Found f;
    f.ink = ink, f.shadow = shadow, f.has_shadow = most > 0;
    f.pixels = b.px;
    // Its shadow's pixels: right, down and both of an ink pixel, the shadow's colour, not also a line's shadow (a
    // line pixel up, left or both of it).
    if (f.has_shadow)
      for (size_t at : b.px) {
        const int32_t x = static_cast<int32_t>(at % pm.w), y = static_cast<int32_t>(at / pm.w);
        for (const auto& [dx, dy] : {std::pair<int32_t, int32_t>{1, 0}, {0, 1}, {1, 1}}) {
          const int32_t sx = x + dx, sy = y + dy;
          if (sx >= pm.w || sy >= pm.h) continue;
          const size_t si = static_cast<size_t>(sy) * pm.w + sx;
          if (!same(pm.rgb[si], shadow)) continue;
          bool line = false;
          for (const auto& [ux, uy] : {std::pair<int32_t, int32_t>{-1, 0}, {0, -1}, {-1, -1}}) {
            const int32_t lx = sx + ux, ly = sy + uy;
            if (lx < 0 || ly < 0) continue;
            const Rgb& c = pm.rgb[static_cast<size_t>(ly) * pm.w + lx];
            line = line || (lit(c) && !same(c, ink) && !same(c, shadow));
          }
          if (!line) f.pixels.push_back(si);
        }
      }
    // Its box: the ink's rows; a letter (the coolant's, against the picture's left) grows right, a number left toward
    // the picture's edge, its right kept by the scale's tick.
    f.label.text = texts[k];
    f.label.y = b.y0, f.label.h = b.y1 - b.y0 + 1;
    if (!(texts[k][0] >= '0' && texts[k][0] <= '9')) {
      f.label.x = b.x0, f.label.w = b.x1 - b.x0 + 2, f.label.align = -1;
    } else {
      f.label.x = 0, f.label.w = b.x1 + 1, f.label.align = 1;
    }
    out.push_back(std::move(f));
  }
  return out;
}

std::vector<Found> find_compass_labels(const Pm& pm) {
  std::vector<Found> out;
  auto at = [&pm](int32_t x, int32_t y) -> const Rgb& { return pm.rgb[static_cast<size_t>(y) * pm.w + x]; };
  for (int32_t k = 0;; ++k) {
    const int32_t c = kCompassFirstX + kCompassStep * k;
    if (c - kCompassHalf >= pm.w) break;
    // Its major ticks where they fall inside the tape: lit the full height, alone; else PM's isn't as expected.
    for (int32_t t : {c - kCompassStep / 2, c + kCompassStep / 2}) {
      if (t < 0 || t >= pm.w) continue;
      for (int32_t y = 0; y < pm.h; ++y)
        if (!lit(at(t, y))) return {};
      if ((t > 0 && lit(at(t - 1, 0))) || (t + 1 < pm.w && lit(at(t + 1, 0)))) return {};
    }
    const int32_t deg = (kCompassFirst + 10 * k) % 360;
    Found f;
    const char* letter = deg == 0 ? "N" : deg == 90 ? "E" : deg == 180 ? "S" : deg == 270 ? "W" : nullptr;
    f.label.text = letter ? letter : std::to_string(deg);
    f.label.y = letter ? 0 : 1, f.label.h = letter ? pm.h : 8;
    f.label.x = c + 0.5 - kCompassRoom / 2.0, f.label.w = kCompassRoom, f.label.align = 0;
    Rgb ink{0, 0, 0};
    for (int32_t y = 0; y < pm.h; ++y)
      for (int32_t x = std::max(0, c - kCompassHalf); x <= std::min(pm.w - 1, c + kCompassHalf); ++x)
        if (lit(at(x, y))) {
          f.pixels.push_back(static_cast<size_t>(y) * pm.w + x);
          if (luma(at(x, y)) > luma(ink)) ink = at(x, y);
        }
    if (f.pixels.empty()) return {};
    f.ink = ink;
    out.push_back(std::move(f));
  }
  return out;
}

std::vector<Found> find_labels(const std::string& name, const Pm& pm) {
  if (name == "hud_compass") return find_compass_labels(pm);
  auto t = scale_texts().find(name);
  return t == scale_texts().end() ? std::vector<Found>{} : find_scale_labels(pm, t->second);
}

// The labels painted as text over `art` (pw x ph canvas pixels at s): each its shadow (one canvas pixel right and
// down) then its ink. False when the painter gives nothing for one (the art then keeps PM's glyphs).
bool paint_labels(const LabelPainter& paint, const std::vector<Found>& found, int32_t pw, int32_t ph, int32_t s,
                  std::vector<uint8_t>* art) {
  const int32_t dw = pw * s, dh = ph * s;
  std::vector<uint8_t> layer(static_cast<size_t>(dw) * dh * 4, 0);
  auto put = [&](const std::vector<uint8_t>& cov, int32_t bw, int32_t bh, int32_t ox, int32_t oy, Rgb c) {
    std::vector<uint8_t> one(layer.size(), 0);
    for (int32_t j = 0; j < bh; ++j)
      for (int32_t i = 0; i < bw; ++i) {
        const int32_t x = ox + i, y = oy + j;
        const uint8_t a = cov[static_cast<size_t>(j) * bw + i];
        if (!a || x < 0 || y < 0 || x >= dw || y >= dh) continue;
        uint8_t* d = &one[(static_cast<size_t>(y) * dw + x) * 4];
        d[0] = c.r, d[1] = c.g, d[2] = c.b, d[3] = a;
      }
    over(layer, one);
  };
  for (const Found& f : found) {
    const int32_t bw = static_cast<int32_t>(std::lround(f.label.w * s)), bh = static_cast<int32_t>(std::lround(f.label.h * s));
    const std::vector<uint8_t> cov = paint(f.label.text, bw, bh, f.label.align);
    if (cov.size() != static_cast<size_t>(bw) * bh) return false;
    const int32_t ox = static_cast<int32_t>(std::lround(f.label.x * s)), oy = static_cast<int32_t>(std::lround(f.label.y * s));
    if (f.has_shadow) put(cov, bw, bh, ox + s, oy + s, f.shadow);
    put(cov, bw, bh, ox, oy, f.ink);
  }
  over(*art, layer);
  return true;
}

}  // namespace

const std::vector<std::string>& names() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const auto& [name, p] : pieces()) v.push_back(name);
    for (const auto& [name, p] : read_pieces()) v.push_back(name);
    std::sort(v.begin(), v.end());
    return v;
  }();
  return n;
}

bool size_of(const std::string& name, int32_t* w, int32_t* h) {
  if (auto r = read_pieces().find(name); r != read_pieces().end()) {
    *w = r->second.w, *h = r->second.h;
    return true;
  }
  auto it = pieces().find(name);
  if (it == pieces().end()) return false;
  *w = it->second.w, *h = it->second.h;
  return true;
}

bool read_from_pm(const std::string& name) { return read_pieces().count(name) != 0; }

void set_reference_painter(bool on) { g_reference = on; }

void set_label_painter(LabelPainter paint) {
  std::lock_guard<std::mutex> hold(painter_lock());
  painter() = std::move(paint);
}

std::vector<Label> labels_of(const std::string& name) {
  auto r = read_pieces().find(name);
  Pm pm;
  if (r == read_pieces().end() || r->second.kind != Read::kLines || !read_pm(name, r->second.w, r->second.h, &pm)) return {};
  std::vector<Label> out;
  for (const Found& f : find_labels(name, pm)) out.push_back(f.label);
  return out;
}

std::vector<uint8_t> draw(const std::string& name, int32_t s) {
  if (auto r = read_pieces().find(name); r != read_pieces().end()) {
    Pm pm;
    if (s < 1 || !read_pm(name, r->second.w, r->second.h, &pm)) return {};
    switch (r->second.kind) {
      case Read::kLines: {
        LabelPainter paint;
        {
          std::lock_guard<std::mutex> hold(painter_lock());
          paint = painter();
        }
        const std::vector<Found> found = s >= 2 && paint ? find_labels(name, pm) : std::vector<Found>{};
        if (found.empty()) return draw_lines(pm, s);
        Pm bare = pm;  // PM's glyphs left out: the text takes their place
        for (const Found& f : found)
          for (size_t i : f.pixels) bare.rgb[i] = {0, 0, 0}, bare.alpha[i] = 0;
        std::vector<uint8_t> art = draw_lines(bare, s);
        if (!paint_labels(paint, found, pm.w, pm.h, s, &art)) return draw_lines(pm, s);
        return art;
      }
      case Read::kHeatBar: return draw_heat_bar(pm, s);
      case Read::kCompassFrame: return draw_compass_frame(pm, s);
      case Read::kLight: return draw_light(pm, s);
      case Read::kReticle: return draw_reticle(pm, s);
      case Read::kAlphaPixels: return draw_alpha_pixels(pm, s);
    }
    return {};
  }
  auto it = pieces().find(name);
  if (it == pieces().end() || s < 1) return {};
  const Piece& p = it->second;
  const int32_t dw = p.w * s, dh = p.h * s;
  std::vector<float> px(static_cast<size_t>(dw) * dh * 4, 0.0f);  // straight RGB (0..255) and alpha (0..1)
  // At s 1 a canvas pixel takes a layer when its centre is in (PM's picture); above, by the 4 x 4 samples in.
  const int32_t n = s == 1 ? 1 : 4;
  for (const Layer& l : p.layers)
    for (int32_t j = 0; j < dh; ++j)
      for (int32_t i = 0; i < dw; ++i) {
        int32_t hits = 0;
        for (int32_t b = 0; b < n; ++b)
          for (int32_t a = 0; a < n; ++a) hits += l.in((i + (a + 0.5) / n) / s, (j + (b + 0.5) / n) / s) ? 1 : 0;
        if (!hits) continue;
        const float c = static_cast<float>(hits) / static_cast<float>(n * n);
        float* d = &px[(static_cast<size_t>(j) * dw + i) * 4];
        const float oa = c + d[3] * (1 - c);
        const float src[3] = {static_cast<float>(l.colour.r), static_cast<float>(l.colour.g), static_cast<float>(l.colour.b)};
        for (int32_t k = 0; k < 3; ++k) d[k] = (src[k] * c + d[k] * d[3] * (1 - c)) / oa;
        d[3] = oa;
      }
  std::vector<uint8_t> out(px.size());
  for (size_t i = 0; i < px.size(); i += 4) {
    for (size_t k = 0; k < 3; ++k) out[i + k] = static_cast<uint8_t>(std::lround(std::clamp(px[i + k], 0.0f, 255.0f)));
    out[i + 3] = static_cast<uint8_t>(std::lround(std::clamp(px[i + 3], 0.0f, 1.0f) * 255.0f));
  }
  return out;
}

}  // namespace mw1::hud_art
