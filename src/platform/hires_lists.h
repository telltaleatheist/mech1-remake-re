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
//
// What's under what (B1 piece 3): when a draw lands on the screen, the canvas's pixels in its rect are kept with it,
// as they were before its blit and after. At a pixel the draws over it form a chain while each one's "before" is the
// last one's "after" (nothing unrecorded was written between them) and the canvas still holds the top one's "after"
// (nothing since). The chain's draws show there, from its base up; below the base, and wherever the canvas changed
// after the top one, the canvas shows. So anything written after a draw, by a blit, a line, a ripple, a blur or the
// engine's own clear, wins where it wrote, with no writer hooked; a draw without its snapshots shows nowhere.
// layers() makes the frame's map: each pixel's chain base.
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

// kLine: mw1 (engine, ours), FIXES 89: a 2D line, line list or circle the canvas drew (the mission radar's), drawn
// again anti-aliased at the frame's scale (hires_lines.h); FIXES 90: a dashed line, a filled polygon too.
enum class Kind : uint8_t { kText, kPicture, kLine };

struct Command {
  Kind kind = Kind::kText;
  Rect rect;              // the part that shows, in the owner's pixels (clipped)
  int32_t x = 0, y = 0;   // text: the line's pen start and top; picture: where the image's (0, 0) lands
  std::string text;       // text: the line (UTF-8)
  FontKey font;
  uint32_t colour = 0;    // text: GDI's 0x00bbggrr
  bool box = false;       // text: OPAQUE, its cells (rect) filled with box_colour first
  uint32_t box_colour = 0;  // GDI's 0x00bbggrr
  std::string name;       // picture: the image's name (image_get's)
  bool alpha = false;     // picture: the image blends (a key or an alpha plane)
  // line: the points (x, y pairs, canvas pixels, from (x, y)): a line's two, a list's joined in order; a circle's
  // centre with its radius (> 0). Its colour the canvas's own pixel (16 bits, the canvas's format).
  std::vector<int32_t> pts;
  int32_t radius = 0;
  bool segments = false;  // FIXES 90: pts in fours, each its own segment (a dashed line's lit runs)
  std::vector<float> poly;  // FIXES 90: a filled polygon's vertices (x, y pairs, canvas pixels from (x, y)), not pts
  uint64_t seq = 0;       // the order it was drawn in, over all owners
  uint64_t origin = 0;    // the draw it copies (its first seq): a grab's copies and a blit's keep it
  std::vector<uint16_t> before, after;  // on the screen: the canvas's pixels in rect before its blit and after
};

class Lists {
 public:
  // At most `cap` draws an owner, and the screen's snapshots at most `snapshot_cap` bytes (a full-screen backdrop's
  // two are 1.6 MB at 853x480): past either, the canvas alone until drawn afresh.
  explicit Lists(size_t cap = 512, size_t snapshot_cap = 24u << 20) : cap_(cap), snapshot_cap_(snapshot_cap) {}

  void clear(const void* owner);  // drawn afresh, or freed: nothing recorded, no longer fallen
  // A text line drawn into `owner`, its rect already clipped to the owner and the DC's rect.
  void text(const void* owner, Command c);
  // mw1 (engine, ours), FIXES 89: a line drawn into `owner` (its rect: its pixels' bounds, a pixel round, clipped).
  // Returns how many draws it added (0 or 1), for snapshot().
  size_t line(const void* owner, Command c);
  // `src` (w x h pixels; all of it or `part`) drawn at (x, y) on `dst` (dst_w x dst_h). `name`: a named image's;
  // `opaque`: every pixel drawn written (no key, no alpha plane).
  // Returns how many draws it added to dst (its list's last ones), for snapshot().
  size_t blit(const void* src, int32_t w, int32_t h, const Rect* part, const void* dst, int32_t dst_w, int32_t dst_h,
              int32_t x, int32_t y, bool opaque, const std::string& name);
  // The last `count` draws of `screen`'s list keep `canvas`'s pixels (pitch in pixels) in their rects: before the
  // blit's pixels are drawn (after = false), then after.
  void snapshot(const void* screen, size_t count, const uint16_t* canvas, int32_t pitch_px, bool after);
  // The frame's map, for each canvas pixel (w x h): 0, the canvas shows (no draw there, or its chain broken at
  // the top); else 1 + the index in screen's list of its chain's base. Draw i shows at p when it covers p and
  // base[p] is 1..i+1. Where draw i is the base and its pixels differ before and after, its "before" is what was
  // under it (the canvas's, to draw first so the canvas's own copy of it doesn't show through).
  void layers(const void* screen, const uint16_t* canvas, int32_t w, int32_t h, int32_t pitch_px,
              std::vector<uint16_t>* base) const;
  // `r`'s part of `screen` copied into `into` at (0, 0).
  void grab(const void* screen, const Rect& r, const void* into);

  const std::vector<Command>* of(const void* owner) const;
  size_t size(const void* owner) const;
  bool fallen(const void* owner) const;
  size_t owners() const { return lists_.size(); }
  size_t snapshot_bytes(const void* owner) const;  // its draws' before and after, in bytes
  size_t cap() const { return cap_; }

 private:
  void append(const void* owner, Command c);
  size_t cap_, snapshot_cap_;
  uint64_t seq_ = 0;
  std::unordered_map<const void*, std::vector<Command>> lists_;
  std::unordered_set<const void*> fallen_;
  mutable std::vector<uint16_t> top_, last_;  // layers()'s, kept from frame to frame
};

}  // namespace platform::hires
