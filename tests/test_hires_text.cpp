// Ours (MW1's FIXES.md item 12, mw1/research/hires-2d.md B1 piece 4): platform::hires::TextRaster, the canvas's text
// at the frame's scale. The line's ink where the canvas's is (times the scale), each glyph between at the big font's
// own spacing (FIXES.md item 94), on MW1's fonts and Pirate's Moon's (read, never changed), at 1080p's, 1440p's and
// 4K's scales.
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "platform/hires_text.h"

namespace {
namespace fs = std::filesystem;
using platform::hires::FontKey;
using platform::hires::GlyphQuad;
using platform::hires::TextRaster;

std::string mw1_font(const char* f) { return (fs::path(__FILE__).parent_path().parent_path() / "mw1" / "data" / "fonts" / f).string(); }

// The canvas font as the host opens it (MONO, no kerning, its style).
TTF_Font* canvas_font(const FontKey& k) {
  TTF_Font* f = TTF_OpenFont(k.path.c_str(), static_cast<float>(k.em));
  if (!f) return nullptr;
  TTF_SetFontHinting(f, TTF_HINTING_MONO);
  TTF_SetFontKerning(f, false);
  TTF_SetFontStyle(f, (k.weight >= 600 ? TTF_STYLE_BOLD : 0) | (k.italic ? TTF_STYLE_ITALIC : 0));
  return f;
}

// The canvas's ink across a line as DrawTextA draws it (whole, or glyph by glyph with the tracking): its first and
// last inked columns, from x 0; and each glyph's pen.
struct CanvasLine { int left = 1 << 30, right = -1; std::vector<int> pens; int width = 0; };
CanvasLine canvas_line(TTF_Font* f, const std::string& s, int tracking) {
  CanvasLine c;
  auto ink = [&](const char* t, size_t n, int at) {
    SDL_Surface* surf = TTF_RenderText_Solid(f, t, n, SDL_Color{255, 255, 255, 255});
    if (!surf) return;
    const auto* p = static_cast<const uint8_t*>(surf->pixels);
    for (int y = 0; y < surf->h; ++y)
      for (int x = 0; x < surf->w; ++x)
        if (p[y * surf->pitch + x]) c.left = std::min(c.left, at + x), c.right = std::max(c.right, at + x);
    SDL_DestroySurface(surf);
  };
  int pen = 0, w = 0, h = 0;
  for (size_t k = 0; k < s.size();) {
    size_t n = 1;
    while (k + n < s.size() && (static_cast<unsigned char>(s[k + n]) & 0xc0) == 0x80) ++n;
    if (tracking) {
      c.pens.push_back(pen);
      ink(s.c_str() + k, n, pen);
      TTF_GetStringSize(f, s.c_str() + k, n, &w, &h);
      pen += w + tracking;
    } else {
      w = 0;
      if (k) TTF_GetStringSize(f, s.c_str(), k, &w, &h);
      c.pens.push_back(w);
    }
    k += n;
  }
  if (!tracking) {
    ink(s.c_str(), s.size(), 0);
    TTF_GetStringSize(f, s.c_str(), s.size(), &c.width, &h);
  } else {
    c.width = pen - tracking;
  }
  return c;
}
// A glyph's ink's start from its pen in the big font: the first inked column of its render, less the render's start
// left of the pen for a negative left bearing (SDL_ttf starts the render at the ink then).
int ink_from_pen(TTF_Font* f, char ch) {
  int minx = 0, maxx = 0, miny = 0, maxy = 0, adv = 0;
  TTF_GetGlyphMetrics(f, static_cast<Uint32>(static_cast<unsigned char>(ch)), &minx, &maxx, &miny, &maxy, &adv);
  SDL_Surface* s = TTF_RenderText_Blended(f, &ch, 1, SDL_Color{255, 255, 255, 255});
  if (!s) return minx;
  SDL_Surface* a = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
  int x0 = a ? a->w : 0;
  for (int y = 0; a && y < a->h; ++y)
    for (int x = 0; x < x0; ++x)
      if (reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(a->pixels) + static_cast<size_t>(y) * a->pitch)[x] >> 24) x0 = x;
  if (a) SDL_DestroySurface(a);
  SDL_DestroySurface(s);
  return x0 + std::min(minx, 0);
}
}  // namespace

