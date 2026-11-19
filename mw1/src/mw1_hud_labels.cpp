// FIXES.md item 82: the painter for the HUD pictures' labels (mw1_hud_art.h, LabelPainter): the text in PM's HUD face
// (its gauges' "Arial": the font file the 2D's own text opens for it, SdlTwodHost::font_path), rendered big with
// SDL_ttf, its ink box taken and averaged down into the label's box: as tall as the box, its width the face's at that
// height or the box's if narrower. The mw1 program's alone (mw1core has no SDL_ttf; without this the labels stay PM's
// pixels).
#include "mw1_hud_labels.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "frontend/sdl/sdl_twod_host.h"
#include "mw1_hud_art.h"

namespace mw1 {
namespace {

// The text's coverage at `size` points: the ink's box alone, one byte a pixel (empty on failure).
std::vector<uint8_t> render(const std::string& path, float size, const std::string& text, int32_t* w, int32_t* h) {
  static std::mutex lock;
  static std::map<float, TTF_Font*> fonts;
  std::lock_guard<std::mutex> hold(lock);
  if (!TTF_WasInit() && !TTF_Init()) return {};
  TTF_Font*& f = fonts[size];
  if (!f) f = TTF_OpenFont(path.c_str(), size);
  if (!f) return {};
  TTF_SetFontKerning(f, false);
  SDL_Surface* s = TTF_RenderText_Blended(f, text.c_str(), 0, SDL_Color{255, 255, 255, 255});
  if (!s) return {};
  SDL_Surface* rgba = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(s);
  if (!rgba) return {};
  int32_t x0 = rgba->w, y0 = rgba->h, x1 = -1, y1 = -1;
  auto alpha = [rgba](int32_t x, int32_t y) {
    return static_cast<const uint8_t*>(rgba->pixels)[static_cast<size_t>(y) * rgba->pitch + x * 4 + 3];
  };
  for (int32_t y = 0; y < rgba->h; ++y)
    for (int32_t x = 0; x < rgba->w; ++x)
      if (alpha(x, y)) x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
  std::vector<uint8_t> out;
  if (x1 >= x0) {
    *w = x1 - x0 + 1, *h = y1 - y0 + 1;
    for (int32_t y = y0; y <= y1; ++y)
      for (int32_t x = x0; x <= x1; ++x) out.push_back(alpha(x, y));
  }
  SDL_DestroySurface(rgba);
  return out;
}

std::vector<uint8_t> paint(const std::string& path, const std::string& text, int32_t bw, int32_t bh, int32_t align) {
  if (bw < 1 || bh < 1) return {};
  // Rendered at about six times the box's height (a digit's ink is about 0.72 of the size), then averaged down.
  int32_t iw = 0, ih = 0;
  const std::vector<uint8_t> ink = render(path, std::max(8.0f, bh * 8.0f), text, &iw, &ih);
  if (ink.empty()) return {};
  const double scale = static_cast<double>(bh) / ih;
  const int32_t tw = std::clamp(static_cast<int32_t>(std::lround(iw * scale)), 1, bw);
  const int32_t left = align < 0 ? 0 : align > 0 ? bw - tw : (bw - tw) / 2;
  std::vector<uint8_t> out(static_cast<size_t>(bw) * bh, 0);
  const double fx = static_cast<double>(iw) / tw, fy = static_cast<double>(ih) / bh;
  for (int32_t j = 0; j < bh; ++j)
    for (int32_t i = 0; i < tw; ++i) {
      // The source rectangle's mean, each source pixel by how much of it is inside.
      const double sx0 = i * fx, sx1 = (i + 1) * fx, sy0 = j * fy, sy1 = (j + 1) * fy;
      double sum = 0;
      for (int32_t y = static_cast<int32_t>(sy0); y < std::min<double>(ih, std::ceil(sy1)); ++y) {
        const double cy = std::min<double>(y + 1, sy1) - std::max<double>(y, sy0);
        for (int32_t x = static_cast<int32_t>(sx0); x < std::min<double>(iw, std::ceil(sx1)); ++x) {
          const double cx = std::min<double>(x + 1, sx1) - std::max<double>(x, sx0);
          sum += cx * cy * ink[static_cast<size_t>(y) * iw + x];
        }
      }
      out[static_cast<size_t>(j) * bw + left + i] = static_cast<uint8_t>(std::lround(std::clamp(sum / (fx * fy), 0.0, 255.0)));
    }
  return out;
}

}  // namespace

void use_hud_label_font() {
  const std::string path = sdlfe::SdlTwodHost::font_path(sdlfe::SdlTwodHost::font_file("Arial"));
  if (path.empty()) return;  // no face: PM's pixels
  hud_art::set_label_painter([path](const std::string& text, int32_t w, int32_t h, int32_t align) {
    return paint(path, text, w, h, align);
  });
}

}  // namespace mw1
