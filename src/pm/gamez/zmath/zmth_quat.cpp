#include "pm/gamez/zmath/zmth_quat.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace pm::zmth {

// Pirate's Moon 1.0 @ 0x0053f020
void quat_relative(const float a[4], const float b[4], float out[4]) {
  out[0] = ((a[0] * b[0] + a[2] * b[2]) + a[1] * b[1]) + b[3] * a[3];
  out[1] = ((a[1] * b[0] - a[0] * b[1]) - a[2] * b[3]) + a[3] * b[2];
  out[2] = ((a[2] * b[0] - a[0] * b[2]) - a[3] * b[1]) + a[1] * b[3];
  out[3] = ((b[0] * a[3] - a[0] * b[3]) - a[1] * b[2]) + a[2] * b[1];
}

// Pirate's Moon 1.0 @ 0x0053f310
void quat_log(const float q[4], float out[3]) {
  const float n2 = (q[1] * q[1] + q[2] * q[2]) + q[3] * q[3];
  if (n2 == 0.0f) {
    out[0] = out[1] = out[2] = 0.0f;
    return;
  }
  int32_t bits;
  std::memcpy(&bits, &n2, 4);
  bits = (bits >> 1) + 0x1fc00000;
  float r;
  std::memcpy(&r, &bits, 4);
  // The division and products rounded to floats as the 24 bits of play give; fpatan's angle is not (precision
  // control leaves the transcendentals alone).
  const double angle = q[0] < 0.0f ? -std::atan2(double{r}, -static_cast<double>(q[0]))
                                   : std::atan2(double{r}, static_cast<double>(q[0]));
  const float a = static_cast<float>(angle / r);
  out[0] = a * q[1];
  out[1] = a * q[2];
  out[2] = a * q[3];
}

}  // namespace pm::zmth
