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

// Pirate's Moon 1.0 @ 0x00573b70: where `v` first stands in the polygon's points (the count when it is not there).
uint32_t first_at(const Polygon& p, uint32_t v) {
  uint32_t i = 0;
  while (i < points(p) && p.vertex_indices[i] != v) ++i;
  return i;
}

// Pirate's Moon 1.0 @ 0x00574e90: the lower point first.
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

// Pirate's Moon 1.0 @ 0x0056b230: within 2 a channel (a tolerance the colours, 0..255, all but always meet).
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

// Pirate's Moon 1.0 @ 0x00574560: the second corners (the point is the same): their normal, shifts and colour.
bool corners_match(const StripCorner& a, const std::vector<char>& clamped, const StripCorner& b,
                   std::vector<std::pair<float, float>>& shifts) {
  if (a.uvs.size() != b.uvs.size()) return false;
  if (a.normal != b.normal) return false;
  return uv_shifts(clamped, a.uvs, b.uvs, shifts) && colors_near(a.color, b.color);
}

// Pirate's Moon 1.0 @ 0x00574620: the shifts added to each corner's coordinates, layer by layer.
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
  int32_t to() const { return rev ? lo : hi; }    // Pirate's Moon 1.0 @ 0x005731c0
  int32_t from() const { return rev ? hi : lo; }  // Pirate's Moon 1.0 @ 0x005731a0
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
  // Pirate's Moon 1.0 @ 0x0056b540: the zone's four bytes.
  if (A.priority != B.priority || A.zone_set != B.zone_set) return false;
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
  const bool same_shift = match1 && match2 && shift1 == shift2;  // Pirate's Moon 1.0 @ 0x00576f70
  int32_t end = 0;  // the exe's local, unset unless join_shape gives a strip
  if (!join_shape(s, a.polygon, b.polygon, key, *first, *second, same_shift, strip, &end)) return false;
  if (*strip == 0)
    add_shifts(m, b.polygon, shift1);
  else if (*strip == 1)
    add_shifts(m, b.polygon, end == 1 ? shift1 : shift2);
  return true;
}

