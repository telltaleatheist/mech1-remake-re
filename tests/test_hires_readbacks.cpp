// Ours (MW1's FIXES.md item 12, mw1/research/hires-2d.md B4 "Read-backs"): one test for each place B0 found that
// reads the canvas back, through the SDL host as the game calls it (screen_grab, screen_grab_rect, blit,
// texture_free, ripple) with the high-resolution draw on. A line of text is recorded on the screen; each site's
// copy and draw-back must keep it showing where the copy brings it back, and nowhere the canvas was written over it.
// Elsewhere: the lab's load box over the lab (lab_filebox.cpp's screen_grab(2) over the held frame) is
// mw1/tests/hires_order.cmake's capture; PM's pause and the mission's grabs hold nothing recorded (no test); the blur
// is an unrecorded write like the ripple (hires_layers_an_unrecorded_write_under_a_text).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "check.h"
#include "frontend/sdl/sdl_twod_host.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_tint.h"
#include "platform/hires_2d.h"
#include "platform/hires_lists.h"

namespace {
constexpr uint16_t kOpaqueKey = 0xf81f;  // a key no pixel here has: the grab drawn back whole

// A 64x48 screen as the host's 2D target, the high-resolution draw on, a host with no engine (as tests make it).
struct Screen {
  static constexpr int32_t kW = 64, kH = 48;
  std::vector<uint16_t> px;
  sdlfe::SdlTwodHost host{nullptr, nullptr, nullptr};
  zrndr::Target2D kept;
  zrndr::FinishTarget kept_finish;
  bool was_on;
  Screen()
      : px(static_cast<size_t>(kW) * kH), kept(zrndr::target2d()), kept_finish(zrndr::finish_target()),
        was_on(platform::hires_2d()) {
    for (int32_t y = 0; y < kH; ++y)
      for (int32_t x = 0; x < kW; ++x) px[static_cast<size_t>(y) * kW + x] = static_cast<uint16_t>(0x0841 * ((x / 4 + y / 4) % 8));
    zrndr::target2d() = {reinterpret_cast<uint8_t*>(px.data()), kW, kH, kW * 2, 6};
    zrndr::finish_target() = {px.data(), kW, kH, kW * 2, kW};  // the ripple's and the tint's (the engine's: the same)
    platform::set_hires_2d(true);
  }
  ~Screen() {
    zrndr::target2d() = kept;
    zrndr::finish_target() = kept_finish;
    platform::set_hires_2d(was_on);
  }
  // A line of text on its own 40x12 image (its glyph pixels white, the rest the key, 0), recorded and blitted keyed.
  zimg::Texture* text(int32_t x, int32_t y) {
    zimg::Texture* img = host.image_new(40, 12);
    auto* p = static_cast<uint16_t*>(img->pixels);
    for (int32_t k = 2; k < 38; k += 4)
      for (int32_t j = 2; j < 10; ++j) p[j * 40 + k] = 0xffff;  // the stems
    platform::hires::Command c;
    c.rect = {0, 0, 40, 12};
    c.text = "READ BACK";
    c.colour = 0x00ffffff;
    host.hires_lists().text(img->pixels, c);
    host.blit(img, x, y, 0, nullptr);
    return img;
  }
  // Something unrecorded over the screen (a dialog's art, a fill): the canvas written there.
  void cover(int32_t l, int32_t t, int32_t r, int32_t b, uint16_t v = 0x39e7) {
    for (int32_t y = t; y < b; ++y)
      for (int32_t x = l; x < r; ++x) px[static_cast<size_t>(y) * kW + x] = v;
  }
  // Whether a recorded draw shows at (x, y) now (B1's layers: a chain there, the canvas still as it left it).
  bool shows(int32_t x, int32_t y) {
    std::vector<uint16_t> b;
    host.hires_lists().layers(px.data(), px.data(), kW, kH, kW, &b);
    return b.size() == px.size() && b[static_cast<size_t>(y) * kW + x] != 0;
  }
  size_t draws() const { return host.hires_lists().size(px.data()); }
};
}  // namespace

// twod_dialog.cpp:314: a dialog grabs the screen behind it (screen_grab(1)), draws over it, and its restore_behind
// blits the grab back. The text comes back with it.
TEST(hires_readback_a_dialog_s_behind_restored) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  EXPECT(s.shows(10, 10));
  zimg::Texture* behind = s.host.screen_grab(1);
  EXPECT(behind && s.host.hires_lists().size(behind->pixels) == 1);
  s.cover(0, 0, Screen::kW, Screen::kH);  // the dialog
  EXPECT(!s.shows(10, 10));
  s.host.blit(behind, 0, 0, kOpaqueKey, nullptr);
  EXPECT(s.shows(10, 10) && s.shows(46, 18));
  EXPECT(!s.shows(50, 10) && !s.shows(4, 4));  // outside the line: the grab's canvas only
  s.host.texture_free(behind);
  s.host.texture_free(t);
}

