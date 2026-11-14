#include "gamez/zmodel/gmod_clipfrustum.h"

#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zutil/zfix.h"

#pragma STDC FP_CONTRACT OFF

// The passes, their formulas and the emulator check: docs/render.md, The unused frustum clippers.
namespace zmod {

namespace {
constexpr int kMax = 64;  // the exe's stack buffers; nothing checks it

// `(a - b) * t + b` in the x87's order, two statements so nothing fuses them.
float lerp(float a, float b, float t) {
  float d = (a - b) * t;
  return d + b;
}

// A polygon's arrays: the caller's or the clipper's own.
struct Poly {
  float (*pos)[3];
  float (*uv)[2];
  float (*rgb)[3];
  uint32_t* code;
};

struct Local {
  float pos[kMax][3];
  float uv[kMax][2];
  float rgb[kMax][3];
  uint32_t code_a[kMax], code_b[kMax];
};

// The y bits of a point (y below -z: 4, above z: 8).
uint32_t y_bits(float y, float z) {
  if (-z <= y) return z < y ? 8u : 0u;
  return 4u;
}

uint32_t outcode(const ClipPlanes& c, bool far_on, const float p[3]) {
  const float x = p[0], y = p[1], z = p[2];
  uint32_t k;
  if (c.near_z <= z)
    k = (z <= c.far_z || !far_on) ? 0u : 0x20u;
  else
    k = 0x10u;
  if (-z <= x) {
    if (z < x) k |= 2;
  } else {
    k |= 1;
  }
  return k | y_bits(y, z);
}

void copy_vertex(Poly& d, int o, const Poly& s, int i, uint32_t code, bool uv, bool rgb) {
  d.pos[o][0] = s.pos[i][0], d.pos[o][1] = s.pos[i][1], d.pos[o][2] = s.pos[i][2];
  if (uv) d.uv[o][0] = s.uv[i][0], d.uv[o][1] = s.uv[i][1];
  if (rgb) d.rgb[o][0] = s.rgb[i][0], d.rgb[o][1] = s.rgb[i][1], d.rgb[o][2] = s.rgb[i][2];
  if (d.code) d.code[o] = code;
}

// The attributes at the cut, from `a` toward `b` by t (`b + (a - b) t`).
void cut_attrs(Poly& d, int o, const Poly& s, int a, int b, float t, bool uv, bool rgb) {
  if (uv)
    for (int k = 0; k < 2; ++k) d.uv[o][k] = lerp(s.uv[a][k], s.uv[b][k], t);
  if (rgb)
    for (int k = 0; k < 3; ++k) d.rgb[o][k] = lerp(s.rgb[a][k], s.rgb[b][k], t);
}

// The near plane: each cut from the previous corner, whichever way the edge goes.
int near_pass(const ClipPlanes& c, const Poly& s, const uint32_t* code, int n, Poly& d, bool uv, bool rgb) {
  int out = 0;
  for (int i = 0, p = n - 1; i < n; p = i++) {
    const bool pb = code[p] & 0x10, cb = code[i] & 0x10;
    if (pb != cb) {
      const float t = (c.near_z - s.pos[p][2]) / (s.pos[i][2] - s.pos[p][2]);
      d.pos[out][0] = lerp(s.pos[i][0], s.pos[p][0], t);
      d.pos[out][1] = lerp(s.pos[i][1], s.pos[p][1], t);
      d.pos[out][2] = c.near_z;
      cut_attrs(d, out, s, i, p, t, uv, rgb);
      ++out;  // its outcode is computed in the exe and never read: all are made anew after this pass
    }
    if (!cb) copy_vertex(d, out++, s, i, 0, uv, rgb);
  }
  return out;
}

// One side: `axis` x (0) or y (1), `sign` -1 for -z (bit 1, 4) or +1 for +z (bit 2, 8). An edge leaving is cut from
// the previous corner by t; one entering from the current corner by 1 - t (t still measured from the previous).
// The cut's coordinate on that axis is set to sign * z; its outcode keeps only the bits later passes read.
int side_pass(const Poly& s, int n, Poly& d, int axis, float sign, uint32_t bit, bool uv, bool rgb) {
  const int other = 1 - axis;
  int out = 0;
  for (int i = 0, p = n - 1; i < n; p = i++) {
    const bool pb = s.code[p] & bit, cb = s.code[i] & bit;
    if (pb != cb) {
      const float* P = s.pos[p];
      const float* C = s.pos[i];
      const float num = sign < 0 ? -P[axis] - P[2] : P[axis] - P[2];
      const float den = sign < 0 ? (C[2] - P[2]) + (C[axis] - P[axis]) : (C[2] - P[2]) - (C[axis] - P[axis]);
      float* q = d.pos[out];
      if (!pb) {  // leaving
        const float t = num / den;
        q[other] = lerp(C[other], P[other], t);
        q[2] = lerp(C[2], P[2], t);
        cut_attrs(d, out, s, i, p, t, uv, rgb);
      } else {  // entering
        const float t = 1.0f - num / den;
        q[other] = lerp(P[other], C[other], t);
        q[2] = lerp(P[2], C[2], t);
        cut_attrs(d, out, s, p, i, t, uv, rgb);
      }
      q[axis] = sign < 0 ? -q[2] : q[2];
      uint32_t k = 0;
      if (bit == 1) k = (q[0] <= q[2] ? 0u : 2u) | y_bits(q[1], q[2]);
      else if (bit == 2) k = y_bits(q[1], q[2]);
      else if (bit == 4) k = q[1] <= q[2] ? 0u : 8u;
      if (d.code) d.code[out] = k;
      ++out;
    }
    if (!cb) copy_vertex(d, out++, s, i, s.code[i], uv, rgb);
  }
  return out;
}

void copy_poly(Poly& d, const Poly& s, int n, bool uv, bool rgb) {
  for (int i = 0; i < n; ++i) copy_vertex(d, i, s, i, 0, uv, rgb);
}

zmth::Vec3* vec(float (*p)[3]) { return reinterpret_cast<zmth::Vec3*>(p); }

// uv_near: 0x00530d90's, carried by the near pass only; then 9 floats of the clipped ones (four and a half pairs:
// the corners' copy's count at the texture coordinates' stride) go back into it, and the side passes drop them.
int32_t clip(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], float (*rgb)[3], int32_t* count,
             bool uv_near = false) {
  bool has_uv = uv != nullptr;
  const bool has_rgb = rgb != nullptr;
  const bool far_on = (c.mask & 0x20) != 0;
  if (*count < 3) return 0;
  zmth::scale_by_cot(vec(pos), static_cast<uint32_t>(*count));
  Local L;
  Poly caller{pos, uv, rgb, L.code_a};
  Poly local{L.pos, L.uv, L.rgb, L.code_b};
  bool in_local = false;
  if (c.mask & 0x10) {
    bool all_behind = true, any_behind = false;
    for (int i = 0; i < *count; ++i) {
      if (c.near_z <= pos[i][2]) {
        L.code_a[i] = 0;
        all_behind = false;
      } else {
        L.code_a[i] = 0x10;
        any_behind = true;
      }
    }
    if (all_behind) return 0;
    if (any_behind) {
      *count = near_pass(c, caller, L.code_a, *count, local, has_uv, has_rgb);
      if (*count < 3) return 0;
      in_local = true;
      // FIX (A): only the floats the cut made; past a four-corner polygon the exe copies leftovers of its stack.
      if (uv_near) std::memcpy(uv, L.uv, (*count * 2 < 9 ? *count * 2 : 9) * sizeof(float));
    }
  }
  if (uv_near) has_uv = false;
  Poly* src = in_local ? &local : &caller;
  Poly* dst = in_local ? &caller : &local;
  if (tri) {
    std::memcpy(tri, src->pos, 9 * sizeof(float));
    zmth::scale_by_tan(reinterpret_cast<zmth::Vec3*>(tri), 3);
  }
  uint32_t all = ~0u, any = 0;
  for (int i = 0; i < *count; ++i) {
    src->code[i] = outcode(c, far_on, src->pos[i]);
    all &= src->code[i], any |= src->code[i];
  }
  if (all) return 0;
  if (any == 0) {
    // FIX (bug): a polygon the near plane cut and nothing else is copied back; the exe returns the caller's
    // arrays as they were (its first `count` corners, scaled back) with the new count. Off in the probes.
    if (in_local && zfix::options().fixed) copy_poly(caller, local, *count, has_uv, has_rgb);
    zmth::scale_by_tan(vec(pos), static_cast<uint32_t>(*count));
    return 1;
  }
  static const struct {
    uint32_t bit;
    int axis;
    float sign;
  } kSides[] = {{1, 0, -1}, {2, 0, 1}, {4, 1, -1}, {8, 1, 1}};
  for (const auto& sd : kSides) {
    if (!(any & sd.bit)) continue;
    *count = side_pass(*src, *count, *dst, sd.axis, sd.sign, sd.bit, has_uv, has_rgb);
    if (*count < 3) return 0;
    Poly* t = src;
    src = dst, dst = t;
    in_local = !in_local;
  }
  if (in_local) copy_poly(caller, local, *count, has_uv, has_rgb);
  zmth::scale_by_tan(vec(pos), static_cast<uint32_t>(*count));
  return 1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00534bf0 (not a Ghidra function)
// Pirate's Moon 1.0 @ 0x0054eb60 (cdecl, every argument on the stack)
int32_t clip_frustum(const ClipPlanes& c, float (*pos)[3], int32_t* count) {
  return clip(c, pos, nullptr, nullptr, nullptr, count);
}

// Mech3.exe 1.2 @ 0x00530d90
// Pirate's Moon 1.0 @ 0x0054aea0 (cdecl, every argument on the stack)
int32_t clip_frustum_tri(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], int32_t* count) {
  return clip(c, pos, tri, uv, nullptr, count, true);
}

// Mech3.exe 1.2 @ 0x00531d90
// Pirate's Moon 1.0 @ 0x0054be20 (cdecl, every argument on the stack)
int32_t clip_frustum_uv(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], int32_t* count) {
  return clip(c, pos, tri, uv, nullptr, count);
}

// Mech3.exe 1.2 @ 0x005331e0
// Pirate's Moon 1.0 @ 0x0054d1c0 (cdecl, every argument on the stack)
int32_t clip_frustum_uv_rgb(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], float (*rgb)[3],
                            int32_t* count) {
  return clip(c, pos, tri, uv, rgb, count);
}

}  // namespace zmod
