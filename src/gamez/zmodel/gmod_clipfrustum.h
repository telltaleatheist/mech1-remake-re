#pragma once
#include <cstdint>

#include "gamez/zmodel/gmod_clip3d.h"

// Four whole-frustum polygon clippers in camera space that nothing in the release calls or tables (docs/render.md,
// The unused frustum clippers): the near plane first, then the four sides, as Sutherland-Hodgman passes over a
// polygon whose x and y are first scaled by the projection's cotangents (zmth::scale_by_cot) so the sides are
// x = +-z and y = +-z, and scaled back at the end. The far plane only drops a polygon wholly beyond it. Each takes
// the clip struct (ECX, ClipPlanes, 0x007402f8 in the exe) and the positions (EDX); the larger ones carry texture
// coordinates (2 floats) and colours (3 floats) along, and copy the first three corners (as clipped by the near
// plane, scaled back) into `tri` when given. 0x00530d90's texture coordinates are cut by the near plane only; then 9
// floats of them go back (four and a half pairs) and the side passes drop them. Preserved, unwired. Original file unknown: between gmod_init.c and
// gmod_matl.c (0x00530d90..0x00535bff).
namespace zmod {

int32_t clip_frustum(const ClipPlanes& c, float (*pos)[3], int32_t* count);                  // 0x00534bf0
int32_t clip_frustum_tri(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], int32_t* count);  // 0x00530d90
int32_t clip_frustum_uv(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], int32_t* count);  // 0x00531d90
int32_t clip_frustum_uv_rgb(const ClipPlanes& c, float (*pos)[3], float* tri, float (*uv)[2], float (*rgb)[3],
                            int32_t* count);  // 0x005331e0

}  // namespace zmod
