// Ours (MW1's FIXES.md item 12, mw1/research/hires-2d.md B1 piece 4): sdlfe::HiresDraw, the screen's recorded draws
// drawn again at the frame's resolution over the canvas's layer, on a recording renderer: where each draw lands,
// what goes under it, and the fail-safe (a draw that can't be drawn here leaves the canvas showing, never a hole).
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "frontend/sdl/hires_draw.h"
#include "frontend/sdl/sdl_platform.h"
#include "platform/hires_images.h"
#include "platform/hires_lists.h"
#include "platform/render3d.h"

namespace {
namespace fs = std::filesystem;
using platform::hires::Command;
using platform::hires::FontKey;
using platform::hires::Lists;
using platform::hires::Rect;

struct Drawn {
  int32_t texture;
  platform::DrawState state;
  std::vector<platform::TLVertex> v;
};

class Recorder : public platform::Render3D {
 public:
  int fw = 256, fh = 128;
  std::map<int32_t, std::vector<uint8_t>> textures;
  std::map<int32_t, std::pair<int, int>> sizes;
  std::vector<Drawn> draws;
  int32_t next = 0;
  bool frame_size(int* w, int* h) const override {
    *w = fw, *h = fh;
    return true;
  }
  int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t) override {
    textures[next].assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    sizes[next] = {width, height};
    return next++;
  }
  void release_texture(int32_t h) override { textures.erase(h), sizes.erase(h); }
  void update_texture(int32_t h, int width, int height, const uint8_t* rgba) override {
    textures[h].assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
  }
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive, const platform::TLVertex* v, int n, const platform::DrawState& s) override {
    draws.push_back({s.texture, s, std::vector<platform::TLVertex>(v, v + n)});
  }
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}
};

class Images : public platform::HiresImages {
 public:
  std::map<std::string, platform::HiresImage> by_name;
  std::vector<uint8_t> px;
  float scale = 0;
  const platform::HiresImage* find(const char* name) override {
    auto it = by_name.find(name);
    return it == by_name.end() ? nullptr : &it->second;
  }
  void set_frame_scale(float s) override { scale = s; }
};

struct Canvas {
  int32_t w, h;
  std::vector<uint16_t> px;
  Canvas(int32_t w_, int32_t h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_, 0) {}
  void fill(Rect r, uint16_t v) {
    for (int32_t y = r.t; y < r.b; ++y)
      for (int32_t x = r.l; x < r.r; ++x) px[static_cast<size_t>(y) * w + x] = v;
  }
};
int text_owner, pic_owner;

// As the host's blit: listed, the canvas kept before, the pixels drawn, kept after.
template <typename Paint>
void blit(Lists& l, Canvas& c, const void* src, int32_t sw, int32_t sh, int32_t x, int32_t y, bool opaque,
          const std::string& name, Paint paint) {
  const size_t added = l.blit(src, sw, sh, nullptr, c.px.data(), c.w, c.h, x, y, opaque, name);
  l.snapshot(c.px.data(), added, c.px.data(), c.w, false);
  paint();
  l.snapshot(c.px.data(), added, c.px.data(), c.w, true);
}

// A picture "pic" for a 64x32 canvas image at 4x (a 256x128 source), opaque grey.
void add_pic(Images& im) {
  im.px.assign(256u * 128u * 4u, 0x80);
  for (size_t i = 3; i < im.px.size(); i += 4) im.px[i] = 0xff;
  platform::HiresImage p;
  p.w = 256, p.h = 128, p.rgba = im.px.data(), p.serial = 42, p.canvas_w = 64, p.canvas_h = 32;
  im.by_name["pic"] = p;
}

const Drawn* premultiplied(const Recorder& r, size_t k) {
  size_t seen = 0;
  for (const Drawn& d : r.draws)
    if (d.state.premultiplied && seen++ == k) return &d;
  return nullptr;
}
// The bounding box of a draw's vertices (canvas pixels).
Rect box(const Drawn& d) {
  float l = 1e9f, t = 1e9f, r = -1e9f, b = -1e9f;
  for (const auto& v : d.v) l = std::min(l, v.sx), t = std::min(t, v.sy), r = std::max(r, v.sx), b = std::max(b, v.sy);
  return Rect{static_cast<int32_t>(l), static_cast<int32_t>(t), static_cast<int32_t>(r + 0.999f), static_cast<int32_t>(b + 0.999f)};
}

}  // namespace