namespace {

int32_t vx(const Model& m, int32_t p, int32_t i) { return static_cast<int32_t>(m.polygons[p].vertex_indices[i]); }
int32_t nx(const Model& m, int32_t p) { return static_cast<int32_t>(points(m.polygons[p])); }
// Pirate's Moon 1.0 @ 0x00573230 / 0x00573210 (its opposite, written !odd)
bool odd(const Model& m, int32_t p) { return m.polygons[p].flags & 1; }

// The polygons' array resized as the exe's realloc does (0x00596520): a new slot is cleared.
void resize_polygons(Model& m, int32_t n) {
  Polygon* grown = new Polygon[static_cast<size_t>(n > 0 ? n : 1)]{};
  for (int32_t q = 0; q < std::min(n, m.count(0x10)); ++q) grown[q] = m.polygons[q];
  delete[] m.polygons;
  m.polygons = grown;
}

void set_count(Model& m, int32_t n) { std::memcpy(m.header + 0x10, &n, 4); }

// The growth loop's removal of a joined polygon (0x00571356..0x00571496): its arrays freed, the model's last moved
// into its slot, the array shrunk.
void remove_polygon(Model& m, int32_t at) {
  Polygon& p = m.polygons[at];
  std::free(p.vertex_indices), std::free(p.colors), std::free(p.scratch);
  std::free(p.materials), p.materials = nullptr;
  if (p.uvs) {
    for (int32_t j = 0; j < p.material_count; ++j) std::free(p.uvs[j]);
    std::free(p.uvs);
  }
  if (p.flags & 0x1000) std::free(p.normal_indices);
  std::free(p.library_indices);
  const int32_t last = m.count(0x10) - 1;
  if (at != last) m.polygons[at] = m.polygons[last];
  set_count(m, last);
  resize_polygons(m, last);
}

// Pirate's Moon 1.0 @ 0x00573250
End first_end(const Model& m, int32_t p) { return end_of(vx(m, p, 0), vx(m, p, 1)); }
// Pirate's Moon 1.0 @ 0x005732c0 (the end record, without 0x00573bf0's turn)
End last_end(const Model& m, int32_t p) {
  return end_of(vx(m, p, nx(m, p) - 2), vx(m, p, nx(m, p) - 1));
}

// After a join: every list forgets a and b, and the model's last original (count - 2, about to move into b's slot)
// is renamed b; then a and b are removed and the new strip's ends listed where the maps have them.
void after_join(Stripper& s, int32_t a, int32_t b, int32_t made) {
  Model& m = *s.model;
  const int32_t moved = m.count(0x10) - 2;
  for (auto& [key, e] : s.edges) {
    e.uses.remove_if([&](const StripEdgeUse& u) { return u.polygon == a; });
    e.uses.remove_if([&](const StripEdgeUse& u) { return u.polygon == b; });
    for (StripEdgeUse& u : e.uses) u.polygon = u.polygon == moved ? b : u.polygon;
  }
  for (auto& [key, vt] : s.vertices) {
    vt.polygons.remove(a);
    vt.polygons.remove(b);
    for (int32_t& q : vt.polygons) q = q == moved ? b : q;
  }
  (void)made;  // always the last slot here; it ends in a's (b's when a was count - 2)
  const int32_t at = a == moved ? b : a;
  remove_polygon(m, a);
  remove_polygon(m, b);
  const auto add = [&](int32_t x, int32_t y) {
    const StripEdgeKey key = edge_key(x, y);
    const auto it = s.edges.find(key);
    bool reversed = false;
    if (it != s.edges.end() && edge_in_polygon(m, at, key, &reversed)) it->second.uses.push_back({at, reversed});
  };
  const int32_t n = nx(m, at);
  if (m.polygons[at].flags & 0x2000 && n == 4) add(vx(m, at, 0), vx(m, at, 2)), add(vx(m, at, 1), vx(m, at, 3));
  add(vx(m, at, 0), vx(m, at, 1));
  add(vx(m, at, n - 2), vx(m, at, n - 1));
  // Pirate's Moon 1.0 @ 0x00574870 / 0x00573bc0: the first point and the last one, listed where the map has them.
  for (const int32_t v : {vx(m, at, 0), vx(m, at, n - 1)}) {
    const auto it = s.vertices.find(v);
    if (it != s.vertices.end() && first_at(m.polygons[at], static_cast<uint32_t>(v)) < points(m.polygons[at]))
      it->second.polygons.push_back(at);
  }
}

}  // namespace

// Pirate's Moon 1.0 @ 0x00573340
void corner_swap(Model& m, int32_t polygon, int32_t i, int32_t j) {
  Polygon& p = m.polygons[polygon];
  std::swap(p.vertex_indices[i], p.vertex_indices[j]);
  std::swap(p.normal_indices[i], p.normal_indices[j]);
  for (int32_t l = 0; l < p.material_count; ++l) {
    std::swap(p.uvs[l][2 * i], p.uvs[l][2 * j]);
    std::swap(p.uvs[l][2 * i + 1], p.uvs[l][2 * j + 1]);
  }
  for (int k = 0; k < 3; ++k) std::swap(p.colors[3 * i + k], p.colors[3 * j + k]);
}

// Pirate's Moon 1.0 @ 0x00573700 (and its copies written out in 0x0056d260 / 0x005705a0)
void corners_reverse(Model& m, int32_t polygon) {
  const int32_t n = nx(m, polygon);
  for (int32_t i = (n >> 1) - 1; i >= 0; --i) corner_swap(m, polygon, i, n - i - 1);
}

// Pirate's Moon 1.0 @ 0x00573760 (from 0; 0x005705a0 writes one out from 1)
void corners_pairswap(Model& m, int32_t polygon, int32_t from) {
  for (int32_t i = from; i < nx(m, polygon) - 1; i += 2) corner_swap(m, polygon, i, i + 1);
}

