// mw3stl: a development tool, not part of the port. Writes each mech of the player's own mechlib.zbd as an STL for
// 3D printing, posed from its motions, read by the ported loaders through the viewer's scene code. Nothing of the
// game's goes in the repo: the files are made locally from the player's install.
//   mw3stl --assets <zbd dir> --parts <mech>     the part tree, with sizes
//   mw3stl --assets <zbd dir> --stats            holes, panels and sizes of every mech
//   mw3stl --assets <zbd dir> --frames <mech> <motion>
//   mw3stl --assets <zbd dir> --out <dir> [--scale 100] [--twist 25] [--pitch 3] [--no-base] [--smooth 0..3 [--sharp deg]] [--raw] [mech ...]
// The stance: the walk's widest stride with both soles down, the torso twisted and the arms raised; holes closed,
// single-sided panels given 0.8 mm, a bevelled hex base under the feet. Millimetres, Z up, facing -Y. --smooth
// subdivides where the game's own normals smooth the shading, keeping its hard edges.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gamez/zutil/zutl_zar.h"
#include "scene.h"

namespace {

using Vec = std::array<float, 3>;

std::vector<std::string> mech_names(const std::string& zbd) {
  std::vector<std::string> names;
  zutl_zar::Archive a;
  zutl_zar::init(a);
  if (zutl_zar::open(a, (zbd + "/mechlib.zbd").c_str()))
    for (int i = 0; i < a.count; ++i) {
      std::string n = a.entries[i].name;
      if (n.rfind("mech_", 0) != 0 || n.size() < 9 || n.substr(n.size() - 4) != ".flt") continue;
      n = n.substr(5, n.size() - 9);
      if (n.find('_') == std::string::npos) names.push_back(n);
    }
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  return names;
}

Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec cross(const Vec& a, const Vec& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
float dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void bounds(const std::vector<Vec>& tris, Vec& lo, Vec& hi) {
  lo = {1e9f, 1e9f, 1e9f}, hi = {-1e9f, -1e9f, -1e9f};
  for (const Vec& v : tris)
    for (int a = 0; a < 3; ++a) lo[a] = std::min(lo[a], v[a]), hi[a] = std::max(hi[a], v[a]);
}

int list_parts(const std::string& zbd, const std::string& mech) {
  viewer::Mech m;
  const std::string err = m.load(zbd, "mech_" + mech + ".flt");
  if (!err.empty()) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  std::vector<Vec> all;
  for (const viewer::Mech::Part& p : m.parts(nullptr)) {
    Vec lo, hi;
    bounds(p.triangles, lo, hi);
    const int two = static_cast<int>(std::count(p.two_sided.begin(), p.two_sided.end(), true));
    std::printf("%-20s <- %-16s t=(%6.2f %6.2f %6.2f) tris %4zu two-sided %3d", p.name.c_str(), p.parent.c_str(),
                p.local[9], p.local[10], p.local[11], p.triangles.size() / 3, two);
    if (!p.triangles.empty()) std::printf("  y %6.2f..%6.2f", lo[1], hi[1]);
    std::printf("\n");
    all.insert(all.end(), p.triangles.begin(), p.triangles.end());
  }
  Vec lo, hi;
  bounds(all, lo, hi);
  std::printf("whole: x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f, %zu triangles\n", lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
              all.size() / 3);
  return 0;
}

// Edges used by one triangle only (after welding corners within 1 mm), and the signed volume (positive: the
// corners wind outward, counter-clockwise from outside).
struct Shape {
  int open = 0;
  int crowded = 0;     // edges of more than two faces
  int unbalanced = 0;  // edges used more often one way round than the other: not sealed (Cura's X-ray sees these)
  double volume = 0;
};
Shape shape(const std::vector<Vec>& tris) {
  std::map<std::array<long, 3>, int> id;
  auto key = [&](const Vec& v) {
    const std::array<long, 3> k{std::lround(v[0] * 1000), std::lround(v[1] * 1000), std::lround(v[2] * 1000)};
    return id.try_emplace(k, static_cast<int>(id.size())).first->second;
  };
  std::map<std::pair<int, int>, int> edges, way;  // way: uses u->v minus v->u, u < v
  Shape s;
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    const int a = key(tris[t]), b = key(tris[t + 1]), c = key(tris[t + 2]);
    for (auto [u, v] : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}})
      if (u != v) ++edges[{std::min(u, v), std::max(u, v)}], way[{std::min(u, v), std::max(u, v)}] += u < v ? 1 : -1;
    const Vec &p = tris[t], &q = tris[t + 1], &r = tris[t + 2];
    s.volume += (p[0] * (q[1] * r[2] - q[2] * r[1]) - p[1] * (q[0] * r[2] - q[2] * r[0]) +
                 p[2] * (q[0] * r[1] - q[1] * r[0])) / 6.0;
  }
  for (const auto& e : edges) s.open += e.second == 1, s.crowded += e.second > 2;
  for (const auto& w : way) s.unbalanced += w.second != 0;
  return s;
}

