#pragma once
// Where a contract's structure stands for the player (FIXES 134; research/maps.md, "Where the structures stand"):
// measures on the ground plane between the player's start A, the enemies' centre B (of every enemy place the map has)
// and the structure (or point) C. Pure geometry with the ground as a callback: no engine, no SDL. The map survey
// (tests, mw1_loop_every_map_row_is_there_headless) gates the map table on some of them; the rest are measured for
// choosing a point or a structure by the same scores later (FIXES 136).
#include <functional>
#include <vector>

namespace mw1::placement {

struct Point {
  float x = 0, z = 0;
};

// The ground's height at (x, z): the highest surface there, NaN where there is none.
using Ground = std::function<float(float x, float z)>;

struct Measures {
  Point enemy_centre;          // B
  int enemies = 0;             // the enemy places counted
  float start_target = -1;     // |AC|
  float target_enemies = -1;   // |CB|
  float start_enemies = -1;    // |AB|
  float along = 0;             // C on the way from A (0) to B (1): AC projected on AB, over |AB|
  float spread = 0;            // the farthest enemy place from B
  float nearest_to_target = -1, nearest_to_start = -1;  // the nearest enemy place to C, to A
  float nearest_to_line = -1;  // the nearest enemy place to the segment A->C
  float angle_at_start = 0;    // degrees between A->B and A->C
  float centre_to_line = -1;   // the closest approach of the segment A->C to B
  float race = -1;             // |AC| / |BC|: under 1, the player is nearer the structure than the enemies' centre
  float ridge = 0;             // the highest ground along B -> the midpoint of A->C above the straight line between
                               // their ground heights (0 when the ground is flat or below it; needs `ground`)
};

// The measures for start A, structure C and the enemy places; `ground` may be empty (no ridge).
Measures measure(Point start, Point target, const std::vector<Point>& enemies, const Ground& ground = {});

// The reach objective's radius: a reach row is won within this of its structure's point (mw1_game.cpp's
// ReachAreaObjective; FIXES.md item 45: the worst row's closest stand, mw3c2's tower at 33 m, and a margin).
inline constexpr float kReachRadius = 40.0f;

// FIXES 136: can the player reach C from A by keeping away from the enemy? A path on a grid (A*, eight neighbours)
// from A to the first cell within kReachRadius of C (where the game counts the reach won), every cell of it at least
// `clearance` from every enemy place but those within `clearance` of A (the start's own: how near the enemy the start
// is, is start_clear, a rule of its own), and inside the mission area (FIXES 138's yellow). With how long it is
// against the straight line to the reach's edge. The steepest step along the path is measured from `ground` (when
// given), not yet a rule.
using Inside = std::function<bool(float x, float z)>;

struct RouteRules {
  float clearance = 200;          // the way there, from every enemy place
  float reach = kReachRadius;     // the goal: any cell this near C
  float cell = 20;                // the grid's step
  float margin = 1500;            // the grid's reach past the start, the goal and the enemies (`inside` bounds it)
};

struct Route {
  bool found = false;
  float length = -1;          // the path's length, A to its end
  float detour = -1;          // length over the straight line from A to the reach's edge (|AC| - reach)
  float start_clear = -1;     // the nearest enemy place to A
  float goal_clear = -1;      // to C itself
  float end_clear = -1;       // to the path's end (within `reach` of C)
  float path_clear = -1;      // to the path past the start's own disc
  float steepest = 0;         // the steepest rise or fall along the path, height over distance, per cell
  bool start_inside = true, goal_inside = true;
  int expanded = 0;           // cells the search took
  std::vector<Point> path;    // A .. the end, a point a cell
};

// `inside` may be empty (no area: the grid's box only).
Route route(Point start, Point goal, const std::vector<Point>& enemies, const Inside& inside, const RouteRules& rules = {},
            const Ground& ground = {});

}  // namespace mw1::placement
