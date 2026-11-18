// Ours (MW1: Critical Hit's FIXES.md item 12, mw1/research/hires-2d.md B1 piece 4): see hires_text.h.
#include "platform/hires_text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace platform::hires {

namespace {
// The font's identity apart from its size and hinting: the file and the style.
using FaceKey = std::tuple<std::string, int, bool>;  // path, bold, italic
FaceKey face(const FontKey& f) { return {f.path, f.weight >= 600 ? 1 : 0, f.italic}; }

int style(const FontKey& f) { return (f.weight >= 600 ? TTF_STYLE_BOLD : 0) | (f.italic ? TTF_STYLE_ITALIC : 0); }

// The UTF-8 glyphs of a line (a lead byte and its continuation bytes): each one's first byte and length.
std::vector<std::pair<size_t, size_t>> glyphs(const std::string& s) {
  std::vector<std::pair<size_t, size_t>> v;
  for (size_t k = 0; k < s.size();) {
    size_t n = 1;
    while (k + n < s.size() && (static_cast<unsigned char>(s[k + n]) & 0xc0) == 0x80) ++n;
    v.emplace_back(k, n);
    k += n;
  }
  return v;
}

constexpr int32_t kFirstAtlas = 512, kPad = 1;
}  // namespace

struct TextRaster::Impl {
  bool ttf_ours = false;
  float sx = 1.0f, sy = 1.0f;
  // The canvas fonts (MONO, at the key's em) and the big ones (LIGHT, at em * sy), by face and em.
  std::map<std::pair<FaceKey, int>, TTF_Font*> canvas, big;
  // A glyph rendered big: its ink's place in the atlas and its offset from the render's origin (the pen, the big
  // font's ascent above the baseline). aw 0: no ink.
  struct Glyph { int32_t ax = 0, ay = 0, aw = 0, ah = 0, ox = 0, oy = 0; };
  std::map<std::tuple<FaceKey, int, std::string>, Glyph> glyphs;
  // The atlas: a shelf packer over one page.
  int32_t w = kFirstAtlas, h = kFirstAtlas;
  std::vector<uint8_t> px = std::vector<uint8_t>(static_cast<size_t>(kFirstAtlas) * kFirstAtlas, 0);
  int32_t shelf_x = 0, shelf_y = 0, shelf_h = 0;
  uint64_t serial = 1;
  bool dirty = false;
  Rect dirt{0, 0, 0, 0};

  ~Impl() {
    for (auto* m : {&canvas, &big})
      for (auto& [k, f] : *m) TTF_CloseFont(f);
    if (ttf_ours) TTF_Quit();
  }
  void mark(int32_t l, int32_t t, int32_t r, int32_t b) {
    if (!dirty) dirt = {l, t, r, b}, dirty = true;
    else dirt = {std::min(dirt.l, l), std::min(dirt.t, t), std::max(dirt.r, r), std::max(dirt.b, b)};
  }
  void remade() {
    ++serial;
    dirty = false;
    mark(0, 0, w, h);
  }
  void clear() {
    std::fill(px.begin(), px.end(), 0);
    shelf_x = shelf_y = shelf_h = 0;
    glyphs.clear();
    remade();
  }
  void drop_big() {
    for (auto& [k, f] : big) TTF_CloseFont(f);
    big.clear();
    clear();
  }
  TTF_Font* open(const FontKey& f, bool large) {
    auto& m = large ? big : canvas;
    const auto key = std::make_pair(face(f), f.em);
    auto it = m.find(key);
    if (it != m.end()) return it->second;
    TTF_Font* t = TTF_OpenFont(f.path.c_str(), large ? static_cast<float>(f.em) * sy : static_cast<float>(f.em));
    if (!t) return nullptr;
    TTF_SetFontHinting(t, large ? TTF_HINTING_LIGHT : TTF_HINTING_MONO);
    TTF_SetFontKerning(t, false);
    TTF_SetFontStyle(t, style(f));
    m.emplace(key, t);
    return t;
  }
  // Room for a w x h glyph: the shelf's next place, a new shelf, the page doubled; false when it's at its largest.
  bool place(int32_t gw, int32_t gh, int32_t* x, int32_t* y) {
    for (;;) {
      if (shelf_x + gw + kPad > w) shelf_y += shelf_h, shelf_x = 0, shelf_h = 0;
      if (shelf_y + gh + kPad <= h && gw + kPad <= w) {
        *x = shelf_x, *y = shelf_y;
        shelf_x += gw + kPad, shelf_h = std::max(shelf_h, gh + kPad);
        return true;
      }
      if (w >= TextRaster::kMaxAtlas) return false;
      grow();
    }
  }
  void grow() {  // doubled, what's there kept where it is (the shelves go on below)
    const int32_t nw = w * 2, nh = h * 2;
    std::vector<uint8_t> n(static_cast<size_t>(nw) * nh, 0);
    for (int32_t y = 0; y < h; ++y) std::memcpy(&n[static_cast<size_t>(y) * nw], &px[static_cast<size_t>(y) * w], static_cast<size_t>(w));
    // The current shelf ends at the old width; the next starts below everything placed.
    shelf_y += shelf_h, shelf_x = 0, shelf_h = 0;
    px.swap(n), w = nw, h = nh;
    remade();
  }
  // The glyph rendered big and put in the atlas (or found there); null when it can't be, and full when the atlas
  // had no room.
  const Glyph* glyph(const FontKey& f, TTF_Font* t, const std::string& g, bool* full) {
    const auto key = std::make_tuple(face(f), f.em, g);
    auto it = glyphs.find(key);
    if (it != glyphs.end()) return &it->second;
    SDL_Surface* s = TTF_RenderText_Blended(t, g.c_str(), g.size(), SDL_Color{255, 255, 255, 255});
    Glyph out;
    if (s) {
      SDL_Surface* a = s->format == SDL_PIXELFORMAT_ARGB8888 ? s : SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
      if (!a) {
        SDL_DestroySurface(s);
        return nullptr;
      }
      auto cov = [&](int32_t x, int32_t y) {
        return static_cast<uint8_t>(reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(a->pixels) + static_cast<size_t>(y) * a->pitch)[x] >> 24);
      };
      int32_t x0 = a->w, y0 = a->h, x1 = -1, y1 = -1;
      for (int32_t y = 0; y < a->h; ++y)
        for (int32_t x = 0; x < a->w; ++x)
          if (cov(x, y)) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
      if (x1 >= 0) {
        const int32_t gw = x1 - x0 + 1, gh = y1 - y0 + 1;
        int32_t px0 = 0, py0 = 0;
        if (!place(gw, gh, &px0, &py0)) {
          if (a != s) SDL_DestroySurface(a);
          SDL_DestroySurface(s);
          *full = true;
          return nullptr;
        }
        for (int32_t y = 0; y < gh; ++y)
          for (int32_t x = 0; x < gw; ++x) px[static_cast<size_t>(py0 + y) * w + px0 + x] = cov(x0 + x, y0 + y);
        mark(px0, py0, px0 + gw, py0 + gh);
        out = {px0, py0, gw, gh, x0, y0};
      }
      if (a != s) SDL_DestroySurface(a);
      SDL_DestroySurface(s);
    } else if (!g.empty() && g != " ") {
      return nullptr;
    }
    return &glyphs.emplace(key, out).first->second;
  }
};