int stats(const std::string& zbd) {
  for (const std::string& mech : mech_names(zbd)) {
    viewer::Mech m;
    if (!m.load(zbd, "mech_" + mech + ".flt").empty()) continue;
    std::vector<Vec> all;
    int two = 0, open_parts = 0, parts = 0, neg = 0, open = 0;
    for (const viewer::Mech::Part& p : m.parts(nullptr)) {
      if (p.triangles.empty()) continue;
      ++parts;
      two += static_cast<int>(std::count(p.two_sided.begin(), p.two_sided.end(), true));
      const Shape s = shape(p.triangles);
      open += s.open, open_parts += s.open > 0, neg += s.volume < 0;
      all.insert(all.end(), p.triangles.begin(), p.triangles.end());
    }
    Vec lo, hi;
    bounds(all, lo, hi);
    std::printf("%-12s h %5.2f w %5.2f d %5.2f  tris %4zu  parts %2d  two-sided %3d  open edges %4d in %2d parts  "
                "inside-out %d\n", mech.c_str(), hi[1] - lo[1], hi[0] - lo[0], hi[2] - lo[2], all.size() / 3, parts, two,
                open, open_parts, neg);
  }
  return 0;
}

// The lowest point and the front-back middle of a foot ("lfoot") and its toes; a foot drawn as part of its calf
// (the Elemental's) is its calf ("lcalf").
struct Foot {
  float low = 0, z = 0;
};
Foot foot(const std::vector<viewer::Mech::Part>& parts, const std::string& root) {
  std::vector<Vec> tris;
  for (const std::string& r : {root, root.substr(0, 1) + "calf"}) {
    for (const viewer::Mech::Part& p : parts)
      if (p.name == r || p.parent == r) tris.insert(tris.end(), p.triangles.begin(), p.triangles.end());
    if (!tris.empty()) break;
  }
  Foot f;
  if (tris.empty()) return f;
  Vec lo, hi;
  bounds(tris, lo, hi);
  f.low = lo[1], f.z = (lo[2] + hi[2]) / 2;
  return f;
}

int frames(const std::string& zbd, const std::string& mech, const std::string& motion) {
  viewer::Mech m;
  viewer::Motion mo;
  if (!m.load(zbd, "mech_" + mech + ".flt").empty() || !mo.load(zbd, mech + "_" + motion).empty()) return 1;
  viewer::Pose pose;
  mo.pose(0, pose);
  std::printf("moves:");
  for (const auto& e : pose) std::printf(" %s", e.first.c_str());
  std::printf("\nloop %.2f s\n", mo.loop_time());
  for (int i = 0; i < 24; ++i) {
    const float t = mo.loop_time() * i / 24;
    pose.clear();
    mo.pose(t, pose);
    const auto parts = m.parts(&pose);
    const Foot l = foot(parts, "lfoot"), r = foot(parts, "rfoot");
    std::printf("t %.2f  lfoot low %5.2f z %5.2f  rfoot low %5.2f z %5.2f  stride %.2f\n", t, l.low, l.z, r.low, r.z,
                std::fabs(l.z - r.z));
  }
  return 0;
}

// Pre-multiplies a part's local transform by a rotation about its own axis (row vectors: p * (R * M) turns p in the
// part's own frame first), the way the game's torso twist and arm pitch turn a part about its pivot.
void turn(std::array<float, 12>& m, int axis, float degrees) {
  const float a = degrees * 3.14159265f / 180, c = std::cos(a), s = std::sin(a);
  float r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  if (axis == 0) r[4] = c, r[5] = s, r[7] = -s, r[8] = c;
  else r[0] = c, r[2] = -s, r[6] = s, r[8] = c;
  float out[9];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) out[3 * i + j] = r[3 * i] * m[j] + r[3 * i + 1] * m[3 + j] + r[3 * i + 2] * m[6 + j];
  std::copy(out, out + 9, m.begin());
}

// Both feet flat: each foot turned to lie as it does at rest (level, pointing ahead) under its parent, and its toes
// back to their rest pose. The walk's stride stays; only the soles are levelled, so a print stands on both.
void flatten_feet(const viewer::Mech& m, viewer::Pose& pose) {
  std::map<std::string, std::array<float, 9>> rest_axes, axes_now;
  std::map<std::string, std::string> parent_of;
  const auto rest = m.parts(nullptr);
  for (const auto& p : rest) {
    rest_axes[p.name] = p.axes, parent_of[p.name] = p.parent;
    if (p.name.find("toe") != std::string::npos) pose[p.name] = p.local;
  }
  for (const auto& p : m.parts(&pose)) axes_now[p.name] = p.axes;
  for (const char* f : {"lfoot", "rfoot"}) {
    if (!pose.count(f) || !axes_now.count(parent_of[f])) continue;
    const auto& want = rest_axes[f];  // world = local * parent, so local = world * parent^T
    const auto& par = axes_now[parent_of[f]];
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        pose[f][3 * i + j] = want[3 * i] * par[3 * j] + want[3 * i + 1] * par[3 * j + 1] + want[3 * i + 2] * par[3 * j + 2];
  }
}

