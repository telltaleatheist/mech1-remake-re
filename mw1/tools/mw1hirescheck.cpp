// mw1hirescheck FRAME.bmp CANVAS_W CANVAS_H [MIN]: is a shot's 2D only the 480-line canvas magnified (FIXES.md item
// 12, research/hires-2d.md B4)? The canvas's texels are found in the frame (the frame's size over the canvas's), and
// at each texel on an edge (its colour unlike a neighbour's) the pixels inside it, its one-pixel border left out (the
// sharp scaling's blend), are compared with its centre's. A canvas magnified is flat inside every texel: a difference
// near 0. Text and pictures drawn at the window's resolution have detail inside a texel. Prints the mean difference
// (luma, 0..255) and exits 1 when it is under MIN (default 2): blocky. The office at 4K reads 0.00 with the
// high-resolution draw off and 4.7 with it on (B1 piece 4, 2026-11-18).
//
// mw1hirescheck --same A.bmp B.bmp CANVAS_W CANVAS_H L T R B [MAX]: does shot A show nothing over what reference B
// shows flat in canvas rect [L, R) x [T, B)? B4's draw order (research/hires-2d.md): the lab's load box with the
// high-resolution draw on (A) against off (B, the canvas, right by construction). Only the texels where B is flat
// (every texel within 2 of it within 12 levels of luma: the box's plain fill, its own words and edges and their
// halo left out, as those are drawn finer in A by design) count; in each, A must hold that fill too: its mean within
// 24 of B's and no pixel more than 48 from it (a lab word drawn over the box is a stroke inside the texel). Prints
// the count of flat texels and of those that differ, and exits 1 when more than MAX (default 0) differ, 2 when it
// can't tell (no shot, or under 1000 flat texels).
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
// A BMP's luma, w x h; empty when it can't be read.
std::vector<float> luma(const char* path, int* w, int* h) {
  std::vector<float> l;
  SDL_Surface* s = SDL_LoadBMP(path);
  SDL_Surface* rgb = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB24) : nullptr;
  if (s) SDL_DestroySurface(s);
  if (!rgb) return l;
  *w = rgb->w, *h = rgb->h;
  l.resize(static_cast<size_t>(*w) * *h);
  for (int y = 0; y < *h; ++y) {
    const auto* row = static_cast<const uint8_t*>(rgb->pixels) + static_cast<size_t>(y) * rgb->pitch;
    for (int x = 0; x < *w; ++x)
      l[static_cast<size_t>(y) * *w + x] = 0.299f * row[3 * x] + 0.587f * row[3 * x + 1] + 0.114f * row[3 * x + 2];
  }
  SDL_DestroySurface(rgb);
  return l;
}

