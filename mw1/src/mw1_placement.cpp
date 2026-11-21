#include "mw1_placement.h"

#include <algorithm>
#include <cmath>

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

}  // namespace mw1::placement