// The firing stance: the walk's widest stride with both levelled soles at nearly one height, the torso twisted and
// the arms aimed. `low` is the higher sole (the base's top), `sink` how far the other goes into the base.
viewer::Pose stance(const viewer::Mech& m, const std::string& zbd, const std::string& mech, float twist, float pitch,
                    float* low, float* sink) {
  viewer::Pose pose, best;
  viewer::Motion walk;
  float score = -1e9f;
  *low = 0, *sink = 0;
  if (walk.load(zbd, mech + "_walk").empty())
    for (int i = 0; i < 48; ++i) {
      pose.clear();
      walk.pose(walk.loop_time() * i / 48, pose);
      flatten_feet(m, pose);
      const auto parts = m.parts(&pose);
      const Foot l = foot(parts, "lfoot"), r = foot(parts, "rfoot");
      // A wide stride with both levelled soles at one height (what sinks into the base): 1 m of stride is worth
      // 5 cm of sink, and more than 15 cm is out.
      const float dl = std::fabs(l.low - r.low), sc = std::fabs(l.z - r.z) - (dl > 0.15f ? 100 : 0) - 20 * dl;
      if (sc > score) score = sc, best = pose, *low = std::max(l.low, r.low), *sink = dl;
    }
  for (const viewer::Mech::Part& p : m.parts(nullptr))
    if (!best.count(p.name)) best[p.name] = p.local;
  if (best.count("torso")) turn(best["torso"], 1, twist);
  // Each arm pitched about its shoulder until its weapons' line (the arm's pivot to its firepoints, where the game
  // spawns the shots) rises `pitch` degrees: level fire, whatever the arm's rest angle.
  for (const char* arm : {"larm", "rarm"}) {
    if (!best.count(arm)) continue;
    for (int pass = 0; pass < 4; ++pass) {
      const auto parts = m.parts(&best);
      std::map<std::string, std::string> parent;
      std::map<std::string, Vec> origin;
      Vec d{0, 0, 0};
      int n = 0;
      for (const auto& p : parts) parent[p.name] = p.parent, origin[p.name] = p.origin;
      for (const auto& p : parts) {  // the barrel: a firepoint's parent (the hand) to the firepoint
        if (p.name.rfind("firepoint", 0) != 0) continue;
        for (std::string up = p.parent; !up.empty(); up = parent[up])
          if (up == arm) {
            const Vec b = sub(p.origin, origin[p.parent]);
            for (int a = 0; a < 3; ++a) d[a] += b[a];
            ++n;
            break;
          }
      }
      if (n == 0) break;  // no weapon on this arm: as it stands
      const float now = std::atan2(d[1], std::hypot(d[0], d[2])) * 180 / 3.14159265f;
      if (std::fabs(now - pitch) < 0.5f || std::fabs(now - pitch) > 60) break;  // aimed, or no sensible line
      turn(best[arm], 0, pitch - now);
    }
  }
  if (score == -1e9f) {  // no walk: the model's own pose, soles at its lowest point
    Vec lo, hi;
    std::vector<Vec> all;
    for (const auto& p : m.parts(&best)) all.insert(all.end(), p.triangles.begin(), p.triangles.end());
    bounds(all, lo, hi);
    *low = lo[1];
  }
  return best;
}

// Drops doubled faces: the same three corners twice the same way round keeps one; twice opposite ways (a sheet of
// no thickness) keeps neither. Some models store a face twice (the Thor's torso).
int drop_doubles(std::vector<Vec>& tris) {
  auto key = [](const Vec& v) {
    return std::array<long, 3>{std::lround(v[0] * 1000), std::lround(v[1] * 1000), std::lround(v[2] * 1000)};
  };
  using K = std::array<long, 3>;
  std::map<std::array<K, 3>, int> seen;  // corners in order from the smallest, value: +1 / -1 per winding
  std::vector<std::array<K, 3>> order;
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    std::array<K, 3> c{key(tris[t]), key(tris[t + 1]), key(tris[t + 2])};
    const int m = static_cast<int>(std::min_element(c.begin(), c.end()) - c.begin());
    std::rotate(c.begin(), c.begin() + m, c.end());
    order.push_back(c);
  }
  std::map<std::array<K, 3>, int> count;
  for (const auto& c : order) ++count[c];
  std::vector<Vec> out;
  std::map<std::array<K, 3>, int> kept;
  int dropped = 0;
  for (size_t i = 0; i < order.size(); ++i) {
    const auto& c = order[i];
    const std::array<K, 3> rev{c[0], c[2], c[1]};
    if (count.count(rev) && count[rev] > 0 && rev != c) { ++dropped; continue; }  // a face and its back: neither
    if (kept[c]++ > 0) { ++dropped; continue; }
    out.insert(out.end(), tris.begin() + 3 * i, tris.begin() + 3 * i + 3);
  }
  tris = std::move(out);
  return dropped;
}

