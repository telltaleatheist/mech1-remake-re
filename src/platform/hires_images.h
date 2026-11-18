#pragma once
// Ours (MW1's 2D at the window's resolution, mw1/research/hires-2d.md, "The interface between B1 and B2"): the
// source-size pictures behind canvas images. The 2D host records a blit of a canvas image (by the name it was fetched
// under) and, at the frame's scale, draws the picture this names in its place. Set by MW1's program only; MW3 and PM
// never set it, so hires_images() is null for them and every image draws from the canvas as before.
#include <cstdint>

namespace platform {

struct HiresImage {
  int32_t w = 0, h = 0;                // the source's size
  const uint8_t* rgba = nullptr;       // w * h * 4: R, G, B, A bytes (SDL_PIXELFORMAT_RGBA32), straight alpha, top row first
  uint64_t serial = 0;                 // unique to this decode, never reused: the key for the renderer's texture
  int32_t canvas_w = 0, canvas_h = 0;  // the canvas image's size: a canvas pixel is w / canvas_w by h / canvas_h source pixels
};

class HiresImages {
 public:
  virtual ~HiresImages() = default;
  // The source behind canvas image `name`, or null (it draws from the canvas). The pointer and its pixels stay valid
  // until the next end_frame(). The main thread only. A source not yet decoded is decoded here (a few ms) and logged.
  virtual const HiresImage* find(const char* name) = 0;
  // Called by the host once after each present, after the frame's pictures are uploaded: entries no longer wanted may
  // be dropped from here on.
  virtual void end_frame() {}
};

void set_hires_images(HiresImages* images);  // null to clear
HiresImages* hires_images();

}  // namespace platform
