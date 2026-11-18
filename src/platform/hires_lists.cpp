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
  l.push_back(std::move(c));
}

void Lists::text(const void* owner, Command c) {
  c.kind = Kind::kText;
  append(owner, std::move(c));
}

void Lists::blit(const void* src, int32_t w, int32_t h, const Rect* part, const void* dst, int32_t dst_w, int32_t dst_h,
                 int32_t x, int32_t y, bool opaque, const std::string& name) {
  Rect s = part ? intersect(*part, Rect{0, 0, w, h}) : Rect{0, 0, w, h};
  if (s.empty()) return;
  // Where the part lands, clipped to the destination; (dx, dy) takes the source's pixels to the destination's.
  const int32_t dx = x - s.l, dy = y - s.t;
  const Rect landed = intersect(Rect{s.l + dx, s.t + dy, s.r + dx, s.b + dy}, Rect{0, 0, dst_w, dst_h});
  if (landed.empty()) return;
  if (opaque) {
    if (landed == Rect{0, 0, dst_w, dst_h}) {
      clear(dst);  // drawn over whole: a fresh start, fallen or not
    } else if (auto it = lists_.find(dst); it != lists_.end()) {
      std::vector<Command>& l = it->second;
      l.erase(std::remove_if(l.begin(), l.end(), [&](const Command& c) { return contains(landed, c.rect); }), l.end());
    }
  }
  if (!name.empty()) {
    Command p;
    p.kind = Kind::kPicture;
    p.rect = landed;
    p.x = dx, p.y = dy;
    p.name = name;
    p.alpha = !opaque;
    append(dst, std::move(p));
  }
  if (src == dst) return;  // a buffer onto itself: what it carries is there already
  auto it = lists_.find(src);
  if (it == lists_.end()) return;
  const std::vector<Command> carried = it->second;  // a copy: appending may move the source's vector
  for (Command c : carried) {
    c.rect = intersect(Rect{c.rect.l + dx, c.rect.t + dy, c.rect.r + dx, c.rect.b + dy}, landed);
    if (c.rect.empty()) continue;
    c.x += dx, c.y += dy;
    append(dst, std::move(c));
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