// Closes each part's holes (the faces the game never drew, hidden inside the next part) with a fan from each
// boundary loop's middle, keeping the winding outward.
int close_holes(std::vector<Vec>& tris) {
  std::map<std::array<long, 3>, int> id;
  std::vector<Vec> at;
  auto key = [&](const Vec& v) {
    const std::array<long, 3> k{std::lround(v[0] * 1000), std::lround(v[1] * 1000), std::lround(v[2] * 1000)};
    auto [it, added] = id.try_emplace(k, static_cast<int>(at.size()));
    if (added) at.push_back(v);
    return it->second;
  };
  std::map<std::pair<int, int>, int> directed;
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    const int a = key(tris[t]), b = key(tris[t + 1]), c = key(tris[t + 2]);
    if (a == b || b == c || c == a) continue;
    ++directed[{a, b}], ++directed[{b, c}], ++directed[{c, a}];
  }
  // A boundary edge a->b has more uses than b->a (none, or fewer where three faces meet on one edge, as in the
  // Thor's torso notch); the hole runs b->a, once for each unmatched use.
  std::multimap<int, int> next;
  for (const auto& [e, n] : directed) {
    const auto back = directed.find({e.second, e.first});
    for (int k = n - (back == directed.end() ? 0 : back->second); k > 0; --k) next.emplace(e.second, e.first);
  }
  int loops = 0;
  while (!next.empty()) {
    std::vector<int> loop{next.begin()->first};
    int v = next.begin()->second;
    next.erase(next.begin());
    while (v != loop[0] && loop.size() < 4096) {
      loop.push_back(v);
      auto it = next.find(v);
      if (it == next.end()) break;
      v = it->second;
      next.erase(it);
    }
    if (v != loop[0] || loop.size() < 3) continue;  // not closed: left to the slicer
    Vec mid{0, 0, 0};
    for (int i : loop)
      for (int a = 0; a < 3; ++a) mid[a] += at[i][a] / static_cast<float>(loop.size());
    for (size_t i = 0; i < loop.size(); ++i)
      tris.push_back(at[loop[i]]), tris.push_back(at[loop[(i + 1) % loop.size()]]), tris.push_back(mid);
    ++loops;
  }
  return loops;
}

// A single-sided panel as a slab `thick` deep, centred on it.
void slab(const Vec& p, const Vec& q, const Vec& r, float thick, std::vector<Vec>& out) {
  Vec n = cross(sub(q, p), sub(r, p));
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (len <= 0) return;
  for (float& v : n) v *= thick / 2 / len;
  const Vec c[3] = {p, q, r};
  Vec top[3], bot[3];
  for (int i = 0; i < 3; ++i)
    for (int a = 0; a < 3; ++a) top[i][a] = c[i][a] + n[a], bot[i][a] = c[i][a] - n[a];
  out.insert(out.end(), {top[0], top[1], top[2], bot[0], bot[2], bot[1]});
  for (int i = 0; i < 3; ++i) {
    const int j = (i + 1) % 3;
    out.insert(out.end(), {bot[i], bot[j], top[j], bot[i], top[j], top[i]});
  }
}

