// Ours (MW1's FIXES.md item 12, mw1/research/hires-2d.md B1): the recorded draws kept with the pixels
// (platform/hires_lists.h): carried by blits, copied by grabs, dropped under an opaque cover, bounded.
#include <algorithm>
#include <cstdio>

#include "check.h"
#include "platform/hires_lists.h"

namespace {
using platform::hires::Command;
using platform::hires::Kind;
using platform::hires::Lists;
using platform::hires::Rect;

Command line(Rect r, const char* text) {
  Command c;
  c.rect = r;
  c.x = r.l, c.y = r.t;
  c.text = text;
  return c;
}
int screen_px, text_px, bg_px, grab_px, other_px;  // owners: only their addresses matter
}  // namespace

TEST(hires_text_carried_by_a_blit_offset_and_clipped) {
  Lists l;
  l.text(&text_px, line({0, 0, 40, 10}, "PAYMENT"));
  // The text image's right half (20..40) drawn at (100, 50).
  const Rect part{20, 0, 40, 10};
  l.blit(&text_px, 40, 10, &part, &screen_px, 853, 480, 100, 50, false, "");
  const auto* s = l.of(&screen_px);
  EXPECT(s && s->size() == 1);
  if (!s || s->empty()) return;
  EXPECT((*s)[0].rect == (Rect{100, 50, 120, 60}));  // the part that shows
  EXPECT((*s)[0].x == 80 && (*s)[0].y == 50);         // the pen where the whole line would start
  EXPECT((*s)[0].text == "PAYMENT" && (*s)[0].kind == Kind::kText);
  // Past the screen's edge: clipped there.
  l.blit(&text_px, 40, 10, nullptr, &screen_px, 853, 480, 830, 475, false, "");
  EXPECT(l.size(&screen_px) == 2 && (*l.of(&screen_px))[1].rect == (Rect{830, 475, 853, 480}));
}

TEST(hires_named_image_is_a_picture_command) {
  Lists l;
  const Rect part{10, 20, 110, 70};
  l.blit(&bg_px, 854, 480, &part, &screen_px, 853, 480, 5, 6, true, "mw1_hub_ice");
  const auto* s = l.of(&screen_px);
  EXPECT(s && s->size() == 1);
  if (!s || s->empty()) return;
  EXPECT((*s)[0].kind == Kind::kPicture && (*s)[0].name == "mw1_hub_ice" && !(*s)[0].alpha);
  EXPECT((*s)[0].rect == (Rect{5, 6, 105, 56}) && (*s)[0].x == -5 && (*s)[0].y == -14);
  // A blending one is marked so (rule 7: drawn over what's recorded under it).
  l.blit(&other_px, 100, 100, nullptr, &screen_px, 853, 480, 0, 0, false, "mw1_dim");
  EXPECT((*l.of(&screen_px))[1].alpha);
}

TEST(hires_opaque_cover_drops_what_it_hides_whole) {
  Lists l;
  l.text(&screen_px, line({10, 10, 50, 20}, "A"));    // under the cover whole
  l.text(&screen_px, line({90, 10, 130, 20}, "B"));   // half under it: kept (the mask hides the half)
  l.text(&screen_px, line({300, 10, 340, 20}, "C"));  // clear of it
  l.blit(&other_px, 100, 100, nullptr, &screen_px, 853, 480, 0, 0, true, "");
  const auto* s = l.of(&screen_px);
  EXPECT(s && s->size() == 2);
  if (s && s->size() == 2) EXPECT((*s)[0].text == "B" && (*s)[1].text == "C");
  // A keyed or alpha blit hides nothing for good.
  l.blit(&other_px, 853, 480, nullptr, &screen_px, 853, 480, 0, 0, false, "");
  EXPECT(l.size(&screen_px) == 2);
  // An opaque one over the whole screen: a fresh start.
  l.blit(&other_px, 853, 480, nullptr, &screen_px, 853, 480, 0, 0, true, "");
  EXPECT(l.size(&screen_px) == 0);
}

TEST(hires_grab_copies_what_shows_in_its_part) {
  Lists l;
  l.text(&screen_px, line({100, 100, 200, 110}, "IN"));
  l.text(&screen_px, line({500, 100, 600, 110}, "OUT"));
  l.text(&screen_px, line({190, 100, 260, 110}, "EDGE"));
  l.grab(&screen_px, Rect{50, 50, 250, 150}, &grab_px);
  const auto* g = l.of(&grab_px);
  EXPECT(g && g->size() == 2);
  if (!g || g->size() != 2) return;
  EXPECT((*g)[0].text == "IN" && (*g)[0].rect == (Rect{50, 50, 150, 60}) && (*g)[0].x == 50);
  EXPECT((*g)[1].text == "EDGE" && (*g)[1].rect == (Rect{140, 50, 200, 60}));
  // Drawn back (a cursor's restore, a backdrop): where they were.
  l.clear(&screen_px);
  l.blit(&grab_px, 200, 100, nullptr, &screen_px, 853, 480, 50, 50, true, "");
  const auto* s = l.of(&screen_px);
  EXPECT(s && s->size() == 2 && (*s)[0].rect == (Rect{100, 100, 200, 110}) && (*s)[0].x == 100);
  // A slice of it (a control's erase background).
  const Rect slice{60, 55, 120, 58};
  l.blit(&grab_px, 200, 100, &slice, &screen_px, 853, 480, 110, 105, true, "");
  EXPECT(l.size(&screen_px) == 3 && (*l.of(&screen_px))[2].rect == (Rect{110, 105, 170, 108}));
}