// Pirate's Moon 1.0 @ 0x005737d0
void make_strip(Model& m, int32_t polygon, int32_t start) {
  const int32_t n = nx(m, polygon);
  std::vector<int32_t> order{start};  // the exe's 0x40-record array on the stack (joins keep n under 0x40)
  int32_t up = start, down = start;
  do {
    up = (up + 1) % n;
    down = (down + n - 1) % n;
    order.push_back(up);
    if (up == down) break;
    order.push_back(down);
  } while (down != (up + 1) % n);
  std::vector<StripCorner> corners;
  for (const int32_t k : order) corners.push_back(strip_corner(m, polygon, k));
  Polygon& p = m.polygons[polygon];
  for (int32_t k = 0; k < n; ++k) {  // Pirate's Moon 1.0 @ 0x00573aa0: point, normal, coordinates, colour
    const StripCorner& c = corners[static_cast<size_t>(k)];
    p.vertex_indices[k] = static_cast<uint32_t>(c.vertex);
    p.normal_indices[k] = static_cast<uint32_t>(c.normal);
    for (int32_t l = 0; l < p.material_count; ++l) p.uvs[l][2 * k] = c.uvs[l].first, p.uvs[l][2 * k + 1] = c.uvs[l].second;
    std::memcpy(p.colors + 3 * k, c.color, 12);
  }
  p.flags |= 0x2000;
}

// Pirate's Moon 1.0 @ 0x0056b580
int32_t strip_append(Model& m, int32_t x, int32_t y, int32_t k) {
  const int32_t at = m.count(0x10);
  resize_polygons(m, at + 1);
  set_count(m, at + 1);
  const Polygon& X = m.polygons[x];
  const Polygon& Y = m.polygons[y];
  Polygon& P = m.polygons[at];
  const uint32_t n = (X.flags + Y.flags + static_cast<uint32_t>(k) - 2) & 0x3ff;
  P.flags = n | (X.flags & 0x1c00) | 0x2000;
  P.priority = X.priority;
  P.zone_set = X.zone_set;
  P.material_count = X.material_count;
  P.materials = static_cast<::zmod::Material**>(std::malloc(sizeof(void*) * static_cast<size_t>(std::max(X.material_count, 1))));
  for (int32_t j = 0; j < X.material_count; ++j) P.materials[j] = X.materials[j];
  P.vertex_indices = static_cast<uint32_t*>(std::malloc(4 * std::max(n, 1u)));
  P.normal_indices = static_cast<uint32_t*>(std::malloc(4 * std::max(n, 1u)));
  P.uvs = static_cast<float**>(std::malloc(sizeof(float*) * static_cast<size_t>(std::max(P.material_count, 1))));
  for (int32_t j = 0; j < P.material_count; ++j) P.uvs[j] = static_cast<float*>(std::malloc(8 * std::max(n, 1u)));
  P.file_uvs = X.file_uvs;
  P.colors = static_cast<float*>(std::malloc(12 * std::max(n, 1u)));
  // The exe's +0x20 is malloc'd and only x's corners set; the rest is cleared here.
  P.scratch = static_cast<uint32_t*>(std::calloc(std::max(n, 1u), 4));
  uint32_t to = 0;
  const auto put = [&](const Polygon& from, uint32_t i, bool scratch) {
    P.vertex_indices[to] = from.vertex_indices[i];
    P.normal_indices[to] = from.normal_indices[i];
    for (int32_t j = 0; j < P.material_count; ++j)
      P.uvs[j][2 * to] = from.uvs[j][2 * i], P.uvs[j][2 * to + 1] = from.uvs[j][2 * i + 1];
    std::memcpy(P.colors + 3 * to, from.colors + 3 * i, 12);
    if (scratch && from.scratch) P.scratch[to] = from.scratch[i];
    ++to;
  };
  for (uint32_t i = 0; i < points(X); ++i) put(X, i, true);
  uint32_t i = static_cast<uint32_t>(2 - k);
  if (k == 3) put(Y, 0, false), i = 0;  // the first point twice: the turn in the winding
  for (; i < points(Y); ++i) put(Y, i, false);
  return at;
}

