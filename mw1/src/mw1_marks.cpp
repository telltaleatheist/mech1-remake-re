#include "mw1_marks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gamez/zimage/zimg_library.h"

namespace mw1::marks {

namespace {
constexpr const char* kPrefix = "mw1_mark_";
constexpr size_t kMaxParts = 8;

// How far a part reaches from the anchor pixel: left, top, right, bottom, whole pixels.
void reach(const Part& p, int32_t* l, int32_t* t, int32_t* r, int32_t* b) {
  if (p.shape == Shape::kLine) {
    *l = std::max(0, -p.dx), *t = std::max(0, -p.dy), *r = std::max(0, p.dx), *b = std::max(0, p.dy);
    return;
  }
  *l = *t = *r = *b = p.size;
}

// Whether the part covers the point (px, py), canvas pixels with the anchor's centre at (cx, cy).
bool covers(const Part& p, float cx, float cy, float px, float py) {
  switch (p.shape) {
    case Shape::kRing: {
      const float d = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
      return std::fabs(d - static_cast<float>(p.size)) <= 0.5f;
    }
    case Shape::kSquare: {
      const float l = cx - 0.5f - p.size, t = cy - 0.5f - p.size, r = cx + 0.5f + p.size, b = cy + 0.5f + p.size;
      const bool in_outer = px >= l && px < r && py >= t && py < b;
      const bool in_inner = px >= l + 1 && px < r - 1 && py >= t + 1 && py < b - 1;
      return in_outer && !in_inner;
    }
    case Shape::kLine: {
      // A line of no length is the anchor's pixel, whole (a world's dot's middle: a disc left its corners open).
      if (p.dx == 0 && p.dy == 0) return std::fabs(px - cx) <= 0.5f && std::fabs(py - cy) <= 0.5f;
      const float vx = static_cast<float>(p.dx), vy = static_cast<float>(p.dy), len2 = vx * vx + vy * vy;
      const float u = len2 > 0 ? std::clamp(((px - cx) * vx + (py - cy) * vy) / len2, 0.0f, 1.0f) : 0.0f;
      const float qx = cx + u * vx - px, qy = cy + u * vy - py;
      return qx * qx + qy * qy <= 0.25f;
    }
  }
  return false;
}
}  // namespace

std::string image_name(const Spec& s) {
  char b[32];
  std::snprintf(b, sizeof b, "%s%02x%02x%02x", kPrefix, s.r, s.g, s.b);
  std::string n = b;
  for (const Part& p : s.parts) {
    if (p.shape == Shape::kLine)
      std::snprintf(b, sizeof b, "_l%d,%d", p.dx, p.dy);
    else
      std::snprintf(b, sizeof b, "_%c%d", p.shape == Shape::kRing ? 'r' : 's', p.size);
    n += b;
  }
  return n;
}

bool parse(const std::string& name, Spec* s) {
  const size_t k = std::strlen(kPrefix);
  if (name.rfind(kPrefix, 0) != 0 || name.size() < k + 6) return false;
  Spec out;
  unsigned rgb = 0;
  int used = 0;
  if (std::sscanf(name.c_str() + k, "%6x%n", &rgb, &used) != 1 || used != 6) return false;
  out.r = static_cast<uint8_t>(rgb >> 16), out.g = static_cast<uint8_t>(rgb >> 8), out.b = static_cast<uint8_t>(rgb);
  size_t at = k + 6;
  while (at < name.size()) {
    if (name[at] != '_' || at + 2 > name.size() || out.parts.size() >= kMaxParts) return false;
    const char kind = name[at + 1];
    const char* rest = name.c_str() + at + 2;
    Part p;
    int a = 0, b = 0, n = 0;
    if (kind == 'l') {
      if (std::sscanf(rest, "%d,%d%n", &a, &b, &n) != 2 || std::abs(a) > 1024 || std::abs(b) > 1024) return false;
      p.shape = Shape::kLine, p.dx = a, p.dy = b;
    } else if (kind == 'r' || kind == 's') {
      if (std::sscanf(rest, "%d%n", &a, &n) != 1 || a < 0 || a > 64) return false;
      p.shape = kind == 'r' ? Shape::kRing : Shape::kSquare, p.size = a;
    } else {
      return false;
    }
    if (n <= 0 || (rest[0] == '+')) return false;
    out.parts.push_back(p);
    at += 2 + static_cast<size_t>(n);
  }
  if (out.parts.empty() || image_name(out) != name) return false;  // one spelling for each picture
  if (s) *s = out;
  return true;
}

bool is_mark(const char* name) { return name && parse(name, nullptr); }

void extent(const Spec& s, int32_t* w, int32_t* h, int32_t* ax, int32_t* ay) {
  int32_t L = 0, T = 0, R = 0, B = 0;
  for (const Part& p : s.parts) {
    int32_t l, t, r, b;
    reach(p, &l, &t, &r, &b);
    L = std::max(L, l), T = std::max(T, t), R = std::max(R, r), B = std::max(B, b);
  }
  *ax = L + 1, *ay = T + 1;  // a pixel's margin all round (a ring's band reaches half a pixel past its radius)
  *w = L + R + 3, *h = T + B + 3;
}

images::Image canvas(const Spec& s) {
  images::Image im;
  int32_t w, h, ax, ay;
  extent(s, &w, &h, &ax, &ay);
  im.name = image_name(s), im.width = w, im.height = h;
  im.pixels.assign(static_cast<size_t>(w) * h, images::rgb565(s.r, s.g, s.b));
  im.alpha.assign(static_cast<size_t>(w) * h, 0);
  for (const Part& p : s.parts) {
    if (p.shape == Shape::kLine) {  // Bresenham's pixels, as the canvas's lines were
      int32_t x = ax, y = ay;
      const int32_t x1 = ax + p.dx, y1 = ay + p.dy, sx = p.dx < 0 ? -1 : 1, sy = p.dy < 0 ? -1 : 1;
      const int32_t adx = std::abs(p.dx), ady = std::abs(p.dy);
      int32_t err = adx - ady;
      for (;;) {
        im.alpha[static_cast<size_t>(y) * w + x] = 255;
        if (x == x1 && y == y1) break;
        const int32_t e2 = 2 * err;
        if (e2 > -ady) err -= ady, x += sx;
        if (e2 < adx) err += adx, y += sy;
      }
      continue;
    }
    for (int32_t y = 0; y < h; ++y)
      for (int32_t x = 0; x < w; ++x)
        if (covers(p, ax + 0.5f, ay + 0.5f, x + 0.5f, y + 0.5f)) im.alpha[static_cast<size_t>(y) * w + x] = 255;
  }
  return im;
}

std::vector<uint8_t> rgba(const Spec& s, int32_t scale) {
  int32_t w, h, ax, ay;
  extent(s, &w, &h, &ax, &ay);
  const int32_t sc = std::max(1, scale), fw = w * sc, fh = h * sc;
  std::vector<uint8_t> out(static_cast<size_t>(fw) * fh * 4, 0);
  const float inv = 1.0f / static_cast<float>(sc), cx = ax + 0.5f, cy = ay + 0.5f;
  for (int32_t y = 0; y < fh; ++y)
    for (int32_t x = 0; x < fw; ++x) {
      int32_t hits = 0;
      for (int32_t j = 0; j < 4; ++j)
        for (int32_t i = 0; i < 4; ++i) {
          const float px = (x + (i + 0.5f) / 4.0f) * inv, py = (y + (j + 0.5f) / 4.0f) * inv;
          for (const Part& p : s.parts)
            if (covers(p, cx, cy, px, py)) {
              ++hits;
              break;
            }
        }
      if (!hits) continue;
      uint8_t* q = &out[(static_cast<size_t>(y) * fw + x) * 4];
      q[0] = s.r, q[1] = s.g, q[2] = s.b, q[3] = static_cast<uint8_t>((hits * 255 + 8) / 16);
    }
  return out;
}

void* canvas_texture(const char* name) {
  Spec s;
  if (!name || !parse(name, &s)) return nullptr;
  const images::Image im = canvas(s);
  zimg::Texture* t = zimg::new_texture_565(static_cast<int16_t>(im.width), static_cast<int16_t>(im.height), im.pixels.data());
  if (!t) return nullptr;
  t->alpha = static_cast<uint8_t*>(std::malloc(im.alpha.size()));
  std::memcpy(t->alpha, im.alpha.data(), im.alpha.size());
  t->flags = static_cast<uint8_t>((t->flags & ~zimg::kNoAlpha) | zimg::kFullAlpha | zimg::kAlphaOwned);
  return t;
}

}  // namespace mw1::marks
