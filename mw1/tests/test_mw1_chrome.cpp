// MW1's chrome (src/mw1_chrome.h): every piece its size, with an alpha plane (a tile frame's window clear, a panel's
// body part see-through, the plaque solid), the same pixels every build, and packed with kFullAlpha. MW1_CHROME_PREVIEW=DIR
// writes each as a BMP (over a checkerboard) to look at.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.h"
#include "mw1_chrome.h"
#include "mw1_images.h"

namespace {
uint32_t fnv(const mw1::images::Image& im) {
  uint32_t h = 2166136261u;
  for (uint16_t p : im.pixels) h = ((h ^ (p & 0xff)) * 16777619u), h = ((h ^ (p >> 8)) * 16777619u);
  for (uint8_t a : im.alpha) h = (h ^ a) * 16777619u;
  return h;
}
uint8_t alpha_at(const mw1::images::Image& im, int32_t x, int32_t y) { return im.alpha[static_cast<size_t>(y * im.width + x)]; }
}  // namespace

TEST(mw1_chrome_pieces) {
  const std::vector<mw1::images::Image> all = mw1::chrome::all();
  EXPECT(all.size() == 5 + 3 + 4 + mw1::chrome::panel_sizes().size() + mw1::chrome::terminal_sizes().size());
  const mw1::images::Image box = mw1::chrome::dialog_frame(), dark = mw1::chrome::dim();
  EXPECT(box.width == mw1::kDialogW && alpha_at(box, 0, box.height / 2) == 255 && alpha_at(box, box.width / 2, box.height / 2) > 200);
  EXPECT(dark.width == mw1::images::kWideW && alpha_at(dark, 10, 10) == mw1::kDimAlpha);
  for (const mw1::images::Image& im : all)
    EXPECT(im.alpha.size() == im.pixels.size() && im.pixels.size() == static_cast<size_t>(im.width * im.height) && im.name.size() <= 31);
  const mw1::images::Image p = mw1::chrome::plaque();
  EXPECT(p.width == mw1::kPlaqueW && p.height == mw1::kPlaqueH && alpha_at(p, p.width / 2, p.height / 2) > 200 &&
         alpha_at(p, p.width / 2, 0) == 255);  // dark glass in a solid frame
  const mw1::images::Image t = mw1::chrome::tile_frame(0);
  const int32_t n = mw1::kTile + 2 * mw1::kTileFrame;
  EXPECT(t.width == n && t.height == n && alpha_at(t, n / 2, n / 2) == 0 && alpha_at(t, 1, n / 2) == 255 &&
         alpha_at(t, n / 2, n - mw1::kTileFrame - 2) > 150);  // the picture clear, the frame solid, the label's band dark
  // A tile's four looks from a picture: opaque, the frame's size; lit, pressed and off differ.
  mw1::images::Image pic;
  pic.width = pic.height = mw1::kTile;
  pic.pixels.assign(static_cast<size_t>(mw1::kTile * mw1::kTile), mw1::images::rgb565(200, 60, 40));
  const std::vector<mw1::images::Image> looks = mw1::chrome::tiles(pic, "mw1_tile_test");
  EXPECT(looks.size() == 4 && looks[0].name == "mw1_tile_test" && looks[1].name == "mw1_tile_test_lit" && looks[3].name == "mw1_tile_test_off");
  for (const mw1::images::Image& l : looks) EXPECT(l.width == n && l.alpha.empty());
  const size_t mid = static_cast<size_t>(n / 2 * n + n / 2);
  EXPECT(looks[0].pixels[mid] == pic.pixels[0] && looks[2].pixels[mid] != pic.pixels[0] && looks[3].pixels[mid] != pic.pixels[0]);
  EXPECT(mw1::chrome::terminal(232, 150).name == "mw1_term_232x150" && mw1::chrome::status_bar().width == mw1::images::kWideW);
  const mw1::images::Image pn = mw1::chrome::panel(232, 120);
  EXPECT(pn.name == "mw1_panel_232x120" && alpha_at(pn, 116, 60) > 150 && alpha_at(pn, 116, 60) < 255);
  // The states differ (lit, pressed, off), and the same each build.
  EXPECT(fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(1)) && fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(2)) &&
         fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(3)) && fnv(mw1::chrome::button(1)) == fnv(mw1::chrome::button(1)));
  // Packed: kFullAlpha, the alphas after the pixels.
  std::string why;
  const std::vector<uint8_t> b = mw1::images::pack({t}, &why);
  EXPECT(why.empty() && b.size() == 0x18 + 0x28 + 0x10 + 3 * t.pixels.size() && b[0x18 + 0x28] == (0x01 | 0x08));
  if (const char* dir = std::getenv("MW1_CHROME_PREVIEW")) {
    for (const mw1::images::Image& im : all) {
      const int32_t row = (im.width * 3 + 3) & ~3, size = 54 + row * im.height;
      std::string bmp(static_cast<size_t>(size), '\0');
      auto put = [&](size_t at, uint32_t v, int k) {
        for (int i = 0; i < k; ++i) bmp[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
      };
      bmp[0] = 'B', bmp[1] = 'M';
      put(2, static_cast<uint32_t>(size), 4), put(10, 54, 4), put(14, 40, 4), put(18, static_cast<uint32_t>(im.width), 4);
      put(22, static_cast<uint32_t>(im.height), 4), put(26, 1, 2), put(28, 24, 2);
      for (int32_t y = 0; y < im.height; ++y)
        for (int32_t x = 0; x < im.width; ++x) {
          const size_t i = static_cast<size_t>(y * im.width + x);
          const uint16_t c = im.pixels[i];
          const int32_t a = im.alpha[i], bg = ((x / 8 + y / 8) % 2) ? 90 : 160;  // over a checkerboard
          auto mix = [&](int32_t v) { return static_cast<char>((v * a + bg * (255 - a)) / 255); };
          const size_t at = 54 + static_cast<size_t>((im.height - 1 - y) * row + x * 3);
          bmp[at] = mix((c & 31) << 3), bmp[at + 1] = mix(((c >> 5) & 63) << 2), bmp[at + 2] = mix((c >> 11) << 3);
        }
      std::ofstream(std::string(dir) + "/" + im.name + ".bmp", std::ios::binary).write(bmp.data(), static_cast<std::streamsize>(bmp.size()));
    }
  }
}
