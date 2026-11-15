#include "pm/gamez/zmodel/gmod_strip.h"

#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <vector>

#include "gamez/zgeometry/zgeo_x87.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_matl.h"

namespace pm::zmod {

StripHooks& strip_hooks() {
  static StripHooks h;
  return h;
}

StripCounts& strip_counts() {
  static StripCounts c;
  return c;
}

namespace {

uint32_t points(const Polygon& p) { return p.flags & 0x3ff; }

// Where `v` first stands in the polygon's points (the count when it is not there).
uint32_t first_at(const Polygon& p, uint32_t v) {
  uint32_t i = 0;
  while (i < points(p) && p.vertex_indices[i] != v) ++i;
  return i;
}

StripEdgeKey edge_key(int32_t a, int32_t b) { return a <= b ? StripEdgeKey{a, b} : StripEdgeKey{b, a}; }

}  // namespace

// Pirate's Moon 1.0 @ 0x00572b40
bool same_loop(const Polygon& a, const Polygon& b) {
  const uint32_t differ = a.flags ^ b.flags;
  if (differ & 0x3ff || differ & 0x400) return false;
  const uint32_t n = points(a);
  const uint32_t at = first_at(a, b.vertex_indices[0]);
  if (at >= n) return false;
  for (uint32_t k = 1; k < points(b); ++k)
    if (a.vertex_indices[(k + at) % n] != b.vertex_indices[k]) return false;
  return true;
}

// Pirate's Moon 1.0 @ 0x00572c20
void append_layers(Polygon& a, const Polygon& b) {
  const uint32_t at = first_at(a, b.vertex_indices[0]);
  const size_t total = static_cast<size_t>(a.material_count + b.material_count);
  a.materials = static_cast<::zmod::Material**>(std::realloc(a.materials, total * sizeof(void*)));
  for (int32_t j = 0; j < b.material_count; ++j) a.materials[a.material_count + j] = b.materials[j];
  a.uvs = static_cast<float**>(std::realloc(a.uvs, total * sizeof(float*)));
  for (int32_t j = 0; j < b.material_count; ++j) {
    float* uv = static_cast<float*>(std::malloc(static_cast<size_t>(points(a)) * 8));
    a.uvs[a.material_count + j] = uv;
    for (uint32_t k = 0; k < points(a); ++k) {
      const uint32_t to = (k + at) % points(a);
      uv[2 * to] = b.uvs[j][2 * k];
      uv[2 * to + 1] = b.uvs[j][2 * k + 1];
    }
  }
  a.material_count += b.material_count;
}

// Pirate's Moon 1.0 @ 0x00572df0
void merge_layers(Model& m) {
  if (strip_hooks().load_images) strip_hooks().load_images();
  // 0x005cb6f0's two vectors of polygon pointers; indices here, fixed the same way when a polygon moves.
  std::vector<int32_t> solid, layers;
  for (int32_t i = 0; i < m.count(0x10); ++i) {
    const Polygon& p = m.polygons[i];
    if (p.material_count == 0) continue;
    int32_t k = 0;
    while (k < p.material_count && (p.materials[k]->flags & 1)) ++k;
    if (k < p.material_count) continue;
    const ::zmod::Material* first = p.materials[0];
    if (!first->texture) continue;  // FIX (A): the exe reads the image through it unasked
    const auto* image = static_cast<const ::zimg::Texture*>(first->texture->image);
    if (!image) continue;
    (!(image->flags & 2) && first->alpha == 0xff ? solid : layers).push_back(i);
  }
  for (size_t s = 0; s < solid.size(); ++s) {
    for (size_t l = 0; l < layers.size();) {
      Polygon& a = m.polygons[solid[s]];
      const int32_t at = layers[l];
      Polygon& b = m.polygons[at];
      // FIX (A): both need texture coordinates; the exe grows a's array from null (its first entries unset) and
      // reads b's through null.
      if (!same_loop(a, b) || a.material_count + b.material_count > 8 || !a.uvs || !b.uvs) {
        ++l;
        continue;
      }
      append_layers(a, b);
      std::free(b.vertex_indices), std::free(b.colors), std::free(b.scratch), std::free(b.materials);
      for (int32_t j = 0; j < b.material_count; ++j) std::free(b.uvs[j]);
      std::free(b.uvs), std::free(b.normal_indices), std::free(b.library_indices);
      const int32_t last = m.count(0x10) - 1;
      if (at != last) {
        m.polygons[at] = m.polygons[last];
        for (int32_t& x : solid) x = x == last ? at : x;
        for (int32_t& x : layers) x = x == last ? at : x;
      }
      std::memcpy(m.header + 0x10, &last, 4);
      layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(l));
    }
  }
}

