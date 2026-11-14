#include "pm/gamez/zeffect/zeff_anim_events.h"

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zclass/class.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/ztimer.h"
#include "pm/gamez/zmath/zmth_quat.h"
#include "pm/gamez/zclass/cls_node.h"
#include "pm/gamez/zclass/light.h"

#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace pm::zeff {

using ::zcls::Node;
using ::zeff::ev::get;
using ::zeff::ev::put;

namespace {
float f32(const uint8_t* e, int off) { return get<float>(e, off); }
}  // namespace

// Pirate's Moon 1.0 @ 0x0051c060
int32_t ev_light_animation(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  return ::zeff::light_animation_with(d, s, e, pm::zcls::light_set_range, pm::zcls::light_set_color);
}

// Pirate's Moon 1.0 @ 0x0051b910
int32_t ev_light_state(::zeff::AnimDef& d, uint8_t* e) {
  if (get<int32_t>(e, 0x2c) < 1) {
    const int32_t i = ::zeff::anim_light_ref(d, reinterpret_cast<const char*>(e + 0xc));  // 0x00522280
    put(e, 0x2c, i);
    if (i < 1) return 2;
  }
  ::zeff::CreatedRef& r = d.lights[get<int32_t>(e, 0x2c)];
  Node* n = static_cast<Node*>(r.made);
  Node* world = static_cast<Node*>(::zeff::anim().world);  // [0x008d3f20]
  ::zcls::node_set_active(n, get<int32_t>(e, 0x34));      // 0x004fca50
  if (get<int32_t>(e, 0x34) == 1) {
    if (!r.get<int32_t>(0x28)) {
      ::zcls::world_add_light(world, n);  // 0x00506160
      r.put<int32_t>(0x28, 1);
    }
  } else if (r.get<int32_t>(0x28)) {
    ::zcls::world_remove_light(world, n);  // 0x00506200
    r.put<int32_t>(0x28, 0);
  }
  if (get<int32_t>(e, 0x38)) zcls::light_set_bit4(n);
  else zcls::light_set_bit3(n);
  const uint32_t f = get<uint32_t>(e, 0x30);
  if (f & 1) zcls::light_set_position(n, f32(e, 0x58), f32(e, 0x5c), f32(e, 0x60));
  if (f & 2) {
    float p[3] = {0, 0, 0};
    Node* a = ::zeff::anim_event_node(d, get<int32_t>(e, 0x54), p);
    if (a) {
      ::zcls::node_world_point(a, p);  // 0x004ff3c0
      zcls::light_set_position(n, p[0] + f32(e, 0x58), p[1] + f32(e, 0x5c), p[2] + f32(e, 0x60));
    }
  }
  if (f & 4) zcls::light_set_orientation(n, f32(e, 0x64), f32(e, 0x68), f32(e, 0x6c));
  if (f & 8) zcls::light_set_range(n, f32(e, 0x70), f32(e, 0x74));
  if (f & 0x10) zcls::light_set_color(n, f32(e, 0x78), f32(e, 0x7c), f32(e, 0x80));
  if (f & 0x800) zcls::light_set_ambient_color(n, f32(e, 0x84), f32(e, 0x88), f32(e, 0x8c));
  if (f & 0x20) zcls::light_set_ambient(n, f32(e, 0x90));
  if (f & 0x40) zcls::light_set_diffuse(n, f32(e, 0x94));
  if (f & 0x1000) zcls::light_set_own_colors(n, get<int32_t>(e, 0x50));
  if (f & 0x80) zcls::light_set_bit2(n, get<int32_t>(e, 0x3c));
  if (f & 0x100) zcls::light_set_ambient_only(n, get<int32_t>(e, 0x40));
  if (f & 0x200) zcls::light_set_diffuse_only(n, get<int32_t>(e, 0x44));
  if (f & 0x2000) zcls::light_set_light_map(n, get<int32_t>(e, 0x48));
  if (f & 0x400) zcls::light_set_directional(n, get<int32_t>(e, 0x4c));
  return 2;
}

// Pirate's Moon 1.0 @ 0x0051f900
int32_t ev_stop_animation(::zeff::AnimDef& d, uint8_t* e) {
  const char* name = reinterpret_cast<const char*>(e + 0xc);
  if (get<int16_t>(e, 0x2c) < 0 && get<int16_t>(e, 0x2e) < 0) {
    for (int i = 0; i < d.count(0x112); ++i) {
      const auto* r = reinterpret_cast<const char*>(d.anim_refs[i].raw);
      if (std::strcmp(r, name) == 0 || std::strcmp(r + 0x20, name) == 0) put<int16_t>(e, 0x2c, static_cast<int16_t>(i));
    }
    if (get<int16_t>(e, 0x2c) < 0) {
      ::zeff::AnimState& a = ::zeff::anim();
      for (int j = 0; j < a.def_count(); ++j)
        if (std::strcmp(a.defs[j].name(), name) == 0) {
          put<int16_t>(e, 0x2e, static_cast<int16_t>(j));
          break;
        }
    }
  }
  const int16_t r = get<int16_t>(e, 0x2c);
  if (r >= 0) {
    ::zeff::anim_stop(d.anim_refs[r].ref);  // 0x0051f830
  } else if (get<int16_t>(e, 0x2e) > 0) {
    ::zeff::anim_stop(&::zeff::anim().defs[get<int16_t>(e, 0x2e)]);
  }
  return 2;
}

