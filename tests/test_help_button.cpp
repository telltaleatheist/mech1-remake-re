// HelpButton's blink (Mech3.exe 1.2 @ 0x0044af20) and ours, set_buttons_steady: a program's buttons hold their lamp.
// In both MW3's and PM's tests (the widget is the same in both).
#include <cstdint>
#include <vector>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "mech3/menu_widgets.h"

namespace {
struct ButtonEngine : mech3::Engine {};
struct Picture {
  std::vector<uint16_t> px;
  zimg::Texture t{};
  Picture(int w, int h) : px(static_cast<size_t>(w * h), 0x1234) {
    t.width = static_cast<int16_t>(w), t.height = static_cast<int16_t>(h), t.width_again = w, t.pixels = px.data();
  }
};
}  // namespace

// A HelpButton under the mouse blinks its lamp (its normal and activate images every `period`), as MW3's and PM's;
// with set_buttons_steady (a program's own, MW1's) it holds the lit one, the rollover image when it has one.
TEST(help_button_blinks_unless_held_steady) {
  ButtonEngine e;
  Picture normal(8, 8), lit(8, 8), roll(8, 8);
  mech3::HelpButton b(e, 1);
  b.img_normal = &normal.t, b.img_activate = &lit.t, b.enabled = 1;
  b.set_image(&normal.t);
  b.on_enter();
  std::vector<zimg::Texture*> seen;
  for (int i = 0; i < 6; ++i) b.update(b.period), seen.push_back(b.image);
  int swaps = 0;
  for (size_t i = 1; i < seen.size(); ++i) swaps += seen[i] != seen[i - 1];
  EXPECT(swaps >= 4);  // blinking
  mech3::set_buttons_steady(true);
  for (int i = 0; i < 6; ++i) {
    b.update(b.period);
    EXPECT(b.image == &lit.t);
  }
  b.img_rollover = &roll.t;
  b.update(b.period);
  EXPECT(b.image == &roll.t);
  b.on_leave();
  EXPECT(b.image == &normal.t);  // off it: its own image again
  mech3::set_buttons_steady(false);
  b.img_rollover = b.img_normal = b.img_activate = nullptr;
  b.image = nullptr;
}