// Pirate's Moon 1.0 @ 0x0056d260. The strip cases follow join_shape's eight, read here on the strips the plain
// polygons have just become; then the plain joins (strip 0); a join nothing else takes appends a and b as they are.
int32_t join_polygons(Stripper& s, int32_t a, int32_t b, StripEdgeKey key, int32_t first, int32_t second,
                      int32_t strip) {
  Model& m = *s.model;
  const auto plain_to_strip = [&](int32_t p) {
    if (m.polygons[p].flags & 0x2000) return false;
    const int32_t n = nx(m, p);
    int32_t i = 0;
    while (i < n && edge_key(vx(m, p, i), vx(m, p, (i + 1) % n)) != key) ++i;
    make_strip(m, p, i < n ? i : 0);  // FIX (A): the exe starts past the end when the key is not an edge (never)
    return true;
  };
  const bool ap = plain_to_strip(a), bp = plain_to_strip(b);
  const auto quad = [&](int32_t p) {
    return m.polygons[p].flags & 0x2000 && nx(m, p) == 4 &&
           (edge_key(vx(m, p, 0), vx(m, p, 2)) == key || edge_key(vx(m, p, 1), vx(m, p, 3)) == key);
  };
  const bool aq = quad(a), bq = quad(b);
  if (aq) corner_swap(m, a, 1, 2);
  if (bq) corner_swap(m, b, 1, 2);
  const auto rev = [&](int32_t p) { corners_reverse(m, p); };
  const auto pairs = [&](int32_t p) { corners_pairswap(m, p); };
  const auto app = [&](int32_t x, int32_t y, int32_t k) { return strip_append(m, x, y, k); };
  const auto pick = [&](int32_t x) { return x == first || x == second; };
  if (strip == 1) {
    {  // a's last end on b's first
      const End a2 = last_end(m, a), b1 = first_end(m, b);
      if (a2.on(key) && b1.on(key) && pick(a2.to())) {
        if (a2.to() == b1.from()) {
          if (!aq) return app(a, b, 1);
          if (!odd(m, b)) return rev(b), rev(a), app(b, a, 1);
        }
        if (a2.to() == b1.to()) {
          if (!aq && bp) return pairs(b), app(a, b, 1);
          if (odd(m, b) && bp) return pairs(b), rev(b), rev(a), app(b, a, 1);
        }
      }
    }
    {  // b's last on a's first
      const End b2 = last_end(m, b), a1 = first_end(m, a);
      if (b2.on(key) && a1.on(key) && pick(b2.to())) {
        if (b2.to() == a1.from()) {
          if (!bq) return app(b, a, 1);
          if (!odd(m, a)) return rev(a), rev(b), app(a, b, 1);
        }
        if (b2.to() == a1.to()) {
          if (!bq && ap) return pairs(a), app(b, a, 1);
          if (odd(m, a) && ap) return pairs(a), rev(a), rev(b), app(a, b, 1);
        }
      }
    }
    {
      const End a2 = last_end(m, a), b1 = first_end(m, b);
      if (a2.on(key) && b1.on(key) && pick(a2.from())) {
        if (a2.from() == b1.from() && !bq && ap && !odd(m, b)) return pairs(a), rev(a), rev(b), app(b, a, 1);
        if (a2.from() == b1.to() && !bq && bp && odd(m, b) && ap)
          return pairs(b), rev(b), pairs(a), rev(a), app(b, a, 1);
      }
    }
    {
      const End b2 = last_end(m, b), a1 = first_end(m, a);
      if (b2.on(key) && a1.on(key) && pick(b2.from())) {
        if (b2.from() == a1.from() && !aq && bp && !odd(m, a)) return pairs(b), rev(b), rev(a), app(a, b, 1);
        if (b2.from() == a1.to() && !aq && ap && odd(m, a) && bp)
          return pairs(a), rev(a), pairs(b), rev(b), app(a, b, 1);
      }
    }
    {  // both last ends
      const End a2 = last_end(m, a), b2 = last_end(m, b);
      if (a2.on(key) && b2.on(key) && pick(a2.to())) {
        if (a2.to() == b2.to()) {
          if (!aq) return rev(b), app(a, b, 1);
          if (!bq) return rev(a), app(b, a, 1);
        } else if (a2.to() == b2.from() && !aq && bp) {
          return pairs(b), rev(b), app(a, b, 1);
        }
      }
      if (a2.on(key) && b2.on(key) && pick(a2.from()))
        if (a2.from() == b2.to() && !bq && ap) return pairs(a), rev(a), app(b, a, 1);
    }
    {  // both first ends
      const End a1 = first_end(m, a), b1 = first_end(m, b);
      if (a1.on(key) && b1.on(key) && pick(a1.to())) {
        if (a1.to() == b1.to()) {
          if (ap && odd(m, a)) {
            if (bp) return pairs(a), rev(a), pairs(b), app(a, b, 1);
          } else if (bp && odd(m, b) && ap) {
            return pairs(b), rev(b), pairs(a), app(b, a, 1);
          }
        } else if (a1.to() == b1.from()) {
          if (ap && odd(m, a)) return pairs(a), rev(a), app(a, b, 1);
          if (!bq && !odd(m, b) && ap) return pairs(a), rev(b), app(b, a, 1);
        }
      }
      if (a1.on(key) && b1.on(key) && pick(a1.from())) {
        if (a1.from() == b1.to()) {
          if (!aq && !odd(m, a)) {
            if (bp) return pairs(b), rev(a), app(a, b, 1);
          } else if (bp && odd(m, b)) {
            return pairs(b), rev(b), app(b, a, 1);
          }
        } else if (a1.from() == b1.from()) {
          if (!aq && !odd(m, a)) return rev(a), app(a, b, 1);
          if (!bq && !odd(m, b)) return rev(b), app(b, a, 1);
        }
      }
    }
  }
  // The plain joins (0x0056ff47 on). Pirate's Moon 1.0 @ 0x005731e0: two ends compared with their sense.
  const auto same = [](const End& x, const End& y) { return x.lo == y.lo && x.hi == y.hi && x.rev == y.rev; };
  {
    const End a2 = last_end(m, a), b1 = first_end(m, b);
    if (a2.on(key) && b1.on(key)) {
      if (!aq) {
        if (same(a2, b1)) return app(a, b, 0);
        if (bp) return pairs(b), app(a, b, 0);
        return app(a, b, 0);
      }
      if (same(a2, b1)) {
        if (!odd(m, b)) return rev(b), rev(a), app(b, a, 0);
        return app(a, b, 0);
      }
      if (odd(m, b) && bp) return pairs(b), rev(b), rev(a), app(b, a, 0);
      return app(a, b, 0);
    }
  }
  {
    const End b2 = last_end(m, b), a1 = first_end(m, a);
    if (b2.on(key) && a1.on(key)) {
      if (!bq) {
        if (same(a1, b2)) return app(b, a, 0);
        if (ap) return pairs(a), app(b, a, 0);
        return app(a, b, 0);
      }
      if (same(a1, b2)) {
        if (!odd(m, a)) return rev(a), rev(b), app(a, b, 0);
        return app(a, b, 0);
      }
      if (odd(m, a) && ap) return pairs(a), rev(a), rev(b), app(a, b, 0);
      return app(a, b, 0);
    }
  }
  {
    const End a2 = last_end(m, a), b2 = last_end(m, b);
    if (a2.on(key) && b2.on(key)) {
      if (same(b2, a2)) {
        if (!bq && ap) return rev(a), pairs(a), app(b, a, 0);
        if (!aq && bp) return rev(b), pairs(b), app(a, b, 0);
        return app(a, b, 0);
      }
      if (!aq) return rev(b), app(a, b, 0);
      return rev(a), app(b, a, 0);
    }
  }
  {
    const End a1 = first_end(m, a), b1 = first_end(m, b);
    if (a1.on(key) && b1.on(key)) {
      if (same(b1, a1)) {
        if (!aq && !odd(m, a) && bp) return rev(a), pairs(b), app(a, b, 0);
        if (!bq && !odd(m, b) && ap) return rev(b), pairs(a), app(b, a, 0);
        if (!aq && odd(m, a) && ap) return pairs(a), rev(a), app(a, b, 0);
        if (!bq && odd(m, b) && bp) return pairs(b), rev(b), app(b, a, 0);
        return app(a, b, 0);
      }
      if (!aq && !odd(m, a)) return rev(a), app(a, b, 0);
      if (!bq && !odd(m, b)) return rev(b), app(b, a, 0);
      if (!aq && ap && bp) return pairs(a), rev(a), pairs(b), app(a, b, 0);
    }
  }
  return app(a, b, 0);
}

