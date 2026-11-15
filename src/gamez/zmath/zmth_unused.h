#pragma once
#include <cstdint>

#include "gamez/zmath/zmth_matrix.h"

// zMath routines nothing in the game calls, ported for preservation (docs/port-audit.md, S13): the maths module
// (0x51b170..0x526920, zmth_matrix.h) carries them beside the ones the engine uses. Not wired in. Their tests run
// them against the exe (test_zmath_unused.py).
namespace zmth {

// ECX a, EDX b, stack out: a x b, made unit length by 0x004020d0 (normalize_x87).
void cross_unit(const float a[3], const float b[3], float out[3]);  // 0x0051e160
// The top of the stack set to the camera space of a place and angles (pitch, yaw, roll): (1, -1, -1) times the
// rotation turned back, the place's offset, then the camera's world matrix (0x005214f0). ECX angles, EDX place.
void view_from_angles(const float angles[3], const float place[3]);  // 0x00520c60
// The same written into view().view (pushed onto the stack and popped round it).
void set_view_from_angles(const float angles[3], const float place[3]);  // 0x00521080
// view().camera from angles and a place: euler's rotation (0x00521670) with its y and z rows negated.
void camera_from_angles(const float angles[3], const float place[3]);  // 0x005214f0
// ECX out (3x4): euler's rotation of three angles (0x00521670's words), no translation.
void rotation_matrix(float out[12], float a, float b, float c);  // 0x00521a90
// ECX half size, EDX fov (h, v): the projection's tangents, half size and centre and pixels per unit, then the
// top multiplied by (fx, fy, 1) (or set to it on an identity top).
void set_screen(const float half[2], const float fov[2]);  // 0x00521e10
// ECX a, EDX b, stack p, along, beside: p against the segment a b in the xy plane. *along: p at or past b
// (along a to b); *beside: p at or behind a. Returns the larger of p's distance past b and its distance off the
// line (the left side positive).
float segment_side(const float a[2], const float b[2], const float p[2], int32_t* along, int32_t* beside);  // 0x00522280
// ECX a, EDX b, stack p: whether p is on b's left of the line a b (in the xy plane), or on it.
int32_t left_of(const float a[2], const float b[2], const float p[2]);  // 0x005223c0
// ECX three points, EDX *facing, stack two_sided, normal: the plane's normal (p0 - p1) x (p2 - p1) and its dot
// with p0 (the origin's side): below -0.005, *facing 1; above 0.005 with two_sided, the normal turned round and
// *facing 0. Returns 0 (normal written, *facing untouched) when within 0.005, or above it one-sided.
int32_t triangle_normal(const float p[9], int32_t* facing, int32_t two_sided, float normal[3]);  // 0x005224b0
// ECX screen points, EDX out, stack count (up to 128): screen to camera space (0x00523590's way), then through
// the inverse of the top below's matrix times the view (0x00522910): screen points back into a model's space.
void unproject_to_model(const Vec3* screen, Vec3* out, int32_t count);  // 0x00523650
// ECX out, EDX in (may be the same): in turned about x by angle.
void rotate_x(float out[3], const float in[3], float angle);  // 0x00523df0
// ECX out, EDX in: about z, but out.y is (x + y) cos, not x sin + y cos (a typo in the original; nothing calls
// it). docs/render-matrix.md, Unused routines.
void rotate_z_typo(float out[3], const float in[3], float angle);  // 0x00523ef0
// Quaternions (w, x, y, z): ECX a, EDX b.
void quat_add(const float a[4], const float b[4], float out[4]);  // 0x00524f80
void quat_negate(const float a[4], float out[4]);                 // 0x00524fd0
void quat_sub(const float a[4], const float b[4], float out[4]);  // 0x00525020
float quat_dot(const float a[4], const float b[4]);               // 0x005250a0
float quat_norm_sq(const float a[4]);                             // 0x005250f0
// ECX points, EDX count: the points' convex hull in the xz plane by gift wrapping, from the one furthest up in z
// (a later one only when over 0.1 past it), each step the smallest pseudo-angle past the last of the whole-unit
// offsets (0..4 round the turn; a tie the nearer); the hull's points swapped to the front in order. The array
// needs a slot past the count (the start is copied there). Returns the hull's count, or 0 when it never closes.
int32_t hull_xz(float points[][3], int32_t count);  // 0x00525fa0

}  // namespace zmth
