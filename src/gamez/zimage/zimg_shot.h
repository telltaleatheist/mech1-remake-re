#pragma once
#include <cstdint>
#include <vector>

namespace zimg {

struct Texture;

// zImage's screenshot (0x6c bytes; original file unknown: between zimg_texture.cpp and zimg_fonts.cpp): the screen
// grabbed as a 16-bit image, turned into 24-bit BGR by the display's masks and bit counts, its rows flipped
// bottom-up, and written as a .bmp. Only the app's debug command 0x7e uses it (0x004abbc0, app_keys), which no key
// reaches in the release. Ported for preservation (docs/screenshots.md).
struct ScreenShot {
  int32_t width = 0, height = 0;   // +0x10, +0x14
  // +0x18 BITMAPFILEHEADER (14 bytes; "BM", the size, 0, the bits' offset 0x36), +0x28 BITMAPINFOHEADER (40).
  uint8_t file_header[14] = {};
  uint8_t info[40] = {};
  uint32_t r_mask = 0xf800, g_mask = 0x7e0, b_mask = 0x1f;  // +0x50  (0x0080272c..)
  int32_t r_bits = 0, g_bits = 0, b_bits = 0;               // +0x5c  (0x00802720..)
  std::vector<uint8_t> pixels;     // +0x68  malloc'd in the exe, width x height x 3

  ScreenShot();                    // 0x005199e0
  void free_pixels();              // 0x00519a20 (its destructor's)
  // 0x00519990: the screen grabbed (0x00579500(2) through `grab`: a new image, freed after), the format's bits and
  // masks taken, then converted. False when the grab gives none.
  bool grab(Texture* (*grab_screen)(int32_t buffer));
  bool convert(const Texture& image);  // 0x00519a40: 24-bit, then flipped; 1
  bool swap_rows(int32_t a, int32_t b);  // 0x00519b30: false when either is past the last row
  bool flip();                     // 0x00519be0
  bool write(const char* path);    // 0x00519c20: the headers and pixels; false when the file can't be made
};

}  // namespace zimg
