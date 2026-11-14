#include "gamez/ztwod/twod_lines.h"

#include <cstring>

namespace ztwod {

// Mech3.exe 1.2 @ 0x005a3cd0
LineLog::LineLog() : lines{FontText(nullptr, 0, 0), FontText(nullptr, 0, 0), FontText(nullptr, 0, 0),
                           FontText(nullptr, 0, 0), FontText(nullptr, 0, 0), FontText(nullptr, 0, 0),
                           FontText(nullptr, 0, 0), FontText(nullptr, 0, 0), FontText(nullptr, 0, 0),
                           FontText(nullptr, 0, 0), FontText(nullptr, 0, 0), FontText(nullptr, 0, 0),
                           FontText(nullptr, 0, 0), FontText(nullptr, 0, 0), FontText(nullptr, 0, 0)} {  // 0x0047eab0
  int32_t y = 30;
  for (FontText& l : lines) {
    if (l.color_top != 0x996a00 || l.color_bottom != 0x95c7ff) {
      l.color_top = 0x996a00, l.color_bottom = 0x95c7ff;
      l.needs_render = 1;
    }
    l.create_font("Arial", 10, 500, 6, 0, 0, 2);
    l.shadow = 1;
    l.shadow_dx = -1, l.shadow_dy = -1;
    l.align = 1;
    l.set_pos(320, y);
    l.hide();
    y += 18;
  }
}

// Mech3.exe 1.2 @ 0x005a3de0
void LineLog::set_font_all(const char* face, int32_t height, int32_t weight, int32_t width) {
  for (int i = kLines - 1; i >= 0; --i) lines[i].create_font(face, height, weight, width, 0, 0, 2);
}

// Mech3.exe 1.2 @ 0x005a3e30
void LineLog::set_colors_all(uint32_t top, uint32_t bottom) {
  for (int i = kLines - 1; i >= 0; --i) {
    FontText& l = lines[i];
    if (top != l.color_top || bottom != l.color_bottom) {
      l.color_top = top, l.color_bottom = bottom;
      l.needs_render = 1;
    }
  }
}

// Mech3.exe 1.2 @ 0x005a3e80
FontText* LineLog::add(float seconds, const char* text) {
  set_active(1);
  if (!(lines[0].flags & kHidden) && std::strcmp(text, lines[0].rendered_text()) != 0) {
    for (int i = kLines - 2; i >= 0; --i) {
      FontText& from = lines[i];
      if (from.flags & kHidden) continue;
      FontText& to = lines[i + 1];
      from.hide();
      to.set_timer(from.timer);
      // FIX (A): the exe passes the line's text as the format (vt+0x7c with no arguments); a '%' in it read the
      // stack. Copied as written (twod.md, The line log).
      to.set_text("%s", from.rendered_text());
      if (from.color_top != to.color_top || from.color_bottom != to.color_bottom) {
        to.color_top = from.color_top, to.color_bottom = from.color_bottom;
        to.needs_render = 1;
      }
      to.show_for(-1.0f);
    }
  }
  FontText& top = lines[0];
  top.set_timer(seconds);
  top.set_text("%s", text);
  top.show_for(-1.0f);
  return &top;
}

LineLog& line_log() {
  static LineLog log;  // [0x00731ac0]
  return log;
}

// Mech3.exe 1.2 @ 0x005a3fd0
void line_log_add(const char* text, float seconds) { line_log().add(seconds, text); }

}  // namespace ztwod
