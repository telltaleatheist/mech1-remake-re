#include "mw1_placement.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>

namespace mw1::placement {

namespace {

float distance(Point a, Point b) { return std::hypot(a.x - b.x, a.z - b.z); }

// The distance from p to the segment a->b.
float to_segment(Point p, Point a, Point b) {
  const float wx = b.x - a.x, wz = b.z - a.z, len2 = wx * wx + wz * wz;
  const float u = len2 > 0 ? std::clamp(((p.x - a.x) * wx + (p.z - a.z) * wz) / len2, 0.0f, 1.0f) : 0.0f;
  return distance(p, Point{a.x + u * wx, a.z + u * wz});
}

}  // namespace

Measures measure(Point a, Point c, const std::vector<Point>& enemies, const Ground& ground) {
  Measures m;
  m.start_target = distance(a, c);
  if (enemies.empty()) return m;
  for (const Point& e : enemies) m.enemy_centre.x += e.x, m.enemy_centre.z += e.z;
  m.enemies = static_cast<int>(enemies.size());
  m.enemy_centre.x /= static_cast<float>(m.enemies), m.enemy_centre.z /= static_cast<float>(m.enemies);
  const Point b = m.enemy_centre;
  m.target_enemies = distance(c, b);
  m.start_enemies = distance(a, b);
  const float abx = b.x - a.x, abz = b.z - a.z, acx = c.x - a.x, acz = c.z - a.z;
  if (m.start_enemies > 0) m.along = (acx * abx + acz * abz) / (m.start_enemies * m.start_enemies);
  if (m.start_enemies > 0 && m.start_target > 0) {
    const float cosine = std::clamp((acx * abx + acz * abz) / (m.start_enemies * m.start_target), -1.0f, 1.0f);
    m.angle_at_start = std::acos(cosine) * 57.29578f;
  }
  for (const Point& e : enemies) {
    m.spread = std::max(m.spread, distance(e, b));
    const float dt = distance(e, c), ds = distance(e, a), dl = to_segment(e, a, c);
    if (m.nearest_to_target < 0 || dt < m.nearest_to_target) m.nearest_to_target = dt;
    if (m.nearest_to_start < 0 || ds < m.nearest_to_start) m.nearest_to_start = ds;
    if (m.nearest_to_line < 0 || dl < m.nearest_to_line) m.nearest_to_line = dl;
  }
  m.centre_to_line = to_segment(b, a, c);
  if (m.target_enemies > 0) m.race = m.start_target / m.target_enemies;
  if (ground) {
    // Every 10 m (at least 8 steps) from B to the midpoint of A->C, the ground above the straight line between the
    // two ends' ground.
    const Point mid{(a.x + c.x) / 2, (a.z + c.z) / 2};
    const float hb = ground(b.x, b.z), hm = ground(mid.x, mid.z);
    if (!std::isnan(hb) && !std::isnan(hm)) {
      const int steps = std::max(8, static_cast<int>(distance(b, mid) / 10.0f));
      for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float h = ground(b.x + (mid.x - b.x) * t, b.z + (mid.z - b.z) * t);
        if (!std::isnan(h)) m.ridge = std::max(m.ridge, h - (hb + (hm - hb) * t));
      }
    }
  }
  return m;
}