TEST(hires_draw_a_picture_from_its_source) {
  Recorder r;
  Images im;
  add_pic(im);
  platform::set_hires_images(&im);
  Lists l;
  Canvas c(64, 32);
  blit(l, c, &pic_owner, 64, 32, 0, 0, true, "pic", [&] { c.fill({0, 0, 64, 32}, 0x1234); });
  sdlfe::HiresDraw d(&r, &l);
  d.draw(c.px.data(), c.w, c.h, c.w * 2, true, false, 0);
  platform::set_hires_images(nullptr);
  EXPECT(d.stats().draws == 1 && d.stats().pictures == 1 && d.stats().failed == 0);
  EXPECT(im.scale == 4.0f * 32.0f / 480.0f);  // the art's scale: the frame's height over 480
  const Drawn* p = premultiplied(r, 0);
  EXPECT(p && r.sizes[p->texture] == std::make_pair(256, 128));
  if (p) {
    const Rect b = box(*p);
    EXPECT(b == (Rect{0, 0, 64, 32}));
    float u1 = 0, v1 = 0;
    for (const auto& v : p->v) u1 = std::max(u1, v.tu), v1 = std::max(v1, v.tv);
    EXPECT(u1 == 1.0f && v1 == 1.0f);
    EXPECT(p->state.blend && p->state.alpha_modulate && !p->state.sharp && !p->state.depth_write);
  }
}

TEST(hires_draw_no_source_leaves_the_canvas) {
  // The fail-safe: no picture behind the name. Nothing drawn here, nothing put under: the canvas's layer shows.
  Recorder r;
  Images im;  // empty
  platform::set_hires_images(&im);
  Lists l;
  Canvas c(64, 32);
  blit(l, c, &pic_owner, 64, 32, 0, 0, true, "pic", [&] { c.fill({0, 0, 64, 32}, 0x1234); });
  sdlfe::HiresDraw d(&r, &l);
  d.draw(c.px.data(), c.w, c.h, c.w * 2, true, false, 0);
  platform::set_hires_images(nullptr);
  EXPECT(d.stats().failed == 1 && d.stats().pictures == 0 && d.stats().under == 0);
  EXPECT(r.draws.empty());
}