// Pirate's Moon 1.0 @ 0x00573bf0
bool edge_in_polygon(const Model& m, int32_t polygon, StripEdgeKey key, bool* reversed) {
  const Polygon& p = m.polygons[polygon];
  const auto v = [&](uint32_t i) { return static_cast<int32_t>(p.vertex_indices[i]); };
  const auto try_edge = [&](int32_t from, int32_t to, bool rev) {
    if (edge_key(from, to) != key) return false;
    *reversed = rev;
    return true;
  };
  if (p.flags & 0x2000) {
    if (points(p) == 4) {  // a quad strip: its diagonals (0x00573100: reversed when the second point is lower)
      if (try_edge(v(2), v(0), v(0) < v(2))) return true;
      if (try_edge(v(1), v(3), v(3) < v(1))) return true;
    }
    if (points(p) < 2) return false;  // FIX (A): the exe reads the point before the first
    if (try_edge(v(0), v(1), v(1) < v(0))) return true;
    // 0x005732c0: the last edge, its sense turned on a strip of an even count (the winding alternates).
    const uint32_t n = points(p);
    bool rev = v(n - 1) < v(n - 2);
    if (!(p.flags & 1)) rev = !rev;
    return try_edge(v(n - 2), v(n - 1), rev);
  }
  for (uint32_t i = 0, prev = points(p) - 1; i < points(p); prev = i++)
    if (try_edge(v(prev), v(i), v(i) < v(prev))) return true;
  return false;
}

// Pirate's Moon 1.0 @ 0x00570b70, its start to 0x00570ef7.
bool strip_intake(Stripper& s, Model* m, int32_t arg, int32_t max_points) {
  if (!m || m->count(0) != 0 || m->count(0x10) == 0) return false;
  merge_layers(*m);
  if (strip_hooks().load_images) strip_hooks().load_images();
  s.model = m, s.arg = arg;
  s.edges.clear(), s.vertices.clear();
  strip_counts().polygons += m->count(0x10);
  for (int32_t i = 0; i < m->count(0x10); ++i) {
    const Polygon& p = m->polygons[i];
    if (!p.uvs || !(p.flags & 0x1000)) continue;
    if (!(p.flags & 0x2000) && static_cast<int32_t>(points(p)) > max_points) continue;
    ++strip_counts().taken;
    for (uint32_t k = 0; k < points(p); ++k) {
      const int32_t from = static_cast<int32_t>(p.vertex_indices[k]);
      const int32_t to = static_cast<int32_t>(p.vertex_indices[(k + 1) % points(p)]);
      const StripEdgeKey key = edge_key(from, to);
      StripEdge& e = s.edges.try_emplace(key, StripEdge{key, {}}).first->second;
      bool reversed = false;
      // Each time the polygon's points run over the edge: a polygon meeting it twice is listed twice.
      if (edge_in_polygon(*m, i, key, &reversed)) e.uses.push_back({i, reversed});
      StripVertex& vx = s.vertices.try_emplace(from, StripVertex{from, {}}).first->second;
      // 0x00573b70 finds the point in the polygon (always, here), so each corner lists it, a repeated point twice.
      if (first_at(p, static_cast<uint32_t>(from)) < points(p)) vx.polygons.push_back(i);
    }
  }
  return true;
}

// Pirate's Moon 1.0 @ 0x005735a0
StripCorner strip_corner(const Model& m, int32_t polygon, int32_t at) {
  const Polygon& p = m.polygons[polygon];
  StripCorner c;
  for (int32_t j = 0; j < p.material_count; ++j) c.uvs.emplace_back(p.uvs[j][2 * at], p.uvs[j][2 * at + 1]);
  c.vertex = static_cast<int32_t>(p.vertex_indices[at]);
  c.normal = static_cast<int32_t>(p.normal_indices[at]);
  std::memcpy(c.color, p.colors + 3 * at, 12);
  return c;
}