// Loop subdivision with creases, `levels` times: each triangle becomes four, smooth where the game smooths. An edge
// stays sharp (a crease) where the game shades it hard: the two triangles' normals differ at either end (the
// artist's split), either has none (no flag 0x200, or a hole's cap), or it is not shared by exactly two.
std::vector<Vec> smooth(const std::vector<Vec>& tris, const std::vector<Vec>& normals, int levels, float sharp) {
  using Edge = std::pair<int, int>;
  auto edge = [](int a, int b) { return a < b ? Edge{a, b} : Edge{b, a}; };
  std::vector<Vec> v;
  std::vector<std::array<int, 3>> t;
  std::map<std::array<long, 3>, int> id;
  for (const Vec& p : tris) {
    const std::array<long, 3> k{std::lround(p[0] * 1000), std::lround(p[1] * 1000), std::lround(p[2] * 1000)};
    if (id.try_emplace(k, static_cast<int>(v.size())).second) v.push_back(p);
  }
  auto at = [&](const Vec& p) {
    return id[{std::lround(p[0] * 1000), std::lround(p[1] * 1000), std::lround(p[2] * 1000)}];
  };
  // Each edge's triangles, with the normals at its two ends as each triangle has them.
  struct Side {
    Vec nu, nv, face;  // face: the triangle's own normal
  };
  std::map<Edge, std::vector<Side>> sides;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    const std::array<int, 3> c{at(tris[i]), at(tris[i + 1]), at(tris[i + 2])};
    if (c[0] == c[1] || c[1] == c[2] || c[2] == c[0]) continue;
    t.push_back(c);
    for (int e = 0; e < 3; ++e) {
      const int a = c[e], b = c[(e + 1) % 3];
      const Vec& na = normals[i + e];
      const Vec& nb = normals[i + (e + 1) % 3];
      const Vec f = cross(sub(tris[i + 1], tris[i]), sub(tris[i + 2], tris[i]));
      sides[edge(a, b)].push_back(a < b ? Side{na, nb, f} : Side{nb, na, f});
    }
  }
  auto same = [](const Vec& a, const Vec& b) {
    const float la = std::sqrt(dot(a, a)), lb = std::sqrt(dot(b, b));
    return la > 0 && lb > 0 && dot(a, b) / (la * lb) > 0.996f;  // within about 5 degrees
  };
  std::map<Edge, bool> crease;
  // With `sharp` > 0, also every edge whose faces meet at more than `sharp` degrees: the shape's own corners (the
  // Sunder's square legs, the Mad Cat's faceted canopy), which the game shades smooth but built square.
  const float bend = sharp > 0 ? std::cos(sharp * 3.14159265f / 180) : -2;
  auto folded = [&](const Vec& a, const Vec& b) {
    const float la = std::sqrt(dot(a, a)), lb = std::sqrt(dot(b, b));
    return la > 0 && lb > 0 && dot(a, b) / (la * lb) < bend;
  };
  for (const auto& [e, list] : sides)
    crease[e] = list.size() != 2 || !same(list[0].nu, list[1].nu) || !same(list[0].nv, list[1].nv) ||
                folded(list[0].face, list[1].face);
  for (int level = 0; level < levels; ++level) {
    std::map<Edge, std::vector<int>> opp;
    for (const auto& c : t)
      for (int e = 0; e < 3; ++e) opp[edge(c[e], c[(e + 1) % 3])].push_back(c[(e + 2) % 3]);
    std::vector<std::vector<int>> nb(v.size()), cn(v.size());
    for (const auto& [e, o] : opp) {
      nb[e.first].push_back(e.second), nb[e.second].push_back(e.first);
      const auto it = crease.find(e);
      if (o.size() != 2 || (it != crease.end() && it->second))
        cn[e.first].push_back(e.second), cn[e.second].push_back(e.first);
    }
    std::vector<Vec> nv(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
      const auto n = nb[i].size();
      if (cn[i].size() > 2 || n < 3) nv[i] = v[i];  // a corner: stays
      else if (cn[i].size() == 2)
        for (int a = 0; a < 3; ++a) nv[i][a] = 0.75f * v[i][a] + 0.125f * (v[cn[i][0]][a] + v[cn[i][1]][a]);
      else {
        const float beta = n == 3 ? 3.0f / 16 : 3.0f / (8 * static_cast<float>(n));
        for (int a = 0; a < 3; ++a) {
          float sum = 0;
          for (int j : nb[i]) sum += v[j][a];
          nv[i][a] = (1 - static_cast<float>(n) * beta) * v[i][a] + beta * sum;
        }
      }
    }
    std::map<Edge, int> mid;
    std::map<Edge, bool> next_crease;
    for (const auto& [e, o] : opp) {
      const auto it = crease.find(e);
      const bool sharp = o.size() != 2 || (it != crease.end() && it->second);
      Vec m;
      for (int a = 0; a < 3; ++a)
        m[a] = sharp ? (v[e.first][a] + v[e.second][a]) / 2
                     : 0.375f * (v[e.first][a] + v[e.second][a]) + 0.125f * (v[o[0]][a] + v[o[1]][a]);
      mid[e] = static_cast<int>(nv.size());
      nv.push_back(m);
      next_crease[edge(e.first, mid[e])] = sharp, next_crease[edge(mid[e], e.second)] = sharp;
    }
    std::vector<std::array<int, 3>> nt;
    for (const auto& c : t) {
      const int ab = mid[edge(c[0], c[1])], bc = mid[edge(c[1], c[2])], ca = mid[edge(c[2], c[0])];
      nt.push_back({c[0], ab, ca}), nt.push_back({c[1], bc, ab}), nt.push_back({c[2], ca, bc}), nt.push_back({ab, bc, ca});
    }
    v = std::move(nv), t = std::move(nt), crease = std::move(next_crease);
  }
  std::vector<Vec> out;
  for (const auto& c : t) out.insert(out.end(), {v[c[0]], v[c[1]], v[c[2]]});
  return out;
}

