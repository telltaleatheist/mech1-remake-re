// Probes for test_clipfrustum.py: the unused frustum clippers (gamez/zmodel/gmod_clipfrustum).
#include <cstdint>
#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_clipfrustum.h"

extern "C" {

// which: 0 0x534bf0, 1 0x530d90, 2 0x531d90, 3 0x5331e0. clip: the 0x24 bytes at 0x7402f8; cot: cot_h, cot_v and
// their inverses ([0x63d750], [0x63d754], [0x63d740], [0x63d744]). The arrays are read and written back; tri null
// for none. Returns EAX.
int32_t probe_clipfrustum(int32_t which, const uint8_t* clip, const float* cot, float* pos, float* tri, float* uv,
                          float* rgb, int32_t* count) {
  zmod::ClipPlanes c;
  std::memcpy(&c, clip, sizeof c);
  zmth::Projection& p = zmth::projection();
  const zmth::Projection keep = p;
  p.cot_h = cot[0], p.cot_v = cot[1], p.inv_cot_h = cot[2], p.inv_cot_v = cot[3];
  auto* P = reinterpret_cast<float(*)[3]>(pos);
  int32_t r = 0;
  switch (which) {
    case 0: r = zmod::clip_frustum(c, P, count); break;
    case 1: r = zmod::clip_frustum_tri(c, P, tri, reinterpret_cast<float(*)[2]>(uv), count); break;
    case 2: r = zmod::clip_frustum_uv(c, P, tri, reinterpret_cast<float(*)[2]>(uv), count); break;
    case 3:
      r = zmod::clip_frustum_uv_rgb(c, P, tri, reinterpret_cast<float(*)[2]>(uv), reinterpret_cast<float(*)[3]>(rgb),
                                    count);
      break;
  }
  p = keep;
  return r;
}

}  // extern "C"