namespace {

// 0x0056b230: within 2 a channel (a tolerance the colours, 0..255, all but always meet).
bool colors_near(const float* a, const float* b) {
  for (int k = 0; k < 3; ++k)
    if (zgeo::abs(zgeo::X(a[k]) - b[k]).v > 2.0) return false;
  return true;
}

// 0x0056b2d0's rounding: the difference plus or minus a half, cut toward zero (_ftol 0x005cef9a).
float whole(zgeo::X d) {
  const zgeo::X r = !(d.v >= 0.0) ? d - 0.5f : d + 0.5f;  // fcom's C0: below zero, or unordered
  return static_cast<float>(static_cast<int32_t>(r.v));
}

// 0x00574560: the second corners (the point is the same): their normal, shifts and colour.
bool corners_match(const StripCorner& a, const std::vector<char>& clamped, const StripCorner& b,
                   std::vector<std::pair<float, float>>& shifts) {
  if (a.uvs.size() != b.uvs.size()) return false;
  if (a.normal != b.normal) return false;
  return uv_shifts(clamped, a.uvs, b.uvs, shifts) && colors_near(a.color, b.color);
}

// 0x00574620: the shifts added to each corner's coordinates, layer by layer.
void add_shifts(Model& m, int32_t polygon, const std::vector<std::pair<float, float>>& shifts) {
  Polygon& p = m.polygons[polygon];
  for (uint32_t k = 0; k < points(p); ++k)
    for (size_t j = 0; j < shifts.size(); ++j) {
      p.uvs[j][2 * k] += shifts[j].first;
      p.uvs[j][2 * k + 1] += shifts[j].second;
    }
}

// The ends join_shape reads off a polygon (0x00573100 / 0x00574e90 records): the edge from its first point to its
// second, `rev` when the second is the lower.
struct End {
  int32_t lo = 0, hi = 0;
  bool rev = false;
  int32_t to() const { return rev ? lo : hi; }
  int32_t from() const { return rev ? hi : lo; }
  bool on(StripEdgeKey k) const { return k.first == lo && k.second == hi; }
};
End end_of(int32_t from, int32_t to) { return {std::min(from, to), std::max(from, to), to < from}; }

// A polygon's two ends: a strip's first and last edges; any other's shared edge and the one halfway round.
bool ends(const Polygon& p, StripEdgeKey key, bool* plain, End* first, End* second) {
  const auto v = [&](int32_t i) { return static_cast<int32_t>(p.vertex_indices[i]); };
  const int32_t n = static_cast<int32_t>(points(p));
  *plain = !(p.flags & 0x2000);
  if (!*plain) {
    *first = end_of(v(0), v(1));
    *second = end_of(v(n - 2), v(n - 1));  // no turn on an even count, as 0x00573bf0 makes
    return true;
  }
  int32_t at = -1;
  for (int32_t i = 0, prev = n - 1; i < n; prev = i++)
    if (edge_key(v(prev), v(i)) == key) {
      at = prev;
      break;
    }
  if (at < 0) return false;  // FIX (A): the exe reads the point before the first (the key is always there)
  *first = end_of(v(at), v((at + 1) % n));
  const int32_t half = at + (n >> 1);
  *second = end_of(v(half % n), v((half + 1) % n));
  return true;
}

// A four-point strip joined across a diagonal: its ends are its diagonals.
bool on_diagonal(const Polygon& p, StripEdgeKey key) {
  if (!(p.flags & 0x2000) || points(p) != 4) return false;
  const auto v = [&](int32_t i) { return static_cast<int32_t>(p.vertex_indices[i]); };
  return edge_key(v(0), v(2)) == key || edge_key(v(1), v(3)) == key;
}

}  // namespace

// Pirate's Moon 1.0 @ 0x0056b2d0
bool uv_shifts(const std::vector<char>& clamped, const std::vector<std::pair<float, float>>& a,
               const std::vector<std::pair<float, float>>& b, std::vector<std::pair<float, float>>& shifts) {
  shifts = a;
  for (size_t i = 0; i < a.size(); ++i) {
    const zgeo::X du = zgeo::X(a[i].first) - b[i].first;
    shifts[i].first = whole(du);
    if (zgeo::abs(du - shifts[i].first).v > 0.05000000074505806) return false;
    const zgeo::X dv = zgeo::X(a[i].second) - b[i].second;
    shifts[i].second = whole(dv);
    if (zgeo::abs(dv - shifts[i].second).v > 0.05000000074505806) return false;
    if (clamped[i] && (shifts[i].first != 0.0f || shifts[i].second != 0.0f)) return false;
  }
  return true;
}

