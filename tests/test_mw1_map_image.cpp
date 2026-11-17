// The travel map's backdrops (src/mw1_map_image.h): every world inside its own House's territory, the Houses' worlds
// in their own colours on the image, the six images packed, and the same pixels every build. MW1_MAP_PREVIEW=DIR
// writes them as BMPs there to look at.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.h"
#include "mw1_images.h"
#include "mw1_map_image.h"
#include "mw1_travel.h"
#include "mw1_travel_map.h"

namespace {
uint32_t fnv(const std::vector<uint16_t>& px) {
  uint32_t h = 2166136261u;
  for (uint16_t p : px) h = ((h ^ (p & 0xff)) * 16777619u), h = ((h ^ (p >> 8)) * 16777619u);
  return h;
}
void write_bmp(const std::string& path, const mw1::images::Image& im) {  // 24-bit, bottom-up
  const int32_t row = (im.width * 3 + 3) & ~3, size = 54 + row * im.height;
  std::string b(static_cast<size_t>(size), '\0');
  auto put = [&](size_t at, uint32_t v, int n) {
    for (int i = 0; i < n; ++i) b[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
  };
  b[0] = 'B', b[1] = 'M';
  put(2, static_cast<uint32_t>(size), 4), put(10, 54, 4), put(14, 40, 4), put(18, static_cast<uint32_t>(im.width), 4);
  put(22, static_cast<uint32_t>(im.height), 4), put(26, 1, 2), put(28, 24, 2);
  for (int32_t y = 0; y < im.height; ++y)
    for (int32_t x = 0; x < im.width; ++x) {
      const uint16_t p = im.pixels[static_cast<size_t>(y * im.width + x)];
      const size_t at = 54 + static_cast<size_t>((im.height - 1 - y) * row + x * 3);
      b[at] = static_cast<char>((p & 31) << 3), b[at + 1] = static_cast<char>(((p >> 5) & 63) << 2), b[at + 2] = static_cast<char>((p >> 11) << 3);
    }
  std::ofstream(path, std::ios::binary).write(b.data(), static_cast<std::streamsize>(b.size()));
}
}  // namespace

TEST(mw1_map_image_every_world_in_its_houses_territory) {
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    EXPECT(mw1::territory_at(p.x, p.y) == mw1::kPlanets[i].house);
  }
  EXPECT(mw1::territory_at(mw1::kMapLeft, mw1::kMapTop) == -1);  // the corner: space
  // Each House's territory one piece around its worlds: its worlds' places all lit in its own image, dimmed in another's.
  const mw1::images::Image all = mw1::backdrop(-1), kurita = mw1::backdrop(0);
  EXPECT(all.name == "mw1_map" && kurita.name == "mw1_map_kurita" && mw1::house_image(4) == "mw1_map_davion");
  EXPECT(all.width == mw1::kMapWidth && all.height == mw1::kMapHeight && all.pixels.size() == static_cast<size_t>(all.width * all.height));
  int32_t brighter = 0, darker = 0;
  auto luma = [](uint16_t p) { return (p >> 11) * 2 + ((p >> 5) & 63) + (p & 31) * 2; };
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    const size_t at = static_cast<size_t>((p.y - mw1::kMapTop + 2) * mw1::kMapWidth + (p.x - mw1::kMapLeft + 2));
    if (mw1::kPlanets[i].house != 0 && luma(kurita.pixels[at]) < luma(all.pixels[at])) ++darker;
    if (mw1::kPlanets[i].house == 0 && kurita.pixels[at] == all.pixels[at]) ++brighter;
  }
  std::printf("  Kurita's image: %d of 30 of its worlds as the plain one's, %d of 115 others dimmed\n", brighter, darker);
  EXPECT(brighter == 30 && darker > 100);
}

TEST(mw1_map_image_six_packed_the_same_every_time) {
  const std::vector<mw1::images::Image> v = mw1::backdrops();
  EXPECT(v.size() == 6);
  std::string why;
  EXPECT(!mw1::images::pack(v, &why).empty() && why.empty());
  EXPECT(fnv(mw1::backdrop(-1).pixels) == fnv(v[0].pixels) && fnv(mw1::backdrop(3).pixels) == fnv(v[4].pixels));
  std::printf("  the plain map's pixels %08x\n", fnv(v[0].pixels));
  if (const char* dir = std::getenv("MW1_MAP_PREVIEW"))
    for (const mw1::images::Image& im : v) write_bmp(std::string(dir) + "/" + im.name + ".bmp", im);
}
