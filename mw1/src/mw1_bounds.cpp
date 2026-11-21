#include "mw1_bounds.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

#include "gamez/zreader/zreader.h"

namespace mw1 {

namespace {
Bounds g_bounds;

std::string lower(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

// A list of (x z) pairs.
bool read_outline(zrdr::Node* list, std::vector<Point2>* out) {
  if (!list || list->type != zrdr::List) return false;
  const int32_t n = zrdr::list_count(list);
  for (int32_t i = 1; i < n; ++i) {
    float p[2];
    if (!zrdr::get_vec2(&list->list[i], p)) return false;
    out->push_back({p[0], p[1]});
  }
  return out->size() >= 3;
}

bool read_pair(zrdr::Node* m, const char* key, Point2* out) {
  float p[2];
  if (!zrdr::get_vec2(m, key, p)) return false;
  *out = {p[0], p[1]};
  return true;
}
}  // namespace

Bounds parse_bounds(const std::string& zrd_path, std::string* why) {
  Bounds b;
  zrdr::Node* root = zrdr::load(zrd_path.c_str());
  if (!root) {
    if (why) *why = "no " + zrd_path;
    return b;
  }
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const int32_t n = top->type == zrdr::List ? zrdr::list_count(top) : 0;
  int32_t index = 0;
  for (int32_t i = 1; i + 1 < n; ++i) {
    zrdr::Node* key = &top->list[i];
    if (key->type != zrdr::String) continue;
    const std::string k = lower(key->s);
    if (k != "bounds" && k != "move") continue;
    zrdr::Node* m = &top->list[++i];
    ++index;
    const char* world = zrdr::get_string(m, "WORLD");
    int32_t ia = 0;
    zrdr::get_int(m, "IA", &ia);
    bool ok = world && *world && ia >= 1;
    if (k == "bounds") {
      Bound e;
      e.world = world ? world : "", e.ia = ia;
      ok = ok && read_outline(zrdr::find(m, "YELLOW"), &e.yellow) && read_outline(zrdr::find(m, "RED"), &e.red);
      if (ok) b.bounds.push_back(std::move(e));
    } else {
      PlaceMove e;
      e.world = world ? world : "", e.ia = ia;
      ok = ok && read_pair(m, "FROM", &e.from) && read_pair(m, "TO", &e.to);
      if (ok) b.moves.push_back(e);
    }
    if (!ok && why) *why += k + " entry " + std::to_string(index) + " doesn't read; ";
  }
  zrdr::free_node(root);
  return b;
}

void set_bounds(Bounds b) { g_bounds = std::move(b); }
const Bounds& bounds() { return g_bounds; }

const Bound* bound_for(const std::string& world, int32_t ia) {
  for (const Bound& b : g_bounds.bounds)
    if (b.world == world && b.ia == ia) return &b;
  return nullptr;
}

bool moved_place(const std::string& world, int32_t ia, float x, float z, float* to_x, float* to_z) {
  const PlaceMove* best = nullptr;
  float best_d = kMoveNear;
  for (const PlaceMove& m : g_bounds.moves) {
    if (m.world != world || m.ia != ia) continue;
    const float d = std::hypot(x - m.from[0], z - m.from[1]);
    if (d <= best_d) best = &m, best_d = d;
  }
  if (!best) return false;
  *to_x = best->to[0] + (x - best->from[0]);
  *to_z = best->to[1] + (z - best->from[1]);
  return true;
}

bool inside_outline(const std::vector<Point2>& o, float x, float z) {
  bool in = false;
  for (size_t i = 0, j = o.size() - 1; i < o.size(); j = i++) {
    const float xi = o[i][0], zi = o[i][1], xj = o[j][0], zj = o[j][1];
    if ((zi > z) != (zj > z) && x < (xj - xi) * (z - zi) / (zj - zi) + xi) in = !in;
  }
  return in;
}

float depth_inside(const std::vector<Point2>& o, float x, float z) {
  float best = std::numeric_limits<float>::max();
  for (size_t i = 0, j = o.size() - 1; i < o.size(); j = i++) {
    const float ax = o[j][0], az = o[j][1], bx = o[i][0], bz = o[i][1];
    const float dx = bx - ax, dz = bz - az, len2 = dx * dx + dz * dz;
    float t = len2 > 0 ? ((x - ax) * dx + (z - az) * dz) / len2 : 0;
    t = std::clamp(t, 0.0f, 1.0f);
    best = std::min(best, std::hypot(x - (ax + t * dx), z - (az + t * dz)));
  }
  return inside_outline(o, x, z) ? best : -best;
}

}  // namespace mw1
