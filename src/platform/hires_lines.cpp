#include "platform/hires_lines.h"

#include <algorithm>
#include <cmath>

namespace platform::hires {

uint32_t canvas_rgb(uint16_t p, bool rgb565) {
  uint32_t r, g, b;
  if (rgb565) r = (p >> 11) * 255 / 31, g = (p >> 5 & 63) * 255 / 63, b = (p & 31) * 255 / 31;
  else r = (p >> 10 & 31) * 255 / 31, g = (p >> 5 & 31) * 255 / 31, b = (p & 31) * 255 / 31;
  return r << 16 | g << 8 | b;
}

namespace {
// FIXES 90: a filled polygon's coverage of each frame pixel: four rows a pixel, each row's spans exact across.
void fill_coverage(const Command& c, float sx, float sy, int32_t W, int32_t H, std::vector<float>* cov) {
  const size_t n = c.poly.size() / 2;
  std::vector<float> xs(n), ys(n), cross;
  for (size_t i = 0; i < n; ++i)
    xs[i] = (static_cast<float>(c.x - c.rect.l) + c.poly[2 * i]) * sx, ys[i] = (static_cast<float>(c.y - c.rect.t) + c.poly[2 * i + 1]) * sy;
  for (int32_t y = 0; y < H; ++y)
    for (int k = 0; k < 4; ++k) {
      const float yy = static_cast<float>(y) + (static_cast<float>(k) + 0.5f) / 4.0f;
      cross.clear();
      for (size_t i = 0, j = n - 1; i < n; j = i++)
        if ((ys[i] <= yy) != (ys[j] <= yy)) cross.push_back(xs[i] + (yy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]));
      std::sort(cross.begin(), cross.end());
      for (size_t p = 0; p + 1 < cross.size(); p += 2) {
        const float a = std::max(0.0f, cross[p]), b = std::min(static_cast<float>(W), cross[p + 1]);
        for (int32_t x = static_cast<int32_t>(std::floor(a)); x < W && static_cast<float>(x) < b; ++x) {
          const float o = std::min(b, static_cast<float>(x + 1)) - std::max(a, static_cast<float>(x));
          if (o > 0) (*cov)[static_cast<size_t>(y) * W + x] += o / 4.0f;
        }
      }
    }
}
}  // namespace

std::vector<uint8_t> line_rgba(const Command& c, float sx, float sy, uint32_t rgb, int32_t* w, int32_t* h) {
  *w = *h = 0;
  if (c.kind != Kind::kLine || c.rect.empty() || sx <= 0 || sy <= 0) return {};
  if (!c.poly.empty()) {  // FIXES 90: a filled polygon
    if (c.poly.size() < 6) return {};
    const int32_t W = static_cast<int32_t>(std::ceil((c.rect.r - c.rect.l) * sx));
    const int32_t H = static_cast<int32_t>(std::ceil((c.rect.b - c.rect.t) * sy));
    if (W <= 0 || H <= 0) return {};
    *w = W, *h = H;
    std::vector<float> cov(static_cast<size_t>(W) * H, 0.0f);
    fill_coverage(c, sx, sy, W, H, &cov);
    std::vector<uint8_t> out(cov.size() * 4, 0);
    const uint32_t r = rgb >> 16 & 0xff, g = rgb >> 8 & 0xff, b = rgb & 0xff;
    for (size_t i = 0; i < cov.size(); ++i) {
      const uint32_t A = static_cast<uint32_t>(std::clamp(cov[i], 0.0f, 1.0f) * 255.0f + 0.5f);
      out[4 * i] = static_cast<uint8_t>(r * A / 255), out[4 * i + 1] = static_cast<uint8_t>(g * A / 255);
      out[4 * i + 2] = static_cast<uint8_t>(b * A / 255), out[4 * i + 3] = static_cast<uint8_t>(A);
    }
    return out;
  }
  if (c.pts.size() < 2) return {};
  const int32_t W = static_cast<int32_t>(std::ceil((c.rect.r - c.rect.l) * sx));
  const int32_t H = static_cast<int32_t>(std::ceil((c.rect.b - c.rect.t) * sy));
  if (W <= 0 || H <= 0) return {};
  *w = W, *h = H;
  std::vector<uint8_t> out(static_cast<size_t>(W) * H * 4, 0);
  // In frame pixels from the rect's corner: a canvas pixel (x, y)'s centre is (x + 0.5, y + 0.5).
  auto fx = [&](int32_t x) { return (static_cast<float>(c.x + x - c.rect.l) + 0.5f) * sx; };
  auto fy = [&](int32_t y) { return (static_cast<float>(c.y + y - c.rect.t) + 0.5f) * sy; };
  const float half = 0.5f * std::min(sx, sy);  // a canvas pixel wide
  std::vector<float> dist(static_cast<size_t>(W) * H, 1e9f);
  auto put = [&](int32_t x, int32_t y, float d) {
    float& o = dist[static_cast<size_t>(y) * W + x];
    o = std::min(o, d);
  };
  const float reach = half + 1.0f;
  if (c.radius > 0) {
    const float cx = fx(c.pts[0]), cy = fy(c.pts[1]);
    const float rx = static_cast<float>(c.radius) * sx, ry = static_cast<float>(c.radius) * sy;
    for (int32_t y = 0; y < H; ++y)
      for (int32_t x = 0; x < W; ++x) {
        const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
        const float k = std::sqrt(dx * dx + dy * dy);
        if (k <= 0) continue;
        // The distance to the ellipse along the ray, in frame pixels (a circle when sx == sy).
        const float along = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
        put(x, y, std::fabs(along - along / k));
      }
  } else {
    for (size_t i = 0; i + 3 < c.pts.size() || (i == 0 && c.pts.size() == 2); i += c.segments ? 4 : 2) {
      const float ax = fx(c.pts[i]), ay = fy(c.pts[i + 1]);
      const float bx = c.pts.size() > i + 3 ? fx(c.pts[i + 2]) : ax, by = c.pts.size() > i + 3 ? fy(c.pts[i + 3]) : ay;
      const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)));
      const int32_t x1 = std::min(W, static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)) + 1);
      const int32_t y0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ay, by) - reach)));
      const int32_t y1 = std::min(H, static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)) + 1);
      const float vx = bx - ax, vy = by - ay, len2 = vx * vx + vy * vy;
      for (int32_t y = y0; y < y1; ++y)
        for (int32_t x = x0; x < x1; ++x) {
          const float px = x + 0.5f - ax, py = y + 0.5f - ay;
          const float t = len2 > 0 ? std::clamp((px * vx + py * vy) / len2, 0.0f, 1.0f) : 0.0f;
          // Round ends, half a pixel past each point as the canvas's last pixel reaches; a list's joins round too.
          const float d = std::hypot(px - t * vx, py - t * vy);
          put(x, y, d);
        }
      if (c.pts.size() == 2) break;
    }
  }
  const uint32_t r = rgb >> 16 & 0xff, g = rgb >> 8 & 0xff, b = rgb & 0xff;
  for (size_t i = 0; i < dist.size(); ++i) {
    const float a = std::clamp(half + 0.5f - dist[i], 0.0f, 1.0f);
    if (a <= 0) continue;
    const uint32_t A = static_cast<uint32_t>(a * 255.0f + 0.5f);
    out[4 * i] = static_cast<uint8_t>(r * A / 255), out[4 * i + 1] = static_cast<uint8_t>(g * A / 255);
    out[4 * i + 2] = static_cast<uint8_t>(b * A / 255), out[4 * i + 3] = static_cast<uint8_t>(A);
  }
  return out;
}

}  // namespace platform::hires
