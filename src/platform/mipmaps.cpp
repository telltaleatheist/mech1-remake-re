#include "platform/mipmaps.h"

#include <algorithm>
#include <cstddef>
#include <numeric>

namespace platform {
namespace {

constexpr int kHalf = 128;  // an alpha at least this is "covered"

// The source texels [lo, hi) a destination texel covers, along one axis of `from` into `to`.
void span(int i, int from, int to, int* lo, int* hi) {
  *lo = static_cast<int>(static_cast<int64_t>(i) * from / to);
  *hi = std::max(*lo + 1, static_cast<int>(static_cast<int64_t>(i + 1) * from / to));
}

}  // namespace

std::vector<MipLevel> mip_chain(int width, int height, const uint8_t* rgba) {
  std::vector<MipLevel> out;
  if (width <= 0 || height <= 0 || !rgba) return out;
  const size_t n0 = static_cast<size_t>(width) * height;
  size_t solid = 0, clear = 0, half = 0;
  for (size_t i = 0; i < n0; ++i) {
    const uint8_t a = rgba[4 * i + 3];
    solid += a == 255, clear += a == 0, half += a >= kHalf;
  }
  const size_t between = n0 - solid - clear;
  const bool cutout = clear > 0 && between == 0;
  // Soft-edged cut-outs (the trees' alpha planes): mostly clear or solid, a fringe between.
  const bool soft_cutout = clear > 0 && between > 0 && between * 4 < n0;
  const double coverage = static_cast<double>(solid) / static_cast<double>(n0);
  const double half_coverage = static_cast<double>(half) / static_cast<double>(n0);
  int w = width, h = height;
  const uint8_t* src = rgba;
  std::vector<float> alpha;
  while (w > 1 || h > 1) {
    const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
    MipLevel m;
    m.width = nw, m.height = nh;
    m.rgba.resize(static_cast<size_t>(nw) * nh * 4);
    alpha.assign(static_cast<size_t>(nw) * nh, 0.0f);
    for (int y = 0; y < nh; ++y) {
      int y0, y1;
      span(y, h, nh, &y0, &y1);
      for (int x = 0; x < nw; ++x) {
        int x0, x1;
        span(x, w, nw, &x0, &x1);
        double c[3] = {}, plain[3] = {}, a = 0;
        int count = 0;
        for (int sy = y0; sy < y1; ++sy)
          for (int sx = x0; sx < x1; ++sx) {
            const uint8_t* p = src + 4 * (static_cast<size_t>(sy) * w + sx);
            for (int k = 0; k < 3; ++k) c[k] += p[k] * static_cast<double>(p[3]), plain[k] += p[k];
            a += p[3], ++count;
          }
        uint8_t* o = &m.rgba[4 * (static_cast<size_t>(y) * nw + x)];
        for (int k = 0; k < 3; ++k)
          o[k] = static_cast<uint8_t>(a > 0 ? c[k] / a + 0.5 : plain[k] / count + 0.5);
        alpha[static_cast<size_t>(y) * nw + x] = static_cast<float>(a / count);
        o[3] = static_cast<uint8_t>(a / count + 0.5);
      }
    }
    if (cutout || soft_cutout) {
      // The same share covered as at full size, the most covered texels first, solid: blended, not tested, a
      // covered texel left at half alpha would show a branch half see-through and a far fence faded. A hard
      // cut-out's others clear; a soft one's a fringe, its alpha scaled as the least covered one kept is to the
      // half and falling off four times as steeply, held under the half.
      const size_t n = alpha.size();
      const auto any = static_cast<size_t>(std::count_if(alpha.begin(), alpha.end(), [](float a) { return a > 0; }));
      const double share = cutout ? coverage : half_coverage;
      const auto keep = std::min(any, static_cast<size_t>(share * static_cast<double>(n) + 0.5));
      std::vector<size_t> order(n);
      std::iota(order.begin(), order.end(), size_t{0});
      std::stable_sort(order.begin(), order.end(), [&](size_t i, size_t j) { return alpha[i] > alpha[j]; });
      const double least = keep ? alpha[order[keep - 1]] : 255.0;
      const double scale = least > 0 ? kHalf / least : 1.0;
      for (size_t r = 0; r < n; ++r) {
        double a = r < keep ? 255.0 : 0.0;
        if (r >= keep && soft_cutout) a = std::clamp(255.0 + (alpha[order[r]] * scale - kHalf) * 4.0, 0.0, kHalf - 1.0);
        m.rgba[4 * order[r] + 3] = static_cast<uint8_t>(a + 0.5);
      }
    }
    out.push_back(std::move(m));
    src = out.back().rgba.data(), w = nw, h = nh;
  }
  return out;
}

}  // namespace platform