// twod_dialog.cpp:462, twod_control.cpp:273, twod_dialog.cpp:528: the dialog's static parts grabbed once
// (static_bg), and each control's erase background a slice of it blitted at its place. The slice brings back the
// part of the text inside it, and only that.
TEST(hires_readback_a_control_s_erase_slice) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  zimg::Texture* static_bg = s.host.screen_grab(1);
  s.cover(0, 0, Screen::kW, Screen::kH);
  const ztwod::Rect slice{20, 8, 30, 20};  // a control's rect
  s.host.blit(static_bg, slice.left, slice.top, kOpaqueKey, &slice);
  EXPECT(s.shows(24, 10) && s.shows(29, 19));
  EXPECT(!s.shows(10, 10) && !s.shows(32, 10));  // the rest still under the dialog
  s.host.texture_free(static_bg);
  s.host.texture_free(t);
}

// twod_msgbox.cpp:31, controls_menu.cpp:317 and :369: a message box or popup grabs the rect it covers
// (screen_grab_rect) and draws it back when it closes.
TEST(hires_readback_a_popup_s_rect_restored) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  ztwod::Rect r{4, 4, 36, 28};
  zimg::Texture* under = s.host.screen_grab_rect(2, &r, nullptr);
  EXPECT(under && s.host.hires_lists().size(under->pixels) == 1);
  s.cover(r.left, r.top, r.right, r.bottom);  // the box
  EXPECT(!s.shows(10, 10) && s.shows(40, 10));  // under the box hidden; the line past it still shows
  s.host.blit(under, r.left, r.top, kOpaqueKey, nullptr);
  EXPECT(s.shows(10, 10) && s.shows(40, 10));
  s.host.texture_free(under);
  s.host.texture_free(t);
}

// twod_object.cpp:494, Cursor::grab_at: the pixels under the cursor saved (a rect grab), the cursor drawn over
// them, then put back when it moves. Under the cursor the text is hidden; once it moves on, it shows again. And the
// screen's list stays flat: each put-back carried a copy of what it saved, the copies multiplied, and the list passed
// its cap (the screen's text blocky until drawn afresh) in under 100 moves; the copy of a draw still on the screen in
// place is no longer added (hires_lists.cpp, Lists::blit).
TEST(hires_readback_the_cursor_s_save_and_restore) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  zimg::Texture* cursor = s.host.image_new(8, 8);
  std::fill(static_cast<uint16_t*>(cursor->pixels), static_cast<uint16_t*>(cursor->pixels) + 64, 0xffe0);
  zimg::Texture* saved = nullptr;
  size_t most = 0;
  bool hidden_under = true, back_after = true;
  for (int32_t step = 0; step < 1000; ++step) {
    const int32_t x = 4 + (step * 7) % 44, y = 4 + (step * 3) % 30;  // all over the line and off it
    ztwod::Rect r{x, y, x + 8, y + 8};
    saved = s.host.screen_grab_rect(2, &r, saved);
    s.host.blit(cursor, x, y, kOpaqueKey, nullptr);
    if (x + 3 >= 8 && x + 3 < 48 && y + 3 >= 8 && y + 3 < 20) hidden_under = hidden_under && !s.shows(x + 3, y + 3);
    s.host.blit(saved, x, y, kOpaqueKey, nullptr);  // moved: put back
    back_after = back_after && s.shows(12, 12) && s.shows(46, 18);
    most = std::max(most, s.draws());
  }
  std::printf("  1000 cursor moves: the screen's draws at most %zu, fallen %d\n", most, s.host.hires_lists().fallen(s.px.data()));
  EXPECT(hidden_under && back_after);
  EXPECT(most <= 2 && !s.host.hires_lists().fallen(s.px.data()));
  s.host.texture_free(saved);
  s.host.texture_free(cursor);
  s.host.texture_free(t);
}