TEST(hires_list_past_its_cap_falls_back_until_drawn_afresh) {
  Lists l(8);
  for (int i = 0; i < 8; ++i) l.text(&screen_px, line({0, i * 10, 40, i * 10 + 10}, "x"));
  EXPECT(l.size(&screen_px) == 8 && !l.fallen(&screen_px));
  l.text(&screen_px, line({0, 90, 40, 100}, "one too many"));
  EXPECT(l.fallen(&screen_px) && l.size(&screen_px) == 0);  // the canvas alone: blocky, never missing
  l.text(&screen_px, line({0, 0, 40, 10}, "ignored"));
  EXPECT(l.size(&screen_px) == 0);
  // A grab of a fallen screen is fallen too.
  l.grab(&screen_px, Rect{0, 0, 100, 100}, &grab_px);
  EXPECT(l.fallen(&grab_px));
  // Drawn over whole: recorded again.
  l.blit(&other_px, 853, 480, nullptr, &screen_px, 853, 480, 0, 0, true, "");
  l.text(&screen_px, line({0, 0, 40, 10}, "again"));
  EXPECT(!l.fallen(&screen_px) && l.size(&screen_px) == 1);
}

TEST(hires_static_bg_redrawn_1000_frames_stays_flat) {
  // A dialog's way (twod_dialog.cpp): its static parts baked into static_bg by a grab, then each frame static_bg
  // drawn back over the whole screen, its controls' erase slices and texts over it.
  Lists l;
  l.blit(&bg_px, 853, 480, nullptr, &screen_px, 853, 480, 0, 0, true, "mw1_hub_ice");
  for (int i = 0; i < 6; ++i) {
    l.clear(&text_px);  // the host clears a text image as its DC opens (image_dc), before drawing it again
    l.text(&text_px, line({0, 0, 80, 12}, "LABEL"));
    l.blit(&text_px, 80, 12, nullptr, &screen_px, 853, 480, 40, 40 + 20 * i, false, "");
  }
  int static_bg = 0;
  l.grab(&screen_px, Rect{0, 0, 853, 480}, &static_bg);
  const size_t baked = l.size(&static_bg);
  EXPECT(baked == 7);
  size_t first = 0, most = 0;
  for (int frame = 0; frame < 1000; ++frame) {
    l.blit(&static_bg, 853, 480, nullptr, &screen_px, 853, 480, 0, 0, true, "");
    for (int i = 0; i < 3; ++i) {  // three live texts, each erased by its slice and drawn again
      const Rect slice{300, 40 + 20 * i, 400, 52 + 20 * i};
      l.blit(&static_bg, 853, 480, &slice, &screen_px, 853, 480, 300, 40 + 20 * i, true, "");
      l.clear(&text_px);
      l.text(&text_px, line({0, 0, 100, 12}, "LIVE"));
      l.blit(&text_px, 100, 12, nullptr, &screen_px, 853, 480, 300, 40 + 20 * i, false, "");
    }
    const size_t n = l.size(&screen_px);
    if (frame == 0) first = n;
    most = std::max(most, n);
  }
  std::printf("  the screen's list: %zu after the first frame, at most %zu over 1000, %zu owners\n", first, most, l.owners());
  // static_bg's 7, then per live text its slice (carrying the backdrop's picture, cut to the slice: drawn again
  // there, in order) and the text: 7 + 3 + 3, the same every frame.
  EXPECT(first == baked + 3 + 3 && most == first && !l.fallen(&screen_px));
  EXPECT(l.owners() <= 4);
}

namespace {
// A small canvas for the layers' map: w x h pixels, pitch w.
struct Canvas {
  int32_t w, h;
  std::vector<uint16_t> px;
  Canvas(int32_t w_, int32_t h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_, 0) {}
  void fill(Rect r, uint16_t v) {
    for (int32_t y = r.t; y < r.b; ++y)
      for (int32_t x = r.l; x < r.r; ++x) px[static_cast<size_t>(y) * w + x] = v;
  }
  uint16_t at(const std::vector<uint16_t>& m, int32_t x, int32_t y) const { return m[static_cast<size_t>(y) * w + x]; }
};
// As the host's blit: the draws listed, the canvas kept before, the pixels drawn (`paint`), the canvas kept after.
template <typename Paint>
void draw(Lists& l, Canvas& c, const void* src, int32_t sw, int32_t sh, int32_t x, int32_t y, bool opaque,
          const std::string& name, Paint paint) {
  const size_t added = l.blit(src, sw, sh, nullptr, &screen_px, c.w, c.h, x, y, opaque, name);
  l.snapshot(&screen_px, added, c.px.data(), c.w, false);
  paint();
  l.snapshot(&screen_px, added, c.px.data(), c.w, true);
}
}  // namespace

