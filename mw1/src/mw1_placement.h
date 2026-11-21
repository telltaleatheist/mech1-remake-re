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

}  // namespace mw1::placement
