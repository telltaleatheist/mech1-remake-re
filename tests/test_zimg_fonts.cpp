// zImage's bitmap fonts (gamez/zimage/zimg_fonts.h): the port's FIX (A), which the exe cannot be run on.
#include <cstdint>
#include <vector>

#include "check.h"
#include "gamez/zimage/zimg_fonts.h"

TEST(font_scan_ends_at_a_glyph_touching_the_right_edge) {
  // Three glyphs, the last one's ink in the image's last column: the exe's scan never ends ("a column past the
  // width is not clear"); the port's stops there, the glyphs it found cut as the exe cuts them.
  const int w = 9, h = 2;
  const uint16_t k = 0x1234, i = 0x0001;
  const std::vector<uint16_t> px = {k, i, i, k, k, i, k, k, i,   // row 0
                                    k, i, k, k, k, i, k, k, i};  // row 1
  zimg::Texture t{};
  t.width = w, t.height = h;
  t.pixels = const_cast<uint16_t*>(px.data());
  zimg::fonts().key = k;
  zimg::Font f;
  f.image = &t;
  const int32_t found = zimg::font_scan(f);
  EXPECT(found == 4);  // three glyphs, counted from 1
  EXPECT(f.space == w / 95 - 1);
  EXPECT(f.glyph[0].left == 1 && f.glyph[0].right == 4 && f.glyph[0].bottom == h - 1);
  EXPECT(f.glyph[1].left == 5 && f.glyph[1].right == 7);
  EXPECT(f.glyph[2].left == 8 && f.glyph[2].right == 10);
}