// Pirate's Moon 1.0 @ 0x0051e2c0
int32_t ev_object_motion_script(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  const int32_t ni = get<int32_t>(e, 0xc);
  if (ni < 0) return 2;
  Node* n = d.nodes[ni].node;
  if (!n) return 2;
  const int32_t si = get<int32_t>(e, 0x10);
  ::zeff::AnimState& a = ::zeff::anim();
  if (si < 0 || si >= a.pm_script_count) return 2;  // ours: the exe takes any index past the list
  const ::zeff::SiScript& sc = a.pm_scripts[si];
  if (s.raw[0x20] == 0) put<int32_t>(e, 0x1c, 0), put<uint32_t>(e, 0x18, 0), put<int32_t>(e, 0x20, 0);
  float time = s.get<float>(0x28) - a.time_left();
  const ::zeff::FrameSource src{sc.data, sc.get<int32_t>(0x14), sc.get<int32_t>(0xc)};
  if (get<int32_t>(e, 0x1c) >= src.limit) return 2;
  int32_t result = 1;
  do {
    const uint8_t* frame = sc.data + get<int32_t>(e, 0x1c);
    a.time_left() = a.time_left() - ::zeff::anim_play_frame(s, e, n, frame, &time, src);
    if (time < s.get<float>(0x28) && !::zeff::anim_next_frame(s, e, frame, src.limit)) result = 2;
  } while (time < s.get<float>(0x28) && result != 2);
  return result;
}

// Pirate's Moon 1.0 @ 0x0051e720. FIX (A) (docs/fixes.md): an index past the def's nodes is none; the exe reads past
// the list.
Node* anim_node_ref(::zeff::AnimDef& d, int32_t index) {
  if (index >= 0 && d.nodes) return index < d.count(0x10a) ? d.nodes[index].node : nullptr;
  if (index == -100) return d.node;
  return nullptr;
}

