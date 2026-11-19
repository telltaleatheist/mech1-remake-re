// mw1imgpack OUT.zbd ART.zbd ART_DIR: MW1's two image archives from their sources in data-src/, at build time.
// - ART.zbd (src/mw1_art.h): the screens' art, ART_DIR's files (tools/mw1art.py's, at their sources' size) copied as
//   they are, each canvas image art.csv names as mw1_<its name>. Their canvas copies are made when they load.
// - OUT.zbd (src/mw1_images.h): the narration's black
//   (mw1_black), the travel map's backdrops drawn from the worlds' data (src/mw1_map_image.h: mw1_map and one per
//   House), what is cut from the art's canvas copies (the service tiles, DEPLOY's, the lab's panels), and the chrome
//   drawn in code (src/mw1_chrome.h), with alpha. The intro's opening picture (mw1_intro) is art (FIXES.md item 83).
#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>

#include "mw1_art.h"
#include "mw1_chrome.h"
#include "mw1_images.h"
#include "mw1_map_image.h"


int main(int argc, char** argv) {
  if (argc != 4) {
    std::fprintf(stderr, "usage: mw1imgpack OUT.zbd ART.zbd ART_DIR\n");
    return 2;
  }
  std::string why;
  std::vector<mw1::images::Image> all = {mw1::black(mw1::kBlackImage)};
  // The black backdrop the wide canvas across (centred, 4:3 shows its middle): nothing of the screen under it shows.
  all[0].width = mw1::kWideW, all[0].pixels.assign(static_cast<size_t>(mw1::kWideW) * mw1::kScreenH, 0);
  for (mw1::images::Image& m : mw1::backdrops()) all.push_back(std::move(m));
  // The art, in art.csv's order (the same archive every build): each file's bytes once, and its canvas copies.
  const std::filesystem::path dir = argv[3];
  std::ifstream csv(dir / "art.csv");
  if (!csv) {
    std::fprintf(stderr, "mw1imgpack: no art.csv in %s\n", argv[3]);
    return 1;
  }
  std::vector<mw1::art::Packed> packed;
  std::vector<std::vector<uint8_t>> files;
  std::vector<std::string> file_names;
  struct Source { int32_t w = 0, h = 0; std::vector<uint8_t> rgba; };
  std::vector<Source> sources;
  std::string line;
  std::getline(csv, line);  // the heading
  while (std::getline(csv, line)) {
    if (line.empty()) continue;
    std::vector<std::string> f;
    for (size_t at = 0;;) {
      const size_t comma = line.find(',', at);
      f.push_back(line.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
      if (comma == std::string::npos) break;
      at = comma + 1;
    }
    if (f.size() != 4) {
      std::fprintf(stderr, "mw1imgpack: art.csv: a bad line: %s\n", line.c_str());
      return 1;
    }
    const auto known = std::find(file_names.begin(), file_names.end(), f[1]);
    int32_t file = static_cast<int32_t>(known - file_names.begin());
    if (known == file_names.end()) {
      std::ifstream in(dir / f[1], std::ios::binary);
      std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      Source s;
      if (bytes.empty() || !mw1::art::decode(bytes.data(), bytes.size(), &s.w, &s.h, &s.rgba)) {
        std::fprintf(stderr, "mw1imgpack: %s doesn't decode\n", (dir / f[1]).string().c_str());
        return 1;
      }
      file_names.push_back(f[1]), files.push_back(std::move(bytes)), sources.push_back(std::move(s));
    }
    const std::string stem = f[0];
    mw1::art::Packed p{"mw1_" + stem, file, std::atoi(f[2].c_str()), std::atoi(f[3].c_str())};
    packed.push_back(p);
    const Source& src = sources[static_cast<size_t>(file)];
    mw1::images::Image im = mw1::art::canvas_image(p.name, src.rgba.data(), src.w, src.h, p.canvas_w, p.canvas_h);
    if (im.pixels.empty()) {
      std::fprintf(stderr, "mw1imgpack: %s: a bad canvas size\n", stem.c_str());
      return 1;
    }
    // (The hub's scenery is darkened when it loads, mw1_art.h.) A service picture framed as a tile in its four looks
    // (mw1_tile_<name>, _lit, _down, _off).
    // DEPLOY's tile (the hub with a contract, FIXES.md item 11): the dropship's launch cut to a tile until its own art
    // (the picture's middle square, each tile pixel the average of its square).
    if (stem == "deploy_launch") {
      mw1::images::Image sq;
      sq.name = "deploy", sq.width = sq.height = mw1::kTile;
      const int32_t side = std::min(im.width, im.height), x0 = (im.width - side) / 2, y0 = (im.height - side) / 2;
      for (int32_t y = 0; y < mw1::kTile; ++y)
        for (int32_t x = 0; x < mw1::kTile; ++x) {
          int32_t r = 0, g = 0, b = 0, n = 0;
          for (int32_t sy = y * side / mw1::kTile; sy < (y + 1) * side / mw1::kTile; ++sy)
            for (int32_t sx = x * side / mw1::kTile; sx < (x + 1) * side / mw1::kTile; ++sx, ++n) {
              const uint16_t px = im.pixels[static_cast<size_t>((y0 + sy) * im.width + x0 + sx)];
              r += (px >> 11) << 3, g += ((px >> 5) & 63) << 2, b += (px & 31) << 3;
            }
          n = std::max(n, 1);
          sq.pixels.push_back(mw1::images::rgb565(static_cast<uint8_t>(r / n), static_cast<uint8_t>(g / n), static_cast<uint8_t>(b / n)));
        }
      for (mw1::images::Image& t : mw1::chrome::tiles(sq, "mw1_tile_deploy")) all.push_back(std::move(t));
    }
    // The lab's panels cut from the hangar (the lab's reskin).
    if (stem == mw1::chrome::kLabHangar)
      for (mw1::images::Image& t : mw1::chrome::lab_panels(im)) all.push_back(std::move(t));
    if (stem.rfind("icon_", 0) == 0)
      for (mw1::images::Image& t : mw1::chrome::tiles(im, "mw1_tile_" + stem.substr(5))) all.push_back(std::move(t));
  }
  const std::vector<uint8_t> art = mw1::art::pack(packed, files, &why);
  if (art.empty()) {
    std::fprintf(stderr, "mw1imgpack: %s\n", why.c_str());
    return 1;
  }
  std::ofstream af(argv[2], std::ios::binary);
  af.write(reinterpret_cast<const char*>(art.data()), static_cast<std::streamsize>(art.size()));
  if (!af) {
    std::fprintf(stderr, "mw1imgpack: can't write %s\n", argv[2]);
    return 1;
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
