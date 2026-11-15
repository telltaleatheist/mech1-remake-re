#include "gamez/zmath/zmth_unused.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "gamez/zgeometry/zgeo_x87.h"
#include "gamez/zmath/zmth_vector.h"

#pragma STDC FP_CONTRACT OFF

// The maths module's routines nothing calls (zmth_unused.h). Values the x87 keeps between stores are zgeo::X
// (doubles at the reference's 53 bits, floats at 24); a store is .f().
namespace zmth {
namespace {
using zgeo::X;

// An angle's sine and cosine as the exe takes them (fsin, fcos), rounded as the x87 does at its precision.
X sin_x(float a) { return X::step(std::sin(double{a})); }
X cos_x(float a) { return X::step(std::cos(double{a})); }

// Mech3.exe 1.2 @ 0x0051e1c0 (ported inline in 0x00524247's caller): v[0..1] to unit length; 0 stays 0.
void normalize2(float v[2]) {
  const float len = sqrt(X(v[0]) * v[0] + X(v[1]) * v[1]).f();  // stored, then 1 over it on the x87
  const X inv = len != 0.0f ? X(1.0f) / len : X(len);
  v[0] = (inv * v[0]).f(), v[1] = (inv * v[1]).f();
}

// The (1, -1, -1) top times the rotation of -angles and the place's offset: 0x00520c60's and 0x00521080's body.
void view_body(const float angles[3], const float place[3]) {
  Mat34& top = matrix_stack().matrix();
  const float diag[12] = {1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0};
  std::memcpy(top.m, diag, sizeof diag);
  matrix_stack().is_identity() = 0;
  const float a = -angles[0], b = -angles[1], c = -angles[2];
  const float sa = sin_x(a).f(), ca = cos_x(a).f();
  const float sb = sin_x(b).f(), cb = cos_x(b).f();
  const float sc = sin_x(c).f(), cc = cos_x(c).f();
  float r[12];
  r[0] = (X(cc) * cb - (X(sc) * sb) * sa).f();
  r[1] = ((X(cc) * sb) * sa + X(sc) * cb).f();
  r[2] = (-(X(sb) * ca)).f();
  r[3] = (-(X(sc) * ca)).f();
  r[4] = (X(cc) * ca).f();
  r[5] = sa;
  r[6] = ((X(sc) * cb) * sa + X(cc) * sb).f();
  r[7] = (X(sc) * sb - (X(cc) * cb) * sa).f();
  r[8] = (X(cb) * ca).f();
  r[9] = ((-(X(place[0]) * r[0]) - X(r[3]) * place[1]) - X(r[6]) * place[2]).f();
  r[10] = ((-(X(place[0]) * r[1]) - X(r[4]) * place[1]) - X(r[7]) * place[2]).f();
  r[11] = ((-(X(place[0]) * r[2]) - X(sa) * place[1]) - X(r[8]) * place[2]).f();
  // Times the top (1, -1, -1): every word exact in any order.
  float out[12];
  for (int i = 0; i < 4; ++i) out[3 * i] = r[3 * i], out[3 * i + 1] = -r[3 * i + 1], out[3 * i + 2] = -r[3 * i + 2];
  std::memcpy(top.m, out, sizeof out);
  matrix_stack().is_identity() = 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0051e160
void cross_unit(const float a[3], const float b[3], float out[3]) {
  out[1] = (X(a[2]) * b[0] - X(a[0]) * b[2]).f();
  out[2] = (X(a[0]) * b[1] - X(a[1]) * b[0]).f();
  out[0] = (X(a[1]) * b[2] - X(a[2]) * b[1]).f();
  normalize_x87(out);
}

// Mech3.exe 1.2 @ 0x00520c60
void view_from_angles(const float angles[3], const float place[3]) {
  view_body(angles, place);
  camera_from_angles(angles, place);
}

// Mech3.exe 1.2 @ 0x00521080: pushed over view().view with the top's flag, the body, popped.
void set_view_from_angles(const float angles[3], const float place[3]) {
  push_copy(&view().view);
  view_body(angles, place);
  pop();
  camera_from_angles(angles, place);
}

// Mech3.exe 1.2 @ 0x005214f0. Every sine and cosine stored as a float (fsincos; the x87's own fsin and fcos only
// past 9.22e18, where fsin gives up, which the port does not follow).
void camera_from_angles(const float angles[3], const float place[3]) {
  const float sa = sin_x(angles[0]).f(), ca = cos_x(angles[0]).f();
  const float sb = sin_x(angles[1]).f(), cb = cos_x(angles[1]).f();
  const X sc = X(sin_x(angles[2]).f()), cc = X(cos_x(angles[2]).f());
  const float cc_cb = (cc * cb).f();
  float* m = view().camera.m;
  m[0] = ((X(sb) * sa) * sc + cc_cb).f();
  m[1] = (sc * ca).f();
  m[2] = ((sc * cb) * sa - cc * sb).f();
  m[3] = (sc * cb - (X(sb) * sa) * cc).f();
  m[4] = (-(cc * ca)).f();
  m[5] = (-(sc * sb) - X(cc_cb) * sa).f();
  m[6] = (-(X(sb) * ca)).f();
  m[7] = sa;
  m[8] = (-(X(cb) * ca)).f();
  m[9] = place[0], m[10] = place[1], m[11] = place[2];
}

// Mech3.exe 1.2 @ 0x00521a90: 0x00521670's words, its sines and cosines stored as floats.
void rotation_matrix(float out[12], float a, float b, float c) {
  const float sa = sin_x(a).f(), ca = cos_x(a).f();
  const float sb = sin_x(b).f(), cb = cos_x(b).f();
  const X sc = X(sin_x(c).f()), cc = X(cos_x(c).f());
  const float cc_cb = (cc * cb).f();
  out[0] = ((X(sb) * sa) * sc + cc_cb).f();
  out[1] = (sc * ca).f();
  out[2] = ((sc * cb) * sa - cc * sb).f();
  out[3] = ((X(sb) * sa) * cc - sc * cb).f();
  out[4] = (cc * ca).f();
  out[5] = (X(cc_cb) * sa + sc * sb).f();
  out[6] = (X(sb) * ca).f();
  out[7] = -sa;
  out[8] = (X(cb) * ca).f();
  out[9] = out[10] = out[11] = 0.0f;
}

// Mech3.exe 1.2 @ 0x00521e10
void set_screen(const float half[2], const float fov[2]) {
  Projection& p = projection();
  p.inv_cot_h = X::step(std::tan(double{(X(fov[0]) * 0.5f).v})).f();
  p.inv_cot_v = X::step(std::tan(double{(X(fov[1]) * 0.5f).v})).f();
  p.half_w = half[0], p.half_h = half[1];
  p.cx = (X(p.origin_x) + p.half_w).f();
  p.cy = (X(p.origin_y) + p.half_h).f();
  p.fx = (X(1.0f) / p.inv_cot_h).f();
  p.fy = (X(1.0f) / p.inv_cot_v).f();
  MatrixStack& st = matrix_stack();
  Mat34& top = st.matrix();
  if (st.is_identity()) {
    const float s[12] = {p.fx, 0, 0, 0, p.fy, 0, 0, 0, 1, 0, 0, 0};
    std::memcpy(top.m, s, sizeof s);
    st.is_identity() = 0;
    return;
  }
  const float* t = top.m;
  const float fx = p.fx, fy = p.fy;
  float r[12];
  r[0] = ((X(t[6]) * 0.0f + X(t[3]) * 0.0f) + X(t[0]) * fx).f();
  r[1] = ((X(t[7]) * 0.0f + X(t[1]) * fx) + X(t[4]) * 0.0f).f();
  r[2] = ((X(fx) * t[2] + X(t[5]) * 0.0f) + X(t[8]) * 0.0f).f();
  r[3] = ((X(t[6]) * 0.0f + X(t[3]) * fy) + X(t[0]) * 0.0f).f();
  r[4] = ((X(t[7]) * 0.0f + X(t[1]) * 0.0f) + X(t[4]) * fy).f();
  r[5] = ((X(t[2]) * 0.0f + X(fy) * t[5]) + X(t[8]) * 0.0f).f();
  r[6] = ((X(t[6]) * 1.0f + X(t[3]) * 0.0f) + X(t[0]) * 0.0f).f();
  r[7] = ((X(t[7]) * 1.0f + X(t[1]) * 0.0f) + X(t[4]) * 0.0f).f();
  r[8] = ((X(t[2]) * 0.0f + X(t[5]) * 0.0f) + X(t[8]) * 1.0f).f();
  r[9] = (((X(t[6]) * 0.0f + X(t[3]) * 0.0f) + X(t[0]) * 0.0f) + t[9]).f();
  r[10] = (((X(t[1]) * 0.0f + X(t[4]) * 0.0f) + X(t[7]) * 0.0f) + t[10]).f();
  r[11] = (((X(t[2]) * 0.0f + t[11]) + X(t[5]) * 0.0f) + X(t[8]) * 0.0f).f();
  std::memcpy(top.m, r, sizeof r);
  st.is_identity() = 0;
}

// Mech3.exe 1.2 @ 0x00522280
float segment_side(const float a[2], const float b[2], const float p[2], int32_t* along, int32_t* beside) {
  float d[2] = {(X(b[0]) - a[0]).f(), 0.0f};
  const X dy = X(b[1]) - a[1];
  d[1] = dy.f();
  float n[2] = {dy.f(), (-X(d[0])).f()};
  normalize2(n);
  const float e[2] = {(X(p[0]) - b[0]).f(), (X(p[1]) - b[1]).f()};
  normalize2(d);
  const float s1 = (X(d[0]) * e[0] + X(d[1]) * e[1]).f();
  *along = s1 >= 0.0f ? 1 : 0;
  d[0] = -d[0], d[1] = -d[1];
  const float g[2] = {(X(p[0]) - a[0]).f(), (X(p[1]) - a[1]).f()};
  const float s2 = (X(d[0]) * g[0] + X(d[1]) * g[1]).f();
  *beside = s2 >= 0.0f ? 1 : 0;
  const float s3 = (X(n[0]) * g[0] + X(n[1]) * g[1]).f();
  return s1 > s3 ? s1 : s3;
}

// Mech3.exe 1.2 @ 0x005223c0
int32_t left_of(const float a[2], const float b[2], const float p[2]) {
  const float u0 = (X(p[0]) - b[0]).f(), u1 = (X(p[1]) - b[1]).f();
  const float n0 = (X(b[1]) - a[1]).f(), n1 = (-(X(b[0]) - a[0])).f();
  const float s = (X(n0) * u0 + X(n1) * u1).f();
  return s >= 0.0f ? 1 : 0;
}

// Mech3.exe 1.2 @ 0x005224b0. The limits are the exe's doubles (-0.005, 0.005).
int32_t triangle_normal(const float p[9], int32_t* facing, int32_t two_sided, float normal[3]) {
  const float d0 = (X(p[0]) - p[3]).f(), d1 = (X(p[1]) - p[4]).f();
  normal[0] = ((X(p[2]) - p[5]) * (X(p[7]) - p[4]) - X(d1) * (X(p[8]) - p[5])).f();
  normal[1] = (X(d0) * (X(p[8]) - p[5]) - (X(p[2]) - p[5]) * (X(p[6]) - p[3])).f();
  normal[2] = (X(d1) * (X(p[6]) - p[3]) - X(d0) * (X(p[7]) - p[4])).f();
  const float s = ((X(p[0]) * normal[0] + X(p[1]) * normal[1]) + X(p[2]) * normal[2]).f();
  if (s < -0.005) {
    *facing = 1;
    return 1;
  }
  if (two_sided && s > 0.005) {
    normal[0] = -normal[0], normal[1] = -normal[1], normal[2] = -normal[2];
    *facing = 0;
    return 1;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00523650. 1/z stays on the x87 for x', stored for y'.
void unproject_to_model(const Vec3* screen, Vec3* out, int32_t count) {
  if (count > 128) return;
  Vec3 cam[128];
  const Projection& p = projection();
  for (int32_t i = 0; i < count; ++i) {
    const X w = X(1.0f) / screen[i].z;
    cam[i].z = w.f();
    cam[i].x = (((X(screen[i].x) - p.cx) * p.inv_fx) * w).f();
    cam[i].y = (((X(screen[i].y) - p.cy) * p.inv_fy) * cam[i].z).f();
  }
  Mat34 m;
  push(&m);
  load_model_view();
  inverse_transform_points_to(cam, out, count);
  pop();
}

// Mech3.exe 1.2 @ 0x00523df0
void rotate_x(float out[3], const float in[3], float angle) {
  const float s = sin_x(angle).f(), c = cos_x(angle).f();
  const float y = in[1], z = in[2];
  out[0] = in[0];
  out[2] = (X(s) * y + X(c) * z).f();
  out[1] = (X(c) * y - X(s) * z).f();
}

// Mech3.exe 1.2 @ 0x00523ef0
void rotate_z_typo(float out[3], const float in[3], float angle) {
  const float s = sin_x(angle).f(), c = cos_x(angle).f();
  const float x = in[0], y = in[1], z = in[2];
  out[1] = ((X(x) + y) * c).f();
  out[0] = (X(x) * c - X(s) * y).f();
  out[2] = z;
}

// Mech3.exe 1.2 @ 0x00524f80
void quat_add(const float a[4], const float b[4], float out[4]) {
  out[0] = a[0] + b[0], out[1] = a[1] + b[1], out[2] = a[2] + b[2], out[3] = a[3] + b[3];
}
// Mech3.exe 1.2 @ 0x00524fd0
void quat_negate(const float a[4], float out[4]) { out[0] = -a[0], out[1] = -a[1], out[2] = -a[2], out[3] = -a[3]; }
// Mech3.exe 1.2 @ 0x00525020
void quat_sub(const float a[4], const float b[4], float out[4]) {
  out[0] = a[0] - b[0], out[1] = a[1] - b[1], out[2] = a[2] - b[2], out[3] = a[3] - b[3];
}
// Mech3.exe 1.2 @ 0x005250a0: the vector part's sum stored, w's product added on the x87.
float quat_dot(const float a[4], const float b[4]) {
  const float v = ((X(a[1]) * b[1] + X(a[2]) * b[2]) + X(a[3]) * b[3]).f();
  return (X(b[0]) * a[0] + v).f();
}
// Mech3.exe 1.2 @ 0x005250f0
float quat_norm_sq(const float a[4]) {
  const float v = ((X(a[1]) * a[1] + X(a[2]) * a[2]) + X(a[3]) * a[3]).f();
  return (X(a[0]) * a[0] + v).f();
}

// Mech3.exe 1.2 @ 0x00525fa0. The offsets truncated to whole units (_ftol); the pseudo-angle compared as the
// x87 holds it, stored when taken.
int32_t hull_xz(float points[][3], int32_t count) {
  int32_t start = 0;
  for (int32_t i = 1; i < count; ++i)
    if ((X(points[i][2]) - points[start][2]).v > 0.1) start = i;
  std::memcpy(points[count], points[start], 12);
  if (count < 1) return 0;
  int32_t next = start;
  float last = 0.0f;
  for (int32_t k = 0;; ) {
    float tmp[3];
    std::memcpy(tmp, points[k], 12);
    std::memcpy(points[k], points[next], 12);
    std::memcpy(points[next], tmp, 12);
    const float prev = last;
    float best = 6.2831855f;
    next = count;
    const float* cur = points[k];
    for (int32_t j = k + 1; j <= count; ++j) {
      const float* q = points[j];
      const auto dx = static_cast<int32_t>(std::trunc((X(q[0]) - cur[0]).v));
      const auto dz = static_cast<int32_t>(std::trunc((X(cur[2]) - q[2]).v));
      const int32_t sum = std::abs(dx) + std::abs(dz);
      X t = sum == 0 ? X(0.0f) : X::step(static_cast<double>(dz) / sum);
      if (dx < 0) t = X(2.0f) - t;
      else if (dz < 0) t = t - X(-4.0f);
      if (!(t.v > prev)) continue;
      if (t.v < best) {
        best = t.f(), next = j;
      } else if (t.v == best) {
        const float* c = points[next];
        const float jx = (X(q[0]) - cur[0]).f(), jz = (X(q[2]) - cur[2]).f();
        const float cx = (X(c[0]) - cur[0]).f(), cz = (X(c[2]) - cur[2]).f();
        const float dj = ((X(jz) * jz + X(jx) * jx) + 0.0f).f();
        const float dc = ((X(cz) * cz + X(cx) * cx) + 0.0f).f();
        if (dj < dc) next = j;
      }
    }
    last = best;
    if (next == count) return k + 1;
    if (++k >= count) return 0;
  }
}

}  // namespace zmth
