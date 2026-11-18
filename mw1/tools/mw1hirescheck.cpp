// mw1hirescheck FRAME.bmp CANVAS_W CANVAS_H [MIN]: is a shot's 2D only the 480-line canvas magnified (FIXES.md item
// 12, research/hires-2d.md B4)? The canvas's texels are found in the frame (the frame's size over the canvas's), and
// at each texel on an edge (its colour unlike a neighbour's) the pixels inside it, its one-pixel border left out (the
// sharp scaling's blend), are compared with its centre's. A canvas magnified is flat inside every texel: a difference
// near 0. Text and pictures drawn at the window's resolution have detail inside a texel. Prints the mean difference
// (luma, 0..255) and exits 1 when it is under MIN (default 2): blocky. The office at 4K reads 0.00 with the
// high-resolution draw off and 4.7 with it on (B1 piece 4, 2026-11-18).
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
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