// A group of parts that touches nothing else gets a strut to the body: a six-sided rod `radius` thick between the
// two closest corners. The Vulture's gun pods hang free of its body, which
// the screen hides and a print cannot.
struct Piece {
  std::string name;
  std::vector<Vec> tris;
  Vec lo, hi;
};
void rod(const Vec& a, const Vec& b, float radius, std::vector<Vec>& out) {
  const Vec axis = sub(b, a);
  const float len = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
  if (len <= 0) return;
  Vec u = std::fabs(axis[1]) < 0.9f * len ? cross(axis, Vec{0, 1, 0}) : cross(axis, Vec{1, 0, 0});
  const float ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  for (float& c : u) c /= ul;
  Vec w = cross(axis, u);
  for (float& c : w) c /= len;
  Vec ra[6], rb[6];
  for (int i = 0; i < 6; ++i) {
    const float t = i * 3.14159265f / 3, c = std::cos(t) * radius, s = std::sin(t) * radius;
    for (int k = 0; k < 3; ++k) ra[i][k] = a[k] + u[k] * c + w[k] * s, rb[i][k] = b[k] + u[k] * c + w[k] * s;
  }
  for (int i = 0; i < 6; ++i) {
    const int j = (i + 1) % 6;
    out.insert(out.end(), {ra[i], rb[j], rb[i], ra[i], ra[j], rb[j], a, ra[j], ra[i], b, rb[i], rb[j]});
  }
}
// Segment p..q through triangle a, b, c (Moller-Trumbore, t in [0, 1]).
bool crosses(const Vec& p, const Vec& q, const Vec& a, const Vec& b, const Vec& c) {
  const Vec d = sub(q, p), e1 = sub(b, a), e2 = sub(c, a), h = cross(d, e2);
  const float det = dot(e1, h);
  if (std::fabs(det) < 1e-9f) return false;
  const Vec s = sub(p, a);
  const float u = dot(s, h) / det;
  if (u < 0 || u > 1) return false;
  const Vec qq = cross(s, e1);
  const float v = dot(d, qq) / det;
  if (v < 0 || u + v > 1) return false;
  const float t = dot(e2, qq) / det;
  return t >= 0 && t <= 1;
}

// Inside a closed part: a ray along +x crosses it an odd number of times.
bool inside(const Vec& p, const Piece& m) {
  if (p[0] < m.lo[0] || p[0] > m.hi[0] || p[1] < m.lo[1] || p[1] > m.hi[1] || p[2] < m.lo[2] || p[2] > m.hi[2])
    return false;
  const Vec far{m.hi[0] + 1, p[1] + 1e-4f, p[2] + 2e-4f};
  int n = 0;
  for (size_t t = 0; t + 2 < m.tris.size(); t += 3) n += crosses(p, far, m.tris[t], m.tris[t + 1], m.tris[t + 2]);
  return n % 2 == 1;
}

// Two parts touch: a corner of one inside the other, or an edge of one through a face of the other.
bool touch(const Piece& a, const Piece& b, float gap) {
  for (int k = 0; k < 3; ++k)
    if (a.lo[k] > b.hi[k] + gap || b.lo[k] > a.hi[k] + gap) return false;
  for (const Vec& v : a.tris)
    if (inside(v, b)) return true;
  for (const Vec& v : b.tris)
    if (inside(v, a)) return true;
  for (const Piece* x : {&a, &b}) {
    const Piece& y = x == &a ? b : a;
    for (size_t t = 0; t + 2 < x->tris.size(); t += 3)
      for (int e = 0; e < 3; ++e)
        for (size_t u = 0; u + 2 < y.tris.size(); u += 3)
          if (crosses(x->tris[t + e], x->tris[t + (e + 1) % 3], y.tris[u], y.tris[u + 1], y.tris[u + 2])) return true;
  }
  for (const Vec& v : a.tris)  // corners within `gap` of each other: a seam the slicer closes
    for (const Vec& w : b.tris) {
      const Vec d = sub(v, w);
      if (dot(d, d) < gap * gap) return true;
    }
  return false;
}

int struts(std::vector<Piece>& pieces, float gap, float radius, std::vector<Vec>& out, std::string* names) {
  // Groups of parts that touch (boxes within `gap`); the biggest is the body, every other group gets a strut to it.
  std::vector<int> group(pieces.size());
  for (size_t i = 0; i < pieces.size(); ++i) group[i] = static_cast<int>(i);
  auto root = [&](int i) {
    while (group[i] != i) i = group[i] = group[group[i]];
    return i;
  };
  for (size_t i = 0; i < pieces.size(); ++i)
    for (size_t j = i + 1; j < pieces.size(); ++j) {
      if (touch(pieces[i], pieces[j], gap)) group[root(static_cast<int>(i))] = root(static_cast<int>(j));
    }
  int added = 0;
  for (;;) {
    std::map<int, size_t> size;
    for (size_t i = 0; i < pieces.size(); ++i) size[root(static_cast<int>(i))] += pieces[i].tris.size();
    if (size.size() <= 1) break;
    int body = size.begin()->first;
    for (const auto& [g, n] : size)
      if (n > size[body]) body = g;
    int loose = -1;
    for (const auto& [g, n] : size)
      if (g != body) loose = g;
    float best = 1e9f;
    Vec pa{}, pb{};
    std::string name;
    for (size_t i = 0; i < pieces.size(); ++i) {
      if (root(static_cast<int>(i)) != loose) continue;
      if (name.empty()) name = pieces[i].name;
      for (size_t j = 0; j < pieces.size(); ++j) {
        if (root(static_cast<int>(j)) != body) continue;
        for (const Vec& a : pieces[i].tris)
          for (const Vec& b : pieces[j].tris) {
            const Vec d = sub(a, b);
            const float dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (dd < best) best = dd, pa = a, pb = b;
          }
      }
    }
    // Each end sunk a radius into its part, so the rod fuses with both.
    const float l = std::sqrt(best);
    Vec a = pa, b = pb;
    if (l > 0) {
      const Vec ab = sub(pb, pa);
      for (int k = 0; k < 3; ++k) a[k] -= ab[k] / l * radius, b[k] += ab[k] / l * radius;
    }
    rod(a, b, radius, out);
    *names += " " + name;
    ++added;
    group[loose] = body;
  }
  return added;
}

