#include "platform/hires_lists.h"

#include <algorithm>

namespace platform::hires {

Rect intersect(const Rect& a, const Rect& b) {
  Rect r{std::max(a.l, b.l), std::max(a.t, b.t), std::min(a.r, b.r), std::min(a.b, b.b)};
  if (r.empty()) return Rect{};
  return r;
}

bool contains(const Rect& outer, const Rect& inner) {
  return inner.empty() || (inner.l >= outer.l && inner.t >= outer.t && inner.r <= outer.r && inner.b <= outer.b);
}

void Lists::clear(const void* owner) {
  lists_.erase(owner);
  fallen_.erase(owner);
}

void Lists::append(const void* owner, Command c) {
  if (c.rect.empty() || fallen_.count(owner)) return;
  std::vector<Command>& l = lists_[owner];
  if (l.size() >= cap_) {  // the safe failure: the canvas alone, blocky but whole
    lists_.erase(owner);
    fallen_.insert(owner);
    return;
  }
  c.seq = ++seq_;
  if (!c.origin) c.origin = c.seq;
  l.push_back(std::move(c));
}

void Lists::text(const void* owner, Command c) {
  c.kind = Kind::kText;
  append(owner, std::move(c));
}

size_t Lists::blit(const void* src, int32_t w, int32_t h, const Rect* part, const void* dst, int32_t dst_w, int32_t dst_h,
                   int32_t x, int32_t y, bool opaque, const std::string& name) {
  Rect s = part ? intersect(*part, Rect{0, 0, w, h}) : Rect{0, 0, w, h};
  if (s.empty()) return 0;
  // Where the part lands, clipped to the destination; (dx, dy) takes the source's pixels to the destination's.
  const int32_t dx = x - s.l, dy = y - s.t;
  const Rect landed = intersect(Rect{s.l + dx, s.t + dy, s.r + dx, s.b + dy}, Rect{0, 0, dst_w, dst_h});
  if (landed.empty()) return 0;
  if (opaque) {
    if (landed == Rect{0, 0, dst_w, dst_h}) {
      clear(dst);  // drawn over whole: a fresh start, fallen or not
    } else if (auto it = lists_.find(dst); it != lists_.end()) {
      std::vector<Command>& l = it->second;
      l.erase(std::remove_if(l.begin(), l.end(), [&](const Command& c) { return contains(landed, c.rect); }), l.end());
    }
  }
  const size_t before = size(dst);
  if (!name.empty()) {
    Command p;
    p.kind = Kind::kPicture;
    p.rect = landed;
    p.x = dx, p.y = dy;
    p.name = name;
    p.alpha = !opaque;
    append(dst, std::move(p));
  }
  auto it = src == dst ? lists_.end() : lists_.find(src);  // a buffer onto itself: what it carries is there already
  if (it != lists_.end()) {
    const std::vector<Command> carried = it->second;  // a copy: appending may move the source's vector
    for (Command c : carried) {
      c.rect = intersect(Rect{c.rect.l + dx, c.rect.t + dy, c.rect.r + dx, c.rect.b + dy}, landed);
      if (c.rect.empty()) continue;
      c.x += dx, c.y += dy;
      // A grab put back where it was taken (the cursor's save, a popup's): the draw it copies still on the screen, in
      // place and over this part, shows again by its own chain as the pixels come back; another copy would only add
      // to the list, every move (the cursor's ran it past its cap in under 100 moves). B4's read-backs.
      const auto there = lists_.find(dst);
      if (there != lists_.end() &&
          std::any_of(there->second.begin(), there->second.end(), [&](const Command& e) {
            return e.origin == c.origin && e.x == c.x && e.y == c.y && !e.after.empty() && contains(e.rect, c.rect);
          }))
        continue;
      c.before.clear(), c.after.clear();
      append(dst, std::move(c));
    }
  }
  const size_t after = size(dst);
  return after > before ? after - before : 0;
}

void Lists::snapshot(const void* screen, size_t count, const uint16_t* canvas, int32_t pitch_px, bool after) {
  auto it = lists_.find(screen);
  if (it == lists_.end() || !canvas) return;
  std::vector<Command>& l = it->second;
  for (size_t i = l.size() - std::min(count, l.size()); i < l.size(); ++i) {
    Command& c = l[i];
    std::vector<uint16_t>& to = after ? c.after : c.before;
    const int32_t w = c.rect.r - c.rect.l;
    to.resize(static_cast<size_t>(w) * static_cast<size_t>(c.rect.b - c.rect.t));
    for (int32_t y = c.rect.t; y < c.rect.b; ++y)
      std::copy(canvas + static_cast<size_t>(y) * pitch_px + c.rect.l, canvas + static_cast<size_t>(y) * pitch_px + c.rect.r,
                to.begin() + static_cast<size_t>(y - c.rect.t) * w);
  }
  if (snapshot_bytes(screen) > snapshot_cap_) {  // the safe failure, as past the draws' cap
    lists_.erase(screen);
    fallen_.insert(screen);
  }
}

size_t Lists::snapshot_bytes(const void* owner) const {
  auto it = lists_.find(owner);
  if (it == lists_.end()) return 0;
  size_t n = 0;
  for (const Command& c : it->second) n += (c.before.size() + c.after.size()) * sizeof(uint16_t);
  return n;
}

void Lists::layers(const void* screen, const uint16_t* canvas, int32_t w, int32_t h, int32_t pitch_px,
                   std::vector<uint16_t>* base) const {
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  base->assign(n, 0);
  auto it = lists_.find(screen);
  if (it == lists_.end() || !canvas) return;
  const std::vector<Command>& l = it->second;
  // Each pixel's top draw (1 + index) and the canvas it left there; kept, no allocation a frame.
  std::vector<uint16_t>& top = top_;
  std::vector<uint16_t>& last = last_;
  top.assign(n, 0), last.assign(n, 0);
  for (size_t i = 0; i < l.size() && i < 0xffff; ++i) {
    const Command& c = l[i];
    const int32_t cw = c.rect.r - c.rect.l;
    const size_t want = static_cast<size_t>(cw) * static_cast<size_t>(c.rect.b - c.rect.t);
    const Rect r = intersect(c.rect, Rect{0, 0, w, h});
    if (r.empty()) continue;
    const bool whole = c.before.size() == want && c.after.size() == want;
    const uint16_t id = static_cast<uint16_t>(i + 1);
    for (int32_t y = r.t; y < r.b; ++y)
      for (int32_t x = r.l; x < r.r; ++x) {
        const size_t p = static_cast<size_t>(y) * w + x;
        if (!whole) {  // no snapshots: it shows nowhere, and nothing under it either
          top[p] = 0, (*base)[p] = 0;
          continue;
        }
        const size_t k = static_cast<size_t>(y - c.rect.t) * cw + (x - c.rect.l);
        if (!top[p] || c.before[k] != last[p]) (*base)[p] = id;  // a chain starts here: something unrecorded under it
        top[p] = id, last[p] = c.after[k];
      }
  }
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const size_t p = static_cast<size_t>(y) * w + x;
      if (!top[p] || canvas[static_cast<size_t>(y) * pitch_px + x] != last[p]) (*base)[p] = 0;  // written over since
    }
}

void Lists::grab(const void* screen, const Rect& r, const void* into) {
  clear(into);
  if (r.empty()) return;
  if (fallen_.count(screen)) {
    fallen_.insert(into);
    return;
  }
  auto it = lists_.find(screen);
  if (it == lists_.end()) return;
  const std::vector<Command> from = it->second;
  for (Command c : from) {
    c.rect = intersect(c.rect, r);
    if (c.rect.empty()) continue;
    c.rect = Rect{c.rect.l - r.l, c.rect.t - r.t, c.rect.r - r.l, c.rect.b - r.t};
    c.x -= r.l, c.y -= r.t;
    c.before.clear(), c.after.clear();
    append(into, std::move(c));
  }
}

const std::vector<Command>* Lists::of(const void* owner) const {
  auto it = lists_.find(owner);
  return it == lists_.end() ? nullptr : &it->second;
}

size_t Lists::size(const void* owner) const {
  auto it = lists_.find(owner);
  return it == lists_.end() ? 0 : it->second.size();
}

bool Lists::fallen(const void* owner) const { return fallen_.count(owner) != 0; }

}  // namespace platform::hires
