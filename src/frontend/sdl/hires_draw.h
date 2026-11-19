#pragma once
// Ours (MW1: Critical Hit's FIXES.md item 12, mw1/research/hires-2d.md B1 piece 4): the screen's recorded draws drawn
// again at the frame's resolution, over the 480-line canvas just laid on the frame. Opt-in: only while
// platform::hires_2d() (MW1's program); MW3's and Pirate's Moon's never get here.
//
// Each frame, over the canvas's layer:
// 1. Lists::layers says where each draw shows (its chain from the canvas up, piece 3). A draw that can't be drawn
//    here (no source picture, a line the glyph atlas can't hold) is taken out with every draw over it at its pixels:
//    there the canvas shows, never a hole.
// 2. Under each chain, its base's "before" (what the canvas had under it), at the canvas's resolution: so the
//    canvas's own copy of the draw (its blocky glyphs, its picture) is covered. A text's box keeps its background
//    where the text filled one (OPAQUE), the colour of the cell beside a glyph.
// 3. Each draw at its pixels: a picture from its source (HiresImages), a line from the glyph atlas (TextRaster) at the
//    canvas's own pen.
// MW1_HIRES_DEBUG=1 tints what is drawn here magenta; what is left the canvas's is untinted.
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

#include "platform/hires_lists.h"
#include "platform/hires_text.h"
#include "platform/render3d.h"

namespace sdlfe {

class HiresDraw {
 public:
  HiresDraw(platform::Render3D* render, platform::hires::Lists* lists) : render_(render), lists_(lists) {}
  ~HiresDraw();
  // After the canvas's layer is drawn, before the frame is shown: the screen `pixels` (the lists' owner, pitch in
  // bytes) at width x height. `keyed`: an overlay over the 3D, `key` its clear pixels.
  void draw(const uint16_t* pixels, int width, int height, int pitch, bool rgb565, bool keyed, uint16_t key);
  // mw1 (engine, ours), FIXES 12: draw()'s first half, before the canvas's layer is uploaded (SdlScreen2D's
  // before_layer): which draws go where, what's under them. On a keyed overlay, the pixels whose chain is drawn here
  // with the key under its base (a pixel each, 1: drop the layer's canvas copy there, so the 3D is under the draws);
  // null when there are none. draw() of the same pixels then uses it.
  const uint8_t* prepare(const uint16_t* pixels, int width, int height, int pitch, bool rgb565, bool keyed, uint16_t key);
  // After the frame is shown: the pictures' sources may go (HiresImages::end_frame), unseen textures are dropped.
  void end_frame();

  // What the last draw() did (the tests' and MW1_HIRES_DEBUG's).
  struct Stats {
    int draws = 0;        // the screen's recorded draws
    int pictures = 0;     // drawn from their sources
    int lines = 0;        // drawn from the atlas
    int failed = 0;       // left to the canvas (no source, no glyphs)
    long under = 0;       // pixels under a chain (its base's "before")
    long unkeyed = 0;     // keyed: the layer's pixels dropped for a chain drawn here over the key
    int overflowed = 0;   // the frame's lines didn't fit one atlas (left to the canvas until the list changes)
    double ms = 0;        // draw()'s own time
  };
  const Stats& stats() const { return stats_; }

 private:
  bool short_before_ = false;  // the last frame had draws left to the canvas (said once)
  std::string short_what_, short_said_;  // which (the first few), for that line; the last said
  struct Picture {
    int32_t texture = -1;
    int unseen = 0;  // frames since last drawn
  };
  void textures_for_scale(float sx, float sy, int height);
  int32_t picture_texture(const platform::hires::Command& c, float* u_scale, float* v_scale);
  bool upload_atlas();

  platform::Render3D* render_;
  platform::hires::Lists* lists_;
  platform::hires::TextRaster text_;
  float sx_ = 0, sy_ = 0, art_scale_ = 0;
  std::unordered_map<uint64_t, Picture> pictures_;  // by HiresImage::serial
  int32_t atlas_tex_ = -1, atlas_w_ = 0, atlas_h_ = 0;
  int32_t under_tex_ = -1, under_w_ = 0, under_h_ = 0;
  std::vector<uint16_t> base_;
  std::vector<uint8_t> rgba_, under_rgba_, under_shown_;  // under_shown_: the under texture as last uploaded
  // The frame's working sets, kept from frame to frame (no allocation a frame once grown).
  std::vector<int32_t> tex_;
  std::vector<float> us_, vs_;
  std::vector<std::vector<platform::hires::GlyphQuad>> glyphs_;
  std::vector<uint8_t> ok_, bad_;
  uint64_t overflow_sig_ = 0;  // the list (its draws' seqs) and scale whose lines didn't fit one atlas
  float overflow_sx_ = 0, overflow_sy_ = 0;
  std::vector<platform::hires::Rect> rects_, open_, next_;
  std::vector<platform::TLVertex> verts_;
  Stats stats_;
  // prepare()'s, for the draw() that follows it.
  const uint16_t* prepared_ = nullptr;
  int prep_w_ = 0, prep_h_ = 0, fw_ = 0, fh_ = 0;
  size_t prep_n_ = 0;
  std::chrono::steady_clock::time_point start_;
  std::vector<uint8_t> mask_;
  std::vector<double> times_;  // MW1_HIRES_DEBUG: draw()'s times, reported each 600 frames
};

}  // namespace sdlfe
