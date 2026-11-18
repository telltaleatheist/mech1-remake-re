#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 12, mw1/research/hires-2d.md B1 piece 4): text rasterised at the frame's
// scale with the canvas's own pen, so a line's glyphs land where the 480-line canvas put them (the layouts, the fit
// guards and the overlap checks stay valid) but each is drawn from a font opened at the frame's size. No GPU: glyph
// coverage (A8) in one atlas the draw side uploads; SDL_ttf only.
//
// The rules (the canvas's: SdlTwodHost::DrawTextA):
// - the pen on the canvas font (opened at the key's em, MONO hinting, no kerning, its style): tracking 0, each glyph
//   at the width of the line's bytes before it measured as one string (the whole-line draw's own positions);
//   tracked, glyph by glyph, each glyph's own width and the tracking after it;
// - each glyph's quad at its pen times sx (rounded to a whole frame pixel) plus where the big glyph's ink starts in
//   its render (its left bearing);
// - the baseline: (top_y + shift + the canvas font's ascent) times sy, rounded; a glyph's ink below it as the big
//   font has it. So every quad's x and y are whole frame pixels (the draw samples the atlas texel on pixel);
// - the big font: em times sy (a float size), LIGHT hinting, no kerning, the same bold and italic, rendered Blended,
//   its coverage kept. The big font's advances are never used.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/hires_lists.h"

namespace platform::hires {

struct GlyphQuad {
  float x, y, w, h;            // frame pixels, (0, 0) the frame's top left
  int32_t ax, ay, aw, ah;      // its place in the atlas
};

class TextRaster {
 public:
  TextRaster();
  ~TextRaster();
  TextRaster(const TextRaster&) = delete;
  TextRaster& operator=(const TextRaster&) = delete;

  // The line's glyphs as quads (appended to `out`; a space or anything without ink none). False when a glyph can't
  // be made (the font missing, a glyph that won't render, the atlas full even for this line alone): nothing is
  // appended and the caller draws nothing (the canvas shows). A full atlas is cleared (atlas_serial changes) and this
  // line placed again from the start, so the quads one call returns are always valid together; earlier calls' quads
  // aren't after a serial change. sx, sy other than set_scale's set them first.
  bool layout(const FontKey& f, const std::string& utf8, float pen_x, float top_y, float sx, float sy,
              std::vector<GlyphQuad>* out);

  // The atlas: one byte of coverage a pixel, w x h (a page, doubling from 512 up to kMaxAtlas square).
  const uint8_t* atlas(int32_t* w, int32_t* h) const;
  static constexpr int32_t kMaxAtlas = 4096;
  // The part written since the last call (true, and `r`), else false. The whole atlas after a serial change.
  bool take_dirty(Rect* r);
  // Changes when the atlas is remade (grown, cleared, a new scale): upload it whole again.
  uint64_t atlas_serial() const;
  // The frame's scale (frame pixels a canvas pixel): a change drops the old scale's big fonts and glyphs.
  void set_scale(float sx, float sy);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace platform::hires