// A bevelled hex, flat side to the front, its top at y = `top`.
void hex_base(float cx, float cz, float radius, float top, float height, float bevel, std::vector<Vec>& out) {
  Vec up[6], down[6];
  for (int i = 0; i < 6; ++i) {
    const float a = (60.0f * i) * 3.14159265f / 180;
    up[i] = {cx + (radius - bevel) * std::cos(a), top, cz + (radius - bevel) * std::sin(a)};
    down[i] = {cx + radius * std::cos(a), top - height, cz + radius * std::sin(a)};
  }
  const Vec cu{cx, top, cz}, cd{cx, top - height, cz};
  for (int i = 0; i < 6; ++i) {
    const int j = (i + 1) % 6;
    out.insert(out.end(), {cu, up[j], up[i], cd, down[i], down[j]});
    out.insert(out.end(), {down[i], up[i], up[j], down[i], up[j], down[j]});
  }
}

// Binary STL in millimetres, Z up, the mech facing -Y (a slicer's front): (x, y, z) -> (-x, z, y), a turn, so the
// winding stays outward.
bool write_stl(const std::string& path, const std::vector<Vec>& tris, float mm, const std::string& title) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  char head[80] = {};
  std::snprintf(head, sizeof head, "%s", title.c_str());
  std::fwrite(head, 1, 80, f);
  const auto n = static_cast<uint32_t>(tris.size() / 3);
  std::fwrite(&n, 4, 1, f);
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    Vec v[3];
    for (int i = 0; i < 3; ++i) v[i] = {-tris[t + i][0] * mm, tris[t + i][2] * mm, tris[t + i][1] * mm};
    Vec nrm = cross(sub(v[1], v[0]), sub(v[2], v[0]));
    const float len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len > 0)
      for (float& c : nrm) c /= len;
    std::fwrite(nrm.data(), 4, 3, f);
    for (const Vec& c : v) std::fwrite(c.data(), 4, 3, f);
    const uint16_t attr = 0;
    std::fwrite(&attr, 2, 1, f);
  }
  return std::fclose(f) == 0;
}

struct Options {
  float scale = 100;  // 1:scale, game metres to millimetres
  float twist = 25, pitch = 3;
  bool base = true;
  int smooth = 0;  // Loop subdivision levels (each: triangles x4)
  float sharp = 0;  // with smooth: edges folded more than this many degrees stay sharp (0: the game's normals only)
  bool raw = false;  // the polygons as the game has them: no hole closed, no strut, no base (to look at, not print)
};