Route route(Point a, Point c, const std::vector<Point>& enemies, const Inside& inside, const RouteRules& rules,
            const Ground& ground) {
  Route r;
  auto nearest = [&](Point p) {
    float best = -1;
    for (const Point& e : enemies) {
      const float d = distance(p, e);
      if (best < 0 || d < best) best = d;
    }
    return best;
  };
  r.start_clear = nearest(a), r.goal_clear = nearest(c);
  r.start_inside = !inside || inside(a.x, a.z), r.goal_inside = !inside || inside(c.x, c.z);
  if (!r.start_inside) return r;
  // The grid: a box round the start, the goal and the enemies, `margin` past them.
  float lo_x = std::min(a.x, c.x), hi_x = std::max(a.x, c.x), lo_z = std::min(a.z, c.z), hi_z = std::max(a.z, c.z);
  for (const Point& e : enemies) lo_x = std::min(lo_x, e.x), hi_x = std::max(hi_x, e.x), lo_z = std::min(lo_z, e.z), hi_z = std::max(hi_z, e.z);
  lo_x -= rules.margin, lo_z -= rules.margin, hi_x += rules.margin, hi_z += rules.margin;
  const float cell = rules.cell;
  const int nx = static_cast<int>((hi_x - lo_x) / cell) + 1, nz = static_cast<int>((hi_z - lo_z) / cell) + 1;
  const auto at = [&](int i) {
    return Point{lo_x + (static_cast<float>(i % nx) + 0.5f) * cell, lo_z + (static_cast<float>(i / nx) + 0.5f) * cell};
  };
  // 0 not yet tested, 1 open, 2 blocked: tested as the search reaches a cell (the area's test isn't free).
  std::vector<uint8_t> state(static_cast<size_t>(nx) * static_cast<size_t>(nz), 0);
  const auto open = [&](int i) {
    uint8_t& s = state[static_cast<size_t>(i)];
    if (s == 0) {
      const Point p = at(i);
      bool ok = !inside || inside(p.x, p.z);
      const bool own = distance(p, a) < rules.clearance;  // the start's own disc
      for (size_t k = 0; ok && !own && k < enemies.size(); ++k) ok = distance(p, enemies[k]) >= rules.clearance;
      s = ok ? 1 : 2;
    }
    return s == 1;
  };
  const int from = std::clamp(static_cast<int>((a.z - lo_z) / cell), 0, nz - 1) * nx +
                   std::clamp(static_cast<int>((a.x - lo_x) / cell), 0, nx - 1);
  if (!open(from)) return r;
  // The end: an open cell within `reach` of C, at least `clearance` from every enemy place (the start's own disc
  // doesn't excuse it).
  const auto end = [&](int i) {
    const Point p = at(i);
    if (distance(p, c) > rules.reach) return false;
    const float n = nearest(p);
    return n < 0 || n >= rules.clearance;
  };
  const float inf = std::numeric_limits<float>::infinity(), diagonal = cell * 1.41421356f;
  std::vector<float> g(state.size(), inf);
  std::vector<int> parent(state.size(), -1);
  const auto h = [&](int i) {
    const Point p = at(i);
    const float dx = std::fabs(p.x - c.x), dz = std::fabs(p.z - c.z);
    return std::max(0.0f, std::hypot(dx, dz) - rules.reach);
  };
  using Item = std::pair<float, int>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> todo;
  g[static_cast<size_t>(from)] = 0;
  todo.push({h(from), from});
  int reached = -1;
  while (!todo.empty()) {
    const auto [f, i] = todo.top();
    todo.pop();
    if (f > g[static_cast<size_t>(i)] + h(i) + 0.01f) continue;  // a stale entry
    ++r.expanded;
    if (end(i)) {
      reached = i;
      break;
    }
    const int ix = i % nx, iz = i / nx;
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dz) continue;
        const int jx = ix + dx, jz = iz + dz;
        if (jx < 0 || jz < 0 || jx >= nx || jz >= nz) continue;
        const int j = jz * nx + jx;
        if (!open(j)) continue;
        if (dx && dz && (!open(iz * nx + jx) || !open(jz * nx + ix))) continue;  // no cutting a blocked corner
        const float step = dx && dz ? diagonal : cell, ng = g[static_cast<size_t>(i)] + step;
        if (ng < g[static_cast<size_t>(j)]) {
          g[static_cast<size_t>(j)] = ng, parent[static_cast<size_t>(j)] = i;
          todo.push({ng + h(j), j});
        }
      }
  }
  if (reached < 0) return r;
  std::vector<Point> cells;
  for (int i = reached; i != -1; i = parent[static_cast<size_t>(i)]) cells.push_back(at(i));
  std::reverse(cells.begin(), cells.end());
  r.path.push_back(a);
  r.path.insert(r.path.end(), cells.begin() + 1, cells.end());
  if (r.path.size() == 1) r.path.push_back(cells.front());  // the start's own cell is the end
  r.found = true;
  r.length = 0;
  r.end_clear = nearest(r.path.back());
  float last_h = ground ? ground(a.x, a.z) : std::nanf("");
  for (size_t k = 1; k < r.path.size(); ++k) {
    const float d = distance(r.path[k - 1], r.path[k]);
    r.length += d;
    const float n = nearest(r.path[k]);
    if (distance(r.path[k], a) >= rules.clearance && n >= 0 && (r.path_clear < 0 || n < r.path_clear)) r.path_clear = n;
    if (ground) {
      const float hh = ground(r.path[k].x, r.path[k].z);
      if (!std::isnan(hh) && !std::isnan(last_h) && d > 0) r.steepest = std::max(r.steepest, std::fabs(hh - last_h) / d);
      last_h = hh;
    }
  }
  const float straight = distance(a, c) - rules.reach;
  r.detour = straight > 0 ? r.length / straight : 1.0f;
  return r;
}

}  // namespace mw1::placement
