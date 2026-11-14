#include "gamez/zmodel/gmod_screen.h"

#include <cstring>

#include "gamez/zmodel/gmod_clip3d.h"
#include "gamez/zmodel/gmod_draw.h"

#pragma STDC FP_CONTRACT OFF

namespace zmod {

namespace {
float as_float(uint32_t v) {
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0053d130 (the same function again, without 0x0053cf10's unset outcode)
// Pirate's Moon 1.0 @ 0x00540a90 (cdecl)
uint32_t project_point(const zmth::Vec3* world, zmth::Vec3* out) {
  zmth::Mat34 scratch;
  zmth::push(&scratch);
  zmth::load_view();
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (st.is_identity()) {
    *out = *world;
  } else {
    const float* m = st.matrix().m;
    const float x = world->x, y = world->y, z = world->z;
    out->x = x * m[0] + y * m[3] + z * m[6] + m[9];
    out->z = x * m[2] + y * m[5] + z * m[8] + m[11];
    out->y = x * m[1] + y * m[4] + z * m[7] + m[10];
  }
  zmth::pop();
  const DrawState& ds = draw_state();
  const float near_z = ds.clip.near_z;            // 0x00740304
  const float left = ds.point_rect[0], top = ds.point_rect[1];
  const float right = ds.point_rect[2], bottom = ds.point_rect[3];
  if (out->z > near_z) {
    zmth::project(out, out, 1);
    uint32_t code = 0;
    if (out->x < left) {
      code = 1;
      out->x = left;
    } else if (out->x > right) {
      code = 2;
      out->x = right;
    }
    if (out->y < top) {
      out->y = top;
      return code | 4;
    }
    if (!(out->y < bottom)) {
      out->y = bottom - 1.0f;  // [0x005ce1d8]
      return code | 8;
    }
    return code;
  }
  out->z = -near_z <= out->z ? near_z : out->z * -1.0f;  // [0x005ce1dc]
  zmth::project(out, out, 1);
  out->y = bottom;
  if (out->x < -5000.0f) out->x = -5000.0f;  // [0x005ce1e0]
  else if (!(out->x < 5000.0f)) out->x = 5000.0f;
  const float clip_right = as_float(ds.clip.side_1c);  // 0x00740314
  out->x = (left + out->x - -5000.0f) / (10000.0f / (clip_right - left));  // [0x005ce1e8]
  return 8;
}

// Mech3.exe 1.2 @ 0x0053d360
// Pirate's Moon 1.0 @ 0x00540ca0 (cdecl)
uint32_t project_points(const zmth::Vec3* world, zmth::Vec3* out, int32_t n) {
  zmth::Mat34 scratch;
  zmth::push(&scratch);
  zmth::load_view();
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (st.is_identity()) {
    for (int32_t i = 0; i < n; ++i) out[i] = world[i];
  } else {
    const float* m = st.matrix().m;
    for (int32_t i = 0; i < n; ++i) {
      const float x = world[i].x, y = world[i].y, z = world[i].z;
      out[i].x = x * m[0] + y * m[3] + z * m[6] + m[9];
      out[i].z = x * m[2] + y * m[5] + z * m[8] + m[11];
      out[i].y = x * m[1] + y * m[4] + z * m[7] + m[10];
    }
  }
  zmth::pop();
  const DrawState& ds = draw_state();
  const float near_z = ds.clip.near_z;  // 0x00740304
  const float left = ds.point_rect[0], top = ds.point_rect[1];
  const float right = ds.point_rect[2], bottom = ds.point_rect[3];
  uint32_t code = 0;
  for (int32_t i = 0; i < n; ++i) {
    zmth::Vec3* o = out + i;
    if (o->z <= near_z) {
      code |= 8;
      o->z = -near_z <= o->z ? near_z : o->z * -1.0f;
      zmth::project(o, o, 1);
      o->y = top;
      if (o->x < -5000.0f) o->x = -5000.0f;
      else if (!(o->x < 5000.0f)) o->x = 5000.0f;
      const float clip_right = as_float(ds.clip.side_1c);  // 0x00740314
      o->x = (o->x + left - -5000.0f) / (10000.0f / (clip_right - left));
    } else {
      zmth::project(o, o, 1);
      if (o->x < left) code |= 1;
      else if (o->x > right) code |= 2;
      if (o->y < top) code |= 4;
      else if (o->y > bottom) code |= 8;
    }
  }
  return code;
}

namespace {
// The rotation of m times v added to m's place, or taken from it.
void move(zmth::Mat34& m, const zmth::Vec3* v, float sign) {
  float* a = m.m;
  const float x = v->x, y = v->y, z = v->z;
  // x and y first, then z, each sum stored before it moves the place
  const float dx = (x * a[0] + y * a[3]) + z * a[6];
  const float dy = (x * a[1] + y * a[4]) + z * a[7];
  const float dz = (x * a[2] + y * a[5]) + z * a[8];
  if (sign > 0) a[9] = a[9] + dx, a[10] = a[10] + dy, a[11] = a[11] + dz;
  else a[9] = a[9] - dx, a[10] = a[10] - dy, a[11] = a[11] - dz;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0053de50
// Pirate's Moon 1.0 @ 0x00541830 (cdecl)
void translate_below(const zmth::Vec3* v) { move(zmth::matrix_stack().below(), v, 1); }

// Mech3.exe 1.2 @ 0x0053df00
// Pirate's Moon 1.0 @ 0x005418e0 (cdecl)
void untranslate_two(const zmth::Vec3* v) {
  move(zmth::matrix_stack().below(), v, -1);
  move(zmth::matrix_stack().matrix(), v, -1);
}

}  // namespace zmod
