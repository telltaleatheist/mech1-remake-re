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

- **mw3c4's boundary is tight in the south.** The yellow line's south edge is
  at z 126 and the red's at z 56, a 70 m band. From ia3's start the yellow is
  680 m off and the red 880 m. Four of ia3's enemy places stand south of
  both lines, so those enemies start beyond the red. ia1's Enemy_01 starts
  between the lines. MW3's own AI is pulled back by the boundary
  (`zai_attack`, 0x004c3000), but a mission placing them there means an enemy
  can start, or be fought, outside the area.
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
  sound action fired with a loaded sound handle), and what MW3's AI does near
  the lines.
