# Mission boundaries on the contract maps (FIXES 138, the survey)

What every map of the contract table (`data/mw1maps.zrd`, 26 maps, 48 rows) does
today about its boundary, measured headless in the port by
`mw1_loop_boundary_survey_headless` (`MW1_BOUNDARY_SURVEY=1`,
`MW1_BOUNDARY_TABLE=<file>` for the table and its `.detail`). Each row is a
contract on that row, with every enemy place the map has filled (the line-up
of `mw1_loop_every_map_row_is_there_headless`). At frame 30 it reads:

- every region loaded whose name starts `reg_inside_` or `reg_outside_`;
- the pair the radar takes (the player's vlist, `mech3/app_hud.cpp`) and what
  the radar draws;
- each boundary vlist entry's valve and the reactions `valves.zrd` gave it;
- whether the start, every enemy and the row's structure lie inside each line.

Then it puts the player 40 m past the yellow line, short of the red (240
frames), then 40 m past the red (300 frames), and logs every valve and
objective that changes and whether the mission ends. A map with neither
region puts the player 5 km out instead. "Nearest crossing" is the shortest
of 16 bearings from the start.

## How MW3's maps do it (the rule already works there)

The player's `aiv.zrd` record carries vlist entries on both regions:

- **Yellow** (`reg_inside_*`): leaving it sets the valve `inside_*` to 1, whose
  reaction is a sound. This is MW3's "leaving mission area" voice, which plays
  once a mission (docs/discoveries.md, the Op 2 Mission 3 tunnel). The defend
  maps (ia3) attach two sounds. No objective changes and the mission goes on.
- **Red** (`reg_outside_*`): leaving it sets `outside_*` to 1. The fail group's
  `valve_change` "Left Mission Boundaries" watches that valve, so the mission
  is lost on the next frame.
- The radar draws both outlines from the mission start, the inner yellow and
  the outer red, closed (corners + 1 points).

MW1's objectives keep MW3's fail block on every row kind (any, reach,
attack, defend).

## The table

| Map | Rows | Yellow / red regions (corners; extent x, z) | Radar | Past yellow | Past red | Nearest crossing from the start (yellow / red) | Start | Enemy places | Structure |
|---|---|---|---|---|---|---|---|---|---|
| mw3c1 ia1 | any | `reg_inside_ia1` 4; x −2325..3133, z −4132..300 / `reg_outside_ia1` 4; x −3506..3533, z −5068..700 | both | sound | lost | 1930 / 2330 m | in | all in | – |
| mw3c1 ia2 | reach, attack iatower | same | both | sound | lost | 1930 / 2330 m | in | all in | in |
| mw3c1 ia3 | any, defend iatower | same | both | 2 sounds | lost | 1970 / 2910 m | in | all in | in |
| mw3c2 ia1 | any | `reg_inside_ia2` 4; x −3351..1250, z −4186..1415 / `reg_outside_ia2` 4; x −3851..1750, z −4686..1915 | both | sound | lost | 1980 / 2480 m | in | all in | – |
| mw3c2 ia2 | reach, attack iatower | same | both | sound | lost | 1550 / 2050 m | in | all in | in |
| mw3c2 ia3 | any, defend iatower | same | both | 2 sounds | lost | 2010 / 2510 m | in | all in | in |
| mw3c3 ia1 | any | `reg_inside_c3m3` 9; x −3438..2578, z −2759..1946 / `reg_outside_c3m3` 9; x −3920..2890, z −3075..2288 (mission 3's) | both | sound | lost | 1830 / 2320 m | in | all in | – |
| mw3c3 ia2 | reach, attack iatower | `reg_inside_ia3` 4; x −3742..1729, z −5281..−832 / `reg_outside_ia3` 4; x −4192..2179, z −5731..−382 | both | sound | lost | 1750 / 2200 m | in | all in | in |
| mw3c3 ia3 | any, defend iatower | same as ia2 | both | 2 sounds | lost | 2220 / 2670 m | in | all in | in |
| mw3c4 ia1 | any | `reg_inside_ia4` 6; x −2298..2379, z 126..2786 / `reg_outside_ia4` 9; x −2965..3125, z 56..3348 | both | sound | lost | 860 / 1420 m | in | 11 in; **Enemy_01 (1122, 181) outside yellow, inside red** | – |
| mw3c4 ia2 | reach, attack iatower | same | both | sound | lost | 860 / 1420 m | in | all in | in |
| mw3c4 ia3 | any, defend iatower | same | both | 2 sounds | lost | 680 / 880 m | in | 8 in; **Enemy_05 (1042, −154), _08 (1045, −252), _09 (1022, −330), _12 (943, −375) outside both** | in |
| c1 ia1, ia2, ia3 | any; attack hitme_twr; any, defend pship_kill | none | nothing | – | nothing (5 km out) | – | – | – | – |
| c2 ia1, ia2, ia3 | any, reach 3twr1; reach, attack portalgate; any, defend mp_tent_campb, tcab05 | none | nothing | – | nothing (5 km out) | – | – | – | – |
| c3 ia1, ia2, ia3 | any, reach xslid01; attack pspwrsrc01; any, defend pspwrsrc01 | none | nothing | – | nothing (5 km out) | – | – | – | – |
| c4 ia1, ia2, ia3 | any, reach modA140; attack warehouse_target40; any, defend warehouse_target40 | none | nothing | – | nothing (5 km out) | – | – | – | – |
| mw3c4b ia1, ia2 | any, reach, attack, defend (ware01..03) | none (MW1's own arctic maps) | nothing | – | nothing (5 km out) | – | – | – | – |

## Notes

- **mw3c4's south side.** The yellow's south side runs from (2294, 126) to
  (−1149, 493) and the red's from (3084, 297) to (1025, 56). Below the
  moved enemies (x ≈ 1000) they lie about 200 m apart (yellow z ≈ 264, red
  z ≈ 56). An earlier reading of the boxes gave 70 m, which was wrong. At the
  yellow's corner (2294, 126) the yellow pokes about 78 m outside the red:
  there a player can cross the red, and lose, without crossing the yellow
  first. That is MW3's data and is left as it is. Four of ia3's enemy places
  stood south of both lines, so those enemies started beyond the red; ia1's
  Enemy_01 started between the lines and its Enemy_03 28 m inside the
  yellow. FIXES 138 moves all six (below). The start (1020, 934) and the
  tower (1179, 896) are 650 m or more from the south side, so no route the
  mission forces runs along it.
- **The maps with no boundary** (PM's c1..c4, MW1's mw3c4b) need an area. What
  it must hold (the start, every enemy place, every row's structure), as the
  survey placed them:

  | Map | x | z | Farthest from the start |
  |---|---|---|---|
  | c1 ia1 | 296..4104 | −4327..−907 | 3798 m |
  | c1 ia2 | 3842..4677 | −4672..−1068 | 3611 m |
  | c1 ia3 | 430..2369 | −4035..−916 | 2069 m |
  | c2 ia1 | 554..2604 | −2861..−308 | 2218 m |
  | c2 ia2 | 737..2703 | −3492..−661 | 3123 m |
  | c2 ia3 | 884..2603 | −1718..−419 | 1209 m |
  | c3 ia1 | −407..3451 | −2926..−457 | 2820 m |
  | c3 ia2 | −404..1771 | −3272..−899 | 2964 m |
  | c3 ia3 | −848..2070 | −3921..−1490 | 2048 m |
  | c4 ia1 | 666..3530 | −3453..−1228 | 2049 m |
  | c4 ia2 | 604..2487 | −3706..−818 | 3061 m |
  | c4 ia3 | 666..3530 | −3066..−1843 | 1899 m |
  | mw3c4b ia1 | −600..2030 | −961..997 | 2633 m |
  | mw3c4b ia2 | −677..2526 | −961..1029 | 2414 m |

  PM's own AI has no sense of an edge (docs/pm-tests.md), so on these maps an
  enemy can walk out of any area we draw.
- **Not measured:** whether the yellow voice is heard (the run is silent; the
  sound action fired with a loaded sound handle).

## The AI and the lines

MW3's boundary pull (0x004c3000; Pirate's Moon's `inout_pull`, 0x004ccaf0)
never reads the `reg_` regions. It reads the world's `inout` outlines (from
its `inoutia` node, `zainet_inout`): red ones push a destination off an
obstacle, and blue ones (wound the other way) keep it inside. Sampled at
frame 30:
- some PM maps carry a blue outer edge (c2 ia3, c4 ia1);
- others carry only obstacles (c1 ia1, c3 ia2), as MW3's maps do;
- the arctic has no inout area.

Nothing keeps the AI inside the `reg_` lines, on MW3's maps or ours. A
project decision (2026-11-21): nothing is built to keep enemies in. The maps
need only their out-of-bounds areas.

## The rule (FIXES 138)

- **MW3's maps** keep their own pair, voice and fail as they are.
- **PM's maps and the arctic** get MW1's pair from `data/mw1bounds.zrd`
  (`src/mw1_bounds.h`), named `reg_inside_mw1` and `reg_outside_mw1` so the
  radar takes them. At the launch (`add_boundary`) the two regions join the
  AI's regions, and the player's vlist gets MW3's three entries:
  - outside the yellow sets `inside_mw1` to 1, which plays `out_of_bounds`
    (PM's sounds.zrd has it, with its subtitle);
  - back inside sets it to 0;
  - outside the red sets `outside_mw1` to 1.

  `block_read` adds the fail item, MW3's "Left Mission Boundaries"
  (`MSG_OBJ_LEFT_MISSION_BOUNDARIES`, `valve_change` on `outside_mw1` at 1).
  Then the radar's 0x0043d920 is run again.
- **The areas:** `tools/mw1bounds.py` builds them from this survey's detail.
  Yellow has 16 sides, their normals 22.5° apart, each 600 m past the
  farthest of the map's places in its direction. An enemy's place counts as
  60 m around it, where the launch may put another beside it.
- **The places are the map's candidates, not one run's draw.** The launch
  picks each enemy's and ally's record at random among the map's records of
  its weight class (`action_lineup`, `crt_rand() % n`), and the generator is
  seeded from the clock (`srand(time())`, as the original). So two survey
  runs made different line-ups (c1 ia3: 21 places in one, 19 in the next), and
  areas drawn from them differed. The survey now logs every record the
  launch may pick (`candidate enemy|ally NAME (x, z)`: aiv.zrd's records from
  the third, mobile HQs aside, on its side, a MOVE applied), and the tool reads
  only the start, the structures and the candidates; the run's own enemy and
  ally lines are not read. The same data gives the same file: `mw1bounds.py
  --repeat <detail>` makes it twice, the second from the logs and their rows in
  reverse, and compares them byte for byte; `mw1bounds.py --launched <detail>
  ...` checks that every enemy and ally some runs made lies at least 600 m
  inside the file's yellow. Red is each
  side 500 m further out. The radar's outline holds at most 20 corners
  (ztwod's PolyLine, `pts[21]`).
- **mw3c4's places:** an enemy made within 80 m of a MOVE's FROM is made as
  far from its TO, on the highest dry ground there (`enemy_launching`):
  - ia1's two go north, at least 150 m inside;
  - ia3's four go two east and two west along the south side, at least
    150 m inside. They stay 1.0–1.45 km from the tower that ia3's own fail
    block watches (`iatowv2`), as they were 1.05–1.28 km before.

  Brought straight north (430 m from the tower), they took it down while a
  test's player stood idle. Where they are now, they still come for it: with
  the player standing and never firing, the tower fell after 1,589 frames
  against 4,965 with the map's own places. Those four southern enemies stood
  beyond the red all mission and never joined.
- **A fix on the way:** a fail group MW1 makes (a map whose objectives have
  none, as the arctic's) now fails on any one item, as `read_objectives`
  makes a file's. Before this, the arctic's defence never lost to the
  structure's fall alone.

The tests:
- `mw1_loop_boundary_survey_headless` checks every row (with
  `MW1_BOUNDARY_SURVEY=1`; by default c2 ia3, mw3c4 ia3 and mw3c4b ia2):
  - both regions, drawn;
  - every place inside the yellow: on MW1's areas by 595 m (the 600 m margin,
    less what the enemies walk in the first 30 frames), on MW3's by 150 m;
  - past the yellow the voice, and play goes on;
  - past the red the mission is lost.
