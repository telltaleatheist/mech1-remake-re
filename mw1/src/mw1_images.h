#pragma once
// MW1's own images (the narration's black, the travel map's backdrops, the cut tiles and the chrome; the intro's picture
// is art since FIXES.md item 83, mw1_art.h): packed at build time into an image archive in Pirate's Moon's
// format, mw1data/zbd/mw1image.zbd, which mw1 registers beside PM's rimage.zbd (zimg_lib::new_image_library); the
// screens then name them as they name PM's. The layout is the engine's own reader's (gamez/zimage/zimg_library.h): a
// 0x18-byte header, an 0x28-byte entry per image (its name and offset, no palette), then each image: a 16-byte info
// (flags kTwoBytes | kNoAlpha, as PM's opaque backdrops, the width and the height) and its pixels, 16-bit 565; an
// image with an alpha plane (the chrome, mw1_chrome.h) is kTwoBytes | kFullAlpha, its 8-bit alphas after the pixels,
// blended by the 2D blit (zrndr::blit16).
#include <cstdint>
#include <string>
#include <vector>

namespace mw1::images {

inline constexpr const char* kImageLibrary = "mw1image";        // zbd/mw1image.zbd in MW1's data
inline constexpr const char* kIntroImage = "mw1_intro";         // the opening picture: art (mw1_art.h), art.csv's "intro"
inline constexpr const char* kBlackImage = "mw1_black";         // 640x480 black, the narration's backdrop
inline constexpr int32_t kScreenW = 640, kScreenH = 480;
inline constexpr int32_t kWideW = 854;  // the 16:9 canvas's width (480 high): the scenes' (tools/mw1art.py)

struct Image {
  std::string name;            // at most 31 characters
  int32_t width = 0, height = 0;
  std::vector<uint16_t> pixels;  // 565, row by row
  std::vector<uint8_t> alpha;    // empty: opaque; else one per pixel (0 clear, 255 solid)
};
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
Image black(const std::string& name);
// The archive's bytes; "" in `why` and the bytes, or why not (a name too long, a size that doesn't fit).
std::vector<uint8_t> pack(const std::vector<Image>& images, std::string* why = nullptr);

}  // namespace mw1::images

namespace mw1 {
using namespace images;
}  // namespace mw1