// Pirate's Moon 1.0 @ 0x005746e0 / 0x0056d040
bool can_join_at(Stripper& s, int32_t a, int32_t b, int32_t v) {
  if (a == b) return false;
  const Model& m = *s.model;
  const Polygon& A = m.polygons[a];
  const Polygon& B = m.polygons[b];
  const int32_t na = nx(m, a), nb = nx(m, b);
  if (na + nb - 2 >= 0x41 || na + nb - 2 > s.arg) return false;
  s.slack = std::min({s.pass, s.arg - na - nb + 2, 0x42 - na - nb});
  if (A.material_count != B.material_count) return false;
  for (int32_t j = 0; j < A.material_count; ++j)
    if (A.materials[j] != B.materials[j]) return false;
  if (A.flags & 0x400 || B.flags & 0x400 || (A.flags ^ B.flags) & 0x800) return false;
  if (A.priority != B.priority || A.zone_set != B.zone_set) return false;
  // 0x0056d040: a plain polygon has the point at its first end only.
  const bool a_strip = A.flags & 0x2000, b_strip = B.flags & 0x2000;
  const int32_t a_first = a_strip ? vx(m, a, 0) : v, a_last = a_strip ? vx(m, a, na - 1) : -1;
  const int32_t b_first = b_strip ? vx(m, b, 0) : v, b_last = b_strip ? vx(m, b, nb - 1) : -1;
  const bool a_odd = A.flags & 1, b_odd = B.flags & 1;
  if (s.slack < 2) return false;
  if (v == a_first && v == b_first) return !a_odd || !b_odd;
  if (v == a_first && v == b_last) {
    if (!b_odd || !a_strip) return true;
  } else {
    if (v != a_last) return false;
    if (v == b_first) {
      if (!a_odd || !b_strip) return true;
    } else {
      if (v != b_last) return false;
      if (a_odd == b_odd) return true;
    }
  }
  return s.slack > 2;
}

