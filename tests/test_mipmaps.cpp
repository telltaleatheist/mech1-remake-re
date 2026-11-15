// The GPU path's texture levels (platform/mipmaps.h, ours; docs/render.md, Mipmaps).
#include <algorithm>
#include <cstdint>
#include <vector>

#include "check.h"
#include "platform/mipmaps.h"

namespace {

std::vector<uint8_t> image(int w, int h, uint32_t (*at)(int x, int y)) {
  std::vector<uint8_t> v(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint32_t c = at(x, y);  // 0xAARRGGBB
      uint8_t* p = &v[4 * (static_cast<size_t>(y) * w + x)];
      p[0] = c >> 16 & 0xff, p[1] = c >> 8 & 0xff, p[2] = c & 0xff, p[3] = c >> 24;
    }
  return v;
}

// The level's share of texels solid (alpha 255), and whether every alpha is 0 or 255.
double solid(const platform::MipLevel& m, bool* binary) {
  size_t n = 0, s = 0;
  *binary = true;
  for (size_t i = 3; i < m.rgba.size(); i += 4, ++n) {
    s += m.rgba[i] == 255;
    if (m.rgba[i] != 0 && m.rgba[i] != 255) *binary = false;
  }
  return static_cast<double>(s) / static_cast<double>(n);
}

}  // namespace

TEST(mip_chain_halves_to_one_texel) {
  const auto v = image(8, 2, [](int, int) { return 0xff102030u; });
  const auto c = platform::mip_chain(8, 2, v.data());
  EXPECT(c.size() == 3);
  EXPECT(c[0].width == 4 && c[0].height == 1);
  EXPECT(c[1].width == 2 && c[1].height == 1);
  EXPECT(c[2].width == 1 && c[2].height == 1);
  EXPECT(c[2].rgba == std::vector<uint8_t>({0x10, 0x20, 0x30, 0xff}));
  EXPECT(platform::mip_chain(1, 1, v.data()).empty());
}

TEST(mip_chain_averages_an_opaque_texture) {
  const auto v = image(4, 4, [](int x, int y) { return (x + y) & 1 ? 0xffff0000u : 0xff0000ffu; });
  const auto c = platform::mip_chain(4, 4, v.data());
  EXPECT(c.size() == 2);
  for (const auto& m : c)
    for (size_t i = 0; i < m.rgba.size(); i += 4)
      EXPECT(m.rgba[i] == 128 && m.rgba[i + 1] == 0 && m.rgba[i + 2] == 128 && m.rgba[i + 3] == 255);
}

TEST(mip_chain_keeps_a_colour_keys_edge_its_colour) {
  // White on the left, the key's clear black on the right: the edge's texels stay white, never grey.
  const auto v = image(8, 8, [](int x, int) { return x < 3 ? 0xffffffffu : 0u; });
  const auto c = platform::mip_chain(8, 8, v.data());
  const auto& m = c[0];  // 4 x 4: column 1 covers x 2 (white) and 3 (clear)
  for (int y = 0; y < 4; ++y) {
    const uint8_t* p = &m.rgba[4 * (static_cast<size_t>(y) * 4 + 1)];
    EXPECT(p[0] == 255 && p[1] == 255 && p[2] == 255);
  }
}

TEST(mip_chain_keeps_a_cutouts_coverage_at_every_level) {
  // A fence: one solid column in four. Averaged, the 4 x 4 level would be a quarter alpha everywhere, a fence
  // faded to a haze; here a quarter of each level is solid and the rest clear.
  const auto v = image(16, 16, [](int x, int) { return x % 4 == 0 ? 0xff808080u : 0u; });
  const auto c = platform::mip_chain(16, 16, v.data());
  EXPECT(c.size() == 4);
  for (size_t l = 0; l + 1 < c.size(); ++l) {
    bool binary = false;
    EXPECT(solid(c[l], &binary) == 0.25);
    EXPECT(binary);
  }
  for (size_t i = 0; i < c[2].rgba.size(); i += 4)  // a solid texel keeps the fence's colour
    if (c[2].rgba[i + 3] == 255) EXPECT(c[2].rgba[i] == 0x80);
  bool binary = false;
  EXPECT(solid(c[3], &binary) == 0.0 && binary);  // 1 x 1: a quarter rounds to clear
}

TEST(mip_chain_averages_graded_alpha) {
  // Smoke: alpha 64 and 192 in a checker averages to 128, not cut.
  const auto v = image(4, 4, [](int x, int y) { return (x + y) & 1 ? 0xc0ffffffu : 0x40ffffffu; });
  const auto c = platform::mip_chain(4, 4, v.data());
  EXPECT(c[0].rgba[3] == 128);
  EXPECT(c[1].rgba[3] == 128);
}

TEST(mip_chain_keeps_a_soft_cutouts_coverage) {
  // A dry tree's branch: a column one texel wide in eight, solid but for one in four of its texels at 0x30,
  // with a soft fringe beside it. Averaged, the 4 x 4 level's branch would sit under half alpha (gone, blended);
  // scaled and steepened, every level keeps the branch covered (at least half, the most nearly solid) in no
  // more than twice the full size's share (a column can't be split).
  const auto v = image(32, 32, [](int x, int y) {
    return x % 8 == 0 ? (y % 4 ? 0xff303030u : 0x30303030u) : x % 8 == 1 ? 0x60303030u : 0u;
  });
  size_t full = 0;
  for (size_t i = 3; i < v.size(); i += 4) full += v[i] >= 128;
  const double share0 = static_cast<double>(full) / static_cast<double>(v.size() / 4);
  const auto c = platform::mip_chain(32, 32, v.data());
  for (size_t l = 0; l + 2 < c.size(); ++l) {
    size_t half = 0, n = 0, top = 0;
    for (size_t i = 3; i < c[l].rgba.size(); i += 4, ++n) half += c[l].rgba[i] >= 128, top = std::max<size_t>(top, c[l].rgba[i]);
    const double share = static_cast<double>(half) / static_cast<double>(n);
    EXPECT(share >= share0 && share <= 2 * share0);
    EXPECT(top >= 240);
  }
}
