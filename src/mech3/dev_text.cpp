#include "mech3/dev_text.h"

#include <cstdio>
#include <string>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/ztwod/twod_lines.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/app_engine.h"
#include "mech3/message_panel.h"

namespace mech3 {

namespace {
DevTextHooks g_hooks;
int32_t g_readout_on = 0;  // [0x00603f8c]
}  // namespace

DevTextHooks& dev_text_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x0040e840
DevText::DevText(const char* format, int32_t px, int32_t py) : FontText(format, px, py) {
  set_font(ztwod::host().font_find("mech_string"));  // 0x00517680 on 0x005e14e4
}

namespace {
DevText row() { return DevText(nullptr, 0, 0); }  // 0x0040e830
}  // namespace

// Mech3.exe 1.2 @ 0x0040ff40
DevLines::DevLines(int32_t x, int32_t y, int32_t dy)
    : rows{row(), row(), row(), row(), row(), row(), row(), row(), row(), row(), row(), row(), row(),
           row(), row(), row(), row(), row(), row(), row(), row(), row(), row(), row(), row()} {
  for (DevText& r : rows) {
    r.set_pos(x, y);
    r.hide();
    add(&r);  // 0x0059fbe0
    y += dy;
  }
}

// Mech3.exe 1.2 @ 0x0040f150
FpsText::FpsText() : DevText("fps", 2, 12) {
  bg_rect.left = x, bg_rect.top = y;
  bg_rect.right = x + 60, bg_rect.bottom = y + 15;
  hide();
}

// Mech3.exe 1.2 @ 0x0040f1f0: the frame counted with the unscaled step; past a second, the rate. The text is put
// in as the format (the exe's own way: no '%' can be in it).
void FpsText::update(float dt) {
  const float n = frames - -1.0f;
  elapsed = (g_hooks.raw_dt ? g_hooks.raw_dt() : 0.0f) + elapsed;
  frames = n;
  if (1.0f <= elapsed) {
    const float t = elapsed;
    frames = 0, elapsed = 0;
    rate = n / t;
  }
  char text[0x80];
  if (g_hooks.net_running && g_hooks.net_running()) {
    int32_t net[3] = {};
    if (g_hooks.net_numbers) g_hooks.net_numbers(net);
    std::snprintf(text, sizeof text, "%1f (%d / %d / %d )", static_cast<double>(rate), net[0], net[1], net[2]);
  } else {
    const int32_t entities = g_hooks.entity_count ? g_hooks.entity_count() : 0;
    std::snprintf(text, sizeof text, "%.1f (%d/%d)", static_cast<double>(rate),
                  g_hooks.frame_number ? g_hooks.frame_number() : 0, entities - 1);
  }
  set_text("%s", text);
  ztwod::Object2D::update(dt);  // 0x0059ffb0
}

DevLines& dev_lines() {
  static DevLines lines(5, 0x5f, 12);  // 0x0040ef60
  return lines;
}
FpsText& fps_text() {
  static FpsText fps;  // 0x0040f010
  return fps;
}
ztwod::Layer& dev_layer() {
  static ztwod::Layer layer;  // 0x0040f0f0 -> 0x0040f130
  return layer;
}

// Mech3.exe 1.2 @ 0x0040f7f0
void dev_text_init(const int32_t* kept, const int32_t* area) {
  ztwod::LineLog& log = ztwod::line_log();
  log.set_font_all("Arial", 10, 500, 6);
  log.set_colors_all(0x996a00, 0x95c7ff);
  const ztwod::Handle font = ztwod::host().font_find("mech_string");
  for (DevText& r : dev_lines().rows) r.set_font(font);  // vt+0x90
  dev_text_reset(kept, area);
}

// Mech3.exe 1.2 @ 0x0040f870
bool dev_text_reset(const int32_t* kept, const int32_t* area) {
  message_panel_place(kept, area);  // 0x0040f330: ECX and EDX passed on untouched
  message_panel_reset();                  // 0x0040f040
  return true;
}

// Mech3.exe 1.2 @ 0x0040f890
void dev_layers_on() {
  dev_layer().set_active(1);
  dev_lines().set_active(1);
}

// Mech3.exe 1.2 @ 0x0040f8b0
void dev_layers_off() {
  dev_layer().set_active(0);
  dev_lines().set_active(0);
}

// Mech3.exe 1.2 @ 0x0040fb50
void dev_readout(int32_t mode) {
  DevText* rows = dev_lines().rows;
  if (mode == 0) {
    g_readout_on = 0;
    for (DevText& r : dev_lines().rows) {
      r.set_text("");  // 0x005f4c90
      r.hide();
    }
    return;
  }
  if (mode == 1) {
    g_readout_on = 1;
    return;
  }
  if (mode != 2 || g_readout_on != 1) return;
  uint32_t zone = 0;
  zcls::camera_walk_zone(&zone);  // 0x004f6c70
  const int32_t count = static_cast<int32_t>(zone & 0xff);
  const int32_t z1 = static_cast<int32_t>(zone >> 8 & 0xff), z2 = static_cast<int32_t>(zone >> 16 & 0xff);
  const int32_t z3 = static_cast<int32_t>(zone >> 24);
  int32_t at = 0;
  switch (count) {
    case 0: at = 1, rows[0].set_text("Last Camera zones  none"); break;
    case 1: at = 1, rows[0].set_text("Last Camera zones  %4d", z1); break;
    case 2: at = 1, rows[0].set_text("Last Camera zones  %4d %4d", z1, z2); break;
    case 3: at = 1, rows[0].set_text("Last Camera zones  %4d %4d %4d", z1, z2, z3); break;
    default: break;
  }
  rows[at].set_text("node/m3d/matl: %d/%d, %d/%d, %d/%d", zcls::node_live(), zcls::node_total(),  // 0x004eacc0/b0
                    zmod::models().count, zmod::models().size,                       // 0x00539140, 0x005269d0
                    zmod::materials().count, zmod::materials().capacity);             // 0x00535f00, 0x00535f10
  zwep::PoolStats p{};
  if (g_hooks.pool_stats) {
    int32_t v[4] = {};
    g_hooks.pool_stats(v);
    p = {v[0], v[1], v[2], v[3]};
  } else {
    zwep::pool_stats(&p);  // 0x00581e50
  }
  rows[at + 1].set_text("weapons: %d/%d (%d free) %d owners", p.flying, p.size, p.free, p.x0c);
  int32_t shown = at + 2;
  uint32_t total = 0, free = 0;
  if (g_hooks.video_memory && g_hooks.video_memory(&total, &free) && total != 0) {
    // Kept on the x87 stack to the text, never a float: doubles here.
    const double used = static_cast<double>(total - free) * 0.0009765625;
    const double all = static_cast<double>(total) * 0.0009765625;
    rows[shown++].set_text("Texture: %.2fkb / %.2fkb (%.2f%%)", used, all, used / all * 100.0);
  }
  for (int32_t i = shown - 1; i >= 0; --i) rows[i].show_for(-1.0f);
}

DevMarks& dev_marks() {
  static DevMarks m;
  return m;
}

// Mech3.exe 1.2 @ 0x0040fef0: every value of the table is 0 (0x005e1500..0x005e1548).
void dev_mark(const char* text) {
  DevMarks& m = dev_marks();
  if (!m.on) return;
  m.value = 0.0f;  // [0x005e1500 + 4 next]
  ++m.next;
  if (m.last < m.next) m.next = m.last;
  program_briefing_line(text);       // 0x00405120
  program_briefing_progress(m.value);  // 0x004050d0
}

// Mech3.exe 1.2 @ 0x0043cb10: into one string kept between calls ([0x00605884]).
const char* hud_zones_text(const uint8_t zones[4]) {
  static std::string s;
  s = "[ ";
  const int32_t n = zones[0] < 3 ? zones[0] : 3;
  char item[16];
  for (int32_t i = 0; i < n; ++i) {
    std::snprintf(item, sizeof item, "%d ", zones[1 + i]);
    s += item;
  }
  s += "]";
  return s.c_str();
}

// Mech3.exe 1.2 @ 0x0043cc00
const char* hud_state_name(int32_t state) {
  static const char* const kNames[] = {"limp",      "stand", "stand_step", "step",    "step_walk",
                                       "walk",      "walk_trot", "trot",   "trot_run", "run",
                                       "dead",      "falling",   "fallenback", "fallenfront"};
  return state >= 0 && state < 14 ? kNames[state] : "unknown";
}

}  // namespace mech3
