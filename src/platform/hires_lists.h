#pragma once
// Ours (MW1's FIXES.md item 12, mw1/research/hires-2d.md B0 and B1): the 2D's recorded draws, kept with the pixels
// they were drawn into, so they survive every copy the 2D makes of itself. Pure bookkeeping, no SDL: the SDL host
// (sdl_twod_host.cpp) calls it from its DC text, its blit and its grabs while platform::hires_2d() is on; the draw
// itself is elsewhere.
//
// An owner is a pixel buffer: an image's pixels, or the 2D target's (the screen). Its commands are in its own pixels.
// - text: a DC's line drawn into an image;
// - blit: the source's commands come along, offset and clipped to the part drawn, and a named image is itself a
//   picture command; an opaque blit drops the destination's commands it covers whole;
// - grab: the commands in the grabbed part are copied into the grab;
// - past `cap` commands an owner falls back to the canvas only (nothing recorded, the canvas drawn as ever: blocky,
//   never missing) until it is cleared or an opaque blit covers it whole.
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace platform::hires {

struct Rect {
  int32_t l = 0, t = 0, r = 0, b = 0;
  bool empty() const { return r <= l || b <= t; }
  bool operator==(const Rect& o) const { return l == o.l && t == o.t && r == o.r && b == o.b; }
};
Rect intersect(const Rect& a, const Rect& b);
bool contains(const Rect& outer, const Rect& inner);  // inner empty, or wholly inside outer

// A font as the draw needs it, by value (the host's font may be deleted before the frame is drawn).
struct FontKey {
  std::string path;   // the .ttf
  int em = 0;         // its canvas size, pixels
  int weight = 400;
  bool italic = false;
  int tracking = 0;   // pixels after each glyph but the last
  int shift = 0;      // the baseline's move down (GDI's ascent less SDL_ttf's)
  bool operator==(const FontKey& o) const {
    return path == o.path && em == o.em && weight == o.weight && italic == o.italic && tracking == o.tracking && shift == o.shift;
  }
};

enum class Kind : uint8_t { kText, kPicture };

struct Command {
  Kind kind = Kind::kText;
  Rect rect;              // the part that shows, in the owner's pixels (clipped)
  int32_t x = 0, y = 0;   // text: the line's pen start and top; picture: where the image's (0, 0) lands
  std::string text;       // text: the line (UTF-8)
  FontKey font;
  uint32_t colour = 0;    // text: GDI's 0x00bbggrr
  std::string name;       // picture: the image's name (image_get's)
  bool alpha = false;     // picture: the image blends (a key or an alpha plane)
  uint64_t seq = 0;       // the order it was drawn in, over all owners (the generation mask's)
};

class Lists {
 public:
  explicit Lists(size_t cap = 512) : cap_(cap) {}

  void clear(const void* owner);  // drawn afresh, or freed: nothing recorded, no longer fallen
  // A text line drawn into `owner`, its rect already clipped to the owner and the DC's rect.
  void text(const void* owner, Command c);
  // `src` (w x h pixels; all of it or `part`) drawn at (x, y) on `dst` (dst_w x dst_h). `name`: a named image's;
  // `opaque`: every pixel drawn written (no key, no alpha plane).
  void blit(const void* src, int32_t w, int32_t h, const Rect* part, const void* dst, int32_t dst_w, int32_t dst_h,
            int32_t x, int32_t y, bool opaque, const std::string& name);
  // `r`'s part of `screen` copied into `into` at (0, 0).
  void grab(const void* screen, const Rect& r, const void* into);

  const std::vector<Command>* of(const void* owner) const;
  size_t size(const void* owner) const;
  bool fallen(const void* owner) const;
  size_t owners() const { return lists_.size(); }
  size_t cap() const { return cap_; }

 private:
  void append(const void* owner, Command c);
  size_t cap_;
  uint64_t seq_ = 0;
  std::unordered_map<const void*, std::vector<Command>> lists_;
  std::unordered_set<const void*> fallen_;
};

}  // namespace platform::hires