// menu_widgets.cpp:183, MenuScreen::bake (MW1's options, controls and upgrades pages run PM's code): the screen with
// its controls grabbed as the still, then drawn whole each frame over a cleared buffer. It keeps showing, and the
// screen's list stays flat.
TEST(hires_readback_a_baked_still_redrawn) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  zimg::Texture* still = s.host.screen_grab(1);
  size_t most = 0;
  for (int32_t frame = 0; frame < 300; ++frame) {
    s.cover(0, 0, Screen::kW, Screen::kH, 0);  // the buffer cleared for the frame
    s.host.blit(still, 0, 0, kOpaqueKey, nullptr);
    most = std::max(most, s.draws());
  }
  EXPECT(s.shows(10, 10) && s.shows(46, 18) && !s.shows(4, 4));
  std::printf("  300 frames of the still: the screen's draws at most %zu\n", most);
  EXPECT(most <= 2);
  s.host.texture_free(still);
  s.host.texture_free(t);
}

// menu_widgets.cpp:189, borrow_background: a layout with no BACKGROUND borrows the main menu's image or a grab, and
// blits it whole. Drawn over the text it covers it (an opaque cover drops what it hides whole); its own recorded
// draws (a grab's) come with it.
TEST(hires_readback_a_borrowed_background) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  zimg::Texture* grab = s.host.screen_grab(1);  // what's borrowed: a grab of a screen with the text on it
  s.cover(0, 0, Screen::kW, Screen::kH);
  zimg::Texture* other = s.host.image_new(Screen::kW, Screen::kH);  // another page's plain background
  std::fill(static_cast<uint16_t*>(other->pixels), static_cast<uint16_t*>(other->pixels) + Screen::kW * Screen::kH, 0x18c3);
  s.host.blit(other, 0, 0, kOpaqueKey, nullptr);
  EXPECT(!s.shows(10, 10) && s.draws() == 0);  // a plain picture with no source: nothing recorded shows over it
  s.host.blit(grab, 0, 0, kOpaqueKey, nullptr);
  EXPECT(s.shows(10, 10));
  s.host.texture_free(other);
  s.host.texture_free(grab);
  s.host.texture_free(t);
}

// texture_free: a grab let go takes its recorded draws with it (none left behind under its address).
TEST(hires_readback_a_freed_grab_keeps_nothing) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  zimg::Texture* g = s.host.screen_grab(1);
  const void* at = g->pixels;
  EXPECT(s.host.hires_lists().size(at) == 1);
  s.host.texture_free(g);
  EXPECT(s.host.hires_lists().size(at) == 0);
  EXPECT(s.shows(10, 10));  // the screen's own unchanged
  s.host.texture_free(t);
}

// twod_object.cpp:597, Ripple::draw_in_rect (PM's slide buttons; none on MW1's screens): the ripple moves the canvas's
// pixels in a ring, an unrecorded write. Where it moved them the canvas shows; outside the ring the text still does.
TEST(hires_readback_a_ripple_shows_the_canvas) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  const std::vector<uint16_t> before = s.px;
  const ztwod::Rect clip{0, 0, Screen::kW, Screen::kH};
  s.host.ripple(28, 14, 0, 10, 3, 6.0f, 1.0f, &clip);
  size_t moved = 0, moved_shown = 0;
  for (int32_t y = 8; y < 20; ++y)
    for (int32_t x = 8; x < 48; ++x)
      if (s.px[static_cast<size_t>(y) * Screen::kW + x] != before[static_cast<size_t>(y) * Screen::kW + x])
        ++moved, moved_shown += s.shows(x, y);
  std::printf("  the ripple moved %zu of the line's pixels\n", moved);
  EXPECT(moved > 0 && moved_shown == 0);
  EXPECT(s.shows(46, 18));  // out of its ring
  s.host.texture_free(t);
}

// app_engine.cpp:982, GameEngine::image_draw: a fade's still drawn back over the whole buffer, outside the host. At
// the screen's size it's the same pixels and the text shows on; scaled (another size), the pixels differ and the
// canvas shows.
TEST(hires_readback_a_fade_s_still_drawn_back) {
  Screen s;
  zimg::Texture* t = s.text(8, 8);
  const std::vector<uint16_t> still = s.px;  // grabs_[h], the buffer as it was
  s.cover(0, 0, Screen::kW, Screen::kH, 0);  // the fade's frames
  std::copy(still.begin(), still.end(), s.px.begin());  // drawn back at its own size: buffer_ = g.pixels
  EXPECT(s.shows(10, 10) && s.shows(46, 18));
  // At another size: each pixel the still's nearest, scaled 3/4.
  for (int32_t y = 0; y < Screen::kH; ++y)
    for (int32_t x = 0; x < Screen::kW; ++x)
      s.px[static_cast<size_t>(y) * Screen::kW + x] = still[static_cast<size_t>(y * 3 / 4) * Screen::kW + x * 3 / 4];
  EXPECT(!s.shows(10, 10));
  s.host.texture_free(t);
}