TEST(hires_layers_a_text_over_a_picture_both_show) {
  Lists l;
  Canvas c(64, 32);
  draw(l, c, &bg_px, 64, 32, 0, 0, true, "mw1_hub_ice", [&] { c.fill({0, 0, 64, 32}, 0x1111); });
  l.clear(&text_px);
  l.text(&text_px, line({0, 0, 20, 8}, "TEXT"));
  // A keyed text: only its glyph pixels written (two of them here), the rest of its box the picture's still.
  draw(l, c, &text_px, 20, 8, 10, 10, false, "", [&] { c.fill({12, 12, 14, 14}, 0x2222); });
  std::vector<uint16_t> b;
  l.layers(&screen_px, c.px.data(), c.w, c.h, c.w, &b);
  EXPECT(c.at(b, 0, 0) == 1 && c.at(b, 63, 31) == 1);  // the picture alone
  EXPECT(c.at(b, 12, 12) == 1 && c.at(b, 20, 15) == 1);  // the picture, then the text over it: no canvas between
  // Something unrecorded written later (a line, a ripple, the engine's clear): there the canvas shows.
  c.fill({12, 12, 13, 13}, 0x3333);
  c.fill({40, 5, 42, 7}, 0x4444);
  l.layers(&screen_px, c.px.data(), c.w, c.h, c.w, &b);
  EXPECT(c.at(b, 12, 12) == 0 && c.at(b, 40, 5) == 0 && c.at(b, 41, 6) == 0);
  EXPECT(c.at(b, 13, 13) == 1 && c.at(b, 45, 5) == 1);  // the rest as it was
}

TEST(hires_layers_an_unrecorded_write_under_a_text) {
  // The picture, then something unrecorded (a panel's line), then a text over both: under the text the line
  // must show, so the chain starts at the text there (its "before", the canvas's line, drawn under it).
  Lists l;
  Canvas c(40, 20);
  draw(l, c, &bg_px, 40, 20, 0, 0, true, "pic", [&] { c.fill({0, 0, 40, 20}, 0x1111); });
  c.fill({0, 5, 40, 6}, 0x5555);  // the line
  l.clear(&text_px);
  l.text(&text_px, line({0, 0, 20, 10}, "OVER"));
  draw(l, c, &text_px, 20, 10, 0, 0, false, "", [&] { c.fill({2, 2, 4, 4}, 0x2222); });
  std::vector<uint16_t> b;
  l.layers(&screen_px, c.px.data(), c.w, c.h, c.w, &b);
  EXPECT(c.at(b, 10, 5) == 2);  // on the line, in the text's box: the text's chain, the picture not under it
  EXPECT(c.at(b, 30, 5) == 0);  // on the line, out of the text's box: the canvas
  EXPECT(c.at(b, 10, 8) == 1);  // off the line, in the text's box: the picture, then the text
  EXPECT(c.at(b, 2, 2) == 1);   // a glyph pixel over the picture
}

TEST(hires_layers_a_later_draw_over_an_earlier_one) {
  Lists l;
  Canvas c(40, 20);
  l.clear(&text_px);
  l.text(&text_px, line({0, 0, 20, 10}, "FIRST"));
  draw(l, c, &text_px, 20, 10, 0, 0, false, "", [&] { c.fill({0, 0, 20, 10}, 0x1010); });
  l.clear(&other_px);
  l.text(&other_px, line({0, 0, 20, 10}, "SECOND"));
  draw(l, c, &other_px, 20, 10, 10, 0, false, "", [&] { c.fill({10, 0, 30, 10}, 0x2020); });
  std::vector<uint16_t> b;
  l.layers(&screen_px, c.px.data(), c.w, c.h, c.w, &b);
  EXPECT(c.at(b, 5, 5) == 1);   // the first alone
  EXPECT(c.at(b, 15, 5) == 1);  // both, the first under
  EXPECT(c.at(b, 25, 5) == 2);  // the second alone
}

TEST(hires_layers_no_snapshot_shows_nowhere) {
  // The fail-safe: a draw whose canvas wasn't kept (or a grab's copy not drawn back) shows nowhere; the canvas does.
  Lists l;
  Canvas c(16, 16);
  l.text(&screen_px, line({0, 0, 16, 16}, "NO SNAPSHOT"));
  std::vector<uint16_t> b;
  l.layers(&screen_px, c.px.data(), c.w, c.h, c.w, &b);
  bool none = true;
  for (uint16_t v : b) none = none && v == 0;
  EXPECT(none);
  // A grab's copies carry no snapshots: drawn back, they take new ones there.
  draw(l, c, &bg_px, 16, 16, 0, 0, true, "pic", [&] { c.fill({0, 0, 16, 16}, 0x1234); });
  l.grab(&screen_px, Rect{0, 0, 16, 16}, &grab_px);
  EXPECT(l.of(&grab_px) && l.of(&grab_px)->size() == 1 && (*l.of(&grab_px))[0].after.empty());
}
