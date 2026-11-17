// mw1imgpack OUT.zbd INTRO.bmp: MW1's image archive (src/mw1_images.h) from its sources in data-src/, at build time:
// the opening picture letterboxed on 640x480 (mw1_intro), the narration's black (mw1_black), and the travel map's
// backdrops drawn from the worlds' data (src/mw1_map_image.h: mw1_map and one per House).
#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "mw1_images.h"
#include "mw1_map_image.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: mw1imgpack OUT.zbd INTRO.bmp\n");
    return 2;
  }
  SDL_Surface* s = SDL_LoadBMP(argv[2]);
  SDL_Surface* rgb = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB565) : nullptr;
  if (!rgb) {
    std::fprintf(stderr, "mw1imgpack: %s: %s\n", argv[2], SDL_GetError());
    return 1;
  }
  std::vector<uint16_t> px(static_cast<size_t>(rgb->w) * static_cast<size_t>(rgb->h));
  for (int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(rgb->pixels) + static_cast<size_t>(y) * rgb->pitch);
    for (int x = 0; x < rgb->w; ++x) px[static_cast<size_t>(y * rgb->w + x)] = row[x];
  }
  const int w = rgb->w, h = rgb->h;
  SDL_DestroySurface(rgb), SDL_DestroySurface(s);
  if (w > mw1::kScreenW || h > mw1::kScreenH) {
    std::fprintf(stderr, "mw1imgpack: %s is %dx%d, larger than the screen\n", argv[2], w, h);
    return 1;
  }
  std::string why;
  std::vector<mw1::images::Image> all = {mw1::letterbox(mw1::kIntroImage, w, h, px), mw1::black(mw1::kBlackImage)};
  for (mw1::images::Image& m : mw1::backdrops()) all.push_back(std::move(m));
  const std::vector<uint8_t> b = mw1::pack(all, &why);
  if (b.empty()) {
    std::fprintf(stderr, "mw1imgpack: %s\n", why.c_str());
    return 1;
  }
  std::ofstream f(argv[1], std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  if (!f) {
    std::fprintf(stderr, "mw1imgpack: can't write %s\n", argv[1]);
    return 1;
  }
  return 0;
}