int export_mech(const std::string& zbd, const std::string& mech, const std::string& dir, const Options& o) {
  viewer::Mech m;
  const std::string err = m.load(zbd, "mech_" + mech + ".flt");
  if (!err.empty()) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  float low = 0, sink = 0;
  const viewer::Pose pose = stance(m, zbd, mech, o.twist, o.pitch, &low, &sink);
  const float mm = 1000 / o.scale, thick = 0.8f / mm;  // panels 0.8 mm deep as printed
  std::vector<Vec> all, feet, calves;
  std::vector<Piece> pieces;
  std::string leaky;
  int doubles = 0, crowded = 0;
  int loops = 0, panels = 0, open = 0;
  const std::vector<viewer::Mech::Part> posed = m.parts(&pose);
  std::set<std::string> foot_parts;  // the feet and toes, and every part under one (parents come first)
  for (const viewer::Mech::Part& p : posed)
    if (p.name.find("foot") != std::string::npos || p.name.find("toe") != std::string::npos || foot_parts.count(p.parent))
      foot_parts.insert(p.name);
  for (const viewer::Mech::Part& p : posed) {
    if (p.triangles.empty()) continue;
    std::vector<Vec> solid, normals;
    for (size_t t = 0; t < p.two_sided.size(); ++t)
      if (p.two_sided[t]) slab(p.triangles[3 * t], p.triangles[3 * t + 1], p.triangles[3 * t + 2], thick, all), ++panels;
      else {
        solid.insert(solid.end(), p.triangles.begin() + 3 * t, p.triangles.begin() + 3 * t + 3);
        normals.insert(normals.end(), p.normals.begin() + 3 * t, p.normals.begin() + 3 * t + 3);
      }
    if (!o.raw) {
      std::vector<Vec> before = solid;
      doubles += drop_doubles(solid);
      if (solid.size() != before.size()) {  // keep the normals with their triangles
        std::map<std::array<long, 9>, size_t> where;
        auto k9 = [](const Vec* t) {
          std::array<long, 9> k{};
          for (int i = 0; i < 9; ++i) k[i] = std::lround(t[i / 3][i % 3] * 1000);
          return k;
        };
        for (size_t t = 0; t + 2 < before.size(); t += 3) where.emplace(k9(&before[t]), t);
        std::vector<Vec> kept_normals;
        for (size_t t = 0; t + 2 < solid.size(); t += 3) {
          const size_t from = where[k9(&solid[t])];
          kept_normals.insert(kept_normals.end(), normals.begin() + from, normals.begin() + from + 3);
        }
        normals = std::move(kept_normals);
      }
      loops += close_holes(solid);
    }
    normals.resize(solid.size(), Vec{0, 0, 0});  // the caps: no normals, so their rims stay sharp
    if (o.smooth > 0) solid = smooth(solid, normals, o.smooth, o.sharp);
    const Shape sh = shape(solid);
    if (sh.unbalanced > 0) leaky += " " + p.name + "(" + std::to_string(sh.unbalanced) + ")";
    open += sh.unbalanced, crowded += sh.crowded;
    Piece piece{p.name, solid, {}, {}};
    bounds(solid, piece.lo, piece.hi);
    pieces.push_back(std::move(piece));
    // A foot's mesh can hang below its node (Pirate's Moon's Centurion: rfoot draws through g107), so its children
    // count too.
    if (foot_parts.count(p.name)) feet.insert(feet.end(), solid.begin(), solid.end());
    if (p.name.find("calf") != std::string::npos) calves.insert(calves.end(), solid.begin(), solid.end());
    all.insert(all.end(), solid.begin(), solid.end());
  }
  std::string loose;
  const int rods = o.raw ? 0 : struts(pieces, 0.02f, 1.0f / mm, all, &loose);
  Vec lo, hi;
  bounds(all, lo, hi);
  if (o.base) {
    if (feet.empty()) feet = calves.empty() ? all : calves;
    Vec flo, fhi;
    bounds(feet, flo, fhi);
    const float cx = (flo[0] + fhi[0]) / 2, cz = (flo[2] + fhi[2]) / 2;
    float reach = 0;
    for (const Vec& v : feet) reach = std::max(reach, std::hypot(v[0] - cx, v[2] - cz));
    const float radius = (reach + 4 / mm) / std::cos(3.14159265f / 6);  // the feet inside the flats, 4 mm spare
    hex_base(cx, cz, radius, low, 4 / mm, 1.2f / mm, all);
  }
  // Recentred over the origin, the base's bottom (or the soles) on the bed.
  bounds(all, lo, hi);
  const float cx = (lo[0] + hi[0]) / 2, cz = (lo[2] + hi[2]) / 2;
  for (Vec& v : all) v[0] -= cx, v[1] -= lo[1], v[2] -= cz;
  const std::string path = dir + "/" + mech + ".stl";
  if (!write_stl(path, all, mm, "mw3stl " + mech)) return std::fprintf(stderr, "could not write %s\n", path.c_str()), 1;
  std::printf("%-12s feet flat, %.1f mm sunk  ", mech.c_str(), sink * mm);
  std::printf("%-12s %6.1f mm tall  %5zu triangles  %3d holes closed  %d doubled faces dropped  %2d panels thickened  "
              "%d struts%s  left: %d unsealed edges%s%s (%d shared by 3+ faces, sealed)\n", mech.c_str(), (hi[1] - lo[1]) * mm, all.size() / 3,
              loops, doubles, panels, rods, loose.c_str(), open, leaky.empty() ? "" : " in", leaky.c_str(), crowded);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string zbd = "game/mw3-1.2/zbd", out, parts;
  std::vector<std::string> only;
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--assets" && i + 1 < argc) zbd = argv[++i];
    else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else if (a == "--parts" && i + 1 < argc) parts = argv[++i];
    else if (a == "--stats") return stats(zbd);
    else if (a == "--scale" && i + 1 < argc) opt.scale = std::stof(argv[++i]);
    else if (a == "--twist" && i + 1 < argc) opt.twist = std::stof(argv[++i]);
    else if (a == "--pitch" && i + 1 < argc) opt.pitch = std::stof(argv[++i]);
    else if (a == "--no-base") opt.base = false;
    else if (a == "--raw") opt.raw = true, opt.base = false;
    else if (a == "--smooth" && i + 1 < argc) opt.smooth = std::stoi(argv[++i]);
    else if (a == "--sharp" && i + 1 < argc) opt.sharp = std::stof(argv[++i]);
    else if (a == "--frames" && i + 2 < argc) return frames(zbd, argv[i + 1], argv[i + 2]);
    else only.push_back(a);
  }
  if (!parts.empty()) return list_parts(zbd, parts);
  if (out.empty()) {
    for (const std::string& n : mech_names(zbd)) std::printf("%s\n", n.c_str());
    return 0;
  }
  int bad = 0;
  for (const std::string& n : only.empty() ? mech_names(zbd) : only) bad += export_mech(zbd, n, out, opt);
  return bad != 0;
}
