#pragma once

// C:\zipper\branchep1\gamez\zMath: Pirate's Moon's own quaternion helpers (MW3's: gamez/zmath/zmth_vector,
// zmth_matrix). q = (w, x, y, z).
namespace pm::zmth {

// Pirate's Moon 1.0 @ 0x0053f020 (cdecl a, b, out): b's conjugate times a, as MW3's 0x00525690 with PM's order of
// the sums (out[0] a0 b0 + a2 b2 + a1 b1 + a3 b3).
void quat_relative(const float a[4], const float b[4], float out[4]);
// Pirate's Moon 1.0 @ 0x0053f310: MW3's quat_log (0x005259c0) with the axis length's square root taken by the float
// bit trick ((bits >> 1) + 0x1fc00000, within about 6% of the root), in floats.
void quat_log(const float q[4], float out[3]);

}  // namespace pm::zmth