// Pirate's Moon 1.0 @ 0x0056b980. The eight strip cases are the exe's, in its order; they are not quite each
// other's mirror (which quad flag each tests), kept as they are.
bool join_shape(const Stripper& s, int32_t pa, int32_t pb, StripEdgeKey key, int32_t first, int32_t second,
                bool same_shift, int32_t* strip, int32_t* end) {
  *strip = 0;
  const Polygon& A = s.model->polygons[pa];
  const Polygon& B = s.model->polygons[pb];
  bool a_plain = false, b_plain = false;
  End a1, a2, b1, b2;
  if (!ends(A, key, &a_plain, &a1, &a2) || !ends(B, key, &b_plain, &b1, &b2)) return false;
  const bool a_quad = on_diagonal(A, key), b_quad = on_diagonal(B, key);
  const auto va = [&](int32_t i) { return static_cast<int32_t>(A.vertex_indices[i]); };
  const auto vb = [&](int32_t i) { return static_cast<int32_t>(B.vertex_indices[i]); };
  if (a_quad) {
    if (b_quad) return false;
    a1 = end_of(va(0), va(2)), a2 = end_of(va(1), va(3));
  } else if (b_quad) {
    b1 = end_of(vb(0), vb(2)), b2 = end_of(vb(1), vb(3));
  }
  const bool a_odd = A.flags & 1, b_odd = B.flags & 1;
  if (s.slack >= 1) {
    *strip = 1;
    const auto pick = [&](int32_t x) {
      if (x != first && x != second) return false;
      *end = (x != first) + 1;
      return true;
    };
    if (a2.on(key) && b1.on(key) && pick(a2.to())) {
      if (a2.to() == b1.from() && (!a_quad || !b_odd)) return true;
      if (a2.to() == b1.to() && ((!a_quad && b_plain) || (b_odd && b_plain))) return true;
    }
    if (b2.on(key) && a1.on(key) && pick(b2.to())) {
      if (b2.to() == a1.from() && (!b_quad || !a_odd)) return true;
      if (b2.to() == a1.to() && ((!b_quad && a_plain) || (a_odd && a_plain))) return true;
    }
    if (a2.on(key) && b1.on(key) && pick(a2.from())) {
      if (a2.from() == b1.from() && !b_quad && a_plain && !b_odd) return true;
      if (a2.from() == b1.to() && !b_quad && b_plain && b_odd && a_plain) return true;
    }
    if (b2.on(key) && a1.on(key) && pick(b2.from())) {
      if (b2.from() == a1.from() && !a_quad && b_plain && !a_odd) return true;
      if (b2.from() == a1.to() && !a_quad && a_plain && a_odd && b_plain) return true;
    }
    if (a2.on(key) && b2.on(key) && pick(a2.to())) {
      if (a2.to() == b2.to()) {
        if (!a_quad || !b_quad) return true;
      } else if (a2.to() == b2.from() && !a_quad && b_plain) {
        return true;
      }
    }
    if (a2.on(key) && b2.on(key) && pick(a2.from()))
      if (a2.from() == b2.to() && !b_quad && a_plain) return true;
    if (a1.on(key) && b1.on(key) && pick(a1.to())) {
      if (a1.to() == b1.to()) {
        if (a_plain && a_odd) {
          if (b_plain) return true;
        } else if (b_plain && b_odd && a_plain) {
          return true;
        }
      } else if (a1.to() == b1.from()) {
        if (a_plain && a_odd) return true;
        if (!b_quad && !b_odd && a_plain) return true;
      }
    }
    if (a1.on(key) && b1.on(key) && pick(a1.from())) {
      if (a1.from() == b1.to()) {
        if (!a_quad && !a_odd) {
          if (b_plain) return true;
        } else if (b_plain && b_odd) {
          return true;
        }
      } else if (a1.from() == b1.from()) {
        if (!a_quad && !a_odd) return true;
        if (!b_quad && !b_odd) return true;
      }
    }
  }
  *strip = 0;
  if (!same_shift) return false;
  if (a2.on(key) && b1.on(key)) {
    if (!a_quad) return a2.rev == b1.rev || b_plain;
    return a2.rev == b1.rev ? !b_odd : (b_odd && b_plain);
  }
  if (b2.on(key) && a1.on(key)) {
    if (!b_quad) return b2.rev == a1.rev || a_plain;
    return b2.rev == a1.rev ? !a_odd : (a_odd && a_plain);
  }
  if (a2.on(key) && b2.on(key)) {
    if (a2.rev != b2.rev) return true;
    return (!b_quad && a_plain) || (!a_quad && b_plain);
  }
  if (a1.on(key) && b1.on(key)) {
    if (a1.rev == b1.rev)
      return (!a_quad && !a_odd && b_plain) || (!b_quad && !b_odd && a_plain) || (!a_quad && a_odd && a_plain) ||
             (!b_quad && b_odd && b_plain);
    if ((!a_quad && !a_odd) || (!b_quad && !b_odd)) return true;
    return !a_quad && a_plain && b_plain;
  }
  return false;
}

