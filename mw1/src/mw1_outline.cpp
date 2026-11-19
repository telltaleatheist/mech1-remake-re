#include "mw1_outline.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace mw1::outline {

namespace {
using Pt = std::pair<int32_t, int32_t>;  // (x, y)

int32_t brightness(uint16_t p) {
  const int32_t r = (p >> 11) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
  return std::max({r, g, b});
}

// The line pixels' joins: the four sides always; a corner only where neither pixel beside it is a line (else the
// two are joined through it).
std::map<Pt, std::vector<Pt>> joins(const std::vector<uint8_t>& line, int32_t w, int32_t h) {
  auto on = [&](int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < w && y < h && line[static_cast<size_t>(y) * w + x]; };
  std::map<Pt, std::vector<Pt>> nb;
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      if (!on(x, y)) continue;
      std::vector<Pt>& n = nb[{x, y}];
      for (const Pt& d : {Pt{1, 0}, Pt{-1, 0}, Pt{0, 1}, Pt{0, -1}})
        if (on(x + d.first, y + d.second)) n.push_back({x + d.first, y + d.second});
      for (const Pt& d : {Pt{1, 1}, Pt{1, -1}, Pt{-1, 1}, Pt{-1, -1}})
        if (on(x + d.first, y + d.second) && !on(x + d.first, y) && !on(x, y + d.second)) n.push_back({x + d.first, y + d.second});
    }
  return nb;
}

// The points of `path` that are true corners: the direction turns, with runs of two or more steps on both sides.
std::set<size_t> corners(const std::vector<Pt>& path, bool closed) {
  std::set<size_t> keep;
  const size_t n = path.size();
  if (n < 3) return keep;
  auto step = [&](size_t i, size_t j) { return Pt{path[j].first - path[i].first, path[j].second - path[i].second}; };
  auto prev = [&](size_t i) { return (i + n - 1) % n; };
  auto next = [&](size_t i) { return (i + 1) % n; };
  for (size_t i = closed ? 0 : 1; i < (closed ? n : n - 1); ++i) {
    const Pt a = step(prev(i), i), b = step(i, next(i));
    if (a == b) continue;
    int32_t r1 = 1, r2 = 1;
    for (size_t k = prev(i); r1 < 3 && (closed || k > 0) && step(prev(k), k) == a; k = prev(k)) ++r1;
    for (size_t k = next(i); r2 < 3 && (closed || k < n - 1) && step(k, next(k)) == b; k = next(k)) ++r2;
    if (r1 >= 2 && r2 >= 2) keep.insert(i);
  }
  return keep;
}

// Douglas-Peucker over pts[a..b]: the points kept (a and b always).
void fit(const std::vector<Pt>& pts, size_t a, size_t b, double eps, std::vector<size_t>* out) {
  double best = -1;
  size_t at = a;
  const double ax = pts[a].first, ay = pts[a].second, dx = pts[b].first - ax, dy = pts[b].second - ay;
  const double len = std::hypot(dx, dy);
  for (size_t i = a + 1; i < b; ++i) {
    const double px = pts[i].first - ax, py = pts[i].second - ay;
    const double d = len == 0 ? std::hypot(px, py) : std::fabs(dx * py - dy * px) / len;
    if (d > best) best = d, at = i;
  }
  if (b > a + 1 && best > eps) {
    fit(pts, a, at, eps, out);
    fit(pts, at, b, eps, out);
  } else {
    out->push_back(b);
  }
}

void add_path(std::vector<Pt> path, bool closed, std::vector<Segment>* lines) {
  if (closed) path.pop_back();  // the start again
  const size_t n = path.size();
  std::set<size_t> keep = corners(path, closed);
  if (closed) {  // started at a corner (when it has one), closed by the start again
    const size_t k0 = keep.empty() ? 0 : *keep.begin();
    std::vector<Pt> r(path.begin() + static_cast<std::ptrdiff_t>(k0), path.end());
    r.insert(r.end(), path.begin(), path.begin() + static_cast<std::ptrdiff_t>(k0));
    std::set<size_t> k;
    for (size_t i : keep) k.insert((i + n - k0) % n);
    r.push_back(r.front());
    path = std::move(r), keep = std::move(k);
  }
  keep.insert(0), keep.insert(path.size() - 1);
  const std::vector<size_t> ks(keep.begin(), keep.end());
  for (size_t j = 0; j + 1 < ks.size(); ++j) {
    std::vector<size_t> pts = {ks[j]};
    fit(path, ks[j], ks[j + 1], kFitTolerance, &pts);
    for (size_t i = 0; i + 1 < pts.size(); ++i)
      lines->push_back({static_cast<float>(path[pts[i]].first), static_cast<float>(path[pts[i]].second),
                        static_cast<float>(path[pts[i + 1]].first), static_cast<float>(path[pts[i + 1]].second)});
  }
}
}  // namespace

