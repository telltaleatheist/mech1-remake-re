#include "mech3/editor_conform.h"

#include <cfloat>
#include <cmath>

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

// Mech3.exe 1.2 @ 0x00403ec0 (its two statics, [0x005f4c64] -FLT_MAX and [0x005f4c58] FLT_MAX; 0.5 at 0x005e02f8,
// 0.1 at 0x005e02fc), Pirate's Moon 1.0 @ 0x00403f30 (its hits 0x2c bytes apart)
bool conform_element(ConformHooks& h, void* e) {
  const float* p = e ? h.position(e) : nullptr;
  if (!p) return true;
  float pos[3] = {p[0], p[1], p[2]};
  std::vector<float> hits;
  if (!h.ground(pos[0], pos[2], &hits)) return true;
  float below = -FLT_MAX, above = FLT_MAX;
  for (float y : hits) {
    if (below < y && y <= pos[1]) below = y;
    if (pos[1] <= y && y < above) above = y;
  }
  float to;
  if (-FLT_MAX < below) {
    to = below + 0.5f;
    if (above - 0.5f < to) to = (above + below) * 0.5f;
  } else {
    to = FLT_MAX - 0.5f <= above ? pos[1] : above + 0.5f;
  }
  if (!(std::fabs(pos[1] - to) > 0.1f)) return true;
  pos[1] = to;
  if (!h.set_position(e, pos)) return true;
  h.moved(e);
  h.modified();
  return true;
}

// Mech3.exe 1.2 @ 0x00404090, Pirate's Moon 1.0 @ 0x00404100
// Mech3.exe 1.2 @ 0x0049d140, Pirate's Moon 1.0 @ 0x004a0530: template 152's button, the dialog's +0x64 the edit
void conform_all(ConformHooks& h) {
  for (int32_t i = h.count() - 1; i >= 0; --i)
    if (void* e = h.at(i)) conform_element(h, e);
}

}  // namespace mech3
