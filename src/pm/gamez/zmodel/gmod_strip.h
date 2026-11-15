#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "pm/gamez/zmodel/gmod_const.h"

// Pirate's Moon's triangle stripper (docs/pm-tests.md, PM's triangle stripper). The original file is unknown: it
// sits between branchep1\gamez\zmodel\gmod_puff.cpp and zreader\zrdr_main.cpp, so it is named for what it does.
// Nothing in play reaches it (pm-map.md, The models' form): preserved, unwired. The exe's MSVC containers are the
// port's std::map / std::list with the exe's keys, comparisons and insert positions.
namespace pm::zmod {

// Tests: 0x005329e0, the textures' images loaded by name (null: nothing; the port's images are loaded already).
struct StripHooks {
  void (*load_images)() = nullptr;
  // Tests: each join as the loop makes it (0 across an edge, 1 at a point), before the polygons are turned.
  void (*joining)(int kind, int32_t a, int32_t b, int32_t first, int32_t second, int32_t strip) = nullptr;
};
StripHooks& strip_hooks();

// 0x00572b40: `b` the same vertex loop as `a` (the same count and back face, b's points a's turned).
bool same_loop(const Polygon& a, const Polygon& b);
// 0x00572c20: b's materials and texture coordinates after a's, the coordinates turned to a's first point.
void append_layers(Polygon& a, const Polygon& b);
// 0x00572df0: each textured polygon whose first image is solid and material opaque takes, from the others on the
// same loop, their layers (up to 8 materials); a polygon given away is freed and the model's last moved into its
// slot. The material refs are left as they were (pm-tests.md).
void merge_layers(Model& m);

// An edge as the stripper keys it: the lower point first (0x00574e90).
using StripEdgeKey = std::pair<int32_t, int32_t>;
struct StripEdgeUse {    // 0x00573f30's, 8 bytes in a list node
  int32_t polygon;
  bool reversed;         // the polygon runs the edge from its higher point
};
struct StripEdge {       // 0x14 bytes, new'd: the key, then a list
  StripEdgeKey key;
  std::list<StripEdgeUse> uses;
};
struct StripVertex {     // 0x10 bytes, new'd: the point, then a list
  int32_t vertex;
  std::list<int32_t> polygons;
};
struct Stripper {
  Model* model = nullptr;                       // [0x008c14fc]
  int32_t arg = 0;                              // [0x008c14f4], the caller's second argument
  std::map<StripEdgeKey, StripEdge> edges;      // 0x00570b70's [esp+0x50]
  std::map<int32_t, StripVertex> vertices;      // [esp+0x6c]
  int32_t pass = 0;                             // [0x008c1500]: the growth pass, 0 to 2
  int32_t slack = 0;                            // [0x008c14f8]: how far a join may still grow (try_join)
};
struct StripCounts {
  int32_t polygons = 0;  // [0x008c1504]: every polygon of every model stripped
  int32_t taken = 0;     // [0x008c1508]: those given to the maps
  int32_t strips = 0;    // [0x008c150c]: strips left at the end
  int32_t others = 0;    // [0x008c1510]: other polygons left
  int32_t triangles = 0; // [0x008c1514]: the strips' triangles
  int32_t degenerate = 0;  // [0x008c1518]: of those, ones with a repeated point
  int32_t longest = 0;   // [0x008c151c]: the most triangles in one strip
};
StripCounts& strip_counts();

// 0x00573bf0: whether `polygon` has the edge where the stripper looks for it, and which way it runs: a strip's
// first and last edges (a four-point strip's diagonals first), each edge of any other polygon from its last.
bool edge_in_polygon(const Model& m, int32_t polygon, StripEdgeKey key, bool* reversed);
// 0x00570b70 to 0x00570ef7: a type-0 model with polygons has its layers merged, then each polygon with texture
// coordinates and normals (and a strip, or at most `max_points` points) goes into the edge and point maps. False
// for a model the stripper refuses.
bool strip_intake(Stripper& s, Model* m, int32_t arg, int32_t max_points);

// A polygon's corner as the join compares it (0x005735a0's 0x24-byte record).
struct StripCorner {
  int32_t vertex, normal;
  std::vector<std::pair<float, float>> uvs;  // each material's at the corner
  float color[3];
};
StripCorner strip_corner(const Model& m, int32_t polygon, int32_t at);  // 0x005735a0
// 0x0056b2d0: each layer's whole-number shift from b's coordinates to a's (rounded from the difference), into
// `shifts`; false when one is more than 0.05 off a whole number, or a clamped layer (`clamped`) needs one.
bool uv_shifts(const std::vector<char>& clamped, const std::vector<std::pair<float, float>>& a,
               const std::vector<std::pair<float, float>>& b, std::vector<std::pair<float, float>>& shifts);
// 0x0056b980: how two polygons on `key` would join: as a strip at one end (*strip 1, *end 1 or 2: the end at
// `first` or at `second`), or else as one polygon (*strip 0, when `same_shift`); false when neither.
bool join_shape(const Stripper& s, int32_t a, int32_t b, StripEdgeKey key, int32_t first, int32_t second,
                bool same_shift, int32_t* strip, int32_t* end);
// 0x00573f50: whether polygon `a.polygon` may take `b.polygon` across `key` (run the other way, the same materials,
// flags, priority and zone, the shared corners alike up to whole texture repeats); sets s.slack, and on yes shifts
// b's coordinates onto a's and gives the corners that matched (-1 for none) and join_shape's answer.
bool try_join(Stripper& s, const StripEdgeUse& a, const StripEdgeUse& b, StripEdgeKey key, int32_t* first,
              int32_t* second, int32_t* strip);

// The corner moves the joins make, on a polygon's points, normals, coordinates and colours (its +0x20 stays put).
void corner_swap(Model& m, int32_t polygon, int32_t i, int32_t j);  // 0x00573340
void corners_reverse(Model& m, int32_t polygon);                     // 0x00573700
void corners_pairswap(Model& m, int32_t polygon, int32_t from = 0);  // 0x00573760: (0,1), (2,3)...
// 0x005737d0: a polygon's loop as a strip from corner `start` (start, start+1, start-1, start+2...), flag 0x2000.
void make_strip(Model& m, int32_t polygon, int32_t start);
// 0x0056b580: a new strip appended, x's corners then y's from its third (k 0), second (1), first (2), or its first
// twice (3); x's materials, priority, zone and flags. Its index.
int32_t strip_append(Model& m, int32_t x, int32_t y, int32_t k);
// 0x0056d260: a and b joined across `key` as try_join allowed (`strip` its answer): both turned and put in order,
// the new strip appended (a and b are left for the caller to remove). Its index.
int32_t join_polygons(Stripper& s, int32_t a, int32_t b, StripEdgeKey key, int32_t first, int32_t second,
                      int32_t strip);
// 0x005746e0 with 0x0056d040: whether two polygons may join at point `v` alone (pass 2 on): the counts, materials,
// flags, priority and zone as try_join asks, then the point at the right ends for their parities. Sets s.slack.
bool can_join_at(Stripper& s, int32_t a, int32_t b, int32_t v);
// 0x005705a0: a and b joined at point `v` (a plain polygon made a strip from the point first). The new index.
int32_t join_at(Stripper& s, int32_t a, int32_t b, int32_t v);
// 0x00570b70: the whole stripper on a model of type 0 with polygons: the intake, then passes 0 to 3 of joins
// across edges (and from pass 2 at points), each pass again while it joined anything; then the counts, flag 0x80
// and the polygons sorted by material (0x0055c470).
void strip_model(Model* m, int32_t arg, int32_t max_points);

}  // namespace pm::zmod