TEST(hires_text_on_the_canvas_pen) {
  if (!TTF_WasInit() && !TTF_Init()) {
    std::printf("  no SDL_ttf\n");
    return;
  }
  struct Case { const char* what; FontKey key; std::string text; };
  const std::string text = "MECH COMPLEX 1,029,000 Wages";
  // The owner's objectives panel (FIXES.md item 94): "tow er", "Objec tives" crowded, "In Progress" uneven.
  const std::string objectives = "Destroy the tower  OP: Tower Primary Objectives In Progress";
  const std::string arial = std::string(MW3_GAME_DIR) + "/arial.ttf";
  std::vector<Case> cases = {
      {"Saira Condensed Bold, tracked", {mw1_font("SairaCondensed-Bold.ttf"), 20, 700, false, 2, 1}, text},
      {"Share Tech Mono, untracked", {mw1_font("ShareTechMono-Regular.ttf"), 16, 400, false, 0, 0}, text},
      {"Share Tech Mono, tracked", {mw1_font("ShareTechMono-Regular.ttf"), 16, 400, false, 1, 0}, text},
      {"Pirate's Moon's impact, untracked", {std::string(MW3_GAME_DIR) + "/impact.ttf", 18, 400, false, 0, 2}, text},
      {"Pirate's Moon's arial 12, the objectives", {arial, 12, 400, false, 0, 0}, objectives},
      {"Pirate's Moon's arial 14 bold, the objectives", {arial, 14, 700, false, 0, 0}, objectives},
  };
  TextRaster r;
  for (const Case& c : cases) {
    const std::string& text = c.text;
    if (!fs::exists(c.key.path)) {
      std::printf("  %s: no font file\n", c.what);
      EXPECT(false);
      continue;
    }
    TTF_Font* cf = canvas_font(c.key);
    EXPECT(cf != nullptr);
    if (!cf) continue;
    const CanvasLine line = canvas_line(cf, text, c.key.tracking);
    for (float s : {2.25f, 3.0f, 4.5f}) {
      std::vector<GlyphQuad> q;
      const float pen_x = 37.0f, top_y = 11.0f;
      EXPECT(r.layout(c.key, text, pen_x, top_y, s, s, &q));
      // A quad a glyph with ink (the spaces have none).
      const size_t inked = text.size() - static_cast<size_t>(std::count(text.begin(), text.end(), ' '));
      EXPECT(q.size() == inked);
      if (q.size() != inked) continue;
      // Each glyph's ink starts where its pen is, times the scale, give or take its bearing: within its first
      // canvas pixel and a frame pixel.
      // The big font as the raster opens it, for each glyph's own left bearing (its ink's start from its pen).
      TTF_Font* bf = TTF_OpenFont(c.key.path.c_str(), static_cast<float>(c.key.em) * s);
      TTF_SetFontHinting(bf, TTF_HINTING_LIGHT);
      TTF_SetFontKerning(bf, false);
      TTF_SetFontStyle(bf, (c.key.weight >= 600 ? TTF_STYLE_BOLD : 0) | (c.key.italic ? TTF_STYLE_ITALIC : 0));
      // The big font's pen before each glyph (frame pixels from the line's start): its advances (no kerning) and the
      // tracking times the scale; and the stretch that takes them to the canvas font's advances times the scale.
      const int tr = c.key.tracking;
      auto adv_of = [](TTF_Font* f, char ch) {
        int a0 = 0, a1 = 0, a2 = 0, a3 = 0, adv = 0;
        TTF_GetGlyphMetrics(f, static_cast<Uint32>(static_cast<unsigned char>(ch)), &a0, &a1, &a2, &a3, &adv);
        return adv;
      };
      std::vector<float> big_pen(text.size(), 0.0f);
      float big_w = 0.0f, canvas_w = 0.0f;
      for (size_t k = 0; k < text.size(); ++k) {
        const bool last = k + 1 == text.size();
        big_pen[k] = big_w;
        big_w += static_cast<float>(adv_of(bf, text[k])) + (last ? 0.0f : static_cast<float>(tr) * s);
        canvas_w += static_cast<float>(adv_of(cf, text[k]) + (last ? 0 : tr));
      }
      const float stretch = canvas_w * s / big_w;
      EXPECT(stretch > 0.9f && stretch < 1.1f);  // the two fonts' lines within a tenth of each other
      size_t at = 0;
      float worst = 0, worst_gap = 0, worst_gap_canvas = 0;
      size_t prev = text.size();
      for (size_t k = 0; k < text.size(); ++k) {
        if (text[k] == ' ') continue;
        const float want = std::round(pen_x * s + big_pen[k] * stretch) + static_cast<float>(ink_from_pen(bf, text[k]));
        worst = std::max(worst, std::abs(q[at].x - want));
        EXPECT(std::abs(q[at].x - want) <= 0.5f);  // the pen snapped to a whole frame pixel: half a pixel at most
        EXPECT(q[at].x == std::floor(q[at].x) && q[at].y == std::floor(q[at].y));  // whole frame pixels
        // Each pen-to-pen step against the big font's own (times the stretch): a frame pixel at most off. The canvas
        // pen's step times the scale (the old placement) for comparison: its whole canvas pixels miss by up to one
        // canvas pixel, 2 to 5 frame pixels.
        if (prev < text.size()) {
          const float step = (q[at].x - static_cast<float>(ink_from_pen(bf, text[k]))) - (q[at - 1].x - static_cast<float>(ink_from_pen(bf, text[prev])));
          const float own = (big_pen[k] - big_pen[prev]) * stretch;
          const float canvas_step = static_cast<float>(line.pens[k] - line.pens[prev]) * s;
          worst_gap = std::max(worst_gap, std::abs(step - own));
          worst_gap_canvas = std::max(worst_gap_canvas, std::abs(canvas_step - own));
          EXPECT(std::abs(step - own) <= 1.0f);
        }
        prev = k;
        ++at;
      }
      TTF_CloseFont(bf);
      // The line's ink across: from its first to its last column where the canvas's is, within a canvas pixel times
      // the scale.
      float left = 1e9f, right = -1e9f;
      for (const GlyphQuad& g : q) left = std::min(left, g.x), right = std::max(right, g.x + g.w);
      const float want_l = (pen_x + static_cast<float>(line.left)) * s, want_r = (pen_x + static_cast<float>(line.right + 1)) * s;
      std::printf("  %s at %.2f: ink %.1f..%.1f, the canvas's %.1f..%.1f; the furthest glyph %.1f from its pen and bearing; "
                  "steps off the font's by %.1f at most (the canvas pen's would be %.1f)\n",
                  c.what, static_cast<double>(s), static_cast<double>(left), static_cast<double>(right), static_cast<double>(want_l),
                  static_cast<double>(want_r), static_cast<double>(worst), static_cast<double>(worst_gap),
                  static_cast<double>(worst_gap_canvas));
      // (Plus the half frame pixel the snap may move a glyph: impact's last glyph is about 1.2 canvas pixels wider
      // big than in the canvas's mono hinting, 5.5 frame pixels at 4.5 before the snap.)
      EXPECT(std::abs(left - want_l) <= s + 1.5f && std::abs(right - want_r) <= s + 1.5f);
      // The line's ink between the canvas line's top and its foot, times the scale.
      const float lh = static_cast<float>(TTF_GetFontHeight(cf));
      for (const GlyphQuad& g : q) EXPECT(g.y >= (top_y + c.key.shift - 1.0f) * s && g.y + g.h <= (top_y + c.key.shift + lh + 1.0f) * s);
      // Each quad's coverage in the atlas, where it says.
      int32_t aw = 0, ah = 0;
      const uint8_t* a = r.atlas(&aw, &ah);
      bool inked_all = true;
      for (const GlyphQuad& g : q) {
        int32_t sum = 0;
        for (int32_t y = 0; y < g.ah; ++y)
          for (int32_t x = 0; x < g.aw; ++x) sum += a[static_cast<size_t>(g.ay + y) * aw + g.ax + x];
        inked_all = inked_all && g.ax + g.aw <= aw && g.ay + g.ah <= ah && sum > 0 && g.w == g.aw && g.h == g.ah;
      }
      EXPECT(inked_all);
    }
    TTF_CloseFont(cf);
  }
}

