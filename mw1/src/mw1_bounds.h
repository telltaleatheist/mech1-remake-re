#pragma once
// FIXES 138: the mission area on every contract map (research/boundaries.md).
//
// MW3's maps (mw3c1..mw3c4) bring their own pair: the yellow reg_inside_* and the red reg_outside_*, drawn on the
// radar, the voice past the yellow and the mission lost past the red (their aiv.zrd, valves.zrd, objectives.zrd).
// PM's maps (c1..c4) and MW1's arctic (mw3c4b) have none, so MW1 gives them one of its own from data/mw1bounds.zrd,
// made to behave as MW3's: the same radar lines, the same "out_of_bounds" voice past the yellow, the same fail
// objective past the red. Its yellow is the convex hull of the map's start, every enemy place and every row's
// structure, pushed out 600 m; its red the yellow pushed out 500 m more (tools/mw1bounds.py, from the survey).
//
// The same file moves enemy places that a map puts outside its own area (mw3c4's southern ones): an enemy of the
// launch made near a MOVE's FROM is made at its TO instead, on the ground there. MW3's files are never changed.
#include <array>
#include <string>
#include <vector>

namespace mw1 {

inline constexpr const char* kBoundsFile = "mw1bounds.zrd";  // MW1's data folder

// The names MW1's own pair goes by: reg_inside_/reg_outside_ as MW3's, so the radar's pick (the player's vlist's last
// of each) takes them.
inline constexpr const char* kBoundInside = "reg_inside_mw1";
inline constexpr const char* kBoundOutside = "reg_outside_mw1";
inline constexpr const char* kBoundInsideValve = "inside_mw1";
inline constexpr const char* kBoundOutsideValve = "outside_mw1";
inline constexpr const char* kBoundSound = "out_of_bounds";  // MW3's and PM's sounds.zrd: the voice, with its subtitle
inline constexpr float kBoundMargin = 600.0f;        // the yellow past the outermost place
inline constexpr float kBoundRedMargin = 500.0f;     // the red past the yellow
inline constexpr float kMoveNear = 80.0f;            // an enemy within this of a MOVE's FROM is moved

using Point2 = std::array<float, 2>;  // x, z

struct Bound {
  std::string world;  // "c1", "mw3c4b"
  int32_t ia = 0;
  std::vector<Point2> yellow, red;  // closed outlines, in MW3's region order (clockwise in x, z)
};
struct PlaceMove {
  std::string world;
  int32_t ia = 0;
  Point2 from{}, to{};
};
struct Bounds {
  std::vector<Bound> bounds;
  std::vector<PlaceMove> moves;
};

// BOUNDS (WORLD ("c1") IA (1) YELLOW ((x z) ...) RED ((x z) ...)) and MOVE (WORLD (..) IA (n) FROM (x z) TO (x z));
// entries that don't read are left out and said in `why`.
Bounds parse_bounds(const std::string& zrd_path, std::string* why = nullptr);
void set_bounds(Bounds b);
const Bounds& bounds();
const Bound* bound_for(const std::string& world, int32_t ia);
// The place an enemy made at (x, z) on that map goes to instead (the nearest MOVE's TO within kMoveNear of its FROM,
// shifted as it lies from FROM: a place beside it keeps its side); false: it stays.
bool moved_place(const std::string& world, int32_t ia, float x, float z, float* to_x, float* to_z);

// The geometry (the game's Regions test the same): inside a closed outline, and how far inside (the distance to its
// nearest side; negative outside).
bool inside_outline(const std::vector<Point2>& outline, float x, float z);
float depth_inside(const std::vector<Point2>& outline, float x, float z);

}  // namespace mw1