// Pirate's Moon 1.0 @ 0x005705a0
int32_t join_at(Stripper& s, int32_t a, int32_t b, int32_t v) {
  Model& m = *s.model;
  const auto plain_to_strip = [&](int32_t p) {
    if (m.polygons[p].flags & 0x2000) return false;
    const int32_t n = nx(m, p);
    int32_t i = 0;
    while (i < n && vx(m, p, i) != v) ++i;
    make_strip(m, p, i < n ? i : 0);  // FIX (A): as join_polygons (the point is always there)
    return true;
  };
  const bool ap = plain_to_strip(a), bp = plain_to_strip(b);
  const int32_t na = nx(m, a), nb = nx(m, b);
  const auto app = [&](int32_t x, int32_t y, int32_t k) { return strip_append(m, x, y, k); };
  if (vx(m, a, 0) == v && vx(m, b, 0) == v) {
    if (!odd(m, a)) return corners_reverse(m, a), app(a, b, 2);
    if (!odd(m, b)) return corners_reverse(m, b), app(b, a, 2);
    return app(a, b, 0);
  }
  if (vx(m, a, 0) == v && vx(m, b, nb - 1) == v) {
    if (!odd(m, b)) return app(b, a, 2);
    if (ap) return corners_pairswap(m, a, 1), app(b, a, 2);
    return app(b, a, 3);
  }
  if (vx(m, a, na - 1) == v && vx(m, b, 0) == v) {
    if (!odd(m, a)) return app(a, b, 2);
    if (bp) return corners_pairswap(m, b, 1), app(a, b, 2);
    return app(a, b, 3);
  }
  if (vx(m, a, na - 1) == v && vx(m, b, nb - 1) == v) {
    if (odd(m, a) == odd(m, b)) return corners_reverse(m, b), app(a, b, 2);
    if (odd(m, a)) return corners_reverse(m, a), app(b, a, 3);
    return corners_reverse(m, b), app(a, b, 3);
  }
  return app(a, b, 0);
}

