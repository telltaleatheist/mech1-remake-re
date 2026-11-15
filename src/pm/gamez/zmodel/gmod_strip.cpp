#include "pm/gamez/zmodel/gmod_strip.h"

#include <cstdlib>
#include <cstring>
#include <vector>

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

}  // namespace pm::zmod