TextRaster::TextRaster() : impl_(std::make_unique<Impl>()) {
  if (!TTF_WasInit()) impl_->ttf_ours = TTF_Init();
  impl_->mark(0, 0, impl_->w, impl_->h);
}
TextRaster::~TextRaster() = default;

void TextRaster::set_scale(float sx, float sy) {
  if (sx == impl_->sx && sy == impl_->sy) return;
  impl_->sx = sx, impl_->sy = sy;
  impl_->drop_big();
}

const uint8_t* TextRaster::atlas(int32_t* w, int32_t* h) const {
  if (w) *w = impl_->w;
  if (h) *h = impl_->h;
  return impl_->px.data();
}

bool TextRaster::take_dirty(Rect* r) {
  if (!impl_->dirty) return false;
  if (r) *r = impl_->dirt;
  impl_->dirty = false;
  return true;
}

uint64_t TextRaster::atlas_serial() const { return impl_->serial; }

bool TextRaster::layout(const FontKey& f, const std::string& utf8, float pen_x, float top_y, float sx, float sy,
                        std::vector<GlyphQuad>* out) {
  if (!out || f.path.empty() || f.em <= 0) return false;
  set_scale(sx, sy);
  Impl& m = *impl_;
  TTF_Font* canvas = m.open(f, false);
  TTF_Font* big = canvas ? m.open(f, true) : nullptr;
  if (!canvas || !big) return false;
  const auto gs = glyphs(utf8);
  // The canvas pen before each glyph (canvas pixels, from pen_x).
  std::vector<float> pen(gs.size(), pen_x);
  int pw = 0, ph = 0;
  if (f.tracking == 0) {
    for (size_t i = 0; i < gs.size(); ++i) {  // the line's bytes before the glyph, measured as one string
      pw = 0;
      if (gs[i].first > 0 && !TTF_GetStringSize(canvas, utf8.c_str(), gs[i].first, &pw, &ph)) return false;
      pen[i] = pen_x + static_cast<float>(pw);
    }
  } else {
    float p = pen_x;
    for (size_t i = 0; i < gs.size(); ++i) {
      pen[i] = p;
      pw = 0;
      if (!TTF_GetStringSize(canvas, utf8.c_str() + gs[i].first, gs[i].second, &pw, &ph)) return false;
      p += static_cast<float>(pw + f.tracking);
    }
  }
  // Snapped to whole frame pixels (the pen below too): the atlas's coverage lands texel on pixel, never resampled
  // half a pixel off (soft); each glyph within half a frame pixel of the canvas pen's place.
  const float baseline = std::round((top_y + static_cast<float>(f.shift + TTF_GetFontAscent(canvas))) * sy);
  const float big_top = baseline - static_cast<float>(TTF_GetFontAscent(big));
  // Twice at most: a full atlas is cleared once and the line placed from the start.
  for (int attempt = 0; attempt < 2; ++attempt) {
    std::vector<GlyphQuad> q;
    bool full = false, failed = false;
    for (size_t i = 0; i < gs.size() && !full && !failed; ++i) {
      const Impl::Glyph* g = m.glyph(f, big, utf8.substr(gs[i].first, gs[i].second), &full);
      if (!g) {
        failed = !full;
        continue;
      }
      if (g->aw == 0) continue;  // no ink
      q.push_back({std::round(pen[i] * sx) + static_cast<float>(g->ox), big_top + static_cast<float>(g->oy), static_cast<float>(g->aw),
                   static_cast<float>(g->ah), g->ax, g->ay, g->aw, g->ah});
    }
    if (failed) return false;
    if (!full) {
      out->insert(out->end(), q.begin(), q.end());
      return true;
    }
    m.clear();
  }
  return false;
}

}  // namespace platform::hires
