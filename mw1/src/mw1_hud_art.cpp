#include "mw1_hud_art.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

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

}  // namespace

const std::vector<std::string>& names() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const auto& [name, p] : pieces()) v.push_back(name);
    return v;
  }();
  return n;
}

bool size_of(const std::string& name, int32_t* w, int32_t* h) {
  auto it = pieces().find(name);
  if (it == pieces().end()) return false;
  *w = it->second.w, *h = it->second.h;
  return true;
}

std::vector<uint8_t> draw(const std::string& name, int32_t s) {
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