int same(int argc, char** argv) {
  if (argc < 10) {
    std::fprintf(stderr, "usage: mw1hirescheck --same A.bmp B.bmp CANVAS_W CANVAS_H L T R B [MAX]\n");
    return 2;
  }
  int aw = 0, ah = 0, bw = 0, bh = 0;
  const std::vector<float> a = luma(argv[2], &aw, &ah), b = luma(argv[3], &bw, &bh);
  const int cw = std::atoi(argv[4]), ch = std::atoi(argv[5]);
  const int L = std::atoi(argv[6]), T = std::atoi(argv[7]), R = std::atoi(argv[8]), B = std::atoi(argv[9]);
  const long max = argc > 10 ? std::atol(argv[10]) : 0;
  if (a.empty() || b.empty() || aw != bw || ah != bh || cw <= 0 || ch <= 0 || L < 0 || T < 0 || R > cw || B > ch || L >= R || T >= B) {
    std::fprintf(stderr, "mw1hirescheck: the shots can't be read, differ in size, or the rect is off the canvas\n");
    return 2;
  }
  const double sx = static_cast<double>(aw) / cw, sy = static_cast<double>(ah) / ch;
  // Each texel's pixels in a shot: their mean, and (for A) the farthest any is from a given level.
  auto box = [&](int i, int j, int* x0, int* x1, int* y0, int* y1) {
    *x0 = static_cast<int>(std::ceil(i * sx)), *x1 = std::min(aw, static_cast<int>(std::ceil((i + 1) * sx)));
    *y0 = static_cast<int>(std::ceil(j * sy)), *y1 = std::min(ah, static_cast<int>(std::ceil((j + 1) * sy)));
  };
  auto mean = [&](const std::vector<float>& l, int i, int j) {
    int x0, x1, y0, y1;
    box(i, j, &x0, &x1, &y0, &y1);
    double s = 0;
    long n = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) s += l[static_cast<size_t>(y) * aw + x], ++n;
    return n ? s / n : -1.0;
  };
  std::vector<double> bm(static_cast<size_t>(cw) * ch, -1);
  for (int j = 0; j < ch; ++j)
    for (int i = 0; i < cw; ++i) bm[static_cast<size_t>(j) * cw + i] = mean(b, i, j);
  long flat = 0, differ = 0;
  for (int j = T; j < B; ++j)
    for (int i = L; i < R; ++i) {
      const double m = bm[static_cast<size_t>(j) * cw + i];
      bool level = m >= 0;
      for (int dj = -2; level && dj <= 2; ++dj)
        for (int di = -2; level && di <= 2; ++di) {
          const int x = i + di, y = j + dj;
          if (x < 0 || y < 0 || x >= cw || y >= ch) continue;
          level = std::fabs(bm[static_cast<size_t>(y) * cw + x] - m) <= 12;
        }
      if (!level) continue;
      ++flat;
      int x0, x1, y0, y1;
      box(i, j, &x0, &x1, &y0, &y1);
      double far = 0;
      for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) far = std::max(far, std::fabs(a[static_cast<size_t>(y) * aw + x] - m));
      if (std::fabs(mean(a, i, j) - m) > 24 || far > 48) ++differ;
    }
  std::printf("mw1hirescheck: %ld of %ld flat texels in the rect differ (at most %ld wanted): %s\n", differ, flat, max,
              flat < 1000 ? "too few to tell" : differ <= max ? "nothing drawn over" : "something drawn over");
  if (flat < 1000) return 2;
  return differ <= max ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--same") == 0) return same(argc, argv);
  if (argc < 4) {
    std::fprintf(stderr, "usage: mw1hirescheck FRAME.bmp CANVAS_W CANVAS_H [MIN]\n");
    return 2;
  }
  const int cw = std::atoi(argv[2]), ch = std::atoi(argv[3]);
  const double min = argc > 4 ? std::atof(argv[4]) : 2.0;
  SDL_Surface* s = SDL_LoadBMP(argv[1]);
  SDL_Surface* rgb = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB24) : nullptr;
  if (!rgb || cw <= 0 || ch <= 0) {
    std::fprintf(stderr, "mw1hirescheck: %s: %s\n", argv[1], SDL_GetError());
    return 2;
  }
  const int w = rgb->w, h = rgb->h;
  std::vector<float> l(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y) {
    const auto* row = static_cast<const uint8_t*>(rgb->pixels) + static_cast<size_t>(y) * rgb->pitch;
    for (int x = 0; x < w; ++x) l[static_cast<size_t>(y) * w + x] = 0.299f * row[3 * x] + 0.587f * row[3 * x + 1] + 0.114f * row[3 * x + 2];
  }
  SDL_DestroySurface(rgb), SDL_DestroySurface(s);
  const double sx = static_cast<double>(w) / cw, sy = static_cast<double>(h) / ch;
  if (sx < 3 || sy < 3) {
    std::fprintf(stderr, "mw1hirescheck: a texel is %.2f x %.2f pixels: too few to look inside (shoot at 4K)\n", sx, sy);
    return 2;
  }
  auto at = [&](int x, int y) { return l[static_cast<size_t>(y) * w + x]; };
  auto centre = [&](int i, int j) {
    return at(std::min(w - 1, static_cast<int>((i + 0.5) * sx)), std::min(h - 1, static_cast<int>((j + 0.5) * sy)));
  };
  double sum = 0;
  long pixels = 0, texels = 0;
  for (int j = 0; j + 1 < ch; ++j)
    for (int i = 0; i + 1 < cw; ++i) {
      const float c = centre(i, j);
      if (std::fabs(c - centre(i + 1, j)) < 32 && std::fabs(c - centre(i, j + 1)) < 32) continue;  // not an edge
      ++texels;
      const int x0 = static_cast<int>(std::ceil(i * sx)) + 1, x1 = static_cast<int>(std::floor((i + 1) * sx)) - 1;
      const int y0 = static_cast<int>(std::ceil(j * sy)) + 1, y1 = static_cast<int>(std::floor((j + 1) * sy)) - 1;
      for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) sum += std::fabs(at(x, y) - c), ++pixels;
    }
  const double mean = pixels ? sum / pixels : 0;
  std::printf("mw1hirescheck: %ld edge texels, %ld pixels inside them, mean difference from the texel's centre %.2f "
              "(at least %.2f wanted): %s\n",
              texels, pixels, mean, min, mean >= min ? "detail inside the texels" : "blocky, the canvas magnified");
  return mean >= min ? 0 : 1;
}