Outline vectorise(const std::vector<uint16_t>& pixels, int32_t w, int32_t h) {
  Outline o;
  if (w <= 0 || h <= 0 || pixels.size() < static_cast<size_t>(w) * h) return o;
  o.width = w, o.height = h;
  const size_t n = static_cast<size_t>(w) * h;
  std::vector<int32_t> v(n);
  int32_t top = 0;
  for (size_t i = 0; i < n; ++i) top = std::max(top, v[i] = brightness(pixels[i]));
  if (top <= kClearMost) return o;
  std::vector<uint8_t> line(n, 0);
  o.fill.assign(n, 0);
  double line_sum = 0, fill_sum = 0;
  int32_t lines = 0, fills = 0;
  for (size_t i = 0; i < n; ++i) {
    if (v[i] <= kClearMost) continue;
    if (v[i] * 2 > top) line[i] = 1, line_sum += v[i], ++lines;
    else o.fill[i] = 1, fill_sum += v[i], ++fills;
  }
  o.fill_level = lines && fills ? static_cast<float>((fill_sum / fills) / (line_sum / lines)) : 0.0f;
  // The chains: from each pixel that isn't a plain link (an end, a fork), along every join not walked; then the
  // loops left (no fork on them).
  const std::map<Pt, std::vector<Pt>> nb = joins(line, w, h);
  std::set<std::pair<Pt, Pt>> walked;
  auto edge = [](Pt a, Pt b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); };
  auto node = [&](const Pt& p) { return nb.at(p).size() != 2; };
  auto walk = [&](Pt start, Pt to) {
    std::vector<Pt> path = {start, to};
    walked.insert(edge(start, to));
    while (!node(path.back()) && path.back() != start) {
      const Pt cur = path.back();
      bool moved = false;
      for (const Pt& q : nb.at(cur))
        if (!walked.count(edge(cur, q))) {
          walked.insert(edge(cur, q)), path.push_back(q), moved = true;
          break;
        }
      if (!moved) break;
    }
    return path;
  };
  for (const auto& [p, ns] : nb) {
    if (!node(p)) continue;
    if (ns.empty()) o.dots.push_back(p);
    for (const Pt& q : ns)
      if (!walked.count(edge(p, q))) add_path(walk(p, q), false, &o.lines);
  }
  for (const auto& [p, ns] : nb)
    for (const Pt& q : ns)
      if (!walked.count(edge(p, q))) {
        std::vector<Pt> path = walk(p, q);
        const bool closed = path.size() > 2 && path.back() == path.front();
        add_path(std::move(path), closed, &o.lines);
      }
  return o;
}