// Pirate's Moon 1.0 @ 0x00570b70
void strip_model(Model* m, int32_t arg, int32_t max_points) {
  Stripper s;
  if (!strip_intake(s, m, arg, max_points)) return;
  s.pass = 0;
  bool joined = false;
  do {
    joined = false;
    for (auto it = s.edges.begin(); it != s.edges.end(); ++it) {
      StripEdge& e = it->second;
      for (auto u1 = e.uses.begin(); u1 != e.uses.end();) {
        bool again = false;
        for (auto u2 = std::next(u1); u2 != e.uses.end(); ++u2) {
          int32_t first = 0, second = 0, strip = 0;
          if (!try_join(s, *u1, *u2, e.key, &first, &second, &strip)) continue;
          const int32_t a = u1->polygon, b = u2->polygon;
          joined = true;
          if (strip_hooks().joining) strip_hooks().joining(0, a, b, first, second, strip);
          after_join(s, a, b, join_polygons(s, a, b, it->first, first, second, strip));
          again = true;  // the same edge's list again from its start
          break;
        }
        u1 = again ? e.uses.begin() : std::next(u1);
      }
    }
    if (s.pass >= 2) {
      for (auto it = s.vertices.begin(); it != s.vertices.end(); ++it) {
        StripVertex& vt = it->second;
        for (auto u1 = vt.polygons.begin(); u1 != vt.polygons.end();) {
          bool again = false;
          for (auto u2 = std::next(u1); u2 != vt.polygons.end(); ++u2) {
            if (!can_join_at(s, *u1, *u2, vt.vertex)) continue;
            const int32_t a = *u1, b = *u2;
            joined = true;
            if (strip_hooks().joining) strip_hooks().joining(1, a, b, it->first, 0, 0);
            after_join(s, a, b, join_at(s, a, b, it->first));
            again = true;
            break;
          }
          u1 = again ? vt.polygons.begin() : std::next(u1);
        }
      }
    }
  } while (joined || ++s.pass < 4);
  StripCounts& c = strip_counts();
  for (int32_t i = 0; i < m->count(0x10); ++i) {
    const Polygon& p = m->polygons[i];
    if (!(p.flags & 0x2000)) {
      ++c.others;
      continue;
    }
    const int32_t t = static_cast<int32_t>(points(p)) - 2;
    ++c.strips, c.triangles += t, c.longest = std::max(c.longest, t);
    for (int32_t k = 1; k <= t; ++k) {
      const uint32_t* q = p.vertex_indices;
      if (q[k] == q[k - 1] || q[k + 1] == q[k - 1] || q[k + 1] == q[k]) ++c.degenerate;
    }
  }
  uint32_t flags;
  std::memcpy(&flags, m->header + 8, 4);
  flags |= 0x80;
  std::memcpy(m->header + 8, &flags, 4);
  sort_by_material(*m);
}

}  // namespace pm::zmod