TEST(hires_text_atlas_scale_and_failure) {
  if (!TTF_WasInit() && !TTF_Init()) return;
  TextRaster r;
  std::vector<GlyphQuad> q;
  // A missing font: false, nothing appended.
  EXPECT(!r.layout({"/nonexistent/font.ttf", 16, 400, false, 0, 0}, "ABC", 0, 0, 3, 3, &q) && q.empty());
  const FontKey mono{mw1_font("ShareTechMono-Regular.ttf"), 16, 400, false, 0, 0};
  platform::hires::Rect d;
  EXPECT(r.take_dirty(&d) && d.r == 512 && d.b == 512);  // the new atlas, whole
  EXPECT(!r.take_dirty(&d));
  EXPECT(r.layout(mono, "AB", 0, 0, 3, 3, &q) && q.size() == 2);
  const uint64_t s3 = r.atlas_serial();
  EXPECT(r.take_dirty(&d) && d.l <= q[0].ax && d.r >= q[1].ax + q[1].aw);  // the new glyphs' part
  q.clear();
  EXPECT(r.layout(mono, "AB", 0, 0, 3, 3, &q) && q.size() == 2 && r.atlas_serial() == s3 && !r.take_dirty(&d));  // cached
  q.clear();
  EXPECT(r.layout(mono, "AB", 0, 0, 4.5f, 4.5f, &q) && r.atlas_serial() != s3);  // a new scale: remade
  EXPECT(q.size() == 2 && q[0].h > 0);

  // Huge glyphs: the page doubles to its largest; a line that can't fit even alone is refused; one that fits is
  // placed after a clear, its quads together valid.
  const FontKey huge{mw1_font("SairaCondensed-Bold.ttf"), 300, 700, false, 0, 0};
  q.clear();
  const bool too_many = r.layout(huge, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0, 0, 4.5f, 4.5f, &q);
  EXPECT(!too_many && q.empty());
  int32_t aw = 0, ah = 0;
  r.atlas(&aw, &ah);
  EXPECT(aw == TextRaster::kMaxAtlas && ah == TextRaster::kMaxAtlas);
  const uint64_t before = r.atlas_serial();
  q.clear();
  EXPECT(r.layout(huge, "WXYZ", 0, 0, 4.5f, 4.5f, &q) && q.size() == 4);
  q.clear();
  EXPECT(r.layout(huge, "ABCD", 0, 0, 4.5f, 4.5f, &q) && q.size() == 4);
  std::printf("  the atlas %dx%d, serial %llu then %llu\n", aw, ah, static_cast<unsigned long long>(before),
              static_cast<unsigned long long>(r.atlas_serial()));
  // The quads of the last call: each inked in the atlas as it stands.
  const uint8_t* a = r.atlas(&aw, &ah);
  for (const GlyphQuad& g : q) {
    int32_t sum = 0;
    for (int32_t y = 0; y < g.ah; y += 4)
      for (int32_t x = 0; x < g.aw; x += 4) sum += a[static_cast<size_t>(g.ay + y) * aw + g.ax + x];
    EXPECT(sum > 0);
  }
}