// Pirate's Moon 1.0 @ 0x00573f50
bool try_join(Stripper& s, const StripEdgeUse& a, const StripEdgeUse& b, StripEdgeKey key, int32_t* first,
              int32_t* second, int32_t* strip) {
  if (a.polygon == b.polygon || a.reversed == b.reversed) return false;
  Model& m = *s.model;
  const Polygon& A = m.polygons[a.polygon];
  const Polygon& B = m.polygons[b.polygon];
  const int32_t na = static_cast<int32_t>(points(A)), nb = static_cast<int32_t>(points(B));
  if (na + nb - 2 > 0x40 || na + nb - 2 > s.arg) return false;
  s.slack = std::min({s.pass, s.arg - na - nb + 2, 0x42 - na - nb});
  if (A.material_count != B.material_count) return false;
  for (int32_t j = 0; j < A.material_count; ++j)
    if (A.materials[j] != B.materials[j]) return false;
  if (A.flags & 0x400 || B.flags & 0x400 || (A.flags ^ B.flags) & 0x800) return false;
  if (A.priority != B.priority || A.zone_set != B.zone_set) return false;  // 0x0056b540: the zone's four bytes
  const int32_t a0 = static_cast<int32_t>(first_at(A, static_cast<uint32_t>(key.first)));
  const int32_t a1 = static_cast<int32_t>(first_at(A, static_cast<uint32_t>(key.second)));
  const int32_t b0 = static_cast<int32_t>(first_at(B, static_cast<uint32_t>(key.first)));
  const int32_t b1 = static_cast<int32_t>(first_at(B, static_cast<uint32_t>(key.second)));
  std::vector<char> clamped;  // 0x005751f0's vector: a layer whose image clamps (flags & 3) takes no shift
  for (int32_t j = 0; j < A.material_count; ++j) {
    const ::zmod::Material* mat = A.materials[j];
    clamped.push_back(mat && mat->texture && mat->texture->image &&
                      (static_cast<const ::zimg::Texture*>(mat->texture->image)->flags & 3));
  }
  std::vector<std::pair<float, float>> shift1, shift2;
  bool match1 = false;
  {
    const StripCorner cb = strip_corner(m, b.polygon, b0), ca = strip_corner(m, a.polygon, a0);
    match1 = ca.uvs.size() == cb.uvs.size() && cb.normal == ca.normal && uv_shifts(clamped, ca.uvs, cb.uvs, shift1) &&
             colors_near(ca.color, cb.color);
  }
  const StripCorner cb = strip_corner(m, b.polygon, b1), ca = strip_corner(m, a.polygon, a1);
  const bool match2 = corners_match(ca, clamped, cb, shift2);
  if (match1)
    *first = key.first;
  else if (!match2)
    return false;
  else
    *first = -1;
  *second = match2 ? key.second : -1;
  const bool same_shift = match1 && match2 && shift1 == shift2;  // 0x00576f70
  int32_t end = 0;  // the exe's local, unset unless join_shape gives a strip
  if (!join_shape(s, a.polygon, b.polygon, key, *first, *second, same_shift, strip, &end)) return false;
  if (*strip == 0)
    add_shifts(m, b.polygon, shift1);
  else if (*strip == 1)
    add_shifts(m, b.polygon, end == 1 ? shift1 : shift2);
  return true;
}

}  // namespace pm::zmod
