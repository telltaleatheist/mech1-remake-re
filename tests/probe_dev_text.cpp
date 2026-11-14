// test_dev_text.py: the developers' text (mech3/dev_text, gamez/ztwod/twod_lines) against the original.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/ztwod/twod_lines.h"
#include "mech3/dev_text.h"

namespace {
std::string g_out;
void put(const void* p, size_t n) { g_out.append(static_cast<const char*>(p), n); }
void put32(uint32_t v) { put(&v, 4); }
void puts0(const char* s) { g_out += s ? s : ""; g_out += '\0'; }
int finish(char* out, int cap) {
  if (static_cast<int>(g_out.size()) > cap) return -1;
  std::memcpy(out, g_out.data(), g_out.size());
  return static_cast<int>(g_out.size());
}
uint32_t fbits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}
void dump_line(ztwod::FontText& l) {
  put32(l.flags & 0x11), put32(fbits(l.timer)), put32(l.color_top), put32(l.color_bottom), puts0(l.text);
}
}  // namespace

extern "C" {

int probe_dev_state_name(int32_t state, char* out, int cap) {
  g_out.clear();
  puts0(mech3::hud_state_name(state));
  return finish(out, cap);
}

int probe_dev_zones(const uint8_t* zones, char* out, int cap) {
  g_out.clear();
  puts0(mech3::hud_zones_text(zones));
  return finish(out, cap);
}

// ops: 'a' seconds(f32) text\0, 'c' top bottom (u32 u32), 'k' line(u8) top bottom. After each op every line's flags (hidden, timed), timer,
// colours and text.
int probe_dev_linelog(const uint8_t* ops, int n, char* out, int cap) {
  g_out.clear();
  {
    ztwod::LineLog log;
    for (int i = 0; i < n;) {
      const char kind = static_cast<char>(ops[i++]);
      if (kind == 'a') {
        float s;
        std::memcpy(&s, ops + i, 4);
        i += 4;
        const char* t = reinterpret_cast<const char*>(ops + i);
        i += static_cast<int>(std::strlen(t)) + 1;
        log.add(s, t);
      } else if (kind == 'k') {  // the test's own: one line's colours, so a scroll has colours to carry
        uint32_t top, bottom;
        const int k = ops[i++];
        std::memcpy(&top, ops + i, 4), std::memcpy(&bottom, ops + i + 4, 4);
        i += 8;
        log.lines[k].color_top = top, log.lines[k].color_bottom = bottom;
      } else {
        uint32_t top, bottom;
        std::memcpy(&top, ops + i, 4), std::memcpy(&bottom, ops + i + 4, 4);
        i += 8;
        log.set_colors_all(top, bottom);
      }
      put32(static_cast<uint32_t>(log.active));
      for (ztwod::FontText& l : log.lines) dump_line(l);
    }
  }
  return finish(out, cap);
}

// The readout with: zone (the walk's packed bytes), counts {node live, node total, models, model size, materials,
// material capacity}, pool {size, free, flying, owners}, video memory (ok, total, free). Runs mode 1 then 2, then
// `clear` with mode 0. Out: each row's hidden flag and text.
namespace {
int32_t g_pool[4];
uint32_t g_vid[3];
}  // namespace
int probe_dev_readout(uint32_t zone, const int32_t* counts, const int32_t* pool, const uint32_t* vid, int clear,
                      char* out, int cap) {
  g_out.clear();
  std::memcpy(g_pool, pool, sizeof g_pool);
  std::memcpy(g_vid, vid, sizeof g_vid);
  mech3::DevTextHooks& h = mech3::dev_text_hooks();
  const mech3::DevTextHooks saved = h;
  h.pool_stats = [](int32_t o[4]) { std::memcpy(o, g_pool, sizeof g_pool); };
  h.video_memory = [](uint32_t* total, uint32_t* free) { *total = g_vid[1], *free = g_vid[2]; return g_vid[0] != 0; };
  const uint32_t zone0 = zcls::scene_walk().zone;
  zcls::scene_walk().zone = zone;
  zcls::NodePool& np = zcls::nodes();
  const int32_t live0 = np.live, cap0 = np.capacity;
  np.live = counts[0], np.capacity = counts[1];
  zmod::ModelPool& mp = zmod::models();
  const int32_t mc0 = mp.count, ms0 = mp.size;
  mp.count = counts[2], mp.size = counts[3];
  zmod::MaterialTable& mt = zmod::materials();
  const int32_t tc0 = mt.count, tcap0 = mt.capacity;
  mt.count = counts[4], mt.capacity = counts[5];
  mech3::dev_readout(1);
  mech3::dev_readout(2);
  if (clear) mech3::dev_readout(0);
  for (mech3::DevText& r : mech3::dev_lines().rows) put32(r.flags & 0x10), puts0(r.text);
  mech3::dev_readout(0);
  zcls::scene_walk().zone = zone0;
  np.live = live0, np.capacity = cap0;
  mp.count = mc0, mp.size = ms0;
  mt.count = tc0, mt.capacity = tcap0;
  h = saved;
  return finish(out, cap);
}

// The frame rate over `n` frames of raw steps; net: running, numbers; the frame number and entity count. Out per
// frame: elapsed, rate, frames (bits) and the text.
namespace {
float g_raw;
int32_t g_net[4], g_frame, g_ents;
}  // namespace
int probe_dev_fps(const float* steps, int n, const int32_t* net, int32_t frame, int32_t entities, char* out, int cap) {
  g_out.clear();
  std::memcpy(g_net, net, sizeof g_net);
  g_frame = frame, g_ents = entities;
  mech3::DevTextHooks& h = mech3::dev_text_hooks();
  const mech3::DevTextHooks saved = h;
  h.raw_dt = [] { return g_raw; };
  h.net_running = [] { return g_net[0] != 0; };
  h.net_numbers = [](int32_t o[3]) { o[0] = g_net[1], o[1] = g_net[2], o[2] = g_net[3]; };
  h.frame_number = [] { return g_frame; };
  h.entity_count = [] { return g_ents; };
  {
    mech3::FpsText fps;
    for (int i = 0; i < n; ++i) {
      g_raw = steps[i];
      fps.update(0.0f);
      put32(fbits(fps.elapsed)), put32(fbits(fps.rate)), put32(fbits(fps.frames)), puts0(fps.text);
    }
  }
  h = saved;
  return finish(out, cap);
}

}  // extern "C"
