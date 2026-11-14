#pragma once
#include <cstdint>

#include "gamez/zmath/zmth_matrix.h"

// A world point on the screen (docs/hud.md section 14). Original file
// unknown: 0x0053cf10 sits between gmod_const.c and gmod_cookie.cpp;
// named after what it does.
namespace zmod {

// Mech3.exe 1.2 @ 0x0053cf10: `world` through the view (identity: as it is)
// and projected; the point clamped to the point rect (0x0074031c), the
// outcode its sides: 1 left, 2 right, 4 top (above), 8 bottom. Behind the
// near plane: 8, y at the bottom and x spread across the screen from
// -5000..5000. FIX (bug): there the original ORs in the low bits of the
// input point's address (an unset local with the identity), returned as 8.
uint32_t project_point(const zmth::Vec3* world, zmth::Vec3* out);

// The unreferenced ones beside it (docs/render.md, The unused zModel helpers; preserved, unwired):
// 0x0053d130 is project_point as the exe wrote it again without the unset outcode (project_point is it).
// 0x0053d360 ECX world, EDX out, n: n points through the view and projected, not clamped; the outcodes ORed (1, 2,
// 4, 8 for left, right, top, bottom, 8 too behind the near plane, put across the screen's top).
uint32_t project_points(const zmth::Vec3* world, zmth::Vec3* out, int32_t n);
// 0x0053de50 ECX v: the matrix under the top moved by v in its own frame (its rotation times v added to its place).
void translate_below(const zmth::Vec3* v);
// 0x0053df00 ECX v: the matrix under the top and the top moved back by v, each in its own frame.
void untranslate_two(const zmth::Vec3* v);

}  // namespace zmod
