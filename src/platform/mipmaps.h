#pragma once
#include <cstdint>
#include <vector>

// Ours (docs/render.md, Mipmaps): the levels below a texture, for the GPU path. The original's Direct3D textures had
// one level, so a far wall or the ground shimmered as its texels went under a pixel; these filter it away. Each
// level halves the last (a dimension at 1 stays 1), each texel the average of the ones it covers, its colour
// weighted by their alpha so a clear texel (a colour key's black) never darkens an edge. A hard cut-out (every
// alpha 0 or 255, with some 0: colour keys, fences) stays cut out on every level, with as many solid texels in
// proportion as the full size has; a soft-edged one (mostly 0 or 255, under a quarter between: the trees' alpha
// planes) has each level's alpha scaled so the same share is at least half as at full size, those made solid. So neither fades
// nor thickens with distance. Any other alpha (smoke, glass) averages.
namespace platform {

struct MipLevel {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // RGBA8, straight alpha
};

// Levels 1.. of a width x height RGBA8 image, to 1 x 1 (none for a 1 x 1 image).
std::vector<MipLevel> mip_chain(int width, int height, const uint8_t* rgba);

}  // namespace platform
