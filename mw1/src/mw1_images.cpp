#include "mw1_images.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace mw1::images {
namespace {
void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>(v >> (8 * i));
}
void put16(std::vector<uint8_t>& b, size_t at, uint16_t v) { b[at] = static_cast<uint8_t>(v), b[at + 1] = static_cast<uint8_t>(v >> 8); }
constexpr size_t kHeader = 0x18, kEntry = 0x28, kInfo = 0x10;
constexpr uint8_t kTwoBytes = 0x01, kNoAlpha = 0x04, kFullAlpha = 0x08;  // zimg_library.h's flags
}  // namespace

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

Image letterbox(const std::string& name, int32_t w, int32_t h, const std::vector<uint16_t>& picture) {
  Image out = black(name);
  const int32_t x0 = (kScreenW - w) / 2, y0 = (kScreenH - h) / 2;
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const int32_t tx = x0 + x, ty = y0 + y;
      if (tx >= 0 && ty >= 0 && tx < kScreenW && ty < kScreenH)
        out.pixels[static_cast<size_t>(ty * kScreenW + tx)] = picture[static_cast<size_t>(y * w + x)];
    }
  return out;
}

Image cover(const std::string& name, int32_t w, int32_t h, const std::vector<uint16_t>& picture, int32_t out_w, int32_t out_h) {
  Image out;
  out.name = name, out.width = out_w, out.height = out_h;
  out.pixels.assign(static_cast<size_t>(out_w) * static_cast<size_t>(out_h), 0);
  if (w <= 0 || h <= 0 || picture.size() < static_cast<size_t>(w) * static_cast<size_t>(h)) return out;
  const double s = std::max(static_cast<double>(out_w) / w, static_cast<double>(out_h) / h);
  const double ox = (w * s - out_w) / 2, oy = (h * s - out_h) / 2;
  auto at = [&](int32_t x, int32_t y, int sh, int mask) {
    x = std::clamp(x, 0, w - 1), y = std::clamp(y, 0, h - 1);
    return (picture[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] >> sh) & mask;
  };
  for (int32_t y = 0; y < out_h; ++y)
    for (int32_t x = 0; x < out_w; ++x) {
      const double sx = (x + 0.5 + ox) / s - 0.5, sy = (y + 0.5 + oy) / s - 0.5;
      const int32_t x0 = static_cast<int32_t>(std::floor(sx)), y0 = static_cast<int32_t>(std::floor(sy));
      const double fx = sx - x0, fy = sy - y0;
      uint16_t px = 0;
      for (const auto& [sh, mask] : {std::pair{11, 31}, std::pair{5, 63}, std::pair{0, 31}}) {
        const double v = (at(x0, y0, sh, mask) * (1 - fx) + at(x0 + 1, y0, sh, mask) * fx) * (1 - fy) +
                         (at(x0, y0 + 1, sh, mask) * (1 - fx) + at(x0 + 1, y0 + 1, sh, mask) * fx) * fy;
        px = static_cast<uint16_t>(px | (static_cast<uint16_t>(std::lround(v)) << sh));
      }
      out.pixels[static_cast<size_t>(y) * static_cast<size_t>(out_w) + static_cast<size_t>(x)] = px;
    }
  return out;
}

Image black(const std::string& name) {
  Image out;
  out.name = name, out.width = kScreenW, out.height = kScreenH;
  out.pixels.assign(static_cast<size_t>(kScreenW * kScreenH), 0);
  return out;
}

std::vector<uint8_t> pack(const std::vector<Image>& images, std::string* why) {
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return std::vector<uint8_t>{};
  };
  if (why) why->clear();
  size_t size = kHeader + kEntry * images.size();
  for (const Image& i : images) {
    if (i.name.empty() || i.name.size() > 31) return fail("the name \"" + i.name + "\" isn't 1 to 31 characters");
    if (i.width <= 0 || i.height <= 0 || i.width > 0x7fff || i.height > 0x7fff ||
        i.pixels.size() != static_cast<size_t>(i.width) * static_cast<size_t>(i.height))
      return fail(i.name + ": its size and pixels don't agree");
    if (!i.alpha.empty() && i.alpha.size() != i.pixels.size()) return fail(i.name + ": its alpha and pixels don't agree");
    size += kInfo + 2 * i.pixels.size() + i.alpha.size();
  }
  std::vector<uint8_t> b(size, 0);
  put32(b, 4, 1);                                        // has_entries
  put32(b, 8, 0);                                        // no global palettes
  put32(b, 12, static_cast<uint32_t>(images.size()));    // texture_count
  size_t at = kHeader + kEntry * images.size();
  for (size_t n = 0; n < images.size(); ++n) {
    const Image& i = images[n];
    const size_t e = kHeader + kEntry * n;
    std::memcpy(&b[e], i.name.data(), i.name.size());
    put32(b, e + 32, static_cast<uint32_t>(at));
    put32(b, e + 36, 0xFFFFFFFFu);                       // no palette
    b[at] = static_cast<uint8_t>(kTwoBytes | (i.alpha.empty() ? kNoAlpha : kFullAlpha));
    put16(b, at + 4, static_cast<uint16_t>(i.width));
    put16(b, at + 6, static_cast<uint16_t>(i.height));
    at += kInfo;
    for (uint16_t p : i.pixels) put16(b, at, p), at += 2;
    for (uint8_t a : i.alpha) b[at++] = a;
  }
  return b;
}

}  // namespace mw1::images
