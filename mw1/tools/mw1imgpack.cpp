// mw1imgpack OUT.zbd INTRO.bmp ART_DIR: MW1's image archive (src/mw1_images.h) from its sources in data-src/, at build
// time: the opening picture letterboxed on 640x480 (mw1_intro), the narration's black (mw1_black), the travel map's
// backdrops drawn from the worlds' data (src/mw1_map_image.h: mw1_map and one per House), the screens' art (ART_DIR's
// PNGs, tools/mw1art.py's, already at their sizes and in 565's levels: each as mw1_<its name>), and the chrome drawn
// in code (src/mw1_chrome.h), with alpha.
#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <algorithm>
#include <filesystem>

#include "mw1_chrome.h"
#include "mw1_images.h"
#include "mw1_map_image.h"

namespace {
// A picture file's pixels as 565 (SDL's loaders: BMP, and PNG since SDL 3.4); false with the reason printed.
bool load(const std::string& path, int* w, int* h, std::vector<uint16_t>* px) {
  SDL_Surface* s = SDL_LoadSurface(path.c_str());
  SDL_Surface* rgb = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB565) : nullptr;
  if (!rgb) {
    std::fprintf(stderr, "mw1imgpack: %s: %s\n", path.c_str(), SDL_GetError());
    if (s) SDL_DestroySurface(s);
    return false;
  }
  px->assign(static_cast<size_t>(rgb->w) * static_cast<size_t>(rgb->h), 0);
  for (int y = 0; y < rgb->h; ++y) {
    const auto* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(rgb->pixels) + static_cast<size_t>(y) * rgb->pitch);
    for (int x = 0; x < rgb->w; ++x) (*px)[static_cast<size_t>(y * rgb->w + x)] = row[x];
  }
  *w = rgb->w, *h = rgb->h;
  SDL_DestroySurface(rgb), SDL_DestroySurface(s);
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::fprintf(stderr, "usage: mw1imgpack OUT.zbd INTRO.bmp ART_DIR\n");
    return 2;
  }
  int w = 0, h = 0;
  std::vector<uint16_t> px;
  if (!load(argv[2], &w, &h, &px)) return 1;
  if (w > mw1::kScreenW || h > mw1::kScreenH) {
    std::fprintf(stderr, "mw1imgpack: %s is %dx%d, larger than the screen\n", argv[2], w, h);
    return 1;
  }
  std::string why;
  std::vector<mw1::images::Image> all = {mw1::letterbox(mw1::kIntroImage, w, h, px), mw1::black(mw1::kBlackImage)};
  for (mw1::images::Image& m : mw1::backdrops()) all.push_back(std::move(m));
  // The art, in name order (the same archive every build).
  std::vector<std::filesystem::path> art;
  for (const auto& e : std::filesystem::directory_iterator(argv[3]))
    if (e.path().extension() == ".png") art.push_back(e.path());
  std::sort(art.begin(), art.end());
  for (const std::filesystem::path& p : art) {
    mw1::images::Image im;
    const std::string stem = p.stem().string();
    im.name = "mw1_" + stem;
    if (!load(p.string(), &im.width, &im.height, &im.pixels)) return 1;
    // The hub's scenery darkened, with faint scanlines (the mockup's); a service picture also framed as a tile in
    // its four looks (mw1_tile_<name>, _lit, _down, _off).
    if (stem.rfind("hub_", 0) == 0) mw1::chrome::darken_scene(im);
    // The lab's panels cut from the hangar (the lab's reskin).
    if (stem == mw1::chrome::kLabHangar)
      for (mw1::images::Image& t : mw1::chrome::lab_panels(im)) all.push_back(std::move(t));
    if (stem.rfind("icon_", 0) == 0)
      for (mw1::images::Image& t : mw1::chrome::tiles(im, "mw1_tile_" + stem.substr(5))) all.push_back(std::move(t));
    all.push_back(std::move(im));
  }
  for (mw1::images::Image& m : mw1::chrome::all()) all.push_back(std::move(m));
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
