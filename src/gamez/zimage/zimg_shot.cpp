#include "gamez/zimage/zimg_shot.h"

#include <cstdio>
#include <cstring>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace zimg {
namespace {
void put16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, 2); }
void put32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }
}  // namespace

// Mech3.exe 1.2 @ 0x005199e0
ScreenShot::ScreenShot() = default;

// Mech3.exe 1.2 @ 0x00519a20
void ScreenShot::free_pixels() { std::vector<uint8_t>().swap(pixels); }

// Mech3.exe 1.2 @ 0x00519990
bool ScreenShot::grab(Texture* (*grab_screen)(int32_t buffer)) {
  Texture* image = grab_screen ? grab_screen(2) : nullptr;
  const zvid::PixelFormat& f = zvid::pixel_format();  // 0x00579260
  r_bits = f.r_bits, g_bits = f.g_bits, b_bits = f.b_bits;
  const zvid::ChannelMasks& m = zvid::channel_masks();  // 0x00579280
  r_mask = m.r, g_mask = m.g, b_mask = m.b;
  if (!image) return false;
  const bool ok = convert(*image);
  free_texture(image);  // 0x00517cc0
  return ok;
}

// Mech3.exe 1.2 @ 0x00519a40: blue the low byte's bits shifted up to 8, green and red shifted down to 8 (the shift
// counts masked to 5 bits, as x86 does).
bool ScreenShot::convert(const Texture& image) {
  width = image.width, height = image.height;
  const uint32_t size = static_cast<uint32_t>(width * height * 3);
  std::memset(info, 0, sizeof info);
  put32(info + 0x00, 0x28);
  put32(info + 0x04, static_cast<uint32_t>(width));
  put32(info + 0x08, static_cast<uint32_t>(height));
  put16(info + 0x0c, 1);
  put16(info + 0x0e, 24);
  put32(info + 0x14, size);
  put32(info + 0x18, 2);
  put32(info + 0x1c, 2);
  pixels.assign(size, 0);
  const uint32_t b = static_cast<uint32_t>(b_bits), bg = static_cast<uint32_t>(b_bits + g_bits);
  const auto* src = static_cast<const uint16_t*>(image.pixels);
  uint8_t* out = pixels.data();
  for (int32_t i = 0; i < width * height; ++i, out += 3) {
    const uint32_t p = src[i];
    out[0] = static_cast<uint8_t>((p & b_mask & 0xff) << ((8u - b) & 0x1f));
    out[1] = static_cast<uint8_t>((p & g_mask) >> ((bg - 8u) & 0x1f));
    out[2] = static_cast<uint8_t>((p & r_mask) >> ((static_cast<uint32_t>(r_bits) - 8u + bg) & 0x1f));
  }
  flip();
  return true;
}

// Mech3.exe 1.2 @ 0x00519b30
bool ScreenShot::swap_rows(int32_t a, int32_t b) {
  if (a == b) return true;
  if (a >= height || b >= height) return false;
  const size_t row = static_cast<size_t>(width) * 3;
  std::vector<uint8_t> tmp(pixels.begin() + static_cast<ptrdiff_t>(row * a),
                           pixels.begin() + static_cast<ptrdiff_t>(row * (a + 1)));
  std::memcpy(pixels.data() + row * a, pixels.data() + row * b, row);
  std::memcpy(pixels.data() + row * b, tmp.data(), row);
  return true;
}

// Mech3.exe 1.2 @ 0x00519be0
bool ScreenShot::flip() {
  for (int32_t i = 0; i < height / 2; ++i) swap_rows(i, height - i - 1);
  return true;
}

// Mech3.exe 1.2 @ 0x00519c20: CreateFileA(CREATE_ALWAYS), then the 14 bytes, the 40 and the pixels. FIX (A): into
// the write dir (zutl::open), not beside the exe.
bool ScreenShot::write(const char* path) {
  std::FILE* f = zutl::open(path, "wb");
  if (!f) return false;
  const uint32_t size = static_cast<uint32_t>(width * height * 3);
  put16(file_header + 0, 0x4d42);
  put32(file_header + 2, size + 0x36);
  put32(file_header + 10, 0x36);
  std::fwrite(file_header, 1, sizeof file_header, f);
  std::fwrite(info, 1, sizeof info, f);
  std::fwrite(pixels.data(), 1, size, f);
  std::fclose(f);
  return true;
}

}  // namespace zimg