TEST(hires_draw_text_over_a_picture_and_an_unrecorded_write) {
  const std::string font = (fs::path(__FILE__).parent_path().parent_path() / "mw1" / "data" / "fonts" /
                            "ShareTechMono-Regular.ttf").string();
  Recorder r;
  Images im;
  add_pic(im);
  platform::set_hires_images(&im);
  Lists l;
  Canvas c(64, 32);
  blit(l, c, &pic_owner, 64, 32, 0, 0, true, "pic", [&] { c.fill({0, 0, 64, 32}, 0x1234); });
  // A line of text on its own image (40x12), its glyph pixels in its colour, blitted keyed at (8, 10).
  l.clear(&text_owner);
  Command t;
  t.rect = Rect{0, 0, 40, 12};
  t.x = 0, t.y = 0;
  t.text = "HELLO";
  t.font = FontKey{font, 12, 400, false, 0, 0};
  t.colour = 0x0000ff00;  // green
  l.text(&text_owner, t);
  const uint16_t green565 = 0x07e0;
  blit(l, c, &text_owner, 40, 12, 8, 10, false, "", [&] {
    c.fill({10, 12, 12, 20}, green565);  // a glyph's stem
  });
  // Something unrecorded over the text's right part, after it (a line the screen draws): there the canvas shows.
  c.fill({30, 8, 64, 24}, 0x0f0f);
  sdlfe::HiresDraw d(&r, &l);
  d.draw(c.px.data(), c.w, c.h, c.w * 2, true, false, 0);
  platform::set_hires_images(nullptr);
  EXPECT(d.stats().draws == 2 && d.stats().pictures == 1 && d.stats().lines == 1 && d.stats().failed == 0);
  const Drawn* pic = premultiplied(r, 0);
  const Drawn* text = premultiplied(r, 1);
  EXPECT(pic && text);
  if (!pic || !text) return;
  // Neither reaches into the unrecorded write.
  for (const Drawn* dd : {pic, text})
    for (size_t i = 0; i + 2 < dd->v.size(); i += 3) {
      float l0 = 1e9f, t0 = 1e9f, r0 = -1e9f, b0 = -1e9f;
      for (size_t k = i; k < i + 3; ++k)
        l0 = std::min(l0, dd->v[k].sx), t0 = std::min(t0, dd->v[k].sy), r0 = std::max(r0, dd->v[k].sx),
        b0 = std::max(b0, dd->v[k].sy);
      const bool inside = l0 < 64 && r0 > 30 && t0 < 24 && b0 > 8;
      EXPECT(!inside);
    }
  // The text in its colour, inside its box (8, 10)-(48, 22).
  EXPECT(!text->v.empty() && (text->v[0].diffuse & 0x00ffffffu) == 0x0000ff00u);
  const Rect tb = box(*text);
  EXPECT(tb.l >= 8 && tb.t >= 10 && tb.r <= 48 && tb.b <= 22);
  // Under it all, the picture's chain: its "before" (the canvas's black before the picture), never the canvas's
  // own blocky glyph.
  const Drawn* under = nullptr;
  for (const Drawn& dd : r.draws)
    if (dd.state.sharp) under = &dd;
  EXPECT(under != nullptr);
  if (under) {
    const auto& u = r.textures[under->texture];
    const size_t at = (static_cast<size_t>(15) * 64 + 11) * 4;  // on the glyph's stem
    uint32_t px;
    std::memcpy(&px, &u[at], 4);
    EXPECT(px == sdlfe::SdlScreen2D::rgba(0, true));
    const size_t off = (static_cast<size_t>(15) * 64 + 40) * 4;  // in the unrecorded write: clear
    std::memcpy(&px, &u[off], 4);
    EXPECT(px == 0);
  }
}

TEST(hires_draw_text_on_canvas_chrome_its_before_under) {
  // A text over something unrecorded (chrome the canvas drew itself): its chain starts at the text, whose "before"
  // (the chrome) goes under it, with an OPAQUE line's background kept.
  const std::string font = (fs::path(__FILE__).parent_path().parent_path() / "mw1" / "data" / "fonts" /
                            "ShareTechMono-Regular.ttf").string();
  Recorder r;
  Lists l;
  Canvas c(64, 32);
  c.fill({0, 0, 64, 32}, 0x2104);  // the chrome
  l.clear(&text_owner);
  Command t;
  t.rect = Rect{0, 0, 40, 12};
  t.text = "OK";
  t.font = FontKey{font, 12, 400, false, 0, 0};
  t.colour = 0x00ffffff;
  l.text(&text_owner, t);
  blit(l, c, &text_owner, 40, 12, 0, 0, true, "", [&] {
    c.fill({0, 0, 40, 12}, 0x0010);    // the line's background (OPAQUE)
    c.fill({4, 2, 6, 10}, 0xffff);     // a glyph's stem
  });
  sdlfe::HiresDraw d(&r, &l);
  d.draw(c.px.data(), c.w, c.h, c.w * 2, true, false, 0);
  EXPECT(d.stats().lines == 1 && d.stats().failed == 0 && d.stats().under == 40 * 12);
  const Drawn* under = nullptr;
  for (const Drawn& dd : r.draws)
    if (dd.state.sharp) under = &dd;
  EXPECT(under != nullptr);
  if (under) {
    const auto& u = r.textures[under->texture];
    uint32_t stem, bg, out;
    std::memcpy(&stem, &u[(static_cast<size_t>(5) * 64 + 5) * 4], 4);
    std::memcpy(&bg, &u[(static_cast<size_t>(5) * 64 + 20) * 4], 4);
    std::memcpy(&out, &u[(static_cast<size_t>(20) * 64 + 50) * 4], 4);
    EXPECT(bg == sdlfe::SdlScreen2D::rgba(0x0010, true));
    EXPECT(stem == bg);  // the glyph's cell: the background beside it, not the blocky glyph
    EXPECT(out == 0);    // no draw there: the canvas's own layer shows
  }
}
