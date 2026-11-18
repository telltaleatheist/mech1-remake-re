#include "frontend/sdl/hires_draw.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "frontend/sdl/sdl_platform.h"
#include "platform/hires_2d.h"
#include "platform/hires_images.h"
#include "platform/render3d.h"

namespace sdlfe {

using platform::hires::Command;
using platform::hires::Kind;
using platform::hires::Rect;

namespace {

constexpr int kUnseenFrames = 120;  // a picture's texture kept this many frames undrawn (research/hires-2d.md B2b)

// GDI's 0x00bbggrr as the canvas's 16-bit pixel (DrawTextA's glyphs are drawn in it exactly).
uint16_t canvas_colour(uint32_t c, bool rgb565) {
  const uint32_t r = c & 0xff, g = c >> 8 & 0xff, b = c >> 16 & 0xff;
  return static_cast<uint16_t>(rgb565 ? (r >> 3) << 11 | (g >> 2) << 5 | b >> 3 : (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
}
// A diffuse (ARGB) for a premultiplied draw: the colour, tinted when debugging.
uint32_t diffuse(uint32_t r, uint32_t g, uint32_t b) {
  if (platform::hires_2d_debug()) r = r * 7 / 16, b = b * 7 / 16, g = 128 + g / 2;
  return 0xff000000u | r << 16 | g << 8 | b;
}

platform::DrawState state(int32_t texture) {
  platform::DrawState s;
  s.texture = texture, s.address_u = s.address_v = 3;
  s.depth_write = false, s.depth_always = true;
  s.blend = true, s.premultiplied = true, s.alpha_modulate = true;
  return s;
}

// A quad (canvas pixels, x0 < x1) as two triangles, its UVs from (u0, v0) to (u1, v1), into `out`.
void quad(std::vector<platform::TLVertex>* out, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
          float v1, uint32_t colour) {
  const platform::TLVertex a{x0, y0, 1, 1, colour, 0xff000000u, u0, v0}, b{x1, y0, 1, 1, colour, 0xff000000u, u1, v0},
      c{x1, y1, 1, 1, colour, 0xff000000u, u1, v1}, d{x0, y1, 1, 1, colour, 0xff000000u, u0, v1};
  out->insert(out->end(), {a, b, c, a, c, d});
}

// A quad clipped to `r`, its UVs clipped with it.
void clipped(std::vector<platform::TLVertex>* out, const Rect& r, float x0, float y0, float x1, float y1, float u0,
             float v0, float u1, float v1, uint32_t colour) {
  const float l = std::max(x0, static_cast<float>(r.l)), t = std::max(y0, static_cast<float>(r.t));
  const float rr = std::min(x1, static_cast<float>(r.r)), b = std::min(y1, static_cast<float>(r.b));
  if (l >= rr || t >= b || x1 <= x0 || y1 <= y0) return;
  const float du = (u1 - u0) / (x1 - x0), dv = (v1 - v0) / (y1 - y0);
  quad(out, l, t, rr, b, u0 + (l - x0) * du, v0 + (t - y0) * dv, u0 + (rr - x0) * du, v0 + (b - y0) * dv, colour);
}

// The pixels of rect `r` where base is 1..top (canvas width w), as rects: each row's runs, a run the same as the
// row above's grown down.
void shown(const std::vector<uint16_t>& base, int32_t w, const Rect& r, uint16_t top, std::vector<Rect>* out,
           std::vector<Rect>& open, std::vector<Rect>& next) {
  out->clear();
  open.clear();
  for (int32_t y = r.t; y <= r.b; ++y) {
    next.clear();
    if (y < r.b) {
      const uint16_t* row = base.data() + static_cast<size_t>(y) * w;
      for (int32_t x = r.l; x < r.r;) {
        if (!row[x] || row[x] > top) {
          ++x;
          continue;
        }
        int32_t e = x;
        while (e < r.r && row[e] && row[e] <= top) ++e;
        next.push_back(Rect{x, y, e, y + 1});
        x = e;
      }
    }
    for (Rect& o : open) {
      auto same = std::find_if(next.begin(), next.end(), [&](const Rect& n) { return n.l == o.l && n.r == o.r && n.t == y; });
      if (same != next.end()) {
        same->t = o.t;  // grown: carried on in next
      } else {
        out->push_back(o);
      }
    }
    open.swap(next);
  }
}

}  // namespace

HiresDraw::~HiresDraw() {
  if (!render_) return;
  for (auto& [serial, p] : pictures_) render_->release_texture(p.texture);
  if (atlas_tex_ >= 0) render_->release_texture(atlas_tex_);
  if (under_tex_ >= 0) render_->release_texture(under_tex_);
}

void HiresDraw::textures_for_scale(float sx, float sy, int height) {
  if (sx == sx_ && sy == sy_) return;
  sx_ = sx, sy_ = sy;
  text_.set_scale(sx, sy);
  for (auto& [serial, p] : pictures_) render_->release_texture(p.texture);
  pictures_.clear();
  const float art = sy * static_cast<float>(height) / 480.0f;
  if (art != art_scale_) {
    art_scale_ = art;
    if (platform::HiresImages* images = platform::hires_images()) images->set_frame_scale(art);
  }
}

int32_t HiresDraw::picture_texture(const Command& c, float* u_scale, float* v_scale) {
  platform::HiresImages* images = platform::hires_images();
  const platform::HiresImage* im = images ? images->find(c.name.c_str()) : nullptr;
  if (!im || !im->rgba || im->w <= 0 || im->h <= 0 || im->canvas_w <= 0 || im->canvas_h <= 0) return -1;
  *u_scale = 1.0f / static_cast<float>(im->canvas_w), *v_scale = 1.0f / static_cast<float>(im->canvas_h);
  Picture& p = pictures_[im->serial];
  p.unseen = 0;
  if (p.texture >= 0) return p.texture;
  rgba_.resize(static_cast<size_t>(im->w) * im->h * 4);
  for (size_t i = 0; i < rgba_.size(); i += 4) {  // straight alpha premultiplied (research/hires-2d.md B2b)
    const uint32_t a = im->rgba[i + 3];
    for (int k = 0; k < 3; ++k) rgba_[i + k] = static_cast<uint8_t>((im->rgba[i + k] * a + 127) / 255);
    rgba_[i + 3] = static_cast<uint8_t>(a);
  }
  p.texture = render_->create_texture(im->w, im->h, rgba_.data(), 4);
  const int32_t t = p.texture;
  if (t < 0) pictures_.erase(im->serial);  // p is gone with it
  return t;
}

bool HiresDraw::upload_atlas() {
  int32_t w = 0, h = 0;
  const uint8_t* a = text_.atlas(&w, &h);
  Rect dirty;
  const bool changed = text_.take_dirty(&dirty);
  if (!a || w <= 0 || h <= 0) return false;
  if (!changed && atlas_tex_ >= 0 && w == atlas_w_ && h == atlas_h_) return true;
  // The whole atlas each time it changed (new glyphs: rarely, once a screen's text is made). Render3D updates only
  // whole textures; a part's upload (take_dirty's rect) wants an update_texture_rect there, not yet worth it.
  rgba_.resize(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0, n = static_cast<size_t>(w) * h; i < n; ++i)  // white, premultiplied by its coverage
    rgba_[4 * i] = rgba_[4 * i + 1] = rgba_[4 * i + 2] = rgba_[4 * i + 3] = a[i];
  if (atlas_tex_ < 0 || w != atlas_w_ || h != atlas_h_) {
    if (atlas_tex_ >= 0) render_->release_texture(atlas_tex_);
    atlas_tex_ = render_->create_screen_texture(w, h, rgba_.data());
    atlas_w_ = w, atlas_h_ = h;
  } else {
    render_->update_texture(atlas_tex_, w, h, rgba_.data());
  }
  return atlas_tex_ >= 0;
}

void HiresDraw::draw(const uint16_t* pixels, int width, int height, int pitch, bool rgb565, bool keyed, uint16_t key) {
  const auto start = std::chrono::steady_clock::now();
  stats_ = Stats{};
  const std::vector<Command>* list = lists_ ? lists_->of(pixels) : nullptr;
  int fw = 0, fh = 0;
  if (!render_ || !list || list->empty() || width <= 0 || height <= 0 || !render_->frame_size(&fw, &fh)) return;
  const int32_t pitch_px = pitch / 2;
  textures_for_scale(static_cast<float>(fw) / width, static_cast<float>(fh) / height, height);
  const std::vector<Command>& l = *list;
  const size_t n = std::min<size_t>(l.size(), 0xfffe);
  stats_.draws = static_cast<int>(n);
  lists_->layers(pixels, pixels, width, height, pitch_px, &base_);

  // Each draw's means: a picture's texture, a line's glyphs (all the lines laid out against one atlas).
  std::vector<int32_t>& tex = tex_;
  std::vector<float>& us = us_;
  std::vector<float>& vs = vs_;
  std::vector<uint8_t>& ok = ok_;
  tex.assign(n, -1), us.assign(n, 0), vs.assign(n, 0), ok.assign(n, 0);
  if (glyphs_.size() < n) glyphs_.resize(n);  // the inner vectors keep their room
  auto& glyphs = glyphs_;
  for (size_t i = 0; i < n; ++i)
    if (l[i].kind == Kind::kPicture) ok[i] = (tex[i] = picture_texture(l[i], &us[i], &vs[i])) >= 0;
  // The lines against one atlas. A line that can't be made (even alone) fails by itself, and is left out when the
  // frame's lines are laid out again after an atlas clear. When they still don't fit together, the text is the
  // canvas's, and stays so (no layout, no atlas churn) until the list or the scale changes.
  uint64_t sig = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) sig = (sig ^ l[i].seq) * 1099511628211ull;
  sig ^= static_cast<uint64_t>(n) << 48;
  const bool overflowed = sig == overflow_sig_ && sx_ == overflow_sx_ && sy_ == overflow_sy_;
  bad_.assign(n, 0);
  for (int pass = 0; pass < 2 && !overflowed; ++pass) {
    const uint64_t serial = text_.atlas_serial();
    for (size_t i = 0; i < n; ++i)
      if (l[i].kind == Kind::kText && !bad_[i]) {
        glyphs[i].clear();
        ok[i] = text_.layout(l[i].font, l[i].text, static_cast<float>(l[i].x), static_cast<float>(l[i].y), sx_, sy_,
                             &glyphs[i]);
        if (!ok[i]) bad_[i] = 1;
      }
    if (text_.atlas_serial() == serial) break;
    if (pass == 1) {  // they don't fit one atlas together: the canvas's, remembered
      for (size_t i = 0; i < n; ++i)
        if (l[i].kind == Kind::kText) ok[i] = 0;
      overflow_sig_ = sig, overflow_sx_ = sx_, overflow_sy_ = sy_;
      ++stats_.overflowed;
    }
  }
  const bool atlas = upload_atlas();
  for (size_t i = 0; i < n; ++i)
    if (l[i].kind == Kind::kText && !atlas) ok[i] = false;

  // The fail-safe: a draw not drawn here takes itself and every draw over it out where it shows; the canvas shows.
  for (size_t i = 0; i < n; ++i) {
    if (ok[i]) continue;
    ++stats_.failed;
    const Rect r = platform::hires::intersect(l[i].rect, Rect{0, 0, width, height});
    const uint16_t id = static_cast<uint16_t>(i + 1);
    for (int32_t y = r.t; y < r.b; ++y)
      for (int32_t x = r.l; x < r.r; ++x) {
        uint16_t& b = base_[static_cast<size_t>(y) * width + x];
        if (b && b <= id) b = 0;
      }
  }

  // Under each chain its base's "before": the canvas's own copy of the draws covered.
  under_rgba_.assign(static_cast<size_t>(width) * height * 4, 0);
  auto* under = reinterpret_cast<uint32_t*>(under_rgba_.data());
  for (int32_t y = 0; y < height; ++y)
    for (int32_t x = 0; x < width; ++x) {
      const uint16_t b = base_[static_cast<size_t>(y) * width + x];
      if (!b) continue;
      const Command& c = l[b - 1];
      const int32_t cw = c.rect.r - c.rect.l;
      const size_t row = static_cast<size_t>(y - c.rect.t) * cw;
      const size_t k = row + (x - c.rect.l);
      uint16_t v = c.before[k];
      if (c.kind == Kind::kText && c.after[k] != c.before[k]) {
        // The text's blit changed it: its background (an OPAQUE line) stays, its glyph is the background beside it,
        // else what was under it.
        const uint16_t glyph = canvas_colour(c.colour, rgb565);
        if (c.after[k] != glyph) {
          v = c.after[k];
        } else {
          for (int32_t d = 1; d < cw; ++d) {
            const int32_t xs[2] = {x - c.rect.l - d, x - c.rect.l + d};
            bool found = false;
            for (int32_t xx : xs) {
              if (xx < 0 || xx >= cw) continue;
              const size_t kk = row + xx;
              if (c.after[kk] == glyph) continue;
              if (c.after[kk] != c.before[kk]) v = c.after[kk];
              found = true;
              break;
            }
            if (found) break;
          }
        }
      }
      if (keyed && v == key) continue;  // over the 3D: clear
      under[static_cast<size_t>(y) * width + x] = SdlScreen2D::rgba(v, rgb565);
      ++stats_.under;
    }
  if (under_tex_ < 0 || under_w_ != width || under_h_ != height) {
    if (under_tex_ >= 0) render_->release_texture(under_tex_);
    under_tex_ = render_->create_screen_texture(width, height, under_rgba_.data());
    under_w_ = width, under_h_ = height;
    under_shown_ = under_rgba_;
  } else if (under_rgba_ != under_shown_) {  // uploaded only when it changed (a still screen: never again)
    render_->update_texture(under_tex_, width, height, under_rgba_.data());
    under_shown_ = under_rgba_;
  }

  render_->begin_scene();
  {
    platform::DrawState s;  // as the canvas's layer
    s.texture = under_tex_, s.address_u = s.address_v = 3;
    s.depth_write = false, s.depth_always = true, s.blend = true, s.sharp = true;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    verts_.clear();
    quad(&verts_, 0, 0, w, h, 0, 0, 1, 1, 0xffffffffu);
    if (stats_.under) render_->draw(platform::Primitive::kTriangles, verts_.data(), static_cast<int>(verts_.size()), s);
  }
  std::vector<Rect>& rects = rects_;
  std::vector<platform::TLVertex>& v = verts_;
  for (size_t i = 0; i < n; ++i) {
    if (!ok[i]) continue;
    const Command& c = l[i];
    shown(base_, width, platform::hires::intersect(c.rect, Rect{0, 0, width, height}), static_cast<uint16_t>(i + 1), &rects, open_,
          next_);
    if (rects.empty()) continue;
    v.clear();
    if (c.kind == Kind::kPicture) {
      const uint32_t white = diffuse(255, 255, 255);
      for (const Rect& r : rects)
        quad(&v, static_cast<float>(r.l), static_cast<float>(r.t), static_cast<float>(r.r), static_cast<float>(r.b),
             (r.l - c.x) * us[i], (r.t - c.y) * vs[i], (r.r - c.x) * us[i], (r.b - c.y) * vs[i], white);
      render_->draw(platform::Primitive::kTriangles, v.data(), static_cast<int>(v.size()), state(tex[i]));
      ++stats_.pictures;
    } else {
      if (c.box) {  // OPAQUE: its cells in their colour first, over whatever is under it
        const uint32_t bk = diffuse(c.box_colour & 0xff, c.box_colour >> 8 & 0xff, c.box_colour >> 16 & 0xff);
        for (const Rect& r : rects)
          quad(&v, static_cast<float>(r.l), static_cast<float>(r.t), static_cast<float>(r.r), static_cast<float>(r.b), 0,
               0, 0, 0, bk);
        platform::DrawState solid = state(-1);
        render_->draw(platform::Primitive::kTriangles, v.data(), static_cast<int>(v.size()), solid);
        v.clear();
      }
      const uint32_t colour = diffuse(c.colour & 0xff, c.colour >> 8 & 0xff, c.colour >> 16 & 0xff);
      const float aw = static_cast<float>(atlas_w_), ah = static_cast<float>(atlas_h_);
      for (const platform::hires::GlyphQuad& g : glyphs[i])
        for (const Rect& r : rects)
          clipped(&v, r, g.x / sx_, g.y / sy_, (g.x + g.w) / sx_, (g.y + g.h) / sy_, g.ax / aw, g.ay / ah,
                  (g.ax + g.aw) / aw, (g.ay + g.ah) / ah, colour);
      if (!v.empty()) render_->draw(platform::Primitive::kTriangles, v.data(), static_cast<int>(v.size()), state(atlas_tex_));
      ++stats_.lines;
    }
  }
  render_->end_scene();
  stats_.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  if (platform::hires_2d_debug()) {
    times_.push_back(stats_.ms);
    if (times_.size() == 600) {
      std::sort(times_.begin(), times_.end());
      std::printf("mw3: hires: draw over 600 frames at %dx%d: median %.3f ms, worst %.3f ms (%d draws, %d pictures, %d "
                  "lines, %d left to the canvas)\n",
                  fw, fh, times_[300], times_.back(), stats_.draws, stats_.pictures, stats_.lines, stats_.failed);
      times_.clear();
    }
  }
}

void HiresDraw::end_frame() {
  if (platform::HiresImages* images = platform::hires_images()) images->end_frame();
  for (auto it = pictures_.begin(); it != pictures_.end();) {
    if (++it->second.unseen > kUnseenFrames) {
      render_->release_texture(it->second.texture);
      it = pictures_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace sdlfe
