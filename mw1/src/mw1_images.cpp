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
