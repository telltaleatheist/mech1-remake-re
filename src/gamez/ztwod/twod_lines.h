#pragma once
#include <cstdint>

#include "gamez/ztwod/twod.h"

// zTwoD's line log (0x005a3cd0..0x005a3fd0, vtable 0x005d0170): 15 lines of Arial 10 down the middle of the
// screen, the newest at the top. Original file unknown: before the console (twod_console.h). One is made
// ([0x00731ac0]); the developer lines set its font and colours (0x0040f7f0), the frame updates it, and anim event
// 38 writes to it (0x005a3fd0). Its lines join no list, so its update reaches none of them and the original never
// draws them (docs/twod.md, The line log). Ported for preservation.
namespace ztwod {

class LineLog : public Layer {
 public:
  static constexpr int kLines = 15;
  LineLog();  // 0x005a3cd0: each line at (320, 30 + 18 i), Arial 10, gold on blue-white, shadowed, centred, hidden
  void set_font_all(const char* face, int32_t height, int32_t weight, int32_t width);  // 0x005a3de0, last first
  void set_colors_all(uint32_t top, uint32_t bottom);  // 0x005a3e30, last first
  // 0x005a3e80: the layer on; unless the newest line shows `text` already, each shown line moves one down (its text,
  // colours and timer; the one it leaves hidden); then the newest is `text` for `seconds` (< 0: untimed). The newest.
  FontText* add(float seconds, const char* text);
  FontText lines[kLines];  // +0x10, 0x4b0 bytes apart
};

LineLog& line_log();                                 // [0x00731ac0]
void line_log_add(const char* text, float seconds);  // 0x005a3fd0 (ECX the seconds)

}  // namespace ztwod