namespace {
// The frame's time for an event that lasts to `dur`: all of what is left, or its part up to the end.
float step_time(::zeff::SeqDef& s, float dur) {
  const float now = s.get<float>(0x28), left = ::zeff::anim().time_left();
  return now <= dur ? left : left - (now - dur);
}

int32_t color_done(::zeff::SeqDef& s, uint8_t* e, Node* n, float* color, float t) {
  pm::zcls::node_set_colors(n, e[0x30] & 1, get<int16_t>(e, 0x70), get<int16_t>(e, 0x72), color, 1);  // 0x0050cb90
  ::zeff::anim().time_left() -= t;
  return get<float>(e, 0x74) <= s.get<float>(0x28) ? 2 : 1;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0051e750
int32_t ev_object_color_motion(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  Node* n = anim_node_ref(d, get<int32_t>(e, 0x2c));
  if (!n) return 2;
  if (s.raw[0x20] == 0) {
    std::memcpy(e + 0x64, e + 0x34, 12);
    std::memcpy(e + 0x58, e + 0x40, 12);
  }
  const float t = step_time(s, get<float>(e, 0x74));
  auto* c = reinterpret_cast<float*>(e + 0x64);
  float rate[3], acc[3];
  std::memcpy(rate, e + 0x58, 12), std::memcpy(acc, e + 0x4c, 12);
  for (int k = 0; k < 3; ++k) {
    float v;
    std::memcpy(&v, e + 0x64 + 4 * k, 4);
    v = t * rate[k] + v;
    std::memcpy(e + 0x64 + 4 * k, &v, 4);
  }
  for (int k = 0; k < 3; ++k) {
    const float v = t * acc[k] + rate[k];
    std::memcpy(e + 0x58 + 4 * k, &v, 4);
  }
  float color[3];
  std::memcpy(color, c, 12);
  return color_done(s, e, n, color, t);
}

// Pirate's Moon 1.0 @ 0x0051e890
int32_t ev_object_color_rate(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  Node* n = anim_node_ref(d, get<int32_t>(e, 0x2c));
  if (!n) return 2;
  if (s.raw[0x20] == 0) std::memcpy(e + 0x58, e + 0x34, 12);
  const float t = step_time(s, get<float>(e, 0x74));
  for (int k = 0; k < 3; ++k) {
    const float v = t * get<float>(e, 0x4c + 4 * k) + get<float>(e, 0x58 + 4 * k);
    std::memcpy(e + 0x58 + 4 * k, &v, 4);
  }
  float color[3];
  std::memcpy(color, e + 0x58, 12);
  return color_done(s, e, n, color, t);
}

// Pirate's Moon 1.0 @ 0x0051b340. PM's +0xb0 is the shared def's +0xa8 (its run time).
void anim_hit_sequence(::zeff::AnimDef* d) {
  ::zeff::SeqDef* s = d->pm_seq2;
  if (!s || !s->events) return;
  ::zeff::anim().time_left() = ::ztimer::state().dt;  // [0x008c1b4c]
  d->put<float>(0xa8, 0.0f);
  ::zeff::anim_seq_restart(s);   // 0x0051fc00
  ::zeff::anim_run_events(d, s);  // 0x00520800
}

// Pirate's Moon 1.0 @ 0x0051fce0
int32_t ev_if(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  ::zeff::AnimState& a = ::zeff::anim();
  const ::zeff::AnimRunHooks& h = ::zeff::anim_run_hooks();
  const ::zcls::Raw<0x6c>& pb = a.pm_block;  // the file's block: +0x34, +0x38, +0x50, +0x60 are PM's own
  const int32_t index = get<int32_t>(e, 0x10);  // always the IF's own node, ELSEIF or not (as MW3's)
  uint32_t f = get<uint32_t>(e, 0xc);
  float value = get<float>(e, 0x14);
  const float own = value, top = get<float>(e, 0x18);  // the IF's own +0x14 and +0x18, read for 0x200, 0x400 and 0x1000
  uint32_t prev = 0;
  float cached = 0.0f;
  for (;;) {
    bool yes = false;
    if (f & 1) {
      cached = a.random[a.random_index];
      a.random_index = (a.random_index + 1) % 200;
      yes = cached <= value;
    } else if (f & 0x20) {
      yes = ::zrndr::device_fog().active != 0;
    } else if (f & 0x40) {
      yes = a.block.get<int32_t>(0x3c) != 0;
    } else if (f & 2) {
      if (a.block.get<int32_t>(0x24)) {
        if (prev != f) cached = ::zeff::distance2_to_player(d.root);
        yes = cached <= value;
      }
    } else if (f & 8) {
      if (a.block.get<int32_t>(0x24)) {
        int32_t hit = 0;
        const int32_t r = h.ray_down ? h.ray_down(h.ctx, a.player_node, a.block.floats(0x2c), value, &hit) : 1;
        yes = r == 0 && hit;
      }
    } else if (f & 0x10) {
      if (Node* n = index >= 0 ? d.nodes[index].node : nullptr) {
        int32_t hit = 0;
        const int32_t r = h.ray_down ? h.ray_down(h.ctx, n, nullptr, value, &hit) : 1;
        yes = r == 0 && hit;
      }
    } else if (f & 0x2000) {
      Node* n = index >= 0 ? d.nodes[index].node : nullptr;
      yes = n && (n->raw[0x24] & 4);
    } else if (f & 0x80) {
      yes = a.block.get<int32_t>(0x24) && a.block.get<float>(0x30) < value;
    } else if (f & 0x100) {
      if (Node* n = d.nodes[index].node) {
        float p[3];
        yes = ::zcls::node_world_position(n, p) == 0 && p[1] < value;
      }
    } else if (f & 4) {
      int32_t v;
      std::memcpy(&v, &value, 4);
      yes = a.level >= v;
    } else if (f & 0x200) {
      if (pb.get<int32_t>(0x34)) {
        if (Node* n = d.nodes[index].node) {
          float angles[3], q[4], rel[4], turn[3];
          ::zcls::node_world_euler(n, angles);                      // 0x004ff2f0
          ::zmth::euler_to_quat(angles[1], angles[0], angles[2], q);  // 0x0053ec80
          float target[4];
          std::memcpy(target, pb.raw + 0x50, 16);
          pm::zmth::quat_relative(q, target, rel);                  // 0x0053f020
          pm::zmth::quat_log(rel, turn);                            // 0x0053f310
          const float t2 = (turn[0] * turn[0] + turn[1] * turn[1]) + turn[2] * turn[2];
          yes = t2 * 4.0f <= own;
        }
      }
    } else if (f & 0x400) {
      const float g = pb.get<float>(0x60);
      yes = pb.get<int32_t>(0x38) && value <= g && g <= top;
    } else if (f & 0x800) {
      yes = d.get<float>(0xb0) <= value;  // PM's +0xb8: the health
    } else if (f & 0x1000) {
      const float hp = d.get<float>(0xb0);
      yes = value <= hp && hp <= top;
    }
    prev = f;
    if (yes) return 2;
    uint8_t* end = s.events + s.get<int32_t>(0x3c);
    uint8_t* c = s.cur;
    do {
      c += get<int32_t>(c, 4);
      s.cur = c;
      if (c[0] == 0x20 || c[0] == 0x21 || c[0] == 0x22) break;
    } while (c < end);
    if (c[0] != 0x21) return 2;
    value = get<float>(c, 0x14);
    f = get<uint32_t>(c, 0xc);
  }
}

int32_t ev_own(::zeff::AnimDef& d, ::zeff::SeqDef& s, uint8_t* e) {
  return e[0] == 44 ? ev_object_color_motion(d, s, e) : ev_object_color_rate(d, s, e);
}

}  // namespace pm::zeff