namespace {
// `o` on a clear W x H (device pixels), its art box (aw x ah device pixels: the part shown) at (ox, oy), T device
// pixels an icon pixel, its lines `half` icon pixels either side of their centres.
std::vector<uint8_t> draw_at(const Outline& o, Colour colour, int32_t W, int32_t H, int32_t ox, int32_t oy, int32_t aw,
                             int32_t ah, double T, double half) {
  std::vector<uint8_t> out(static_cast<size_t>(std::max(0, W)) * std::max(0, H) * 4, 0);
  if (o.empty() || W <= 0 || H <= 0 || T <= 0) return out;
  constexpr float kFar = 1e9f;
  std::vector<float> dl(static_cast<size_t>(W) * H, kFar), df(dl.size(), kFar);
  // An icon coordinate's device pixel span, clipped to the art's box on the canvas.
  auto span = [&](double lo, double hi, int32_t off, int32_t size, int32_t* a, int32_t* b) {
    *a = std::max(off, static_cast<int32_t>(std::floor((lo + 0.5) * T)) + off);
    *b = std::min(off + size, static_cast<int32_t>(std::ceil((hi + 0.5) * T)) + off);
  };
  auto icon_x = [&](int32_t dx) { return (dx - ox + 0.5) / T - 0.5; };
  auto icon_y = [&](int32_t dy) { return (dy - oy + 0.5) / T - 0.5; };
  const double reach = std::max(half, 0.5) + 1.0 / T;
  for (const Segment& g : o.lines) {
    int32_t x0, x1, y0, y1;
    span(std::min(g.x0, g.x1) - reach, std::max(g.x0, g.x1) + reach, ox, aw, &x0, &x1);
    span(std::min(g.y0, g.y1) - reach, std::max(g.y0, g.y1) + reach, oy, ah, &y0, &y1);
    const double dx = g.x1 - g.x0, dy = g.y1 - g.y0, len2 = dx * dx + dy * dy;
    for (int32_t y = y0; y < y1; ++y)
      for (int32_t x = x0; x < x1; ++x) {
        const double px = icon_x(x) - g.x0, py = icon_y(y) - g.y0;
        double dist;
        if (dx == 0 || dy == 0) {  // along a pixel row or column: square ends, as PM's boxes have
          const double along = dx != 0 ? px : py, across = dx != 0 ? py : px, end = dx != 0 ? dx : dy;
          const double over = std::max({0.0, -along * (end < 0 ? -1 : 1), (along - end) * (end < 0 ? -1 : 1)});
          dist = std::max(over, std::fabs(across));
        } else {  // a slope: round ends
          const double t = std::clamp((px * dx + py * dy) / len2, 0.0, 1.0);
          dist = std::hypot(px - t * dx, py - t * dy);
        }
        float& d = dl[static_cast<size_t>(y) * W + x];
        d = std::min(d, static_cast<float>(dist));
      }
  }
  for (const auto& [cx, cy] : o.dots) {
    int32_t x0, x1, y0, y1;
    span(cx - reach, cx + reach, ox, aw, &x0, &x1);
    span(cy - reach, cy + reach, oy, ah, &y0, &y1);
    for (int32_t y = y0; y < y1; ++y)
      for (int32_t x = x0; x < x1; ++x) {
        float& d = dl[static_cast<size_t>(y) * W + x];
        d = std::min(d, static_cast<float>(std::max(std::fabs(icon_x(x) - cx), std::fabs(icon_y(y) - cy))));  // a square
      }
  }
  for (int32_t fy = 0; fy < o.height; ++fy)
    for (int32_t fx = 0; fx < o.width; ++fx) {
      if (!o.fill[static_cast<size_t>(fy) * o.width + fx]) continue;
      int32_t x0, x1, y0, y1;
      span(fx - 0.5 - reach, fx + 0.5 + reach, ox, aw, &x0, &x1);
      span(fy - 0.5 - reach, fy + 0.5 + reach, oy, ah, &y0, &y1);
      for (int32_t y = y0; y < y1; ++y)
        for (int32_t x = x0; x < x1; ++x) {
          const double ex = std::max(std::fabs(icon_x(x) - fx) - 0.5, 0.0), ey = std::max(std::fabs(icon_y(y) - fy) - 0.5, 0.0);
          float& d = df[static_cast<size_t>(y) * W + x];
          d = std::min(d, static_cast<float>(std::hypot(ex, ey)));
        }
    }
  const double cr = colour.r, cg = colour.g, cb = colour.b, fl = o.fill_level;
  for (size_t i = 0; i < dl.size(); ++i) {
    const double al = dl[i] >= kFar ? 0.0 : std::clamp((half - dl[i]) * T + 0.5, 0.0, 1.0);
    const double af = df[i] >= kFar ? 0.0 : std::clamp((0.5 - df[i]) * T + 0.5, 0.0, 1.0);
    const double a = std::max(al, af);
    if (a <= 0.0) continue;
    const double k = (fl * af * (1.0 - al) + al) / a;  // straight colour: the fill under the line, by coverage
    auto byte = [](double c) { return static_cast<uint8_t>(std::clamp(std::lround(c), 0L, 255L)); };
    out[i * 4] = byte(cr * k), out[i * 4 + 1] = byte(cg * k), out[i * 4 + 2] = byte(cb * k), out[i * 4 + 3] = byte(a * 255.0);
  }
  return out;
}


}  // namespace

std::vector<uint8_t> draw(const Outline& o, Colour colour, int32_t scale, int32_t canvas_w, int32_t canvas_h, int32_t s) {
  s = std::max(1, s), scale = std::max(1, scale);
  const int32_t aw = std::min(o.width * scale, canvas_w), ah = std::min(o.height * scale, canvas_h);
  return draw_at(o, colour, canvas_w * s, canvas_h * s, (canvas_w - aw) / 2 * s, (canvas_h - ah) / 2 * s, aw * s, ah * s,
                 static_cast<double>(scale) * s, 0.5);
}

std::vector<uint8_t> draw_fit(const Outline& o, Colour colour, int32_t box_w, int32_t box_h, int32_t s, double min_line) {
  s = std::max(1, s);
  const int32_t W = box_w * s, H = box_h * s;
  if (o.empty() || W <= 0 || H <= 0) return std::vector<uint8_t>(static_cast<size_t>(std::max(0, W)) * std::max(0, H) * 4, 0);
  const double T = std::min(static_cast<double>(W) / o.width, static_cast<double>(H) / o.height);
  const int32_t aw = static_cast<int32_t>(std::lround(o.width * T)), ah = static_cast<int32_t>(std::lround(o.height * T));
  return draw_at(o, colour, W, H, (W - aw) / 2, (H - ah) / 2, aw, ah, T, std::max(0.5, min_line / 2.0 / T));
}

}  // namespace mw1::outline
