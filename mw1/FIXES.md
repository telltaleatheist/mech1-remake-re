# Fixes from Owen's first play (PC, 2026-11-17)

Owen's list from playing the Windows build. One line each, then what it needs.
Tick an item when it is merged into `main`.

**How this file works (Owen, 2026-11-17):** every bug and feature Owen raises goes here, numbered, the moment he raises it. mw1 dev 1 works through the open items until all are done. The order is set by the lead at the bottom ("Order"). Each item is ticked `[x]`, with its branch, when it's merged into `mw1`. `[-]` means superseded.

## 140. [x] MW3's worlds have no mission map: wire MW3's own maps.zrd *(done: mw1-maps-140; in 1.0.8)*

- **Found (2026-11-22, item 136's point search):** on MW3's worlds (mw3c1..mw3c4) Shift+R shows only the plain radar,
  for structure rows and point rows alike; PM's worlds draw their mission map (their maps.zrd). The nav point still
  shows on the radar, in the OP line and its arrow, which is enough for now (the lead).
- **Wanted, later:** MW3's own maps.zrd wired for its converted worlds, so the radar's map mode draws them as PM's do.
  Then 136's "on the mission map's image" filter can apply to MW3's worlds too (today it applies to PM's only).
- **Agreed (2026-11-22, a project decision):** MW1 side only, no engine change. mw3c3 ia1 takes MW3's c3m3 (its area
  lies off c3ia), the arctic (mw3c4b) MW3's c4m4 (its start and all three structures on it), the others their own
  cNia. The two point rows off their map (mw3c2 ia1, ia2) chosen again with the on-map filter and all of 136's rules;
  the two structure rows off theirs (mw3c1 ia2's barracks, mw3c3 ia2's warehouses) stay, led by their radar nav.
  c1ia and c2ia have a positive scale: MW3's half-turned compass and reversed radar cone kept as MW3 has them.
- **Done (`mw1-maps-140`, `src/mw1_mission_maps.h`):**
  - **The maps:** at the start (once PM's list is read) MW3's entries are added after PM's from MW3's own maps.zrd
    (its `zbd/reader.zbd`, read as the engine reads PM's), renamed `mw3c1ia`, `mw3c2ia`, `mw3c3ia`, `mw3c3m3`,
    `mw3c4ia` and `mw3c4b`, each with its `_small`: 12.
  - **The pictures:** the image provider (`mw1_art.cpp`) serves those names from MW3's `zbd/rimage.zbd` (the
    library's layout, `zimg::read_texture`), with or without MW1's art archive. MW3's files are only read.
  - **The pick:** at each launch (`mission_launched`) the world's map is made the mission's (the radar's map mode,
    the waypoints, the lance's map pick). On PM's worlds nothing changes: their briefing picks their own (c2: c2m3),
    and any of ours its pick loaded (PM's `c1ia` is inside `mw3c1ia`) is let go.
  - **The rows:** mw3c2 ia1 (−730, −286), 3/3 arrived, 38/5/8%; mw3c2 ia2 (−1791, −793), 3/3, 18/22/44% (the
    seventh of eight the search kept: the six before it either stall 30 to 120 m short in the west's ground, which
    the grid walks and the 'Mech can't, or go down). Both 150 m and more inside MW3's own yellow.
  - **Tests:** `mw1_loop_each_mw3_world_s_map_picked_and_the_start_on_it_headless`: every table row on MW3's worlds
    (32 launches) has its map, its picture read from MW3's install and the start on it, and its objective on it but
    for the two structures above; PM's c2 asks for its own pictures only. The point search's filter
    (`MW1_POINT_SEARCH`) now uses MW3's maps on MW3's worlds.
  - **In play (hidden shots, Shift+R on each world):** every map drawn under the radar; on c1ia and c2ia the compass
    reads the heading half a turn round (facing −z: S on c1ia) and the cone points with it, both measured from the
    picture's top, which on those two is +z. MW3's own behaviour, nothing found wrong.

## 139. [x] The window title and the save folder say MechWarrior 1 Remake *(done: mw1-139-rename and mw1-139-move-fixes; in 1.0.6)*

- **Why (2026-11-21):** the game is renamed MechWarrior 1 Remake (short MW1 Remake; repo mech1-remake-re). 1.0.5
  changed the visible text (title screen, credits, read-me, notes); the window title and the save folder
  (`MicroProse/MechWarrior 1 Critical Hit`) still carry the old name.
- **Wanted:** the window title "MechWarrior 1 Remake"; the save folder `MicroProse/MechWarrior 1 Remake`, with a
  migration: at start, when the new folder is missing and the old one exists, move it (or copy then verify), so saves,
  settings and mw1.cfg carry over. Tests: an old folder migrates whole; both present keeps the new; a failed move
  leaves the old untouched and the game still starts.
- **Agreed (2026-11-21, with the mw3 lead):** `SDL_GetPrefPath("MicroProse", "MechWarrior 1 Remake")`, the family's
  pattern (MicroProse/MechWarrior 3, MicroProse/Pirate's Moon). Nothing on main or pm reads MW1's folder; main's
  README.md and tools/windows/installer.iss name the old one in text, and the mw3 lead updates them with this release.
- **Done (mw1 dev 2, `mw1-139-rename`):** `mw1::move_write_dir` (src/mw1_start.h), from mw1_main's usual write dir
  (not a `--write-dir`). SDL makes the new folder when asked, so an empty one counts as missing. The old folder is
  renamed in place, else copied beside the new one (`<new>.moving`), every file compared byte for byte, the copy
  renamed into place, and only then the old one removed; either way the new folder is left owner-writable (a
  read-only old folder still gives a write dir the game can save in). Files in both: the new one is used, the old one left and the
  log says so. A failed move removes the half copy, leaves the old folder as it was, uses it for the run and says why.
  The window's title comes from `sdlfe::window_name()` (ours, opt-in; empty for MW3 and PM), set to "MechWarrior 1
  Remake"; the log's banner, the crash box, the start box and the deploy box take the same name, and the Windows
  program's description too. Tests (test_mw1_start.cpp): old only (renamed, and the checked copy), both, new only, a
  read-only old folder, and a failed move.
- **Windows check (mw1 dev 2, 2026-11-21):** the move's cases built with mingw from mw1_start.cpp and run on a PC
  under %TEMP%: all passed, a save held open with no sharing among them (the old folder kept and used, moved on the
  next start). Two small follow-ups (`mw1-139-move-fixes`): the copy path's removal of the old folder cleared its
  read-only bits first (only after the copy is checked and in place; Windows' remove_all stops at a read-only file
  and left part of the old folder), and copy_tree stops at its first error, so the log says "the copy failed: <why>"
  where the iterator's next step used to clear the error and only the byte check caught it. Tests for both.

## 138. [x] Out of bounds: leaving the battle area fails the mission *(design; with 136; `mw1-fix138-boundary-survey`)*

- **Raised (2026-11-21):** "do we have out of bounds that the player loses the mission if they cross? we probably
  should."
- **What exists:** MW3's own Instant Action blocks fail you for leaving the boundary (`outside_ia*` in
  objectives.zrd, regions `reg_inside_*`/`reg_outside_*`; research/maps.md). Pirate's Moon's IA archives have no
  boundary files or boundary objective (research/maps.md, "Boundaries"). MW1 builds its contracts with no boundary rule
  of its own, so whether one applies today depends on the map. Not yet checked in play.
- **Wanted (2026-11-21), as MW3 does it:** two lines on the HUD's radar, a yellow inner one and a red outer one
  (MW3's `reg_inside_*` / `reg_outside_*` pair, drawn by hud 0x0043d920). Crossing the yellow line says "leaving
  mission area"; crossing the red line loses the mission at once. Every contract map gets the pair, PM's maps
  included. AI pathing already respects the boundary's pull.
- **First:** a headless survey of what each contract map does now (boundary or none, and what crossing it does).
  Then the rule, the area for maps without one (PM's), and 136's points checked to lie inside it.
- **Survey (mw1 dev 2, 2026-11-21; research/boundaries.md):** MW3's maps (mw3c1..mw3c4) already did it all. The
  radar draws both lines, past the yellow the "out_of_bounds" voice plays, and past the red "Left Mission
  Boundaries" loses the mission on the next frame. PM's c1..c4 and the arctic mw3c4b had nothing, even 5 km out.
  On mw3c4, ia3's four southern enemy places stood beyond the red and ia1's Enemy_01 between the lines.
- **Decided (project decision, 2026-11-21):** nothing is built to keep enemies in; the maps only need their
  out-of-bounds areas. MW3's AI pull reads the world's `inout` outlines, never the `reg_` lines, and on MW3's own maps
  nothing keeps its AI inside them either.
- **Done (mw1 dev 2):**
  - PM's maps and the arctic get MW1's own pair (`data/mw1bounds.zrd`, written by `tools/mw1bounds.py`; read by
    `mw1_bounds.h`), which behaves like MW3's. Yellow is 16 sides, each 600 m past the farthest start, enemy place
    (60 m around each, for the launch's extras) or structure; red is 500 m further out.
  - mw3c4's six enemy places outside or near the edge are moved at least 150 m inside the yellow by MW1's MOVE
    entries; MW3's files are unchanged.
  - Found on the way: a fail group made by `block_read` (on the arctic, whose objectives have none) needed every
    item, so the arctic defence's structure falling never lost the mission. It now fails on any one item, as MW3's
    reader does.
  - Test: `mw1_loop_boundary_survey_headless` (all 48 rows with `MW1_BOUNDARY_SURVEY=1`) checks that every map has
    both lines drawn, that every start, enemy and structure lies inside the yellow (MW1's areas by 595 m, MW3's by
    150 m), that the voice plays past the yellow, and that the mission is lost past the red.
- **Repeatable (mw1 dev 2, after 1.0.6; `mw1-survey-repeatable`):** dev 1 found two surveys on the same data giving
  two files (c1 ia3's line-up 21 places, then 19). The cause: the launch picks each record at random among those of
  its weight class (`crt_rand() % n`, seeded from the clock, as the original). The survey now logs every record the
  launch may pick (the candidates), and `tools/mw1bounds.py` builds the areas from the starts, structures and
  candidates only, so the same data gives the same file (`--repeat` checks it; `--launched` checks a run's places
  against the file). Regenerated; the 595 m test now counts every candidate too, and passes on all 48 rows.

## 137. [x] A defence on the arctic map 1 again *(done: mw1-fix137-arctic-defence)*

- **Why:** FIXES 134 dropped c4b ia1's defence (every warehouse stands on the enemies' side of the start), leaving
  ice one defence map; ice contracts are about 68% mw3c4b, under item 46's 70..80% band, and the pool test carries a
  65% exception for it.
- **Plan:** move our own arctic ia1 player start (`data/arctic/ia1`) so a warehouse is on the player's side and passes
  the survey's defence rule, add the row back, and remove the 65% exception from
  `mw1_missions_maps_drawn_by_the_planets_pool` (ice back to about 75%).
- **Done:** the company now starts at (640, −960) on the low rise south of the domed base, 255 m from the south
  warehouse (`ware03`), facing the base; the lancemates stand behind it. c4b ia1's rows are now reach `ware02`
  (1430 m, along 1.27), attack `ware01` (1399 m, along 1.25) and defend `ware03` (along −0.04, the enemies' centre
  1163 m off, 4.6× as far as the start). The supply warehouse and the depot stand 55 m apart, so one can't be the
  attack's and the other the defence's with any margin; the south warehouse, alone, is the defence's, as on ia2.
  The nearest enemy place is 757 m from the new start (1077 m from the old one). Item 138's area for c4b ia1
  (`data/mw1bounds.zrd`, a 16-sided outline 600 m round the places) left the new start only 503 m inside its yellow,
  so the boundary survey failed all four c4b ia1 rows; that area is redrawn by `tools/mw1bounds.py` from a fresh
  boundary survey (the other maps' areas kept: the survey's enemy line-up varies run to run, so redrawing them all
  only moves them a little each time). The pool test's exception is gone: ice is 75.4% mw3c4b, every pool 70% or
  more.

## 136. [x] Objectives on a triangle: the target off the line between you and the enemy *(done: mw1-fix136 routes, the decoy run, mw1-reach-points; in 1.0.6 and 1.0.7)*

- **Rescoped (2026-11-21):** "it doesn't need to be a full triangle, but it should be reachable by avoiding the enemy." Then: "let's do it the
  right way." So 136 is done in full after 1.0.5 (point reach rows on open, reachable floor; targets among each map's
  structures; thresholds from the measured table; the decoy run), not the closest-approach-only shortcut.
- **The design (2026-11-20):** you start at A, the enemy at B, the objective at C, a corner of a triangle, not on
  the A to B line. The enemy comes toward you, but a 'Mech outfitted to run (less armour, more speed) can make C
  first; or high ground screens the path. That matches the original intent of MW1's raid and withdraw contracts:
  a careful player can avoid the fight.
- **Measures (FIXES 134's survey reports them):** the angle at A between A to B and A to C; the closest approach of
  A to C to B; the race ratio |AC| / |BC|; the highest ground between B and the A to C path.
- **The lance as a decoy (2026-11-20):** order the lancemates at the enemy and slip past to the objective. That
  needs the enemy to fight whoever is in front of it, not to make for the player. MW3's AI already does
  (docs/ai.md, 0x4ca600): the nearest enemy contact first, else its last attacker when hit within 1 s, else any
  contact. So a lance in front draws them. Still confirm it in play with a headless run (lance ordered to attack,
  the player heading for C, who each enemy targets logged).
- **Then:** reach points chosen by those scores (reach_area takes any point, on open, reachable floor); attack
  targets chosen among each map's real structures by the same scores. Thresholds from the measured table.
- **The rule (2026-11-21, set from the measured table):** a reach row's structure must be reachable by keeping
  away from the enemy. A route (grid A*, 20 m cells, `src/mw1_placement.h`) from the start to the first cell within
  the reach's radius (40 m, the game's `kReachRadius`), every cell past the start's own 200 m at least 200 m from
  every enemy place (the map's own places, not the extras made beside them), the end cell as clear, all inside the
  mission area (item 138's yellow), at most 2.0 times the straight way; and the start at least 150 m from the
  nearest enemy place (not in a fight the moment the mission starts). Attacks are exempt: 134 puts their target
  among the enemies. Defences keep 134's along rule. The steepest step on the route is measured, not yet a rule.
- **The closest-approach table** (the straight line, before the route rule): with D = 200 m, 14 of 20 attack and
  reach rows clear the enemies' centre and 5 of 20 the nearest enemy place; most reach rows' straight way runs
  through an enemy place. The route rule asks instead whether a way round exists.
- **Measured (the survey, `mw1_loop_every_map_row_is_there_headless`):** 10 reach rows; 5 pass (mw3c4b ia1 and
  ia2, c2 ia1, c3 ia1, c4 ia1; detours 1.05 to 1.12), 5 fail at the goal: c2 ia2 `portalgate` (an enemy place
  32 m off) and the four mw3cN ia2 `iatower` rows (MW3's tower stands among its defenders, 51 to 73 m; on mw3c4
  7 of the 12 places ring it). None fails for want of a way round. The survey gates every reach row; those five are
  listed as known failures with why (a listed row that passes fails the survey, so the list stays honest).
- **Choosing structures:** `MW1_REACH_CANDIDATES=<file>` lists, per reach row, every named node of the map (as the
  game's `node_find` takes it) that passes the same rule from the row's start, with its place, route and
  clearances; `MW1_ROUTE_DUMP=<file>` writes each row's start, goal, enemy places, yellow and path. The fix order:
  another structure; then the start; enemy places last, and never outside the yellow; never MW3's tower moved.
- **The fixes (2026-11-21, picked from the candidate list):** mw3c2 ia2 reaches the hangar (`noruff_hang`, a
  ship in it; route 1379 m, detour 1.01), mw3c1 ia2 the barracks (`urbbar02`; 1335 m, 1.04), mw3c3 ia2 the
  warehouses (`whsA01`; 2224 m, 1.07) and mw3c4 ia2 the dock (`dockfal`; 411 m, 1.12, the end 264 m clear; the
  house beside it only 220 m). c2 ia2's reach is dropped: no structure there is clear of the enemy, and the map
  keeps its attack on the portal gate. Every reach row passes; the known-failure list is empty. The pools keep their
  70..80% (desert 75.7%, tropical 75.2%, ice 75.4%). The steepest step measured on the new routes is mw3c2's 1.24
  (report only).
- **The steepest step (report only):** mw3c2's 1.24 is not the hangar's ramp. Its steepest step is at
  (−123, −1520), about 1 km before the hangar: 35.4 m to 0 within 8 m. The route's ground is the highest polygon
  under each point (the ridge's callback), so beside or under a structure it reads the roof, not the floor. A slope
  rule would read the walking floor (the AI nav ground's level nearest the step before).
- **The decoy check (2026-11-21, `mw1_loop_lance_as_a_decoy_headless`, measured):** three lancemates, eight enemies,
  the rolls seeded, the commander driven along the route; the lance ordered to ATTACK at frame 31 against the lance
  left following. The order helps where the lance meets the enemy in front of the commander (mw3c4's dock: the
  enemies on the commander 2% against 9%; mw3c1: 0% against 21%, the control's commander lost) and hurts where it
  draws the fight onto the commander's way (mw3c2: 65% against 0%, the commander lost). Two gaps found:
  - mw3c4b ia1's commander is lost in both runs: the arctic enemies patrol their nets and close to 142 m of a
    route kept 200 m from their start places. The rule wants the patrol nets in the discs.
  - The route grid knows only the enemies and the yellow, not walls, buildings or cliffs: on mw3c3 the commander
    stands against a wall at (−320, −4356) for the rest of the run (mw3c1's and mw3c2's runs too, once each). The
    rule wants the walking floor (the AI nav ground, a step limit between cells). Done below (walkable routes).
- **Walkable routes (2026-11-21, the second gap):** the route walked on the engine's own walking grid
  (`mech3/zai_nav.h`, the lancemates' pathing: 4 m cells, the floors, `Grid::step` with its climb of 0.7, tan 35
  degrees, the skis' clamp; the wall casts; the 4 m body buffer), kTight cells weighed 3 times as `find_path` weighs
  them, with the same clearance, reach, yellow and detour rule (`walk_route`, the survey's gate; the open-ground route
  stays in the table). Structures as blocked cells: a cell is not walked where a floor beside it or over it rises
  past a step at any height. The grid's buffer counts rises below kHead (12 m) only, a taller one taken for room
  overhead: right under a bridge, wrong beside mw3c3's 20 m solid block with no floor under it, which a 'Mech's body
  meets; and the wall casts miss a roof over open ground. (MW3's own nav behaviour: not raised upstream unless a
  lancemate or AI 'Mech is seen stuck on one.)
  - Every reach row passes on it but mw3c2 ia2's hangar: mw3c4b ia1 1518 m (detour 1.09, the path 200 m clear),
    mw3c4b ia2 1100 (1.08), c2 ia1 1334 (1.07), c3 ia1 304 (1.06), c4 ia1 533 (1.04), mw3c1 ia2's barracks 1346
    (1.05), mw3c3 ia2's warehouses 2244 (1.08), mw3c4 ia2's dock 401 (1.09).
  - Driven (`MW1_DRIVE_ONLY=1` on the decoy test: the enemies disabled, the commander following the walking route
    cell by cell): mw3c1, mw3c3 and mw3c4 arrive. The earlier "stuck" runs were the test's driver too: its escape
    set itself again every frame and never steered again, and its sparse waypoints cut corners into mw3c1's 12 m
    ditch and mw3c3's block.
  - **mw3c2 ia2's reach dropped:** the hangar stands at y −173, 182 m below the start (y 9), in a pit no walk
    reaches: neither `walk_route` (with no enemies and no yellow either, 4M cells) nor the engine's own `find_path`
    (to the hangar and to 8 points on its 40 m ring; the same call finds mw3c1's barracks). The walkable candidates
    there: a mine train (`train01`, g13), a vent's fans, a mine turret (an attack, read so), and a mobile HQ at a
    mobile field base station (`mfb_station04` < `mob_hq03` < `mobile_hq`; route 2333 m, detour 1.14, the path 476 m
    clear). The field base reads as a place, but a mobile HQ may be a working unit in MW3 (a repair stop, a vehicle
    record), not brought into a reach contract untested: the option kept here. mw3c2 keeps its attack; tropical's
    reach is mw3c1's and mw3c3's. The pools keep 70..80% (desert 75.8%, tropical 75.1%, ice 75.4%).
- **1.0.6 as shipped:** c2 ia1, c3 ia1 and mw3c3 ia2 passed the static check and, by the first live drive below,
  failed play (c2 and c3 unchanged from 1.0.5; mw3c3 replaced a guarded tower), so 1.0.6 is no worse than before.
  The fixed driver later showed c3's and mw3c3's failures to be its own stalls under fire; c2 ia1 passes the rule
  as set. Fixed in the next release.
- **The patrol nets (2026-11-21, the first gap, measured):** each enemy's nets read from its brain's orders (a
  NetLink to a compiled net, every step with a place), each net once (an extra walks its record's): arctic 4-point
  loops, PM's and MW3's maps 2 to 76 points each, up to 2 km across.
  - Every point in the 200 m discs (`MW1_NETS`): 5 of 8 reach rows fail (mw3c4b ia1 on Enemy_03's loop, mw3c4 ia2
    on Enemy_02's, c2 ia1 and c4 ia1 with patrols ringing the start, 106 to 150 m; mw3c1 ia2's start 84 m from
    one). PM's designers ran patrols past the player's start, so strict would take reach off PM's own maps.
  - Patrol points with their own clearance, the start's disc exempt (`MW1_NET_CLEAR`): at 150 m and at 100 m,
    c2 ia1 and c4 ia1 still fail; 100 and 150 differ only on mw3c4b ia1.
  - mw3c4b ia1's two starts (137's, 757 m from the nearest place, and the one before, −600 300, 1077 m) both fail
    with the nets, both pass without: the start was not the trouble, Enemy_03's loop by the depot was.
- **The live drive (the measure closest to "reachable by avoiding the enemy"):** each reach row driven along its
  walking route with the enemies alive and patrolling, the commander passive (not firing), the lance following
  with no orders, the rolls seeded 1, 2, 3; arrival, down, the damage and who did it from how far.
  - No clearance radius agreed with it: the first drive passed rows the discs failed (c4 ia1) and failed rows they
    passed; fire came from 400 to 800 m, past any disc, and a long exposure counts that no disc sees.
  - The driver's own faults, found on the way and fixed: its escape re-armed itself every frame (never steered
    again); sparse waypoints and full throttle on a sharp turn overshot it (c4 ia1's 20%: the 'Mech drifted 12 m off
    the route under an 11.6 m overhang, the engine's ceiling crush, every part at once, no attacker); its last leg
    walked on into the structure. Now: every cell a waypoint, the nearest of the next 12 taken after an overshoot,
    full throttle on the heading and half into a turn past 20 degrees, and the last leg to the route's end, held
    there. c3 ia1's and mw3c3 ia2's failures were its stalls.
- **The rule (2026-11-21, a project decision):** the live drive is the gate (`mw1_loop_every_reach_row_reached_live
  _headless`): each reach row, three seeds, must arrive and never go down. Taking fire on the way, even from 800 m,
  is the contract's challenge and its fun, so the damage is not gated; it is said each seed with its attackers and
  their distances (the REACH-LIVE lines, `MW1_SURVEY_TABLE`), so a data change shows as a number. The static
  walking route stays as the prefilter and the path driven.
  - **mw3c4b ia1:** down on all three seeds (Enemy_03 63 to 71% from 136 to 593 m, Enemy_04 the rest). Enemy_03 and
    its loop (our data, 137's map) moved from its post 246 m from the depot to (1900, 700)..(2020, 820), the far
    east beside Enemy_07 and Enemy_08, 791 m from the depot: arrives on all three, 28, 41, 28% (Enemy_04 from 211
    to 567 m). The attack (ware01, along 1.18, the nearest enemy 650 m) and the defence (ware03, along −0.05, the
    enemy 511 m off) keep their geometry. ware01 and the depot stand 55 m apart, so the attack's enemies and the
    reach's way pull against each other on this map. (Enemy_04 moved too gave 0%, before the rule dropped the
    damage limit: not kept.)
  - **The table (seeds 1, 2, 3; all arrive, none down):** mw3c4b ia1 28/41/28%; mw3c4b ia2 0/0/0; c2 ia1
    60/50/48% (Enemy_11 from 53 to 519 m, Enemy_01, 03, 12); c3 ia1 0/0/0; c4 ia1 0/0/0; mw3c1 ia2 0/0/0; mw3c3 ia2
    0/0/0; mw3c4 ia2 0/0/0.
- **(c) The decoy check again (2026-11-21, measured on the release branch, not gated):** each reach row driven as
  the gate drives it, twice per seed (1, 2, 3): the lance ordered to ATTACK at frame 31, and left following (the
  control); the share of the living enemies' targets on the commander, sampled every 30 frames, ATTACK / follow:
  - c2 ia1 12/15, 10/22, 24/35%; c4 ia1 17/22, 17/31, 20/27%; mw3c3 ia2 3/7, 2/6, 2/7%; mw3c4 ia2 3/6, 1/7, 2/7%:
    the order draws some fire off the commander. mw3c1 ia2 2/2% on all three; mw3c4b ia2 0/0 (no enemy targets).
  - c3 ia1 15/7, 32/7, 21/23%: more on the commander with the order, on two seeds of three.
  - mw3c4b ia1 10/1% on all three, and the reach LOST with the order (the commander down at frames 3574, 4745,
    4807), won without it (frame 4904). Following, the lance takes 8 to 9% of the targeting there; sent off to
    attack, none, and Enemy_04 does it all (the DECOY-HURT lines: 100% on each seed, closing to 118, 128, 112 m;
    following, Enemy_04 28, 41, 28%, as the live gate). Kept as it is (a project decision): sending the whole lance
    away on this map is the choice's fair cost, and a player fires back and evades where the test's commander doesn't. The reach is won by the rule's own
    drive (the lance following), so the row stays; a player who sends the lance away on this map pays for it.
  - Every other run won the reach, with the order and without.
- **Point reach rows (2026-11-22, done):** a reach row may name a point (`POINT (x z)`, the words "the nav point";
  `mw1_missions.h` `reach_point`): the engine's `reach_area` there, its nav point on the radar and the OP line.
  - **The search (`MW1_POINT_SEARCH`):** 100 m cells inside the yellow with 100 m to spare, on the mission map's image
    (since item 140, MW3's worlds' too), 800 to 2500 m from the start, 200 m from every enemy place, one dry
    floor, at most 90 degrees at A off the way to the enemies' centre (behind the start is a walk, not a contract:
    a map with none gets no point row, a project decision); ranked by the race, then the angle; walked by the gate's
    rule, then driven live (seeds 1..3 must arrive, the damage said).
  - **The rows:** c1 ia1 (2842, −1596) 48/14/3%; c1 ia2 (4542, −1468) 75/0/75%; c2 ia2 (1707, −761), c3 ia2
    (1071, −1299), c4 ia2 (2076, −918), mw3c2 ia2 (−991, −2093) and mw3c4 ia1 (−536, 1585) 0/0/0; mw3c1 ia1
    (−1189, −1926) 46/71/39% (the race 0.97, kept: being shot at on the way is the contract); mw3c2 ia1 (−930,
    −1886) 10/25/14%. (Item 140 moved mw3c2's two onto MW3's map.) None on ia3 or mw3c3 ia1. The table is in `research/maps.md`.
  - **The cloud death band:** c1's first picks went down at y −30 with no attacker: PM's `cloud_death` band (c1 and
    c3: −10000..−30; `mech3/cloud_band.h`) kills a 'Mech below it (`cloud_kill`, every part). The walk and the search
    refuse a cell with any floor in the band (+2 m) within `kBandClear` (36 m, beside `kReachRadius`; measured on
    c1 ia1, the drive cuts corners) of it; a world with no band is untouched (c2's check).
  - **The clearance is a planning margin (2026-11-22, a project decision):** it binds the routes we choose (the point
    search, the gate's first pass). A shipped structure row whose walk can't keep it is walked again with band floors
    still refused but no clearance, labelled NEAR BAND in the survey with the band's nearest distance, and passes only
    by the live drive (three seeds, all arrive, never down); with no route on either pass it fails as before. c3 ia1's
    land bridge is the one: its approach runs beside a channel just under y −30 (12 m from the route at its nearest)
    and its canyon lies in the band under the deck; NEAR BAND, it arrives 3/3 at 0%. (A depth or drop-height scale
    was measured and dropped: c1's fatal ledge also first meets a floor 0.6 m into the band, and the kill is the 'Mech's
    y inside the band at any depth.)
  - **Who hit the commander:** a ram (`Mech::collide`, the heavier's crash, no ally test, as the original: PM
    0x00423730) leaves no attacker to read, since `mech_after_frame` clears it each frame; the drive names an AI
    'Mech within `kRamContact` (15 m) as "ram: <pilot>". c1 ia2's 75% is a lancemate catching up with the slow or
    stopped commander: the original's rule, kept.
  - **The lance's spawn (`MW1_LANCE_SPAWN`, measured):** no map starts a lancemate within contact (the nearest 38 m,
    mw3c1 ia3) and none rams the commander in its first 30 s, standing or walking off: no spacing change.
  - **mw3c1 ia3's start (`MW1_FLOOD`, `MW1_BASIN_TRACE`, measured):** not boxed in: the walk reaches 11.6 km² of the
    yellow, the north rim and the west; the south block and the east are separate high ground. Driven at three points
    there, the 'Mech stops at the cliffs and never leaves the yellow: the game agrees with the grid.
  - **The objectives audit (`MW1_OBJ_AUDIT`):** each row's live objective and fail tree, each valve with what sets it
    (the valve map), and the world's cloud band.
- **Destroy all on a Defend archive (2026-11-22, a project decision):** the ia3 maps' own fail block includes their
  base's fall (c1 `pirate_base_down`, c2 `able_base_down`, c3 and c4 `base_down`, the mw3c* maps `iatowv2`), which
  lost a battle whose contract says only destroy all. A destroy-all contract now takes out the map's fails on valves
  set by structure destruction (health_below valves, and conditions made only of them), MW1's side only, and keeps
  every other (the player's own 'Mech, the timers, the boundary). Missions holds the fail group's handle, so its top
  group stays. Other kinds keep the map's fails unchanged. Tests: each ia3 row's taken-out valves named, the
  boundary kept, one of each kept kind checked; live, mw3c2 ia3's tower falls at frame 3846 and the battle goes on.
- **Still to do:** the attack targets chosen by the same scores.

## 135. [x] 'Mech prices vary a little *(done: mw1-fix135-price-variety)*

- **The idea (2026-11-20):** about +-5% on 'Mech prices. Proposed: one roll per model per market refresh, applied
  to both buying and selling at that world (keeps the 10% buy/sell gap, so no flip profit), on a bell curve, rounded
  to 1,000, saved with the market. Approved 2026-11-21.
- **The shift:** at each market refresh (`refresh_market`) every model the market deals in gets one shift for that
  world, `roll_shift`: the sum of three rolls of -17 to +17 permille, clamped to +-50 (+-5%); sd about 1.75%, two
  thirds within +-1.7%, about 1.5% beyond +-4%. It's per model, not per stock row, so a mech the company brings in
  sells at the same shift. Kept in `Market::shift` (permille by PM mech id) until the next refresh.
- **Prices:** `buy_price` / `sell_price(mech, tier)` stay the base; `buy_price` / `sell_price(mech, tier, shift)` scale
  the base by (1000 + shift) / 1000 before the floor to 1,000; `market_buy_price` / `market_sell_price(company, mech)`
  are what the world charges and pays. Buying and selling share the shift, so selling stays below buying at every
  tier and shift (nothing gained by buying and selling back).
- **Where:** the market screen's FOR SALE (and its can-afford dimming) and YOUR MECHS, the BUY and SELL questions,
  `can_buy`'s funds check, `buy`, `sell`, the "Bought:" / "Sold:" lines and the log, and a wreck's value
  (`wreck_value`, so a wreck and the market still agree). `can_sell` checks no price.
- **Left at base:** a mech lost on a mission (repairs' `lost_value`, FIXES 98): no market quotes it, and the House's
  cover of the mission's net shouldn't ride on one world's roll. The travel map's `buy_percent` / `sell_percent` stay
  the tier's share.
- **Save:** version 12 adds `market_shift=` (the permille list); a version 12 save without it, or with a shift past
  +-50, is refused. An older save loads with no shift (the base prices) until the next refresh rolls them.
- **Tests (mw1tests):** `mw1_market_shift_range_and_spread` (40,000 rolls: the range, mean, sd and the bell's shape; a
  refresh shifts every model), `..._buy_and_sell_share_it` (every model, tier and shift: on 1,000s, sell below buy,
  shift 0 the base), `..._shown_is_charged` (the screen's rows and the questions show what can_buy, buy and sell
  take, to the C-bill), `..._wreck_agrees`, `..._saved` (round trip; version 11 loads with none; refusals). The
  trade-loop test now expects the tier 3 world's shifted price, and the headless market run one shift for both its
  deals. The refresh's rolls move the seeded stream after it, so `mw1hiresoff`'s office screen (MW1_SEED=7) shows
  another offer: its golden re-recorded (hi-res off and on still give the one canvas).

## 134. [x] An attack's target that isn't where the fight is *(done: mw1-fix134-phantom-targets; for 1.0.4)*

- **Reported (2026-11-20):** "destroy the pirate base on c3ia2": all three enemies dead, the mission still running;
  the nav point (1712, −980) about 100 m from the start, nothing seen there.
- **Cause:** not a missing structure. Headless, `mpbbase` is live, drawn, its meshes hand weapon damage to the target,
  and destroying it wins; the target simply stood beside the player's start, 2.2 km from the enemies at Able, and the
  player left it behind. An attack isn't won by the enemies alone (by design), so the mission waited on it.
- **Fix:** the map survey measures where every structure row stands against the start and all the map's enemy
  places, and gates on it (research/maps.md, "Where the structures stand"): an attack's target on the enemies' side,
  away from the start, a defence's on the player's side. Rows dropped: c3 ia2 attack `mpbbase` (c3 ia2's attack on `pspwrsrc01`, at Able,
  stays), c3 ia3 defend `mpbbase` (across the map; the defence of `pspwrsrc01` by the start stays), c4b ia1 defend
  `ware02` (every warehouse on the enemies' side). c4b ia2's attack and defence swapped warehouses. The survey's
  attack play now wears the structure down through one of its meshes' damage handlers, as a weapon's hit does. No
  world or climate is left without an attack or a defence.
- **Decided:** an attack's target and a reach's place needn't sit among the enemies; a player who tries may slip
  past them. Attack: at least 150 m from the start and on the enemies' side (at least halfway to their centre).
  Reach (MW1's withdraw): at least 150 m from the start, whichever way it leads. The survey also measures, not yet
  as rules, the angle at the start, the enemies' closest approach to the way, a race ratio and a screening ridge
  (`src/mw1_placement.h`, ready for FIXES 136's chosen points).
- **Ice's share:** with c4b ia1's defence gone, ice has one defence map (c4b ia2), and the no-repeat rule sends a
  defence drawn right after a c4b ia2 contract to SOMETIMES, so ice contracts are now about 68% mw3c4b (the band's
  75%, item 46). The pool test's floor is 65% for a pool it can name a kind with a single MOSTLY map for (it prints
  the pool, the kind and the share); every other pool keeps 70%. Item 137 removed the exception (ice 75.4%).

## 133. [x] Ice missions cool the 'Mech faster *(done: mw1-fix133-arctic-cooling)*

- **Reported (2026-11-20):** the arctic missions didn't lower heat, as MW3's snow missions do.
- **Cause:** `ambient_temperature` is the engine's heat scale, not degrees: dissipation is `1 / ambient_temperature`
  (docs/heat.md, world-params.md), and zero or below falls back to the default 20. Our arctic maps set -15 and -25,
  so they cooled as a temperate map does.
- **Fix:** ia1 10 (twice as fast, as MW3's c4b m6), ia2 2 (ten times, as c4b m4). An install re-lays the arctic
  maps when their text changes (`arctic_stale`). Test: `mw1_arctic_maps_cool_faster_than_the_default`.

## 132. [x] D in combat freezes the game for about 2 seconds *(done: mw1-fix132-mfd-freeze; MW1 only)*

- **Reported (2026-11-20):** pressing D in combat to cycle the MFD from the mech's 3D front view to the damage
  diagram or the bars stalls the game for about 2 seconds. Not in MW3 or PM.
- **Measured (hidden `MW1_SCREEN=mission` runs, D tapped six times, a Mac Release build):** the whole stall was one
  picture, the damage display's backdrop `dmg_bg_blue`, painted on the main thread when the display first showed:
  467 ms at 1080p (a 474 ms frame), 1,288 ms at 4K (1,303 ms). Its 8 part pieces took under 1 ms each. It was painted
  again on every return after three seconds away: the art store dropped a drawn piece 180 frames
  (`Store::kKeepFrames`) after its last use. The other directions (to the nav list, back to the 3D view) cost no frame
  over 20 ms: the 3D view has no cost of its own on show.
- **The cause:** `mw1_hud_art.cpp`'s `paint()`. The backdrop is 102 layers (one per colour and alpha of PM's
  picture), and each layer sampled every device pixel 16 times through a `std::function` with a `std::map` lookup
  for its corner joins, then laid a whole-picture layer over the last: about 50 million calls at 4K for 4,293 lit
  pixels. MW1's code alone (MW3 and PM never draw these pictures); nothing in the shared hires host.
- **The fix (the pictures byte-identical):**
  - `paint()` finds the pixel layers' shape (`PixelShape`, a named functor with dense arrays) in the `std::function`
    and visits only the canvas pixels it can reach: a device pixel's samples all lie in one canvas pixel, so only a
    lit pixel (every sample in) or one beside a corner join is sampled. Each hit is laid over at once (a pixel no
    sample hits is unchanged). The backdrop: 5.6 ms at 4K. The reticle 25 to 0.3 ms, the heat bar 17 to 0.2 ms at
    1080p.
  - The HUD's pictures are kept once made (`DrawnArt::keep`), until a new scale or a reopen, not dropped after 180
    frames: all of them are 8.4 MB at 4K. The zoom window, the reticles, the heat bar and the lights are among them.
  - Measured again: D's first press at 4K 9.1 ms over the median frame (was 1,296), at 1080p 6.2 ms (was 466); back
    to the damage display 300 frames later 2.7 ms. No painting ahead of time needed.
- **Tests:**
  - `mw1_loop_hud_art_fast_painter_draws_the_same_pictures`: every HUD picture (89) at 1 to 6 device pixels a canvas
    pixel, byte for byte the painter before (kept as `set_reference_painter(true)`, the tests' only), and the backdrop
    at 4K's scale under 100 ms.
  - `mw1mfdstall` (`tests/mfd_stall.cmake`, `MW1_SCREEN=mfd`): D four times in a hidden mission at 4K and 1080p; each
    press's frames within 20 ms of the median frame, the backdrop made at the first and kept 300 frames away. It fails
    with the painter before (1,296 ms over at 4K) and with the pieces not kept ("kept no").

## 131. [x] A win paid short says the House did it *(done: mw1-fix131-breach; for 1.0.3, in the 1.2.0 rebuild)*

- **Found in the 1.2.0 new-player pass:** a won mission ended "breach of contract", 0 C-bills and 0 salvage, which
  looked like a bug.
- **The cause is MW1's own settlement roll, kept:** at 081B:1EB6, `MwRandomUpTo(100) <= DS:1241[house]` pays
  DS:124B[house]% of the fee and salvage.
  - Kurita and Liao pay 0% (the breach), Marik 50%, Steiner 75%; Davion never short-pays.
  - The chance is 11 in 101 for Kurita and Steiner, 31 in 101 for Marik and Liao.
  - Story battles skip it (MW1 skips the Dark Wing base).
  - Ours already matched the original exactly: 0 to 100 inclusive, compared with `<=`, as MwRandomUpTo's
    `maximum + 1`.
- **Owen (2026-11-20):** keep the roll, but say plainly that the employer did it. Ours:
  - **the message box:** after MW1's own lines, one line of ours, "House Kurita declared a breach: fee and salvage
    forfeit." or "House Marik withheld 50% of your fee and salvage." (`deduction_note`). It is shown even without
    the exe's words. With MW1's two paragraphs it fits the 6-line box at both widths; the tightest case is 6 of 6.
  - **PAYMENT:** the fee's line, beside its figure: "Fee: 0 (House Kurita: breach)", "Fee: 105,000 (Steiner
    withheld 25%)" (`fee_dock`).
  - **the company log:** the won line, then the same line. The result entry keeps the House (+1) in c and the
    percent kept back in d; an older save's 0 shows the won line alone.
- **Tests:**
  - `mw1_campaign_the_settlement_roll_is_mw1s_and_skips_davion_and_story_battles`: 20,000 settlements a House. The
    rates are within 1 point of MW1's, and every short payment is the House's percent. Davion: none. A story battle:
    no roll, and the generator isn't drawn.
  - `mw1_results_a_win_paid_short_names_the_house_and_fits`: the wording, the widths, the log line, and the fit with
    every House's and deduction's MW1 text.

## 130. [ ] MW1 Original: the 1989 game rebuilt as our own C++ *(scoping: mw1-fix130-scoping; no code yet)*
- **Owen (2026-11-20), via the lead:** rebuild MechWarrior (1989) as our own code, the way mw3-re rebuilt MW3.
  - **The base:** Elthial's SDL port, forked with history and converted to clean C++20 (classes, namespaces, plain names, no goto, organized by function, the original's address per function).
  - **No emulation libraries.**
  - **Pull as little from the exes as we can:** only MW1's creative content is extracted at first run; UI words and rules are ours, in our own data files.
- **The report:** mw1/research/original-rebuild-scoping.md. It covers:
  - the port mapped (34.4k lines, 265 gotos, only 3k of it MIT);
  - the exes (Owen's BTECH differs from the port's only inside segment 0810, about 272 bytes; the data layout is the same);
  - every exe table sorted into extracted, ours and code;
  - the import, in Inception's shape;
  - the engine on SDL3;
  - its own repo, a fork;
  - the plan: about 14-17 developer-weeks, with Inception's DOSBox-X rig as the behaviour check;
  - the comparison with Inception (nothing shared beyond EXEPACK and Microsoft C's runtime).
- **To settle first:** the port's src/Original is excluded from its MIT grant, so we need Elthial's permission to fork it as the base. A copy of the 174,267-byte BTECH.EXE is needed for the revision diff.

## 128. [x] Key bindings and a joystick in MW1 as in MW3 *(done: mw1-fix128-controls, tests only; for 1.0.2)*
Owen: "key binds and joystick should work in mw1 just like it does in mw3". MW1 uses Pirate's Moon's key map, command
map and stick code unchanged (SETTINGS, CONTROLS, KEY BINDINGS opens PM's CTL_CFG_DIALOG in MW1's look), so the work
was checking each part in MW1, against MW3/PM. Nothing in the game needed fixing; two tests now hold it.
- **Rebinding:** KEY, KEY 2, a mouse button and a stick button each rebind through the screen, and each drives its
  command in an MW1 battle. ACCEPT keeps the map; it is written at the game's exit to `settings.reg` in MW1's write
  dir (`DefaultCmdMap`), and read back at the next start. SAVE KEYS and LOAD KEYS write and read `keys\<name>.zrd` in
  MW1's write dir. MW3's and PM's installs are never written (the test compares PM's files before and after).
- **The stick:** JOYSTICK DEFAULTS puts throttle on Z, turn on X, pitch on Y and twist on RZ; in an MW1 battle the
  lever, the turn, the twist, the trigger (button 1) and the hat all work. MOUSE DEFAULTS puts PM's mouse setup back
  (the mouse's X twists, its Y pitches; throttle and turn on the keys). ASSIGN AXIS is PM's popup (tested in PM).
- **As MW3 (kept):** the screen captures stick buttons 1 to 10 only (the original's loop); a stick plugged in after
  the start is not opened until a restart (one unplugged and plugged back is reopened); the in-game menus take the
  mouse and Escape (Enter in text fields and boxes), and a gamepad drives only the launcher. MW1's screens are the
  same, plus travel's arrows, Enter and typing and the results' Up/Down/PgUp/PgDn.
- **MW1's own battle actions:** none. Lance orders are PM's Communications Controls (CMD_DIRECT_ATTACKT and the
  rest), in the map and bindable.
- **The tests' harness:** `menu_run` takes an input (keys, mouse buttons, a stick with named axes), a `settings.reg`
  in its write dir and a kept write dir (a restart); with settings it saves them at the end as the game's exit does
  (`app.destroy()`).
- **Tests:** `mw1_controls_rebound_through_settings_kept_in_mw1s_write_dir_and_live_in_battle_headless`,
  `mw1_controls_a_stick_drives_the_mech_in_mw1s_battles_headless`.
- **After the engine's joystick fix (main a936e6ca, through pm; for 1.0.2):** a stick plugged in while MW1 runs is
  opened, and buttons 11 to 15 bind. MW1 needed no change: its screens poll through PM's `GameEngine::input_poll`,
  and it starts through PM's `Mech3App::start_game`.
  - **The test:** no stick at the start; one plugged in at the hub, and zInput opens it. Then JOYSTICK DEFAULTS, and
    TURN RIGHT's JOYSTICK captured as button 13. In an MW1 battle the lever sets the throttle, and button 13 turns
    right.
  - **Test:** `mw1_controls_a_stick_plugged_in_later_drives_the_mech_and_binds_button_13_headless`.
## 129. [x] Lance orders in MW1's battles, each one *(done: mw1-fix129-lance-orders, a test only; for 1.0.2)*
The lead, after 128: does a lancemate act on each of Pirate's Moon's lance orders in an MW1 battle? Yes, every one, as
in PM; nothing needed fixing. MW1's lancemates are PM AI 'Mechs on the player's side with a radio, in PM's lance
roster (the pocket commander's panel lists them), so PM's dispatcher, brains and radio take the orders unchanged.
- **FOLLOW** (CMD_DIRECT_FOLLOW): "follow", and it walks to the player. **PAUSE** (CMD_DIRECT_PAUSE): "stop", and it
  stands. **ATTACK** (CMD_DIRECT_ATTACKT, on the player's target): "confirm", and it turns and walks at the target
  (FIXES 72 and 110 time it). **DEFEND** (CMD_DIRECT_DEFENDT): on an enemy "disregard", as PM (it wants an ally); on
  the other lancemate (AWAITCMD keeping the recipient while W targets the ally) "confirm". **REFIT**
  (CMD_DIRECT_REFITT): "disregard" with no repair site on the map, as PM's. **POCKET** (CMD_DIRECT_POCKET): PM's
  pocket commander; its number key 1 picks lancemate 1 (PM's command 0x5d, through the key map the mode pushes) and
  the next order goes to it alone; with none picked, to no one.
- **Test:** `mw1_loop_each_lance_order_in_an_mw1_battle_headless` (two lancemates; each order to A alone, A's radio's
  answer, its state after, its walk).

## 126. [x] One SETTINGS screen in place of Pirate's Moon's option pages *(done: mw1-options-126, merged 7c989d2b; shipped in MW1 1.0.2)*
- **Owen (2026-11-20), via the lead:** "lets make the options menu more intuitive. its currently based on mw1's
  options menu but it doesnt have to be." He approved the mockups ("those new options pages look good"; outbox
  2026-11-20/mw1-options-126/mockup). The lead's calls: (a) every change applies at once; (b) TEXTURES and MFB
  REGROUP hidden in MW1; (c) the cheats stay in GAME as their own red-headed group.
- **The screen** (`src/mw1_settings.{h,cpp}`, layout `data/mw1settings.zrd` written by `tools/mw1settings_zrd.py`):
  - Section tabs down the left: GAME, DISPLAY, SOUND, CONTROLS, ENHANCEMENTS. The chosen one is lit amber. DEFAULTS
    (this section's) and DONE sit under them. DONE and Escape go back.
  - The section's rows in the panel: a plain label, then its control. A choice of two or three shows every value,
    the chosen one lit amber (a cheat's ON and HEAT's NONE in red). A longer list (RESOLUTION, LIGHTING, MOUSE
    SENSITIVITY) shows its value between arrows, with "n of N". A volume is ten bars and its %; clicking the top lit
    bar again goes one lower, so 0 can be had.
  - The status line says what the row under the mouse does (each hint at most 62 letters).
  - GAME: DIFFICULTY, CAMERA, TORSO RE-CENTRES, THROTTLE STAYS SET, JUMP ASSIST; CHEATS: INVINCIBLE, UNLIMITED AMMO,
    HEAT. DISPLAY: RESOLUTION; QUALITY: DETAIL, EFFECTS, LIGHTING, SHADOWS, SELF SHADOWS; MENUS: WIDE MENUS (taken
    at the next start). SOUND: MUSIC, MUSIC VOLUME, EFFECTS VOLUME, SOUND QUALITY. CONTROLS: MOUSE SENSITIVITY,
    INVERT MOUSE, KEY BINDINGS (PM's key map, as it is). ENHANCEMENTS: IMPROVED AI, ARTEMIS TRACKING, WATER MOVEMENT,
    SMALL FIXES and its list.
  - Not offered: TEXTURES (set to the device's largest at each start, `settings::at_start`), MFB REGROUP (MW1 fields
    no MFB), and the video device (the SDL build has one; PM's own page shows a single entry too).
  - RESOLUTION is the battles' size (PM's in-game mode), taken when the next battle starts or the pause is left, so
    no keep-or-revert check: a window takes any size.
- **Saving:** each change is applied and saved at once. PM's values go through its own setters and `settings_write`
  (as its ACCEPT did). The sound goes through `music::set_settings` (mw1.cfg via mw1_start's read_cfg and
  write_cfg). The switches and WIDE MENUS go through PM's upgrades saver (mw3.cfg's keys).
- **Where it opens:** the hub's OPTIONS (its SETTINGS), and the pause's OPTIONS through
  `mech3::pause_options_hook()` (an opt-in engine hook, its own commit: unset in MW3 and PM, whose pause is unchanged).
- **The hub's OPTIONS** (on FIXES 125's dialog): SETTINGS, SAVE GAME, LOAD GAME, MAIN MENU, BACK. Item 100's MUSIC and
  EFFECTS rows are gone from it (they are SETTINGS' SOUND), the rest moved up, the panel 232x256.
- **Test:** `mw1_settings_every_row_applies_saves_and_defaults_headless` (mw1tests). In every section each row's
  own control changes it: shown and applied at once, and saved (PM's settings written, mw1.cfg, the switches). The
  chosen value is lit, DEFAULTS gives back every row's default, every hint fits, DONE goes back, and the pause's
  hook opens the screen with PM's key map made for it.
- **Shots:** every section at 1080p (16:9 menus) and 4:3; `MW1_SCREEN=settings_<section>` shows one.

## 125. [x] Three save slots, each one overwritable *(done: mw1-fix125-save-slots; for MW1 1.0.2 with the launcher's 1.2.0)*
Owen: "lets make it so people have three game save slots and they can overwrite any of them." Then: "save and load
both, yes. maybe it lists three slots and you pick one, and if you want to start a new game you can click new game
and pick the slot to overwrite".
- **The files:** `mw1save1.txt`, `mw1save2.txt` and `mw1save3.txt` in the write dir, each written atomically as
  before (a `.tmp` renamed). `.prev` (the company NEW CAMPAIGN replaced) and `.bad` (a save that didn't read) are per
  slot: `mw1save2.prev`, `mw1save2.bad`. A damaged slot is set aside alone; the other two still load.
- **Migration:** at start, `mw1save.txt` (with its `.prev` and `.bad`) becomes slot 1's, its bytes untouched, once, and
  only when there is no slot 1 (`migrate_slots`; the log says "mw1: saves: mw1save.txt is now mw1save1.txt").
- **The slot in play** is kept in `mw1.cfg` (`save_slot=`, 1 to 3; none means slot 1). The autosave writes to it.
- **The screen** (`mw1_slots.h`, `mw1slots.zrd`, 4:3 and 16:9): one panel per slot, headed SLOT n (and IN PLAY), the
  world in big letters, the House and date, the funds and kills, and when it was saved. An empty slot says EMPTY and a
  damaged one says DAMAGED. Each slot's button is the use's verb:
  - **NEW CAMPAIGN** (the title screen): START on any slot.
  - **LOAD** (the title screen; on when any slot is used): LOAD on a used slot.
  - **SAVE GAME** and **LOAD GAME** (the hub's OPTIONS; the dialog is now SETTINGS, the music rows, SAVE GAME,
    LOAD GAME, MAIN MENU, BACK).
- **The confirm** is in the screen, MW1's look, never a system dialog. SAVE over a used slot asks OVERWRITE SLOT n?
  (SAVE / CANCEL); START over a used or damaged one asks START OVER IN SLOT n? and names its `.prev`. While it asks,
  the slots and BACK are hidden. NEW CAMPAIGN's old question (a message box) is gone; it is this confirm now.
- **Decisions:** SAVE into another slot makes that the slot in play. SAVE over the slot in play doesn't ask (it is the
  autosave's own file). LOAD from the hub saves the slot in play first (any change since the autosave), then loads.
  Each slot is headed by its world, not a name: the commander's name comes from the player's MW_MAIN.EXE and is the
  same in every slot.
- **The save format:** a `saved_at=` line (Unix seconds) after the version. Unknown keys are ignored, so no version
  bump, and 1.0.1 still reads 1.0.2's saves. A save without it shows the file's time.
- **Found on the way (the soak):** loading a save whose Bar recruits were all hired drew new ones, because the
  rule for saves before version 4 (no Bar) tested for an empty list. It now tests the save's version
  (`Company::recruits_unsaved`). It only showed once LOAD read a slot again over the company in play.
- **Tests:** `mw1_save_slots_migrate_the_old_save_to_slot_1`, `mw1_save_slots_peek_words_and_a_damaged_slot_alone`,
  `mw1_save_slots_the_slot_in_play_in_mw1_cfg`; `mw1_menu_continue_and_new_over_a_save_headless` rewritten (LOAD
  through the slots, NEW's confirm CANCEL then START with `mw1save1.prev`, the hub's SAVE GAME into slot 2 and its
  LOAD GAME of slot 1 with the change saved first, no system box); `mw1_menu_new_campaign_plays_the_intro_then_the_hub_headless`
  starts in slot 2; the soaks, the music tests and the via-menu run go through the slots screen.
- **Shots:** `MW1_SCREEN=slots_save`, `slots_confirm` (from the hub), `slots_load`, `slots_new` (from the menu).

## 123. [x] mw1's log opens with Pirate's Moon's banner and an engine version *(done: mw1-banner-123, for MW1 1.0.2)*
- **What shows:** the first line of every mw1 run, on the console and in `<write dir>/mw3.log`, is
  `mw3pm (Pirate's Moon rebuild) pm-v1.1.3-657-gcd180547, log …/mw3.log`, not MW1's name and `mw1-v1.0.1`.
  The Windows package (no console) writes `mw3pm (Pirate's Moon rebuild) …: started` instead. Seen in the
  1.0.1 release acceptance (mw1 dev 2, 2026-11-20); the same on every earlier build, so not new. `mw1 --version`
  and the launcher's "Ready, mw1-v1.0.1" are right: only this banner is PM's.
- **Where:** `mw1/src/mw1_main.cpp` ends in `mw3_game_main` (the engine's game main, PM's build of it), whose
  banner is `src/frontend/sdl/mw3_main.cpp:811-814`: a fixed "mw3pm (Pirate's Moon rebuild)" and the engine
  library's `MW3_VERSION` (git describe against PM's tags, so a pm-v… string on an MW1 commit). The crash report
  just below (`:816`) is installed under the name "mw3pm" too, and the log file is `mw3.log`.
- **Likely fix:** an engine hook, opt-in (b): the program's name and version for the banner and the crash report,
  set by `mw1_main` before it calls `mw3_game_main` (for example `sdlfe::set_program("mw1", "MechWarrior 1:
  Critical Hit", MW1_VERSION)`), PM's own by default, so main and PM print exactly what they print today. Its
  own "mw1 (engine, ours)" commit. Optionally the log named `mw1.log`; that touches what players and the
  launcher's "see its log" point at, so the lead's call. Test: mw1's first log line names MW1 and its
  version, and a PM run's banner is unchanged.
- **Fixed (the lead pulled it into 1.0.2):** `sdlfe::program_name()` (`src/frontend/sdl/crash_report.h`, its own
  "mw1 (engine, ours)" commit): an id, a title and a version, empty in MW3, PM and the launcher, so their lines are
  byte for byte as before. `mw1_main` sets it first (mw1, MechWarrior 1: Critical Hit, its version tag's describe,
  --version's), and the game main's banner (`mw1 (MechWarrior 1: Critical Hit) mw1-v…, log …/mw3.log`, or
  `…: started` on Windows), the crash report's lines (`mw1: crashed: …`) and the crash box's title and words
  ("MechWarrior 1: Critical Hit crashed (…)") then name MW1. The log stays `mw3.log` (renaming it is the lead's
  separate call).
  - Found on the way: on the Mac and Linux a crash's handler starts the program again as `mw1 --crash-box <text>` to
    show the box, and MW1's start took that for a game's start: its arguments went through MW1's own start, so the
    game main never saw the bare `--crash-box <text>` it answers (read, not seen in a run). `mw1_main` answers it
    first now.
  - Test `mw1banner` (ctest; `mw1/tests/banner.cmake`): mw1 and mw3pm each run once with MW3_CRASH_TEST (exit 70
    after the crash report's line) under SDL's dummy video; mw1's first log line is its name and the build's describe
    and its crash line `mw1: crashed: segmentation fault`; mw3pm's are `mw3pm (Pirate's Moon rebuild) …` and
    `mw3pm: crashed: …` as before.

## 122. [ ] An arms dealer: weapons and equipment for sale *(planned for MW1 1.1.0; not in 1.0.0)*
- **Owen (2026-11-20), via the lead:** the weapons a campaign never meets come from an arms dealer, in 1.1.0. Today
  MW1 has no item shop: items come only from the starting stores, the loadouts of 'Mechs bought, and salvage (111).
- **What it is:** a hub shop that sells weapons and equipment into the company's stores, which the mech lab fits
  from. Stock varies by world: richer worlds and edge worlds carry rarer stock. It sits beside 118's permanent
  upgrades, which are always on sale at the hub; the two can share one shop screen and one price table for Owen.
- **It must cover the items no fielded loadout carries** (dev 1's 111 report; none was seen in 10 runs to the Dark
  Wing, and none can be, short of the stores):
  - autocannons: AC 2, AC 5, LB-2X, LB-5X, LB-20X, Ultra AC 2, Ultra AC 10, Ultra AC 20;
  - energy: PPC, ER Laser (S), Clan Heavy Laser (S);
  - missiles: SRM 2, Streak SRM 4, Streak SRM 6, Clan Streak SRM 4, MRM 10, MRM 40, TBOLT 10, TBOLT 15, Clan LRM 5;
  - other: Clan light MG, MASC, ECM, and the Inner Sphere targeting computer.
- **And the items carried only by Alternates and Exotics,** never seen in those runs: the Clan AMS, Clan ECM, C3
  computer, Clan TAG, Clan heat sink and Clan heavy MG. The Clan targeting computer and Clan AMS are also lost to most
  kills by the exe's torso rule (121), so the dealer is their sure source whatever 121 decides.
- **To settle with Owen before building:**
  - which worlds count as rich or edge, and how rare each item is (the stock table);
  - prices against MW1's 'Mech prices and the pay (111: early contracts net 220-350k);
  - whether stock refreshes over game time or per visit, and whether the dealer buys items back.
- **Tests:** a seeded hub's stock, by world, holds the rare items only where the table puts them; a purchase debits
  the funds and lands in the stores; the lab fits it. 111's gauge (`MW1_ITEM_SOURCES`, the ITEM lines) is rerun to
  show every item above reachable.
- **Links:** 118 (the hub's other money sink), 121 (torso items lost to salvage), 111 (the weapons report).
- **1.1.0's plan:** mw1/research/plan-1.1.0.md. Done so far:
  - **Slice 4, the dealer's model (mw1-110-dealer):** `mw1_dealer.h`. The wares are PM's weapons in each tech, their
    ammunition in one-ton lots, and the equipment the lab fits, except plain armour. Every item listed above is a
    ware. Rarity: Clan wares are rare (tier 3 and the Clan worlds); the Inner Sphere items above are uncommon (tier 1
    and up); the rest are common. The stock is rolled at each market refresh from its own seed (the world and the
    date), so the campaign's dice are untouched. BUY puts one into the stores; SELL takes one out of them, at the
    tier's percentages as a 'Mech's. Save 13 adds `dealer_stock`. The prices and the stock's numbers are placeholders
    in `mw1_dealer.h`; `MW1_PRICE_TABLE=1` with `mw1_dealer_price_table` prints every ware's prices with PM's weights.
    No screen yet.

## 121. [x] Clan targeting computers and AMSes almost never salvaged *(closed: by design, the exe's rule kept; mw1-fix121-salvage-tc-ams)*
From dev 1's 111 weapons report (the lead, 2026-11-20):
- The Clan TC is on the Puma and Masakari Primaries and a Thor Alternate, in the right torso. It was salvaged in 1 of
  10 runs, and seed 2's three Puma Primary kills gave none.
- The Clan AMS is on 17 loadouts and was never salvaged.

**The answers (dev 2):**
- **The loadout fielded is the one MW1 lists.** The Puma Primary in the mission carries the Clan TC at location 8
  (the right torso), and a double heat sink at 7, as listed.
- **Salvage drops them, by the original exe's rule; it isn't a port bug.**
  - The engine's mech has one torso part for ids 1, 7 and 8 (the torso, the left and the right torso: Mech3.exe
    0x0041d510, docs/structs.md Parts).
  - Salvage keeps an equipment item only when its part isn't destroyed (Mech3.exe 0x00472f00, PM 0x00474f80).
  - Every death but one by both legs destroys the torso. A torso kill does it directly. A head kill does too:
    `Head::destroy` calls `Torso::destroy` (Mech3.exe 0x004671e0, PM 0x00468c50), and the hip does the same
    (0x00462490).
  - So any item in a torso place is lost to a head or torso kill, and salvaged only off a mech brought down by its
    legs. Weapons are salvaged by type, without a part check, and sinks by their count, so neither is affected.
  - The stock loadouts:
    - all 7 TCs are in the torso;
    - the 17 loadouts with a Clan AMS have 31 of them in the torso and 2 in the head (the Mad Cat's and the
      Masakari's Exotics), which a head kill destroys too.
  - So dev 1's 1 in 10 was a legs kill.
- **MW1's stores intake doesn't drop them.** A TC salvaged reaches the company's stores on the win.
- **Test:** `mw1_loop_a_puma_primarys_targeting_computer_salvaged_off_a_standing_torso_headless` kills a Puma
  Primary by the head, by the torso and by both legs, and reads the mission's salvage and the stores after OK. The
  head and torso kills give no TC; both legs give one, into the stores. The census of the stock TCs and Clan AMSes
  by place is pinned too.
- **Class:** not (a). Main and PM are faithful to the exe, and MW3 and PM players lose torso equipment the same way.
  Changing it in MW1 would have been an MW1 choice (b), such as keeping a torso item unless the mech died by its
  torso.
- **Owen (2026-11-20, via the lead): kept as it is.** "everybody knows you have to knock the leg out to get good
  salvage. we can keep the logic." MW1 keeps the exe's rule: torso equipment is salvaged only off a mech brought down
  by its legs. The test stays as the rule's pin, so a change to it fails there first.

## 120. [x] The same seed rolls a different game on each platform *(done: mw1-fix120-portable-seed-r, for MW1 1.1.0)*

Found in the 1.0.0 release check (dev 2, 2026-11-20). The packaged mw1 was run with the same `MW1_SEED=7` on Mac and
Linux. The Mac started on Robinson with a defence contract on c3ia3; Linux started on Matsuida with a
destroy-the-tower contract on mw3c1ia2.
- **Cause:** `mw1::Rng::upto` (mw1_campaign.h) rolls with `std::uniform_int_distribution`. The C++ standard leaves
  that distribution's algorithm to the library, so libc++ (Mac), libstdc++ (Linux) and MSVC's each turn the same
  `std::mt19937` stream into different numbers. The mt19937 itself is portable. No other MW1 roll uses a standard
  distribution, shuffle or sample.
- **Players:** not affected. An unseeded game draws its seed from `std::random_device`, and a save keeps what was
  rolled, so a save carries over between platforms unchanged. Only seeds are not portable: the tests' and the
  playthrough's (`MW1_SEED`, `MW1_PLAYTHROUGH_SEED`). A test pinned to one seed's result can pass on one platform and
  fail on another.
- **The lead (2026-11-20):** not for 1.0.0; for 1.1.0.
- **What it needs:**
  1. An MW1-only portable roll in `Rng::upto` over the mt19937's raw 32-bit output: Lemire's multiply-shift with
     rejection, or plain modulo with rejection. No standard distribution. It stays unbiased, and all three
     platforms produce the same numbers.
  2. Re-pinning the seeded expectations. Every seed's rolls change on the Mac too, so every test that names a seed
     must be rechecked and re-pinned: the loop tests, the playthrough's default seed, and the cmake page tests'
     `MW1_SEED=7`. The commit lists each one.
  3. A test that the same seed gives the same first contract on every platform, MSVC included: a seed's first rolls
     and its first offered contract (map, objective, fee, enemy) checked against fixed values written in the
     test. It runs in mw1tests on the Mac, Linux and Windows, so any platform that disagrees fails.
- **Fixed:** `Rng::upto` is Lemire's multiply-shift with rejection over the mt19937's 32 bits (unbiased); `max <= 0`
  is 0 and draws nothing (libc++ drew nothing there, libstdc++ one). Every MW1 roll goes through it (`EnemyRng`
  wraps it). Saves hold what was rolled, never the generator, and loading rolls nothing from save version 8 on, so
  a save reads as before.
  - Test `mw1_rng_the_same_seed_rolls_the_same_everywhere` (test_mw1_campaign.cpp), no game install needed: seeds 7
    and 20261120's first 16 rolls over widths 2 to 2^31; seeds 7 and 1's company and first contract on the shipped
    map table and pools; max <= 0 draws nothing; 0..2 even over 30,000 rolls; a seeded company and contract saved and
    read back in a run rolled otherwise, unchanged. Run on all three, every value the same: the Mac (clang, libc++),
    Linux (WSL Ubuntu, g++ and libstdc++) and Windows (clang-cl, MSVC's STL).
  - mw1tests had never built on Windows: `test_mw1_loop.cpp`'s peak memory (`sys/resource.h`) is now
    `tests/peak_memory.cpp` (the working set on Windows; its own file, as `windows.h`'s macros clash with the
    engine's headers), and setenv/unsetenv come from `tests/posix_env.h` there. Only the new test was run on
    Windows and Linux (no game installs there).
  - Re-pinned, every seed rolling otherwise on the Mac too; nothing else in mw1tests or ctest moved:
    - `mw1_story_the_dark_wing_won_and_lost_headless`: the seed rolls a tropical Dark Wing planet, and the test's
      table (PM's maps alone) has no tropical map that holds 8, so `story_battle`'s fallback is right. It checks the
      rule now: the planet's climate whenever the table has such a map of it.
    - `mw1_travel_the_trade_loop_buy_low_sell_high`: the tier 3 limit rolls 2 and the test bought 2, so selling ended
      with an empty hangar; it buys 3.
    - `mw1_repairs_settle_mission_pays_after_the_repairs`: seed 5 starts at a tier 1 world; the pool pinned to the
      wrecks' worth there (3,388 + 2,812 thousand).
    - `mw1hiresoff`: the office offer shows seed 7's first contract; its canvas (the same with MW1_HIRES off and on)
      and Darwin-arm64's frame line re-recorded.
    - The playthrough (`MW1_PLAYTHROUGH=1`, seed 5): still to the ending, 242 months, 61 contracts; nothing pinned.
  - `MW1_SEED=7` now starts on Matsuida with a destroy-the-tower contract on every platform, as Linux did before
    (likely because recent libstdc++ maps with Lemire's method too; not checked further).

## 116. [x] Our combat music in the missions *(done: mw1-combat-music)*
Owen (2026-11-19, via the lead): the combat tracks he picked play in MW1's missions in place of Pirate's Moon's disc
music. It's the release gate.
- **The tracks** (`data/music/combat-7106.ogg`, `-7101.ogg`, `-7102.ogg`, ours; YuE2 "combat relentless", E minor,
  140 BPM). 7106 is the soundtrack's pick, and 7101 and 7102 are "okay and usable". They are encoded as FIXES 100's
  theme was: libvorbis q6, 48 kHz stereo, with no tags. 7106 and 7101 already end in their own tail. 7102 stopped
  loud, so its last 5 s are faded out for the loop. The FLAC masters are in keep/mw1-critical-hit/music.
- **Which one plays:**
  - Each launch, a replay's too, picks one (`music::launched`, `pick_combat`). The first is any of the three, then
    never the one the last launch played.
  - It plays through the CD's extra track (FIXES 100's, its file swapped), from the launch, looped (FIXES 103's
    flush), at the music volume. That's PM's own, which OPTIONS and the pause's GRAPHICS & SOUND set, so it follows
    them at once. MUSIC: OFF keeps it off.
  - It pauses with the game. The mission's end stops it, as it stopped PM's music, and the theme fades in at the
    results.
  - `music::frame` now takes where the game is (MW1's screens, DEPLOY's load, the mission).
- **PM's disc music never plays in MW1:**
  - MW1's CD has no disc tracks: `num_tracks` is 0 each frame, so PM's mission start picks none.
  - MW1 no longer mounts PM's player's music/ or reads the player's ripped tracks (`music_from` and `has_music` are
    gone, with their test).
  - MW3 and PM are unchanged. There's no engine change: the CD's extra track is FIXES 100's.
- **Size:** the package grows by 14.7 MB (5.3 + 4.7 + 4.7). mw1data/music/ goes from 7.8 to 22.6 MB.
- **Tests:**
  - `mw1_music_theme_screens_mission_settings_and_quit_headless` now covers the launch. The last track is set to
    7106; in the mission another one plays, looped, at the music volume, with no disc tracks. PM's slider at 30%
    moves it at once. At the results the theme is back.
  - `mw1_music_a_launch_never_plays_the_last_combat_track`: 3,000 picks, never the last, each about a third.

## 116b. [x] The combat track started again after the mission's end *(done: mw1-fix116b-music-restart)*
Found in the 1.0.0 release check of 116 (dev 2, 2026-11-20). In the packaged mw1's headless mission the log read
"music: combat …" a second time just after "the mission's end: won".
- **Cause:** the mission's end stops the CD, as 116 meant. The frames after it (PM's end screen and its fade,
  Instant Action's end) still told `music::frame` "the mission", because the launch was still on. The track wasn't
  playing, so the mission branch started it again from the top until MW1's screens.
  - After a win, a player heard the music cut at the end, then the same track again from its start for a couple of
    seconds, then the theme.
  - After a failure that offers REPLAY it was worse: the restarted track played on over the results screen, and the
    theme never came.
- **The fix (MW1's alone):**
  - A fourth place, `Where::kEnded`. The game passes it once the launch's outcome is latched (`ending_latched_`, the
    frame the mission ends), until MW1's screens.
  - There `music::frame` stops a combat track that still plays and never starts one. The silence lasts until MW1's
    screens, where the theme fades in at the results as before, the failure's too.
  - A replay's launch clears the latch, so its mission plays its own new pick.
- **Test:** `mw1_music_combat_once_a_launch_silent_after_the_end_and_a_new_pick_on_replay_headless`. A mission is
  failed by PM's own Exit Mission (its end screen, as a player leaves), REPLAY at the results, and the replay is won
  the same way. It checks:
  - each launch starts its combat track exactly once (`Seen::combat_starts`, new);
  - from each end until the theme, no combat track is on or started;
  - the replay's pick is a new one;
  - both results screens have the theme a second in.
  - Without the fix it fails: 4 starts, and a combat track on in 251 of the 253 frames after the ends.
- **Left for 1.1.0:** mw1's log still prints PM's start note "No music: … Put your own rip in <write dir>/music as
  track02.ogg …" (mw3_main.cpp's `start_notes`). Since 116 MW1 reads no ripped tracks, so the note is wrong there.
  Only the log shows it: mw1 always passes `--install`, so the first start's box never shows it. MW1 has no flag
  that turns the note off, so silencing it needs an engine change (an opt-out in the game main, MW1 choice (b)).

## 111. [x] The pace to the Dark Wing, and the weapons met on the way *(no change, Owen 2026-11-20)*
- **Owen (2026-11-20), via the lead:** no change. The pace stays the original's: no rung-1 or contract-time change, and
  the pay stays at 100% (`kPayPercent`). The missing weapons wait for 1.1.0's arms dealer (122). The measurements
  below stay as the record, and the gauge stays in the playthrough test.
Owen, via the lead (2026-11-20): about 40 fights to the Dark Wing, contracts plus story battles, about 7 hours, with
a steady build-up, and about 5 game years. "Maybe we could just increase the money payout or something a bit." Also
"make sure we're getting enough weapons over the course of the campaign".
- **The knob:** `kPayPercent` (mw1_campaign.h, 100) scales the House's pay ceiling, so the opening, every
  counter-offer, the advance and the floor follow it. It's applied on top of MW1's own arithmetic and kept in its
  bounds. `pay_percent()` and `pay_tier_percent()` are what a sweep sets.
- **The measure:** 93's playthrough as a gauge (`MW1_PLAYTHROUGH=1 MW1_PLAYTHROUGH_TO_DW=1`, `MW1_PAY_PERCENT`,
  `MW1_PAY_TIERS`, `MW1_ITEM_SOURCES`). It stops at the Dark Wing's first battle and prints:
  - PACE: each step's month, contracts and fights;
  - TIME: the game months spent on contracts, story battles, travel and the Bar;
  - ITEM: each weapon's and equipment's first fight, salvaged and owned;
  - SOURCE: each fielded chassis and loadout's items.
  About 50 s a seed. Its player wins every battle at once, by a headshot, so its repairs are near nothing and its
  salvage is the kill's.
- **Fights and months to the Dark Wing's first battle** (12 seeds each; medians, with the range):

  | pay | fights | months | rung 1 at fight (month) |
  |---|---|---|---|
  | 100% | 41 (37-60) | 102 (83-138) | 26.5 (57) |
  | 125% | 34 (31-41) | 86 (74-110) | 24.5 (52) |
  | 150% | 35 (29-109) | 85 (66-397) | 22 (48) |
  | 200% | 33.5 (29-125) | 77 (71-456) | 20.5 (44) |
  | 300% | 27.5 (21-43) | 71 (55-87) | 15 (32) |
  | tiers 200/150/100/100 | 35 (29-109) | 84 | 22 (48) |
  | tiers 300/200/100/100 | 33.5 (27-125) | 77 | 20.5 (44) |

  - 73 fights in 93's run counted all three Dark Wing tries and the rebuilds between them. To the first, it's about
    40 at today's pay.
  - Rung 1 is the grind. It takes 110 t against a 35 t commander, which means three light 'Mechs at 3.1-3.7M each.
    Early contracts net 220-350k. Rung 1 comes at fight 26 of 41, and after it the rest come fast: funds run 16M at
    rung 1 and over 100M at the Dark Wing.
  - More pay, flat or by tier, shortens everything together. Rung 1 stays 60-70% of the road, so pay doesn't give a
    steady build-up.
  - The months: a contract takes about 2 game months (MW1's mission time), travel 20-40 months in a run, the Bar and
    the story battles about 2. About 40 fights is about 100 months, whatever the pay.
  - The few long runs (109, 125 fights) and the step-7 stalls (a few runs) are the test player's, not the game's: it
    spends down and can't pay the fare to the lead's world.
- **Weapons and equipment** (pay 100%, 10 runs; the lab offers 110):
  - Had by fight 10, 20, 30 and 40: 22, 41, 55 and 62 (medians), 17, 36, 50 and 58 of them salvaged. Pay 125% is the
    same: 22, 42, 60, 63.
  - FF armour by fight 14 and Clan FF by fight 2, in every run.
  - The Inner Sphere targeting computer: no loadout carries it, so it never comes. The Clan one is on the Puma's
    Primary, the Masakari's five and a Thor Alternate (the right torso). It was seen in 1 run of 10, and seed 2's
    three Puma Primaries left none: why is open.
  - Never seen, and no fielded loadout carries them: AC 2, AC 5, PPC, ER Laser (S), SRM 2, Streak SRM 4 and 6 (and
    the Clan Streak 4), LB-2X, LB-5X, LB-20X, Ultra AC 2, 10 and 20, MRM 10 and 40, TBOLT 10 and 15, Clan LRM 5, Clan
    light MG, Clan Heavy Laser (S), MASC, ECM.
  - Never seen, though fielded loadouts carry them (Alternates and Exotics only): the Clan AMS (17 loadouts, also
    open), Clan ECM, C3 computer, Clan TAG, Clan heat sink, Clan heavy MG.

## 119. [x] The release tidy: MW1's own build warnings cleared *(done, mw1-fix119-warnings)*

For clean 1.0.0 builds: no warning from MW1's own code (mw1/src, mw1/tests) on Mac clang or Linux GCC 13.3; the
engine's warnings are left alone. No behaviour change.
- Clang's 18: the unused title_case, ge and kLabChassisPage gone; the results lists' rows and the story's Reader
  with every field written; the tests' unused names and unwritten fields.
- GCC's 3 more: the market's tons column with room for any int (the text the same), a size compared as size_t,
  a back button from four screens' enums as int32_t.
- 93's playthrough test: an unused counter, an OwnedMech with every field.

## 1. [x] The main menu is Pirate's Moon's *(done: title screen ours)*
- **Seen:** starting MW1 shows a PM-style menu.
- **Wanted:** MW1's own title screen, using the generated `title_bg` with NEW / LOAD / QUIT in MW1's style.

## 2. [x] NEW CAMPAIGN sits on a black screen for about 30 s
- **Seen:** after NEW CAMPAIGN, a black screen for about 30 seconds, then the intro picture.
- **Cause (the PC's log):** "the intro, 0 lines of narration". mw1.exe was opened by itself, not from the launcher, so it had no MechWarrior (1989) folder and no MW_MAIN.EXE to read the narration from. The intro still waited out the narration's ~25 s on an empty screen.
- **Fixed:**
  - With no narration, the intro goes straight to the picture.
  - mw1.exe opened without all three games says which are missing and offers to open the launcher beside it, which finds them.

## 3. [x] Every screen between missions must look like MW1, with our generated art *(swept 2026-11-18: mw1-sweep-status, -overlap, -screens, -status-fit, -loadbox-style, -dev1; PM wordings left for Owen in 28)*
- **Seen:** the screens are text panels on Pirate's Moon's chrome. "There's no reason why this should play like Legend of the Red Dragon."
- **Wanted:** MW1's layouts (Owen's reference shots), rebuilt with the art generated for this. The art is in `/Volumes/Callisto/claude-files/keep/mw1-remake/assets/mw3/` (142 images, catalogued in `CATALOG.md` there). `mockups/` has the hub and Bar mockup.
  - **Hub:** full-screen world scenery picked by the world's type (`hub_*`, 13 types plus Ander's Moon and Dustball). Riveted plaques across the top with the world's name and the date. Picture tiles down both sides for the services (`icon_*`: House office, starport/travel, Mech Complex, Bar, company, options), clicked to enter.
  - **Bar:** the bar scene (`bar_scene`) with a framed menu panel: ORDER DRINK / RECRUIT CREW / LEAVE.
  - **Crew:** MW1's grid. Portraits across the top (`recruit_*`, MW1's 42). NAME / GUNNERY / PILOTING / WAGE / MECH rows, then each pilot's mech picture underneath (`mech_*`), with DONE.
  - **Mech Complex, dealer, repair, ammo** (`mechcomplex_*`, `mech_*`), **House office and liaisons** (`office_liaison`, `liaison_<house>`), **NewsNet**, **company**, **travel sequence**, **story scenes** (`story_*`, `char_*`), **after action**, **endings**: one screen each, per CATALOG.md's 25.
  - **Chrome:** the plaques, frames and buttons are drawn by hand, sharp and in MW3's style, not generated.
  - **Done so far:** the title screen and the hub, after Owen's mockup. Owen played it on the Mac (2026-11-17): "significantly better".

## 4. [-] The briefing screen, rebuilt in MW1's style *(superseded by 18: no pre-launch screen)*
- **Seen:** the contract goes straight into Pirate's Moon's Instant Action briefing.
- **Wanted:** our own briefing and deployment screen in the same look: the Mercenary Contract (employer, world, mission, enemy force, pay, salvage, advance), then the launch. Uses `contract_terminal`, `briefing_holotable` and `deploy_launch`.
- **The final screen before launch, as in MW1's original:**
  - the buttons run down the **left side**, as in MW1, not along the bottom as in MW3;
  - MW1's LAUNCH button is there;
  - a **MECH LAB** button goes above LAUNCH and opens Pirate's Moon's mech lab, then returns to this screen.
- **The rule for all of it (Owen):** completely rework MW3/PM's interface so it looks like an updated MW1. MW1's layouts win over MW3's wherever the two differ, for example buttons on the left, not the bottom.

## 5. [x] No Pirate's Moon or MW3 dialogs, buttons or art left anywhere in MW1 *(swept 2026-11-18: mw1-sweep-status, -overlap, -screens, -status-fit, -loadbox-style, -dev1; PM wordings left for Owen in 28)*
- **Seen (Owen, Mac, 2026-11-17):** after the new hub, PM's own dialogs, buttons and art still show. For example: PM's message boxes (the quit confirm: "Are you sure you want to quit?"), its buttons and frames, and its menus and screens around the mission.
- **Wanted:** "remove all the mw3/pm assets and dialogs and buttons and stuff. switch it to mw1 style dialogs."
  - Every message box, confirm, notice and error goes through one MW1-style dialog: our chrome, our fonts, buttons in MW1's look.
  - No PM image, frame, button or font anywhere a player sees in MW1, apart from the 3D battle itself.
  - The screens MW1 borrows from PM keep their logic but get MW1's look: the mech lab (reached from the launch screen), the options and settings, load and save, and the pause and in-mission menus.
  - Remove whatever MW1 never uses, such as the multiplayer screen PM builds in the background.
- **How to check:** a screen-by-screen pass with shots. A test that fails if an MW1 screen loads a PM dialog file or binds a PM button.

## 6. [x] The hub's scenery by MW1's landscape *(done: mw1-hub-landscape; the Bar has no landscape art yet)*
- **Owen (Mac, 2026-11-17):** "In the original, it just showed you the background of whatever city you were in. If you're in an icy area, it shows an icy background. If you're in a tropical area it's very green."
- **MW1:** every world has a landscape: desert (48 worlds), tropical (79) or arctic (18) (research/economy.md, the planet record's code). The hub draws that landscape's picture. MW1 also uses it behind the Bar, the company and the options screens.
- **Ours today:** the hub picks a scene by our own "world type" classification (agri, capital, industrial ...), which ignores the climate.
- **Wanted:**
  - The hub's scenery follows the world's landscape. There are five settings per landscape (starport, walled city, Mech factory, frontier town, garrison), being generated now as `hub_<landscape>_<setting>` (`keep/mw1-remake/jobs.py`, section 5), with the world picking one so neighbours differ.
  - Ander's Moon and Dustball keep their own scenes.
  - The Bar's window view and the options backdrop follow the landscape where we have art.

## 7. [x] Contracts only through the House office, negotiated, as in MW1 *(done: mw1-hub-office)*
- **Owen:** "As soon as you start a new campaign, it enters into the 'take this contract' screen. I want the contract screen to be accessible in the same way it was accessible on MW1. You had to go to the menu option to take a contract, and the contracts are negotiated."
- **Wanted:**
  - The hub shows no offer, and has no ACCEPT/DECLINE.
  - The House office is where contracts come from: ask for a mission, see the offer, move to the next offer, negotiate the terms, ACCEPT or LEAVE. That's MW1's CONTRACT_OFFICE, REQUEST_MISSION_OFFER, NEXT_MISSION_OFFER and NEGOTIATE_CONTRACT.
  - With a contract accepted, the hub shows the current contract in its panel and offers the deployment: MW1's hub has a deployment tile.
  - A new campaign starts on the hub, after the intro, with no offer up.
- **Owen again (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-hub-offer.png`):** the hub's CONTRACT ON OFFER with ACCEPT/DECLINE "pops up as soon as I start the game. Contracts are usually accepted within a specific screen, and should be accessible there."

## 8. [x] The mech lab in the Mech Complex, beside repair *(done: mw1-mech-complex)*
- **Owen:** "We need the mech lab available as an option, too. Probably in the same submenu as the repair item is in the original MW1."
- **MW1:** Mech Complex, then OWNED MECHS, then the mech's status screen: REPAIR / REPAIR ALL / RELOAD / SELL / DONE.
- **Wanted:**
  - MECH LAB goes on that status screen with the others. It opens PM's mech lab (in our reskin) for that mech and returns to the status screen.
  - The pre-launch screen keeps its MECH LAB above LAUNCH (item 4).

## 9. [x] The title screen's picture scrolls and tiles *(done: mw1-still-pictures + set_menus_still)*
- **Owen (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-title-scrolling.png`):** "The opening scene is scrolling but it doesn't really work for our images. We should make it still images instead."
  - The shot shows `title_bg` drifting and wrapping: its right and bottom edges repeat the picture, with hard seams.
  - "CRITICAL HIT" is drawn with broken letter spacing.
- **Wanted:**
  - Every picture in MW1's screens stays still and fills the screen at any shape (cover, centred): the title, the intro's pictures, the hub's scenery and the story's scenes.
  - No drifting or panning anywhere. Check every screen for it.
  - Fix the subtitle's spacing.

## 10. [x] The story's event panel in MW1's look *(done: mw1-event-panel)*
- **Owen (shot `keep/mw1-remake/reference/owen-play-event-panel.png`):** "This uses the standard PM button style in the far corner, and it also has the text in the top left for some reason. Maybe the text should be centred with a faded/darkened background image behind it."
- **Wanted:**
  - The event's text centred on screen, in a framed terminal panel, over a darkened, faded scene picture: the event's own `story_*`/`char_*` art where we have it, else the world's hub scenery.
  - The world and ARRIVAL heading on our plaque.
  - NEXT/OK and a choice's captions as MW1 buttons: our chrome, not PM's ring button in the corner.
  - This was phase 5. It moves up, because every campaign opens on it.
- **Owen again (2026-11-17, `owen-play-event-panel-2.png`):** on entering the game it's still "a 4:3 black screen ... with the same misplaced text in the top left and the same PM button in the bottom right." It's drawn as a 4:3 box over the title, with the title showing at both sides. "Buttons should match the intro style that we've established, not the PM style. We should be using the same NEW/LOAD button style."
  - The panel fills the screen at 16:9 (no 4:3 box, no title showing through).
  - **Every button in MW1 is the title's NEW/LOAD button** (`mw1_chrome`'s framed button): OK, NEXT, a choice's captions, the load box's OK/NO, and any other place PM's ring or dot buttons still show.

## 11. [x] The hub's tiles where MW1 puts them *(done: mw1-hub-office)*
- **Owen (2026-11-17):** "I don't think the menu items are in the right spot. I think the bar was on the right side, the second menu option ... the contract spot is in the bottom left I think? I want to arrange everything correctly."
- **MW1's own tables** (MW_MAIN.EXE DS:1083, 10D7 and 1101, decoded from Owen's copy; the port's MW_MAIN_081B_PLANET.c draws them). Two columns of three:

  | | Left | Right |
  |---|---|---|
  | Top | COMPANY (the company menu, 247A) | STARPORT (travel, 5620) |
  | Middle | MECH COMPLEX (2DA6) | BAR (692E) |
  | Bottom | HOUSE OFFICE (contracts, 4AA2): the House's crest; empty on a world with no office | OPTIONS (6CFA) |

  - **With a contract accepted,** MW1 draws only the left column: COMPANY, MECH COMPLEX, and DEPLOY (1B55, the launch) where the office was. The right column goes: no travel, Bar or options until the contract is fought.
- **Ours today:** Mech Complex / Bar / Starport on the left, House Office / Options on the right, no Company tile, and a MAIN MENU button.
- **Wanted:**
  - MW1's arrangement exactly, including the contract-accepted hub.
  - A COMPANY tile (`icon_company`) opening MW1's company screen. That is where MW1 keeps the crew grid (CREW_MENU), so YOUR LANCE moves there from the Bar, and the Bar goes back to MW1's ORDER DRINK / NEWS / RECRUIT CREW / LEAVE.
  - The House office tile shows the world's House crest (`icon_house_<house>`).
  - DEPLOY gets a tile of its own (a dropship launch: art to come; until then `deploy_launch` cropped).
  - MAIN MENU moves inside OPTIONS (MW1's options hold save, load and quit).

## 12. [x] Blocky, low-resolution text and pictures *(done: the 2D at window resolution (FIXES 90 with it); the windowed check passed 2026-11-21, "text looks sharp and fine now")*
- **Owen (2026-11-17):** "The text looks like it's really low resolution. Chunky and blocky ... hard to read. I get the impression it's not the font itself, it's something else."
- **Cause:**
  - It isn't the fonts (Saira Condensed and Share Tech Mono are sharp). PM's 2D layer draws every menu on a canvas 480 pixels high (854×480 at 16:9), and the window scales that canvas up: about 2.3× in Owen's 2000×1125 window, 4.5× at 4K.
  - So the glyphs are rasterised at 480-line size and magnified, and so are the pictures (packed at 854×480 from 1344×768 sources).
  - Phase 1 noted this limit and deferred a high-resolution 2D path until Owen found it soft. He has.
- **Wanted:**
  - MW1's 2D drawn at the window's own resolution. Layouts stay in canvas coordinates; text is rasterised at the scaled size; pictures are packed at a higher resolution (at least the 1344×768 sources, scaled down per window) and drawn at device pixels.
  - An "ours" engine path, opt-in, with MW3 and PM unchanged.
  - The image archive's size needs a plan (compressed pixels, or PNGs kept as PNGs) before the art is repacked at a higher resolution.
- **Step 1 (2026-11-18, dev 2):** shots at 1080p and 4K in `outbox/2026-11-17/hires-step1/`. They confirm a nearest-neighbour upscale of the 480-line canvas (4.5× at 4K), while the 3D is drawn at full resolution. A possible stopgap is a linear filter on the canvas blit (softer, not sharper). For Owen: stopgap first, or straight to the real path?
- **Owen (2026-11-18): "Real fix"** (option B in `research/hires-2d.md`, no stopgap). Pictures: JPEG q90 for scenes and portraits, PNG for alpha and hard edges (the plan's recommendation). PM's widget art stays canvas-sharp. Split: dev 2 takes B0, B1, B3 and B4 (the engine and text); dev 1 takes B2 (the pictures).
- **Owen (2026-11-18), after the 4K audit:** "i want to do it the right way ... no shortcuts or band-aids."
  - Everything on MW1's screens is drawn at the window's own resolution from a true source. No upscaling of small art stands in for that.
  - **Text:** already right. The fonts are real outline fonts (Saira Condensed, Share Tech Mono) rasterised at window size; no font change is needed.
  - **'Mech icons (Owen, corrected):** PM's own amber outlines, not new art and not the 3D render. PM ships them only at 41x50 (rimage.zbd; the MFB salvage screen uses the same ones). So each is vectorised once into its line and fill layers (PM's exact three colours) and redrawn at window scale. An upscale is the last resort only (dev 1).
  - **The Mech Complex's 'Mech (Owen, 2026-11-18):** "3d renders of the mechs from the front in gunmetal for that screen, not using image renders". The panel draws the 'Mech's 3D model live, front view, in PM's GUNMETAL skin, at window resolution, as the lab draws its 'Mech. That covers every sold chassis (it replaces the plan to pre-render all 24). The other screens using stored renders (market rows, Company, loading, briefing) are to be asked (dev 1 plans).
  - **Owen's names (2026-11-18):** **mech outlines** are PM's amber outlines redrawn as vectors: small, they fit anywhere. **Gunmetal renders** are the full 3D 'Mech in GUNMETAL: they can spin.
  - **Where each goes (Owen, 2026-11-18):**
    - Mech Complex panel: the gunmetal render, live, still and facing front; spinning is built as an option.
    - Company page: each pilot's 'Mech as its mech outline, under the pilot's name.
    - Mech Complex market: a mech outline next to each 'Mech's name in the list for sale.
    - Loading screen: a stored gunmetal render, made for all 24 sold chassis at first run.
    - Lab slot: the mech outline.
  - **Star map:** the backdrop is redrawn at window scale (done). The dots, rings and jump line are drawn by MW1 at window scale (dev 2, b2).
  - **Chrome lines and PM's list arrows / scrollbars:** no longer left at canvas size. They're drawn at window scale too, from geometry or redrawn art. This reverses the earlier "PM's widget art stays canvas-sharp".
  - **Framed tiles, DEPLOY's tile, the lab panels:** from their full-size source pictures (dev 1, B2).
  - **The mission HUD's keyed-overlay gap:** to be scoped. It's shared with PM's engine, so plan it with the mw3 lead.
  - **The 4K sweep (dev 2, 2026-11-19):** every MW1_SCREEN page and a mission at 3840x2160, hidden, read for "draws
    left to the canvas" and looked at in crops.
    - Fixed: PM's own 'Mech icons as PM's screens show them (the lab's chassis at its top, `icon_<Mech>_<colour>`) are
      now drawn from their mech outlines, in the icon's own colour. At 4K they look the same (PM's pixel art traced
      edge for edge), but the log no longer lists them.
    - Fixed: mw1_clear (a part MW1 doesn't show) and mw1_black (the travel map's and the narration's backdrop) are
      drawn flat at any scale, so the log lists only real gaps. Every menu page now logs none.
    - Left: the mission radar's circle, sweep cone and boundary lines are drawn into the canvas as lines. The
      high-resolution draw has only text and pictures, so they stay blocky at 4K, and the log can't see them. That is
      this keyed-overlay gap, for the mw3 lead. Text, contacts, bars, the compass and the damage display are already
      sharp. The cockpit's dashboard is a 3D texture, not 2D.
  - **The 2D at 4K is finished with item 89 (dev 2, 2026-11-19):** the radar's lines and circles are drawn
    anti-aliased at the window's scale.
    - A follow-up (FIXES 90: filled polygons and dashed lines, the same way) was built and tested, then merged on
      2026-11-20 (Owen's OK, rebased by dev 1 as `mw1-fix90-fills-dashes-r`). It is opt-in through hires_2d, MW1 only.
      A probe of the mission, pause, results, travel, lab, hub, market and bar pages found only two host polygons,
      both whole-pixel quads on the mission HUD that look the same either way, and no dashed lines (nothing in the
      port sets a line's dash).
    - Nothing draws differently yet; the path is ready for a later screen that needs it.
    - Still open in item 12: only the windowed check, which waits on Owen.

## 13. [x] The Mech Complex's market: lists and buttons don't work; PM's look *(done: mw1-mech-complex)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-market.png`):** "These buttons/lists don't seem to be working. Also, can we make the buttons, colours, and general style closer to the other MW1 style we've established?"
- **The shot:** the House market (FOR SALE / YOUR MECHS) has PM's ring buttons along the bottom (BUY / SELL / PILOT / BACK), olive list boxes and PM's terminal font, with the title at the top left.
- **Wanted:**
  - Find out why the lists and buttons don't respond, and fix it: clicking a row selects it, the arrows scroll, and BUY / SELL / PILOT act. Add a loop test that does each one by mouse.
  - Rebuild the screen in our look: the plaque title, framed terminal panels (the hub's COMPANY panel), and NEW/LOAD buttons down the left as in MW1.
  - Rebuild it as MW1's Mech Complex flow, which is item 8's: the root menu, OWNED MECHS, the mech's status screen (REPAIR / REPAIR ALL / RELOAD / SELL / MECH LAB / DONE), and the purchase list.

## 14. [x] Mech pictures rendered from the game's own models *(done: mw1-mech-renders, mw1-render-cover, mw1-render-pass)*
- **Bug (2026-11-18, dev 2):** the first-run pass wrote archives holding a capture of the lab's 2D screen, not mechs, and stamped them current. Fixed (mw1-render-pass): zipkit's conversion left its nodes in the pool, so the lab skipped loading its world and models on a first run; the pool is now freed, renders are v2, and a capture sanity guard checks each picture.
- **Owen's pick (2026-11-17, from `outbox/2026-11-17/mech-renders/`):** the amber-toned grade, front view.
- **Owen (2026-11-17):** "Is there any way we could take the mech vertices and create a snapshot of them from the front to use as the images instead of the AI-generated images? The AI-generated images aren't very accurate."
- **Wanted:**
  - Every picture of a specific mech (market rows, the Mech Complex, the crew grid, the hangar tile if it shows one mech, briefing enemies) is a render of that mech's own model by our engine. The renderer is the one the mech lab draws its mech with.
  - The camera is from the front (a slight three-quarter if it reads better), on a dark backdrop, in MW1's grade.
- **Constraint:**
  - The models are MW3's and PM's, from the player's install, so the renders can't go in the public repo. Make them at first run (or at build time from the install), like zipkit's conversions, into the write dir's cache.
  - AI art stays for scenes only.
- **Size:**
  - One picture per chassis and per skin the market shows.
  - Rendered at the item 12 resolution.

## 15. [x] A button's hover: the frame flashes once, the dot keeps blinking *(done: mw1-mech-complex)*
- **Owen:** "When I hover over the buttons, the border blinks yellow for a split second and then it goes off, but the dot next to it blinks yellow consistently. Is that intended?"
- **Not intended.** It's PM's button states (a one-frame highlight, then the blinking lamp) showing through our chrome.
- **Wanted:**
  - On hover, the frame lights amber and stays lit while the pointer is on it. The dot is lit steadily, with no blinking.
  - The same on keyboard focus.
  - One rule for every MW1 button.

## 16. [x] The hub's centre panel: a message you dismiss, then the whole scene *(done: mw1-hub-messages)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-hub-panel.png`):** "In the original, sometimes messages pop up, but there's a dismiss button that makes it go away so you can see the whole background image."
- **MW1:** the hub shows its notifications (MwShowPendingNotifications, on entry) in a box with a button. Once you've dismissed them, the hub is the landscape and its tiles.
- **Ours today:** the big centre panel is always there. It shows the hovered tile's description (HOUSE OFFICE: ...), and it covers the scenery.
- **Wanted:**
  - The centre panel appears only when there's something to say: arrival notices, payday, news, a contract's state change. It has an OK button (our framed button), and it's gone once dismissed.
  - With no message, the hub is the scene and the tiles.
  - A tile's description on hover goes to the bottom status line, which already says "Pick a service".
  - The COMPANY readout stays (Owen's mockup has it), unless he says otherwise.
  - With a contract accepted, its summary shows in that readout or on the status line, not in a permanent centre panel.

### Owen on items 8 and 13 (2026-11-17)
- "I was expecting entrance through the Mech Complex. We might not need the right-side menu in the Mech Complex. We can assign the pilots to their corresponding mechs in the mech lab. I guess the list on the right can show the mechs we currently own, that would be fine."
- **So:**
  - The Mech Complex is where the mech lab is entered.
  - Its right-hand list is YOUR MECHS (the owned mechs). There's no separate right-side menu.
  - Assigning pilots to mechs (today's PILOT) moves into the mech lab.

## 17. [x] The mech lab: readable, separated, nothing overlapping *(done: mw1-lab-legible + mw1-lab-17b)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-lab.png` and `owen-play-lab-criticals.png`):** "I actually really like that a lot. But we have to do something about the font. Is the problem the resolution? It's really hard to read. We need more separators and we need to clean it up to make it easier to see what's happening. And some things are poorly positioned. Overlapping text and lines." On the criticals page: "the right side is coming out of the boxes."
- **Two causes:**
  1. **The resolution (item 12).**
  2. **Our font mapping.** PM's lab fonts were mapped to Saira Condensed, which at the lab's small heights crowds and breaks apart ("Inter nal St ruct ure"). The mapped fonts' widths also differ from PM's, so text sized for PM's fonts overruns PM's boxes.
- **In the shots:**
  - **Chassis page:**
    - LOAD-OUT's first row crosses its rule.
    - 'MECH DATA runs to the panel's edge and covers the page icon.
    - There's a stray ring in SKINS.
    - Amber glow blobs sit behind the chosen rows.
    - PILOT/YOU floats in empty space.
  - **Criticals page:**
    - AVAILABLE SUPPLIES' tabs (Weapons Ammunition Equipment Armor) run past the panel's right edge.
    - ENGINE SELECTION's rows (Rating, Manufacturer, Type, Tonnage, Critical Points) spill below its panel.
    - Engine's values and arrows overlap (a ring over Rating's arrow).
    - The location list (Head ... Right Leg) is crammed.
    - PILOT/YOU shows here too, over the supplies list's space.
    - The load-out's down arrow sits outside its panel.
- **Wanted:**
  - Data rows in a wider face (Share Tech Mono, like the COMPANY readout); titles in Saira, uncramped.
  - A rule under each title, row separators, right-aligned value columns.
  - **Everything inside its box** at 16:9 and 4:3 on every lab page (chassis, weapons, criticals, groups) and the load box.
  - Our amber bar for chosen rows.
  - PILOT on the chassis page only, as a defined row.
  - No stray icons.
  - If PM's layout can't hold this, the lab gets its own layout (set_layout) with PM's controls bound, as options did.
  - Guard test: every text object's measured width fits its panel.

## 18. [x] DEPLOY goes straight into the mission *(done: mw1-deploy-direct-2)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-deploy-briefing.png`):** "When I hit deploy it takes me to this screen. This is a PM screen I believe, with the background removed. Instead, it should skip this page and just bring me to the game itself."
- **Owen again (2026-11-17, `owen-play-deploy-briefing-2.png`):** "this page is still coming up too."
- **The shot:** PM's briefing (BriefingScreen) with its backdrop missing. It shows PM's page icons, PM's LOADING lettering, a PM mech, and "MechWarrior 3 Pirate's Moon 1.0.11.5".
- **Wanted:**
  - DEPLOY launches the mission directly. PM's briefing is never shown in MW1.
  - While the mission loads, show an MW1 loading screen: the mission's map or the world's climate scene dimmed, our plaque with the contract's world, employer and objective, and LOADING in our type. No PM version text or PM art.
- **Supersedes item 4's pre-launch screen.** The lab is now reached from the Mech Complex (items 8/13), so there's no MECH LAB above LAUNCH. The briefing's facts (objective, enemy, map) show on the loading screen and in the hub's contract readout.

## 19. [x] After a mission: MW1's own results screen, with the salvage *(done: mw1-results-screen)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-debrief.png`):** "When I win, I was expecting it to show the salvage I got. Also, we should redesign this screen."
- **The shot:** PM's Instant Action debrief, with three green PM columns ('MECH STATUS / MISSION RESULTS / SALVAGE FOUND), PM's icons, and PM's score ("Mission Total 300"). SALVAGE FOUND is empty because our salvage is collected by mw1 (program_salvage_mech into the mission's own store), not by PM's salvage.
- **Wanted:**
  - PM's debrief is skipped in MW1, as its briefing is (item 18). The mission ends on MW1's own results screen, in our look over the dimmed climate scene, with our plaque (world, employer, VICTORY / DEFEAT / RETREAT).
  - **Results:**
    - the objective and its outcome;
    - the enemy mechs destroyed (by whom);
    - the time;
    - lancemates' fates.
  - **Pay:**
    - the fee (with any deduction);
    - salvage cash: "x% of wrecks' value", with each wreck's value listed;
    - the total, and the new funds.
  - **Salvage to stores:** every weapon, ammo, armour and equipment item added to the stores, with quantities (none on a loss).
  - **Your mech's state:** a damage readout per location.
  - Our framed buttons: CONTINUE back to the hub.
  - It replaces the hub's AFTER ACTION panel; the hub then shows a short message (item 16) if anything is left to say.

## 20. [x] Offers name the mechs you'll actually fight, by weight class, highlighted *(names: b3cd78b1; highlight: mw1-offer-highlight; colour for Owen to confirm, see 28)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-offer.png`):** "This shows a 'Jenner' as the enemy. We aren't using MW1 mechs, we're using exclusively MW3 mechs right now. And in the original, it shows the enemies/objectives/etc highlighted in a brighter colour so you can easily scan through and see what you're taking on. Can we light up the number of mechs, or the types of mechs? Like I think the original said '2 heavy' or '2 medium'."
- **Cause:** contracts are drawn from MW1's mech kinds (kMechKinds), and each spawns as its PM stand-in (Jenner → Owens, research/contracts.md). The text shows MW1's name, so Owen fought an "Owens" that the offer called a Jenner.
- **Wanted:**
  - Every player-facing mention of an enemy mech (offers, the hub's contract readout, the loading screen, the results screen) uses the PM mech's name.
  - MW1's own kind names stay internal: logs, tests, research.
  - The offer's enemy line reads by weight class and count, as MW1's does: e.g. "Enemy: 2 Heavy, 1 Medium", with the mechs' names after it (Avatar, Bushwacker, ...).
  - The key facts are drawn in a brighter highlight colour (white or bright amber) against the green-grey: the employer, the target, the objective, the map, the enemy count and class, and the pay.
  - Check MW1's own offer text in research/ for its exact wording of the class line.

### Owen on item 16 (2026-11-17)
- "Still can't get rid of the big message here on the main page." Item 16 isn't merged yet.
- "And the green box that records my kills, I don't think we need all that info. Where did that fit in the original? Maybe we keep C-bills, payroll, and kills at the top, between the GREELY and MAY 3024 boxes, so it's out of the way."
  - MW1's hub had no such box. Its hub is the landscape, the two plaques (world, date) and the six tiles. The company's figures were on the Company screen.
- **So:**
  - The green COMPANY box goes.
  - C-BILLS, PAYROLL and KILLS go in one slim strip between the two top plaques.
  - STORES (the count of parts in the company's stores) leaves the hub. The parts list lives in the Mech Complex and the lab.
  - The mech line (OWENS LIGHT) leaves the hub too.
  - The accepted contract's summary goes on the status line.

## 21. [x] Travel: every world findable, and each world's trade facts on the map *(done: mw1-travel-readout)*
- **Owen (2026-11-17):** "Doesn't look like our map has every single planet available for travel. Does travel work? ... Can we make sure they have every single planet and every planet matches the attributes in the original? High mech sale price or cheaper prices, different contracts, etc. That's how a lot of people won this game originally. They travel, sell the Jenner, travel back, buy a Shadow Hawk ..."
- **Today:**
  - All 145 of MW1's travel worlds are on the map (research/economy.md, mw1_travel.h). Each has MW1's own tier, House, office flag, position and travel weight, read from MW_MAIN.EXE.
  - MW1's trade rules are ported:
    - sale 90/110/120/140% by tier;
    - stock size and damage by tier;
    - the dealer's 1–3 / 1–2 buying limit at tiers 2 and 3;
    - ammo mark-ups.
  - But only the five capitals are labelled. The other worlds are small dots, and the office-less ones are tinier, so the map looks sparse and you can't tell a rich world from an edge world without visiting it.
- **Wanted:**
  - **Hover or select any world** for a readout: its name, House, office (contracts) or none, "Mechs sell at 140% (tier 3)" / "buys at 100%", the dealer's limit, the distance, and the trip's cost and time.
  - **Bigger, clearer dots,** and a filter (FILTER exists) that can show by tier: where to sell high, where to buy cheap.
  - **A test that the arbitrage loop works end to end:** buy at tier 0, travel to a tier 3 world, sell for a profit, travel back. Check with the dealer limit and the travel cost.
  - **Check the contracts per world:** the planet record's byte 3 (the biggest mission category there), against how mw1_missions picks a category. Use it if MW1 does.

### Owen on item 17 again (2026-11-17, `owen-play-lab-2.png`)
- "The mech lab is still poorly positioned. The box that contains data about run speed is too low and is being overlapped by other stuff. And the font is really poor quality. It looks all pixelated. Why is that? It's also overlapping the dividers/lines."
- Item 17 moves to the front.

## 22. [x] Our cursor everywhere *(done: mw1-lab-17b)*
- **Owen (2026-11-17):** "It's using the MW3 cursor now. I liked our other one."
- **The shots:** our amber crosshair on the hub and company screens; PM's small ring on the lab (`owen-play-lab-2.png`, left edge) and on PM screens.
- **Wanted:** our crosshair on every screen in MW1, including PM's own (lab, settings, load box) and over the loading screen. One guard test.

## 23. [x] The Company screen shows PM's mech icons, not generated pictures *(done: mw1-company-icons)*
- **Owen (`owen-play-company.png`):** "You know the icon that exists at the top of this, where I pick the mech for the individual? We should use those mech icons in the company screen instead of the generated images."
- **That's** the lab's mech-slot icon: PM's line-art icon for each chassis, the yellow wireframe at the top of the lab.
- **Wanted:**
  - Each mech on the Company screen is drawn with its chassis' PM icon, in our amber, read from the player's PM install at run time.
  - It replaces the AI mech art (`mech_*`) there.
  - The same icon may serve the Mech Complex rows until item 14's renders exist. Item 14 then decides per screen.


## 24. [x] Hold an arrow to keep stepping *(done: mw1-hold-repeat)*
- **Owen (2026-11-17):** "On the negotiation page, I can hold the arrow down and it'll go up or down as I hold."
- **Wanted:**
  - The office's term arrows (pay, salvage, advance) repeat while held: one step on the press, then repeating after about 0.4 s, and faster after 2 s.
  - The repeating stops when the button is released or the pointer leaves it.
  - It's an opt-in flag on our framed button, used by the list scroll arrows too.

## 25. [x] PM's own buttons still blink (the rest of item 15) *(done: mw1-steady-buttons)*
- **From the review of item 15:** our framed buttons are steady, but PM's HelpButtons on PM's screens shown in MW1 still blink: the lab's page icons, the load box's OK/NO, and settings.
- **Wanted:** an opt-in engine switch (like set_menus_still), so that while MW1 runs a HelpButton's lamp is steady when hovered and never blinks. MW3 and PM keep theirs.

## 26. [x] The mech lab's leftovers *(done: mw1-lab-leftovers)*
- **From the lab reskin's review (mw1-lab-reskin):**
  - The mech-slot icons at the top are still PM's look: PM's lab_player* pictures don't exist, so they're drawn from elsewhere.
  - Page 1 (weapons) is empty, as in PM, which has no ICON_WEAPONS. Either hide its page icon or give it content.
  - On criticals, the armour toggles' arrows sit beside "Internal Structure", as in PM's layout.
- **Wanted:** the slot icons in our amber; no empty page reachable; the armour toggles placed with their own rows. mw1labfit still passes.

## 27. [x] Every mech picture: the PM line-art icon until item 14's renders *(done: mw1-company-icons)*
- **Owen (2026-11-17):** "including the company -> mech images," together with item 23.
- **Wanted:** wherever MW1 shows a picture of a specific mech (the Company screen first, then the Mech Complex rows if they get pictures, and the briefing/results screens), use PM's line-art chassis icon in our amber, not the AI `mech_*` pictures. Item 14 may later replace them with renders, screen by screen, once Owen has seen a test render.

## 29. [x] The lab's bottom buttons: one size and shape *(done: mw1-lab-buttons)*
- **Owen (2026-11-18, playing):** "the buttons on the bottom of the mech lab are different sizes and shapes. and the customize button isnt the right size. its overlapping. weapons grouping isnt too tall but its wider than the others i think. they should all be the same size/shape, including accept"
- **Cause:** MW1's lab icons keep PM's four sizes: lab_icon1 (chassis) 51x58, lab_icon4 (criticals/customize) 55x49, lab_icon5 (groups) 65x48, lab_accept 60x49. They sit at PM's positions in dlgMechLabII.
- **Wanted:** all four the same framed size and shape, evenly spaced along the bottom bar, with none overlapping, at both shapes. mw1labfit checks that they are equal and don't overlap.
- **Changed by 70 (Owen 2026-11-19, option A):** not evenly spaced any more. The buttons are in pairs at the bar's ends (chassis and criticals left, groups and ACCEPT right; mw1_chrome.h's lab_tab_x), so the warnings get the clear middle. Same size, no overlap, labels centred over their buttons, as before.

## 30. [x] Don't hold the mouse: let it leave the window *(merged: mw1-pointer-hands-off, mw1-free-mouse-2; the feel check passed 2026-11-21)*
- **Owen (2026-11-18, playing):** "lets make it so it doesnt hold the mouse. if i scroll the mouse out of the window (because i have 2 screens), or if i move it outside the viewport, it will move outside the viewport. mw3 works that way and its fine"
- **Where:** SdlInput::capture puts the window in relative mouse mode (src/frontend/sdl/sdl_input.cpp), and `want` decides when (focused, acquired, mission, window, released).
- **Wanted:** in MW1, the pointer leaves the window freely, as in MW3. Find which input decides it differently for MW1 (the menus at least; in missions, match whatever MW3 does) and make MW1 behave like MW3. Opt-in if it touches the engine; MW3 and PM unchanged.

## 31. [x] The results screen's picture: victory with a big gun, defeat kicking rocks *(with 38: the screen laid out like MW1's)*
- **Owen (2026-11-18):** "can we use pc crucible to generate an image for end-of-game that looks like the original? if you were victorious you got a shot of yourself with a big gun ... if you failed youre kicking rocks" (MW1's two debrief pictures, beside CREW STATUS, MECH STATUS, SALVAGE and PAYMENT, with a line of text below).
- **What MW1 shows (the port's after-action code, read as reference; the pictures decoded read-only from Owen's MW_PICS.BIN, `outbox/2026-11-18/mw1-original-debrief-pictures.png`):**
  - **Three pictures, 149×127:**
    - 0x4C, a win, and also an uneventful garrison tour: the commander standing with a big gun beside his mech;
    - 0x4E, a failed mission with the whole crew alive (a retreat): kicking a rock;
    - 0x48, a failed mission with someone killed or missing: the commander slumped, hurt, in a wrecked, burning cockpit, framed in a circle.
  - **The current world's House crest** in the box under them, which we already have as icon_house_*.
  - **The message under them:**
    - an uneventful garrison tour;
    - a win, worded per House;
    - a failure, worded per House;
    - then, on a win paid short: the supply-overrun or "disappointment" deduction, or the breach of contract (no pay);
    - the "abandoned by the House transports" line.
- **Owen's picks (2026-11-18):** the win is end_win_22, the retreat end_retreat_22 and the lost end_lost_11 (keep/mw1-remake/assets; prompts in endings.py, "you" worded from char_gideon).
- **Art:** the lead is making victory and defeat candidates on Crucible (qwen-image-2.1, the mw3 style, the commander as char_gideon) for Owen to pick from.
- **Then:** the results screen gets a picture panel (victory picture on a win; defeat picture on a loss or retreat), kept at source size for item 12. If MW1's exe holds the debrief lines ("A very boring and uneventful tour of garrison duty..."), read them from the player's exe as the pitches are, under the picture.

## 28. [x] Decisions waiting for Owen (not bugs; asked in chat)
- ~~Salvage on a loss~~ Owen: lost or retreated loses all salvage. Salvage cash: see 51.
- The morning list's open story questions (`outbox/2026-11-17/mw1-morning-decisions.md`): the deaths, the deadline, the ending, the commander's name, the Dark Wing battle, multi-stage battles, the major campaign's dates.
- The travel map's seven look-and-feel questions, which will be folded into item 21's readout.
- An arctic Instant Action layout built from MW3's snow world (m4/m6) at conversion time?
- ~~Item 20's highlight colour~~ Owen: bright amber stays. Should the hub's status-line contract readout be highlighted too?
- zipkit should free its own node pool (MW1 frees it after the conversion today, mw1_worlds.cpp); move the fix into zipkit?
- For the mw3 lead, not MW1: on 16:9 the engine's widening shifts a centred dialog's list arrows by the 107 px margin once too often (PM's own load box shows it). MW1 works round it in MW1LOADSAVE_WIDE.
- ~~The lab's remaining PM wordings~~ Owen: MW3's terms (Alternate, Primary, …) are fine; keep them. Was: the variant names ("Alternate A (incomplete)"), the Rename, Save and Delete links, "'Mech Name", and the SKINS cycle's camo names (GUNMETAL). Keep them, or use MW1 words?
- ~~Bar hints~~ Owen: 4:3 is rare but supported; the short hints stand. Was: Dev 2 shortened three Bar hints to fit 4:3 ("Order a drink, read the news, or find pilots for hire.", etc.). Is the wording OK?
- Item 19's results screen: Owen hasn't seen it in play yet (needs his rebuilt game). The shot is in `outbox/2026-11-17/results-screen/`; anything to change?
- **AFTER ACTION review (dev 1, 2026-11-19, branch mw1-results-review):** every ending shot at 1080p and 4:3 (`outbox/2026-11-19/mw1-results-review-28/`): win, loss, retreat, a story battle, the Dark Wing lost, a fee paid short, a lancemate KILLED, MISSING on a retreat, your 'Mech lost with a spare, your only 'Mech lost, and DETAILS. Nothing is cut or overlapping at either size.
  - **Fixed (plain):**
    - A retreat settles no fates (settle_mission: a retreat isn't counted), but CREW STATUS said "destroyed, MISSING" or "KILLED" and the picture was the someone-lost one. Now it says "destroyed", and the picture is the retreat's.
    - A retreat's SALVAGE TO STORES said "None: the field was lost"; it now says "None: you withdrew".
    - "Fee: 0" on a loss or retreat now says "Fee: none (lost)" or "Fee: none (withdrew)", like the salvage line.
    - "Salvage: all of 9,804,683 = 9,804,683" (a story battle) is now "Salvage: all of 9,804,683".
    - "Next: you take the Firefly" when it's a spare (not a lancemate's) now says "the spare Firefly".
    - test_mw1_loop checks the fee and retreat wordings. MW1_RESULTS (development) now takes a comma list: win, loss, retreat, lost, mechlost, killed, missing, story, darkwing, docked.
  - **For Owen:**
    1. A retreat costs only standing: no repair bill, no mech lost, nobody killed or missing, even when a lancemate's 'Mech went down. Is that right, or should a retreat bill repairs and keep losses?
    2. A story battle pays all the wrecks' worth as salvage (kStorySalvagePercent): about 9.8 million C-bills in the shot, ten times a contract's pay. Intended?
    3. A story battle shows the world's House crest and the House's "WELL DONE!" line, though no House hired you. Keep them, or show no message and no crest?
    4. A win with a lancemate KILLED shows the victory picture (MW1's rule: the cockpit picture only on a failure). Keep it?

## 32. [x] Missing in action: a downed lancemate can come back to the hiring pool *(mw1-missing-pilots)*
- **Owen (2026-11-18):** yes, add MW1's MISSING.
- **MW1:**
  - The after-action roster has KILLED and MISSING.
  - A MISSING pilot leaves the lance, but their unavailable flag is cleared (`PILOT_STATUS_RELEASE_AVAILABILITY`), so they can turn up in a Bar again.
- **Ours today:** `pilot_killed()` (mw1_pilots.cpp) kills every pilot whose mech is destroyed on a counted mission.
- **To do:**
  - A destroyed mech's pilot is MISSING by a fixed chance (one constant), otherwise KILLED.
  - Missing goes back to the recruit pool, keeping their skills and record.
  - AFTER ACTION shows MISSING.
  - The mech is lost either way.
  - Old saves still load.
  - Tests.

## 33. [x] The commander can't die: no campaign restart *(Owen 2026-11-18: "you cant die in this one. that was a weird mechanic")*
- MW1 restarts the campaign on a story death. Ours never does.
- `kStoryDeathsRestart` stays off for good, and story.md's Q1 is answered. A dev can drop the switch when next in that code, but there's no behaviour change.

## 34. [x] A message over a screen keeps that screen's picture, the text in its free side *(mw1-message-in-place)*
- **Owen (2026-11-18):** ORDER DRINK at Greely's Bar showed the tip full-screen, over the world's dimmed scenery (the snow), instead of over the Bar.
- **Wanted:** like the original, the message sits to one side with the screen's own picture still visible.
  - **At the Bar:** in the left area, the bartender's picture behind it, not dimmed.
- **Applies to every message that fires over a screen with its own art:**
  - the Bar's drinks, news and tips;
  - events over the hub, the Bar, the Mech Complex and the office (research/story.md: MW1's events fire over the screen they belong to);
  - the office's notes.
- **Where the text goes:** each screen names its free side.
- **What stays as it is:** the travel and story pages that have their own picture (`event_art`).
- **Done means:** shots of the Bar's drink, news and an event at 1080p and 4:3.

## 35. [x] The main menu (NEW / LOAD / EXIT, the mech behind) flashes between screens
- **Owen (2026-11-18):** the main menu shows for a moment at every screen change, which is jarring.
- **Wanted:** a screen change goes straight from one MW1 screen to the next, never showing the main menu under it.
  - Likely cause: the menu stack, since MW1's menu lies under the hub (mw1_game.cpp, "MW1's main menu over PM's ... under the hub").
- **Done means:** a frame-by-frame capture of each change (hub to Bar, Bar to hub, hub to office, the office to the lab, a mission's return), with no menu frame in any of them.

## 36. [x] An arrow clicked again does nothing until the other arrow is clicked
- **Owen (2026-11-18):** in the office's TERMS, clicking an arrow once works, and holding it repeats. A second click on the same arrow does nothing until the other arrow has been clicked.
- **Wanted:** every click steps once, and holding repeats. This applies to every arrow pair: the office's terms, the lab's, and anywhere else PM's spinner arrows are used.
- **Done means:** a test that clicks the same arrow three times and sees three steps.

## 37. [x] The lab's bottom buttons: their hover labels sit in the wrong places
- **Owen (2026-11-18):** each button's label shows away from its button:
  - "Select Chassis" lies over the orange line;
  - "Customize" sits to the right of its button;
  - "Weapon Grouping" overlaps the top of its button;
  - "Accept" sits over the button's corner.
- **Likely cause:** item 29 resized and moved the buttons, but the labels still use PM's old positions.
- **Wanted:**
  - each label in the same place relative to its own button: centred above it, clear of the line and of the button;
  - the same at 16:9 and 4:3.
- **Also seen in the same shots:** ACCEPT's check mark is drawn blocky (pixel steps). It should be smooth like the other icons, at the window's resolution (item 12).
- **Done means:**
  - shots of each label shown at 1080p and 4:3;
  - a fit check: each label over its own button, inside the screen, overlapping nothing.

## 38. [x] AFTER ACTION laid out like MW1's, with a scrollable salvage list (and a salvage limit?)
- **Owen (2026-11-18):** today's four green terminals (RESULTS, PAY, SALVAGE TO STORES, YOUR <MECH>) should follow MW1's arrangement:
  - **Top left:** the picture (item 31).
  - **Top right:** the status:
    - the flat payment near the top;
    - the total mech damage;
    - under them, the full salvage list, scrollable (today it isn't, and a long list is cut).
  - **Along the bottom:** the outcome message (31's lines from the exe), with the House crest at the bottom right.
- **Salvage choice:** today everything is collected, with no clicks.
  - **If there's a limit** (Owen: "maybe there should be"): two lists like MW3's salvage screen. On the right, what the mission brought in; on the left, the keep list. Click to move items between them.
  - **The lead's suggestion for Owen:** the limit is the contract's negotiated salvage term. Its percent of the salvage's value is what you may keep, so the term negotiated in the office means something on this screen.
  - **Until Owen decides:** collect everything, scrollable.
  - **Owen (2026-11-18): no tonnage limit** on the stores.
- **Done means:** shots of a win, a retreat and a lost mission at 1080p and 4:3, plus a long salvage list scrolled.

## 39. [x] The lab's 'MECH LOAD-OUT and AVAILABLE SUPPLIES lists sit off-centre in their boxes
- **Owen (2026-11-18):**
  - in LOAD-OUT, the rows end well short of the box's right edge, leaving a wide empty strip, and the header sits high with a gap under it;
  - in AVAILABLE SUPPLIES, the list and the tabs start far right of the box's left edge.
- **Wanted:** each list centred in its box with even margins, its columns spread to the box's width. The same at 16:9 and 4:3.
- **Done means:** shots and a fit check (equal left and right margins within a few pixels).

## 40. [x] WEAPON GROUPING: something overlaps; and a group's weapons can't be reordered
- **Owen (2026-11-18):**
  - "some kind of weird overlapping thing going on in weapons grouping". A screenshot has been asked for; until then, shoot the screen at 1080p and 4:3 and find it.
  - Weapons always go into a group in the same fixed order. He wanted the ER weapons at the end of a group and couldn't put them there.
- **Wanted:** the order the player gives is kept, and the group's list shows it. Check whether PM's grouping screen allows reordering and MW1 lost it, or whether PM never had it, and say which.
- **Done means:** shots before and after, and a test that a chosen order survives a save and reload.

## 41. [x] The Mech Complex shows the company's funds
- **Owen (2026-11-18):** in the mech-buying area he can't see how much money he has, so he can't tell what he can afford.
- **Wanted:**
  - funds shown on every Mech Complex page (buying, selling, the hangar, the stores), the way the Bar's plaque shows "1,231,995 C-BILLS";
  - updated the moment something is bought or sold;
  - a price the company can't afford shown dimmed, or in the warning colour.
- **Done means:** shots at 1080p and 4:3, and a test that the shown funds change with a purchase.

## 42. [x] Unobtrusive messages: a small box at the bottom, the artwork clear, nowhere a darkened screen
- **Owen (2026-11-18, the arrival at Rasalhague on a build from before 34):**
  - find every screen that hides its picture behind a darkened filter;
  - instead, put a small message box at the bottom of the screen, so the artwork can be seen while reading.
- **This revises 34:**
  - no side panel the height of the screen;
  - a box at the bottom, sized to its text: a few lines, the full width or less, the buttons inside it or beside it;
  - the picture behind undimmed.
- **Long texts:** the box grows upward only as far as the text needs (it pages beyond about a third of the screen). There is no full-screen terminal.
- **Find them all:**
  - the arrival and the other events;
  - the Bar's drinks and news;
  - the office's notes;
  - the story pages over scenery;
  - the travel messages;
  - every place a dim (mw1_dim) or a darkened backdrop is used to hold text.
  - List them in the branch's note.
- **Keep:** pages that are their own art (event_art), and real dialogs that need an answer (the quit question). Those may keep the dim. List those too, each with its reason.
- **Done means:** a shot of each screen found, at 1080p and 4:3.

## 43. [x] A "reach" contract has no nav point: the player can't find the place
- **Owen (2026-11-18, Rasalhague):** the contract "reach the pump complex" on c2ia1 (Road to Madre) gave no nav point and no marked place. He couldn't tell where to go.
- **Cause:** our ReachAreaObjective (mw1_game.cpp, kReachStructure) checks the structure's node (`3twr1`), but nothing puts a nav point there. c2ia1's only nav is Start.
- **Wanted:** every "reach" objective has its place as a nav point, named for it (e.g. "Pump complex"), selectable and on the map like MW3's and PM's. The same goes for any objective whose place isn't otherwise shown, such as a defended structure.
- **Done means:** a test that every reach row in mw1maps.zrd gives a nav point at its node, and a shot of the nav display in that mission.

## 44. [x] An ice world's contract plays on a desert map
- **Owen (2026-11-18):** Rasalhague (the north of Kurita's blue region) sent him to Road to Madre, a desert, though he took it for an arctic world.
- **Cause:** no Instant Action map is arctic (research/maps.md, "An arctic map"). A contract in the ice climate falls back to its kind anywhere, which here is the desert.
- **To check:** that Rasalhague's battles really are ice in MW1's data.
- **The choices, for Owen:**
  - (a) Build arctic IA maps from MW3's c4b snow missions (m4/m6). They load and play as IA today; they need an ia block with m4's snow, fog and light, and the campaign script stripped.
  - (b) Until then, put ice contracts on the coldest-looking map instead of the desert.
  - (c) Show the real climate in the offer, so it's no surprise.
- **The lead's pick:** (a), with (c) at once.
- **Owen (2026-11-18): (a), real arctic maps from MW3's snow missions.** Show the climate in the offer (c) meanwhile.
- **Owen, refined (2026-11-18):** "a proper arctic instant action map for mw3 and/or pm rebuild ... the same as road to madre or any other once it's authored and shipped with the main games".
  - **The plan:** an authored IA map. It's our own layout data in the repo (start, enemy records, nav points, the ia block's snow, fog and light, objectives, targets), laid on the snow terrain of the player's converted MW3 c4b. It isn't reconstructed by script from the campaign missions, and none of MW3's files are shipped.
  - **For MW1 first.** Listing it in MW3's and PM's own Instant Action is the mw3 lead's call; MW3's and PM's files are never modified (VISION).

## 45. [x] Every kind of contract is winnable: reach, attack, defend, withdraw, oppose withdrawal
- **Owen (2026-11-18):** are the reach missions winnable now? Check every kind other than destroy-all.
- **Today:**
  - 43's survey proves each structure row's nav point sits on its node;
  - the loop tests win one reach row (c2 ia2, the portal gate);
  - nothing plays every row to a win.
- **Wanted:** a headless test that plays every structure row in mw1maps.zrd to a win, the way a player would:
  - **reach:** drive there along the ground;
  - **attack:** destroy the target by damage;
  - **defend:** kill the enemies with the structure standing.
  - It also checks the losing cases (a destroyed defended structure, the enemies killed but the target untouched).
- **The risk:** kReachRadius is 10 m from the node. A node inside a building or on a roof may be out of a mech's reach. Measure the closest approach per row.
- **Done means:** a table with one line per row (won, how, time, closest approach). Every row is won, or dropped with its reason.

## 46. [x] Map variety: every IA map in play, weighted by the planet's climate
- **Owen (2026-11-18):** a player will fly 30–50 missions, and the terrain mustn't get boring. Use every IA terrain we have, but favour the planet's environment: a cold world mostly arctic and never desert, the moon mostly Pirate's Moon. Grasslands is jungle enough.
- **Today:** mw1maps.zrd has PM c1–c4 and MW3 c2 only, 15 maps. MW3's Big Lake (c1), Rolling Hills (c3) and Gorge (c4), with three IA maps each, are missing.
- **The pools (agreed with Owen):**
  - **Desert world:**
    - about 75% from PM c1–c4, MW3 Big Lake and MW3 Gorge;
    - the rest from any other map but arctic;
    - never arctic.
  - **Tropical world:**
    - about 75% from MW3 Grasslands, MW3 Rolling Hills and Big Lake;
    - the rest from any other map but arctic;
    - never arctic.
  - **Ice world:**
    - about 75% arctic (44);
    - the rest from Grasslands, Rolling Hills and Gorge;
    - never a desert map.
    - Until 44, all from those three.
  - **Ander's Moon:** PM's worlds.
  - **Everywhere:** never the same map twice in a row (kept in the save).
  - **Structure contracts (reach, attack, defend):** they use the pool's rows of their kind, then the "sometimes" maps, then any map.
- **Also:**
  - survey the new MW3 maps' rows (45's win test and 43's nav check);
  - the offer's climate word (44 c) shows the map's own climate.
- **Done means:** a test that draws 10,000 offers per climate and reports the shares, with never a forbidden pairing and never a repeat; plus the new rows passing 45.

## 47. [x] The story made overt and hard: a journal that points to the spot, and a battle at every step
- **Owen (2026-11-18):** give the player something to do and something to look for. A journal that says how to do the next thing in the story and points right to the spot. Every step in the chain very hard, so even knowing what to do, the player has to build up a powerful army first. The final battle very hard, with a set number of mechs.
- **The journal (a STORY button on the hub, a page like the news):**
  - **Chapters:** every story step seen so far, in order, with MW1's own text read from the exe as now.
  - **The current lead:** our own short line saying where and what, e.g. "Go to the House office on Galedon V" or "Ask about the Stone Arrow in the Bar on Mosiro". It names the planet and the place (Bar, Mech Complex, office, arrival).
  - **The strength:** for a step with a battle, the expected force, e.g. "Expect 4 heavy 'Mechs, about 260 tons. Bring a full lance."
- **The star map:** the lead's planet is marked (a story marker), and travel can pick it straight from the journal (GO THERE).
- **Battles at the steps (ours; MW1's steps are text choices):** each FIGHT and each confrontation in the chain becomes a battle with its own fixed force. A loss means retreat and try again on the next arrival, never a death (33). The trail waits there until it's won.
  - **The ladder** (provisional numbers; one table so they're easy to tune):

    | Step | Battle | Expected force |
    |---|---|---|
    | 1 | Grig's men on Land's End (after the Bar's meeting) | 2 mediums |
    | 2 | Brown on Dustball (17, FIGHT) | 3 mediums and heavies, about 180 t |
    | 3 | The ambush on ref2 (27, FIGHT) | 4 heavies, about 260 t |
    | 4 | Kearney's address (2F..35, FIGHT) | 4 heavies and assaults, about 300 t |
    | 5 | The Matabushi raid on Albiero (40) | 6 'Mechs, about 400 t, veterans |
    | 6 | The Dark Wing (72, ATTACK) | 8 'Mechs: a 70 t and four 85 t (as MW1's text tells it) plus 3 escorts, about 650 t, elite pilots |

  - The enemy pilots' skill rises with the step.
  - The pay for a story battle is none, but the salvage is kept (the reason to fight it).
- **The old deaths (33):** each of MW1's seven death choices becomes a setback: a lost lancemate, a fine, or the step's battle at its hardest. The trail goes on.
- **After the final battle (Owen 2026-11-18):** the credits play (MW1's own credits, read from the exe as the other texts, over our art), then play goes on. Extra content after the win is to be decided later. Nothing ever stops the player from playing on.
- **No deadline (Owen 2026-11-18):** it takes as long as it takes, and the player can play on indefinitely. MW1's 20 Mar 3029 limit is dropped: 0x73 (too late) never fires, and ATTACK stays offered until won. The news still runs by date.
- **Done means:**
  - a headless walk of the whole chain through the journal's leads (each lead's planet and place correct, each battle launched with its force);
  - shots of the journal, the map marker and a step battle's offer;
  - the ladder in one table.

## 48. [x] Lift the 10-'Mech limit in a battle (MW1's line-up)
- **Owen (2026-11-18):** there's no reason to keep a battle capped at 10 'Mechs. We may not want many more in play, but the cap shouldn't be the engine's.
- **Today:**
  - PM's ActionGame line-up holds 10 (lineup[10], add() refuses past 9; pilot_games.cpp:1012), the player included.
  - The IA maps have 8 enemy records each.
  - So a full lance (4) meets at most 6 to 7 enemies, and the Dark Wing loses an escort (dev 1's (b)).
- **Wanted (engine, ours, opt-in for MW1; MW3 and PM untouched):**
  - the line-up's size raised to 16 for MW1, every offset use of PM's struct checked first;
  - more enemy places than a map's 8 records: a spawn spread beside the existing records (clear ground, not inside each other), the same way 44's authored map lays its own;
  - the Dark Wing back to 8 against a full lance.
- **Done means:**
  - a headless battle with 4 friends against 12;
  - frame time measured at 1080p with 16 'Mechs;
  - MW3's and PM's own IA unchanged (their suites).

## 49. [x] A flat white band at the horizon on converted MW3 worlds
- **Seen (2026-11-18, dev 2's 46 shots: Gorge, Big Lake, also Grasslands):** a band of flat white about a tenth of the screen tall, between the sky and the ground, all round the horizon. MW3 itself shows haze or distant terrain there.
- **Likely cause:** something the conversion or our sky lacks, such as the sky dome's lower part, the horizon's fog colour, or a far-plane clear colour. It must be found before the arctic map (44), which is a converted MW3 world too.
- **Wanted:** the horizon as MW3 draws it. Compare a shot of the same spot in MW3's own program (the mw3 build), then fix it in the converter or in MW1's world setup. MW3's and PM's files are never modified.
- **Done means:** before and after shots of Grasslands, Big Lake, Rolling Hills and Gorge, and the cause named.
- **Found (dev 2, 2026-11-18):** the band is in shot files only.
  - MW3's horizon ring `g15055` is untextured and fog-coloured. Through PM's flat batch (pack_material) it gets alpha 0, and the --shot BMP keeps that alpha, so viewers lay it over white.
  - On screen it's MW3's own haze ring.
  - The fix is one line in the shared shot path, with a test; the patch is in outbox/2026-11-18/mw1-horizon. It's with the mw3 lead for main and pm, and mw1 takes it by merging pm.
  - **Owen (2026-11-18): "looks correct".** The shot fix is enough; it's left to the mw3 lead to land.

## 50. [x] The office's liaison portrait: as wide as the buttons, its bottom on the terms box's bottom
- **Owen (2026-11-18):** the portrait at the bottom left of the office (negotiation) screen leaves room unfilled.
- **Wanted:**
  - the portrait as wide as the buttons above it (NEXT OFFER through LEAVE), left and right edges lined up with theirs;
  - its bottom edge on the bottom of the TERMS box;
  - its top a button gap under LEAVE.
  - The picture keeps its proportions: cropped from the top or the sides, never stretched. Faces stay in frame.
- **The same at 16:9 and 4:3.**
- **Done means:**
  - shots at 1080p and 4:3, with every liaison (each House);
  - a fit check: the edges line up within 1 px.

## 51. [x] Salvage pays from what's left after your repairs; parts are always yours
- **Owen (2026-11-18), answering 28's salvage questions:**
  - **Lost or retreated:** you lose all salvage (cash and parts). That's today's rule; keep it.
  - **Parts:** every weapon, ammo, armour and equipment item salvaged on a win is yours to keep, with no limit (no tonnage limit, no pick list). So 38's "salvage limit" is not wanted.
  - **Cash, the new rule:**
    1. The pool is the destroyed enemy 'Mechs' worth (the wrecks' value, as today).
    2. Your own 'Mech's repairs come off the top of the pool.
    3. The contract's salvage term (the percent agreed in the office) is paid on what's left.
    - Owen's example: a 3,000,000 wreck, 2,000,000 of damage to you, so 1,000,000 is left. At a 10% term you get 100,000, and your 'Mech is repaired.
  - **Repairs over the pool:** if the repairs cost more than the pool, the pool covers what it can and the rest is billed as today. *(Lead's default; tell Owen.)*
  - **Lancemates' 'Mechs:** repaired from the pool the same way, after yours. *(Lead's default; tell Owen.)*
- **AFTER ACTION's PAYMENT shows the sum:** the wrecks' worth, less repairs, = left; times the term = salvage pay; then the fee and the total.
- **Story battles** pay no fee, but the salvage rule is the same (Dark Wing included).
- **Done means:** a test with Owen's numbers (3,000,000 / 2,000,000 / 10% → 100,000, repaired, nothing billed); a test with repairs over the pool; and shots of the PAYMENT block.

## 52. [x] Fade to black and back between screens
- **Owen (2026-11-18):** "when we switch screens, it jumps from one thing to another. can we have it fade to black and fade back in to the new screen ... maybe half a second fade in/out."
- **Wanted:** every change between MW1's own screens fades out to black and then in to the new screen. That covers the hub, Bar, Mech Complex, office, lab, starport, travel, the STORY page, AFTER ACTION, the loading screen and the main menu.
  - About half a second in all: 0.25 s out and 0.25 s in. The timing is one constant, so it can be tuned.
  - Input is ignored during the fade, so a double click can't skip a screen.
  - Event messages in the bottom box (34/42) don't fade the screen; only a change of screen does.
  - No main-menu flash (35) during the fade.
- **With 12:** the fade must look right on the high-resolution 2D path. Draw it as a tint quad over the frame (research/hires-2d.md: the tint path), not by reading pixels. This folds in 12's parked fade.
- **Done means:** a frame capture of hub → Bar → hub and hub → lab at 1080p, showing the ramp, no flash, and the half-second timing.

## 53. [x] A cap on 'Mechs owned
- **Owen (2026-11-18):** "lets see if we can find an actual limit. if not, lets set the limit to 100 i suppose? or 255? i have to assume people are really going to try to break it, so we shouldnt leave it open ended."
- **Today:** the hangar (Company::hangar) has no limit.
- **First:** find MW1's own limit in the original exe: the save's company 'Mech table, or a check in the Mech Complex's BUY. Report it with the address.
- **Then:**
  - If MW1 has a limit, use it (raise it if it's tiny; ask Owen).
  - If not, the cap is 100 'Mechs owned, the piloted one included (Owen's first figure).
  - BUY is refused at the cap with a short message. Salvage never brings in 'Mechs, only money and parts (Owen), so BUY is the only way in.
  - The hangar's list scrolls to the cap.
  - Saves past the cap still load.
- **Also:** check the stores (no tonnage limit, Owen) for a count overflow. Clamp each item's quantity at a sane maximum (e.g. 9,999) and fit the lab's Qty column to it.
- **Done means:** the exe's answer with its address; a test buying to the cap; a shot of the full hangar list.
- **Found:** MW1's limit is 12 (081B:4674, DS:0530). **Owen (2026-11-18): make it 30.**
- **Owen (2026-11-18), on 51:** repairs stay cheap (kRepairScale unchanged).

## 54. [x] The travel animation back, as a video: the DropShip launches on departure
- **Owen (2026-11-18):** "in the original, when you take off to a new location, it shows an animation of the spaceship taking off ... we could use ltx to generate a 5 second video of the spaceship launching ... use the image as the first frame."
- **MW1:** MwPlayHiredTransportAnimation (MW_MAIN 081B:6095) on a paid trip. There's also an owned-DropShip version (60AD).
- **The clips:** LTX-2.5 on the Mac, from data-src/art/travel_dropship_launch.jpg. Takes 11 and 22 are in outbox/2026-11-18/mw1-launch-video/. **Owen (2026-11-18): take 11.**
- **Built (dev 1's plan):**
  - mw1_movie, MPEG-1 + MP2 via pl_mpeg (no new dependency), full window on the high-res 2D path;
  - plays at the paid departure; click or key skips; fades with 52;
  - skipped if the file is missing, or headless;
  - 1280x704 at 5 Mbit/s, about 3.2 MB.
- **Later, if Owen wants:** a clip for the owned DropShip and the Dark Wing victory (the same player); the Mech Complex idle loop and the intro's background (needs loop + rectangle).
- **Done means:** GO → movie → hub with and without the file; skip by key and click; a 1080p capture of the fade in and out.

## 55. [x] The launch video: a pause before it, and its sound replaying at the end
- **Owen (2026-11-18, in play):**
  - "put a slight pause before the video plays showing the dropship taking off. it felt very sudden."
  - "it played the dropship sound for a split second after loading the new screen when the dropship animation was complete. like it was starting it over."
- **Wanted:**
  - the first frame held still and silent (about 1 s, one constant) before the motion and sound start;
  - the sound stopped for good at the clip's end and on a skip, never heard on the next screen.
- **Done means:** the cause of the replay named; a test that no sound is playing after the movie ends or is skipped; a windowed check with sound on.

## 56. [x] The office: NEGOTIATE and ACCEPT under the terms they act on *(done: mw1-office-layout)*
- **Owen (2026-11-18):** "lets move the negotiate and accept buttons down under the terms box. it isnt intuitive that youd have them where they are and know what to click." NEXT OFFER and LEAVE "can stay where they are ... or we can move them to a more intuitive spot."
- **The layout (the lead's proposal, mock-up `outbox/2026-11-18/office-layout-mockup.png`):** the screen reads top to bottom, contract → terms → act.
  - **NEGOTIATE and ACCEPT** sit in one row right under TERMS, right-aligned to its edge.
    - ACCEPT is the primary action, framed in amber, at the far right where the eye ends.
    - NEGOTIATE sits beside it. It stays dimmed until a term has been changed, as today.
  - **NEXT OFFER and LEAVE** stay at the top of the left column. They're about which contract and whether to stay, not about the terms.
  - **The liaison's portrait** grows to fill the column below them. Its bottom stays on TERMS' bottom line, as in 50.
  - **The hint lines** in CONTRACT ON OFFER ("Change a term with its arrows, then NEGOTIATE ...") go. The layout makes them unnecessary, and the status bar gives each button's hint on hover. CONTRACT ON OFFER shrinks by those lines, and TERMS moves up to make room for the button row.
- **The same at 4:3.**
- **Done means:**
  - shots at 1080p and 4:3, with NEGOTIATE dim and lit;
  - the fit checks (mw1screenfit, 50's line-up test, updated);
  - the office golden re-recorded (off and on identical).
- **Done (built to the mock-up):**
  - **Layout** (`data/mw1office.zrd`, both sizes, the same canvas rows):
    - NEXT OFFER (REQUEST MISSION) at 84 and LEAVE right under it at 122.
    - The liaison's frame from a button gap under LEAVE (160) down to the button row's foot (446): 168x286, the portrait 160x278.
    - CONTRACT ON OFFER 76..228 (152 high: its seven lines, the hint lines gone).
    - TERMS a button gap under it, 236..408 (its rows and arrows moved up with it).
    - NEGOTIATE and ACCEPT a button gap under TERMS, 416..446: ACCEPT's right edge on the terms box's, NEGOTIATE a button gap to its left.
  - **Portrait foot:** the frame's foot is on the button row's foot, as in the mock-up. TERMS' foot is no longer the column's bottom, so the spec's "on TERMS' bottom line" became "on the right column's bottom line".
  - **ACCEPT is the primary action:** a new chrome piece, `mw1_button_primary`, framed in amber when still. Lit, pressed and off it looks as any button, so hovering it still lights it.
  - **The House's answer** used the hint lines' place (NOTES), which is gone. It goes on the status line, the liaison's line:
    - a counter-offer reads "The House counters: 320,000, salvage 5%, advance 6%.";
    - the last round reads "Losing patience: ..." instead (the hint before the last round kept);
    - a broken-off round says nothing there: the terms box already says why;
    - a button's hover words replace the answer while over it. Pressing NEGOTIATE clears the hover, so the answer shows at once.
  - **Every button's status words on hover:** NEGOTIATE, ACCEPT, NEXT OFFER, REQUEST MISSION, LEAVE and the pay and advance arrows (the salvage arrows' kept). A greyed NEGOTIATE says "Change a term with its arrows, then NEGOTIATE." (78's greyed hook). All fit the 4:3 status line.
  - **Portraits:** `tools/mw1art.py` cuts `<liaison>_office` to 160:278: the whole height, the middle of the width. Rerun on the keep folder, only the five office pictures and art.csv changed.
  - **Tests:**
    - 50's line-up test now checks the whole column (LEAVE under REQUEST, contract → terms → button row a gap apart, ACCEPT on the terms' right edge, NEGOTIATE beside it, the frame's foot on the row's, above the status bar).
    - New `mw1_office_the_status_line_says_it`: no hint lines; the answer and every button's words on the status line, each within 62 letters, the longest counter-offer too.
    - The chrome count and the primary button's look; the art sizes (160x278).
    - The office golden re-recorded: the canvas off and on identical (5b448363...).
    - screenfit and labfit green.
  - **Dev switch:** `MW1_OFFICE_ASK=1` with MW1_SCREEN=office_offer asks a step more pay (NEGOTIATE lit); `=2` asks far over the offer and negotiates (the counter-offer on the status line).
  - **Shots:** `outbox/2026-11-19/mw1-office-layout-56/` (1080p and 4:3: NEGOTIATE dim, lit, and the House's counter).
  - **Based on 78's branch**, for its greyed-button hook.

## 57. [x] The repair line belongs on the mission summary, not a hub message
- **Owen (2026-11-18, in play):** a MESSAGES box on the hub after a mission said "Repairs: the Owens 4,963, from the wrecks". He asked "is this right? also, this should probably appear on the mission summary screen".
- **Right:** yes. The Owens's repairs (4,963) were paid from the wrecks' worth (51), so nothing was billed. But the wording is unclear, and a box after the fact is the wrong place for it.
- **Wanted:**
  - The repairs show on the results screen (AFTER ACTION), in PAYMENT or right under it, one line per 'Mech, in plain words. For example: "Owens repaired: 4,963 C-bills, paid from salvage", or "... 400,000 billed to you" when the pool ran short.
  - It must fit at 1080p and 4:3. If several 'Mechs don't fit, show the total with the per-'Mech lines in DETAILS.
  - The hub no longer raises a MESSAGES box for repairs. Other after-mission notes that only the hub can show stay.
  - Check every other line in that hub box for the same problem: anything that belongs to the mission's result goes on the results screen.
- **Done means:** shots of the results screen (one 'Mech, a full lance, repairs over the pool) at 1080p and 4:3; no repair box on the hub; the tests updated.

## 58. [x] The Mech Complex's gunmetal render doesn't follow BUY
- **Owen (2026-11-18, in play):** after buying a Firefly, the selection moved to it in YOUR MECHS, but the big panel still showed the Strider, the FOR SALE row chosen before. Switching rows corrected it.
- **Wanted:** the panel follows every change of the chosen 'Mech: BUY, SELL, the selection moving between lists, and the first entry. Add a test (dev 1).

## 59. [x] Remove REPAIR and RELOAD from the Mech Complex
- **Owen (2026-11-18):** "lets get rid of the repair/reload buttons in the mech bay, too, since we dont need them".
- **Why they're dead:** every mission starts with every 'Mech repaired and rearmed (MW3's rule), and the bill is settled from the mission (51/57), so the buttons do nothing useful.
- **Wanted:**
  - The Mech Complex column is BUY, SELL, MECH LAB, BACK, evenly spaced. The live gunmetal panel keeps its place under BACK.
  - The hint line no longer mentions them.
  - Remove the dead code paths and their tests, or update them.
  - It must fit at 1080p and 4:3.
- **Done means:** shots at 1080p and 4:3, and mw1screenfit, labfit, hiresoff and smooth passing (update the goldens where the market changes).

## 60. [x] Confirm before buying or selling a 'Mech
- **Owen (2026-11-18):** "lets put up a verify prompt when they buy a mech. you want to buy [x]? ... y/n".
- **Wanted:**
  - BUY raises a box in the same style as the game's other boxes, e.g. "Buy the Firefly for 2,605,000 C-bills?", with YES and NO buttons. The Y and N keys work too, Enter means YES and Esc means NO.
  - The purchase happens only on YES; NO leaves everything unchanged.
  - The box covers the live 'Mech, as the quit box does.
  - When the buy can't happen (not enough C-bills, the hangar full at 30), say why instead of asking.
- **SELL too (Owen: "yes, same thing for sell"):** "Sell the Owens for 2,772,000 C-bills?", same keys. Selling the piloted 'Mech stays allowed (today's rule), and the question names the 'Mech you'll pilot next: "... You'll pilot the Firefly." When the sale can't happen (the last 'Mech, a full dealer), say why.
- **Done means:** shots of both boxes at 1080p and 4:3, and tests that NO leaves the money and roster alone.

## 61. [x] AMS doesn't stop missiles (works; no fix needed)
- **Owen (2026-11-18, in play):** "i dont think the ams is working. im getting hit with missiles and the ams dont appear to be stopping anything".
- **Where to start:** Mech::update_ams (src/mech3/mech.cpp, PM 0x00419200) fires only when the mech has an AMS fitted (equipment[0] > 0), m.ams exists and ams->on is true, and only when g_calls.equipment_update isn't set (mech_update.cpp:176). Check each in an MW1 mission:
  - Does the player's 'Mech actually carry an AMS and ammo (MW1's loadouts, the lab)?
  - Is the AMS on at the start, as in PM? Is its toggle key bound, and does the HUD show it?
  - Does it find and fire on incoming missiles?
  - Compare with PM's own game on the same 'Mech.
- **Done means:** the cause found and fixed, with a test or a scripted mission that shows missiles shot down. If it's PM's engine and PM is affected too, it goes to the mw3 lead.
- **Closed (dev 2, hidden scripted mission):** the AMS is on from the first frame with 100 rounds. It found the hostile missiles and shot down 14 of 35 (rounds 100 to 86). But the stock Owens carries no AMS (equipment[0] = 0); it fires only once one is fitted in the lab. That likely explains the first report.
- **Update (Owen, later the same session):** "i was in an owens. and it did work just now. might be false alarm". Lowered to a quick check: in one hidden scripted mission, confirm the AMS is on at the start and shoots down missiles, then close this or report.

## 62. [x] A news headline at the hub doesn't say where to read it
- **Owen (2026-11-18, in play):** "it said \"personal message from jordan rowe\" but im not seeing where to click to view it".
- **Cause:** hub_events adds "NEWS: <headline>" to the hub's messages, but the full text is only on the Bar's NEWS (NewsNet). Nothing on the hub points there.
- **Wanted:**
  - The hub line names where to read it, e.g. "NEWS: Personal message from Jordan Rowe. Read it at the BAR, under NEWS."
  - Better still, a READ button beside OK opens that story straight from the hub, using the NewsNet's own pages. If it does, reading it there counts as read.
  - Pick the cleanest; the wording comes from our code, never from MW1's text.
- **Done means:** a shot of the hub with a news line; reading it from where the hub points; tests.

## 63. [x] A log of every message
- **Owen (2026-11-18, in play):** "im getting messages like \"wanted - 50,000...\". not sure waht that means exactly but it would be useful to see a log of all the messages and everything somewhere".
- **The WANTED line** is a NewsNet headline (story news 0x16 in his play.log). The hub showed only the headline; the story is at the BAR, under NEWS. 62 fixes the pointer.
- **Wanted:** a message log the player can open any time, e.g. a LOG page off COMPANY or the hub. Newest first, each with its game date:
  - the hub's notices;
  - the news headlines, each opening its full story (NewsNet's pages);
  - story events seen;
  - mission results: outcome, pay, repairs (57), salvage;
  - purchases and sales; hires, departures and deaths; travel.
  - It is saved with the company and capped (e.g. the last 200 entries).
  - It fits at 1080p and 4:3, and scrolls the way the market lists do.
  - MW1's own text is shown from the player's exe as now and never stored in the save beyond event IDs. Store IDs and numbers, and build the words when shown.
- **Done means:** shots of the log at 1080p and 4:3, a test that entries survive save and load, and the cap.

## 64. [x] Don't show market tiers; show what MW1's own travel panel showed *(done: mw1-no-tiers)*
- **Owen (2026-11-18):** "i dont think we should show the tiers. let people figure it out" ... "unless the original said it. did it?"
- **What MW1 showed:** it didn't. Its destination panel (SDL port MW_MAIN_081B_TRAVEL.c, ~081B:5600) draws:
  - the world's name and its House's crest;
  - its landscape label (travelLandscapeLabels[(code-1) % N], the same byte whose (code-1)/3 is the tier);
  - its population in millions;
  - its description.
  There is no tier, no buy or sell percentage and no dealer limit. The market shows only actual prices.
- **Take out:**
  - the travel panel's "tier N" and the "You sell mechs at", "You buy mechs at" and "Dealer buys" lines (mw1_travel_screen.cpp ~107-116);
  - the map's TIER filters, including "TIER 3: SELL HIGH" and "TIER 0: BUY CHEAP" (mw1_travel_map.cpp ~131);
  - the Mech Complex banner's "TIER N WORLD · SELLING AT N%" (mw1_market_screen.cpp ~105). Put something neutral in its place, e.g. the world's landscape and population, or nothing.
- **Keep:** prices in the Mech Complex (they're real), and the dealer's refusal when it's full (said when it happens, as MW1 did).
- **Add, as MW1 had:** the landscape label, the population and the description on the travel panel. Read the labels and description from the player's MW_MAIN.EXE at run time, like our other MW1 text; never store them. Without the exe, leave them out.
- **Done means:** the tests updated (test_mw1_travel, test_mw1_travel_map); shots of the travel panel and the Mech Complex banner at 1080p and 4:3.
- **Done (dev 1):**
  - **Gone:** the readout's tier, its buy and sell percentages and the dealer's limit; the map's four TIER filters (FILTER now cycles all, each House, offices, affordable, will deal); the Mech Complex banner's tier and percentage; and the "DEALER BUYS N MORE" on YOUR MECHS (MW1 had no running count either; the refusal when the dealer is full stays, said when it happens). Tiers still set the prices, unseen.
  - **Added:** travel_rules::world_text reads a world's landscape label (DS:5CA6/5CAD/5CB6, title case), population (bytes 8-9, grouped, with DS:5B83's suffix) and description from the player's MW_MAIN.EXE at run time, checking the record is that world's. Nothing is stored; without the exe all three are left out.
  - **The readout:** "House Kurita · Desert", "Population 1,009,000,000", MW1's description (in its capitals, wrapped to 26 columns), then the office, battles and Clan lines. To keep every world's readout within the 14 lines above GO, "You can't afford it." lost its blank line above.
  - **The banner:** "TROPICAL · POP. 3,251,000,000", or the date without the exe.
  - **Tests:** test_mw1_travel_map (the filter cycle, no TIER); test_mw1_travel's readout (nothing of tiers with or without the exe; every world's three facts with it; every line ≤ 26 columns; every world's hover readout ≤ 14 lines, 14 at most now). MW1's words are checked, never printed.

## 65. [x] The launch video: black, then fading in as it flies, with no frozen first frame
- **Owen (2026-11-18):** "instead of pausing on the first frame for a second, lets just have it fade in from black for that second and then take off. no freeze frame. just be black for about a second, and then fade in from black as it flies away".
- **Done (lead):** after the screen change it stays black and silent for kHoldSeconds (1.0). Then the motion and the sound start together, and the picture fades in from black over kFadeInSeconds (1.0) (mw1_movie.h fade_in_black, MovieState::black()). test_mw1_loop checks no picture during the hold, a part-fade during the motion, and none after it.

## 66. [x] The FOR SALE list's last row overlaps the box's bottom edge
- **Owen (2026-11-18, in play):** "the mechs in the purchase list overlap with the buttom of the box". Eight rows: the eighth (Awesome) and its outline sit on the box's bottom border.
- **Wanted:**
  - The visible rows end inside the box with a margin equal to the top one, so fewer rows show if needed; the arrows scroll.
  - The same check for YOUR MECHS.
  - Find out why mw1screenfit didn't catch it: outlines and text touching a box's border should fail. Add that check.
- **Done means:** shots at 1080p, 4K and 4:3 with a full list; the fit check fails on the old layout.

## 67. [x] Several news lines read the same: "NEWS: BRIEF HEADLINES" three times
- **Owen (2026-11-18, in play):** "i got a message that said: NEWS: BRIEF HEADLINES (x3) ... is that intentional". No.
- **Cause:** story_rules::headline() takes the line before a story's first blank line. Several of MW1's NewsNet stories are bulletins that share one title (news 0x66, 0x67 and 0x68 arrived together in his play.log), so the hub shows that title once per story.
- **Wanted:**
  - A headline that tells stories apart. When a story's title line is one that other stories share, use its first item's line instead, or the title plus that line. This is all built at run time from the exe's text, as now.
  - Never show the same line twice in one hub box: same-titled stories collapse to "NEWS: <title> (3)", each readable through READ (62).
  - The log (63) uses the same headline.
- **Done means:** a test over the whole script: no two live news stories give the same hub line, or they're collapsed with a count.

## 68. [x] The Bar's hiring page: HIRE, PREV, NEXT, DONE (no DECLINE)
- **Owen (2026-11-18):** "hire, decline, prev, next, done probably arent all necessary on the bar hiring page. can we reduce it to next/prev/hire/done".
- **Done (lead):**
  - DECLINE is gone: the button, its id, the game's case and the pilots rule decline(); recruits now just stay on offer.
  - The column reads HIRE, PREV, NEXT, then DONE set apart at its old place (DONE is shared with YOUR LANCE's column).
  - The hint is "HIRE pays their wage; PREV and NEXT show the rest."
  - mw1tests 181/0; mw1labfit, screenfit, hiresoff and smooth 4/4.

## 69. [x] The lab's top row: the lance's slots, not FUNDS; the lab assigns 'Mechs, not the Company page *(done: mw1-lab-slots)*
- **Owen (2026-11-18, in play):** "i just hired two lancemates. but they arent showing in the mech lab. i dont think we need the \"funds\"/c-bills at the top, where the lancemates are normally listed. lets do it the way mw3 does - put blank slots at the top if i have pilots with no mechs, and no slots at all if there arent any pilots".
- **Wanted:** the top row is PM's or MW3's lance strip, done as MW3 does it:
  - one slot per pilot (you, then each lancemate), showing the mech outline of their assigned 'Mech;
  - a blank slot for a pilot with no 'Mech;
  - no slots for pilots you don't have.
  - Clicking a slot picks that pilot's 'Mech, as in MW3; check how a blank slot behaves there.
  - FUNDS goes. If money must show somewhere in the lab, ask me first.
- **More from Owen (2026-11-18):** "we can get rid of the \"pilot (you)\" area under the mech list now too since we're doing it differently. and we shouldnt haev to assign mechs on the company page. the company page should be for firing or looking at the stats of my company members".
  - **The lab is where 'Mechs are assigned,** MW3's way: pick a pilot's slot at the top, then pick a 'Mech from YOUR MECHS. A 'Mech already held by another pilot moves over, or swaps, whichever MW3 does.
  - **The PILOT selector** (◀ YOU ▶) under YOUR MECHS goes.
  - **The Company page (COMPANY, the Bar's lance page)** keeps FIRE, PREV, NEXT and LEAVE for firing and the stats. ASSIGN goes, with its hint. Remove ASSIGN only once the lab assigns, so there is always a way.
- **Done means:** shots with you alone, with two lancemates and one 'Mech (two blank slots), and with a full lance, at 1080p and 4:3; assigning in the lab, and a test that the Company page has no ASSIGN.
- **How it works now:**
  - MECH LAB opens PM's lab with one slot per pilot: you, then each lancemate (Instant Action's line-up). A lancemate without a 'Mech gets a dashed empty frame (mw1_slot_none).
  - YOUR MECHS lists the spares and the shown slot's own 'Mech, MW3's way. A 'Mech in another slot isn't listed; free it from its own slot first. Picking a spare swaps: the old 'Mech becomes a spare.
  - Each 'Mech keeps its own loadout in and out of a slot (MW1's way; the lead, 2026-11-19). Two of the same chassis are matched in hangar order.
  - A lancemate's slot also offers "No 'Mech".
  - MECH LAB on a spare opens on your slot, with the spare listed. It never auto-assigns.
  - ACCEPT makes the lance the company's (mw1_pilots.h's apply_lance). The offer is redrawn when the lance weight changes, as ASSIGN did.
- **Removed:** the PILOT selector (mw1_lab_pilot), FUNDS, the Company page's ASSIGN with its hint, and `assign`, `set_pilot` and `pilot_choices`. FIRE, PREV, NEXT and LEAVE close up, and the recruits page's DONE moves up with them.
- **Engine (ours, opt-in):** GameEngine::set_lab_lance. A program changes a slot's chassis itself (Engine::lab_slot_swap) and offers "No 'Mech" in any game. With it on, the "no mech" row is the selected one for an empty slot. In PM, find_mech's fallback to row 0 picked whatever row the sort put first.
- **Dev:** MW1_LAB_LANCE=<mates>:<their mechs>[:<spares>] and MW1_LAB_SLOT=<slot>[:<mech>|none] for --shot pictures.

## 70. [x] The lab's warnings are hidden behind the bottom buttons *(done: mw1-lab-slots)*
- **Owen (2026-11-18, in play):** "the warning \"need [x] crits, only have [y]\" or the tonnage warning are both behind the buttons now. cant see the warnings". The shot shows "Not enough criticals free (need 14, have 10)" drawn across the third and fourth of the bottom icon buttons.
- **Wanted:** every lab warning shows in full where it can be read: in clear space (e.g. above the button row, or in a panel's own status line), above everything, at 1080p, 4K and 4:3. labfit should check that the warning line isn't covered by a button.
- **Done means:** shots of the crits and the tonnage warnings, and a labfit case that fails on the current layout.
- **Why the first idea failed:** the lab's 3D view (210..430 x 52..288 of the layout) draws over the 2D, so a warning above the centre box sat under the 'Mech's feet and the reticle. The only clear 2D space in the centre column is the top strip, which 69's slots use.
- **Owen chose option A (2026-11-19):** the tray buttons in pairs at the ends (item 29 updated), the warning in the clear middle. It is centred, as wide as its line, and wraps past 300 (kLabWarnW) onto a second line.
- **The check:** mw1labfit shows every lab warning at its longest (tonnage, the criticals for XL and endo steel, 16 weapons, heat sinks, jump jets). It fails any that leaves the layout, covers a tray button or its label, or crosses a panel's frame. On the old layout all 9 failed "over the groups button". Dev: MW1_LAB_WARN=0..8 holds one warning up for --shot pictures.

## 71. [x] The Mech Complex buttons stacked at the top
- **Owen (2026-11-19):** "lets stack the buttons closer together so they dont look as spread out. stack them up against the top".
- **Done (lead):** BUY, SELL, MECH LAB and BACK now sit at y 84, 122, 160 and 198 in both layouts (the Bar's 38 apart; was 84/147/211/274). The live 'Mech panel stays at the column's foot. kMarketButtonTop follows.

## 72. [x] Lancemates stand still and don't attack
- **Owen (2026-11-19, in play):** "my lancemates arent attacking targets. were they bouncing against a awll or something? they were stationary".
- **Unknown yet:** the log of that mission was overwritten when the game was restarted. The lancemates were freshly hired, and possibly without 'Mechs of their own (69).
- **To find out, in a hidden scripted MW1 mission with two lancemates in 'Mechs:**
  - Are they spawned with PM's squad AI, or as plain AI 'Mechs?
  - Which order do they start with: PM's default (form on me) or none?
  - Do they follow, and do they engage on "attack my target" and on their own?
  - Is their path blocked at the drop zone (a wall, or an MW3 world's nav data)?
  - Compare an Instant Action mission in PM with lancemates on the same map.
- **More from Owen (2026-11-19):** "i targeted one lancemate, john zoe, and then i targeted my other lancemate, alex veer ... when you target a lancemate with w, it puts their name in blue at the top because thats who youre giving orders to ... when i switched from john zoe to alex veer ... the top name remained john zoe, though it did target alex veer ... when i targeted enemies, it would only target them with john zoe".
  - So the order recipient sticks to the first lancemate chosen. Targeting the second lancemate with W moves the target, but not the recipient (the blue name at the top). Orders, such as attack, then go to the first lancemate only.
  - Find out whether this is MW1's lance setup (e.g. the squad's member list or slot numbers built wrong for MW1's lancemates) or PM's own recipient logic. Compare with PM's Instant Action with two lancemates.
- **Done means:** the cause and the fix, with a test where lancemates move and fire, and where W on each lancemate in turn makes it the recipient and an attack order reaches it. If PM's engine is at fault, it goes to the mw3 lead.

## 73. [x] The log's polish: cut headlines and the money format
- **Lead's review of 62/63/67 (the LOG shot):**
  - A headline taken from inside a shared-title story stops mid-sentence: "JUSTIN ALLARD NOW JUSTIN XIANG SEEKS", "ALLARD TRIAL ENDS IN CONFUSION PRINCE". When the line doesn't end a sentence, end it with "…" so it reads as cut on purpose.
  - Money: "Wages paid: 2400 C-bills." has no thousands comma, while the other lines do. Every amount in the log and the hub box goes through the same thousands().
- **Done means:** the log test checks that each headline ends in punctuation or "…", and that every number of four digits or more has commas.

## 74. [x] The travel search's "Find:" line could run past GO *(done: mw1-find-line)*
- **Found in 64 (dev 1):** typing a world's name added a blank line and "Find: <typed>" under the readout. On the longest readouts (a world under the mouse with an office, battles, Clan mechs and a two-line description) that made 16 lines, two more than fit above GO.
- **Done:** what's typed goes on the status line ("Find: THA"), over the last GO's line, and the readout gains nothing. test_mw1_travel checks every world under the mouse, affordable or not, typed or not: at most 14 readout lines, and the status line says what's typed. test_mw1_loop's typed-name walk still finds "Find: THA" on the screen.

## 75. [x] AFTER ACTION: Owen's answers from the results review (28) *(done: mw1-retreat-counts)*
- **Owen (2026-11-19), on dev 1's four questions:**
  1. **A retreat:** "Bill repairs, keep losses". With no salvage, the repairs come out of funds. A 'Mech that went down is lost, and its pilot is KILLED or MISSING as on a loss. settle_mission must stop skipping retreats for the fates and the bill.
  2. **A story battle's salvage:** "Keep full value". No change.
  3. **A story battle's crest and message:** "No crest or House message". No House hired you.
  4. **A win with a lancemate KILLED:** "Keep victory picture", as MW1 does.
- **Done means:**
  - tests: a retreat with a downed lancemate bills repairs from funds and settles the fates; a story battle has no crest and no House message;
  - shots of both at 1080p and 4:3 (dev 1).
- **Done (dev 1):**
  - settle_mission settles every ending: the fates (after_mission), then the repairs from the pool, which is empty on a retreat, so the bill comes out of funds. settle_pending and the text results no longer special-case a retreat. CREW STATUS and the picture follow the real fates (FIXES 28's retreat wording is undone).
  - A story battle's results: no crest (house -1) and no House message. The Dark Wing keeps its own attack-failed line.
  - **Tests:**
    - test_mw1_repairs `a_retreat_bills_repairs_and_settles_the_fates`, KILLED and MISSING: no fee or salvage, the repairs billed and paid from funds, the 'Mech gone, the pilot dead or free.
    - test_mw1_loop: the quit case's field-base repair is now billed (the test is renamed `..._won_failed_and_quit_headless`). The step and Dark Wing runs check no crest, and the step runs also check no message.
  - **The "-j4 flake" at test_mw1_loop.cpp:913 wasn't a flake.** DETAILS' kills title gets " (1-6 of N)" once a downed lancemate's repair rows pass six, and the check matched it exactly. The lance-orders run fails whenever its roll downs Holmaas. It now matches the title's start.
  - **For Owen:** a story battle (no message) and play without the exe show an empty message box along the bottom. Hide the box when it's empty?

## 76. [x] AFTER ACTION: no message, no message box *(done: mw1-empty-message)*
- **Found in 75 (dev 1); the lead: a plain fix.** A story battle has no message since 75, and play without the exe never had one. Either way an empty bordered box sat along the bottom.
- **Done:** results_view hides the message panel when there's no message. Everything else keeps its place, and the crest shows as before (a contract's, still without the exe). test_mw1_results `no_message_no_box` checks it at both shapes. Shots of a story battle and a run without the exe (a folder with only BTECH.EXE given as --mw1-game) at 1080p and 4:3.

## 77. [x] The look without MW1's exe *(done: mw1-no-exe-look, no code change)*
- **Asked (the lead, after 76):** dev 1 saw the results panel's text "in another font" without the exe. Find the cause. Every screen should look the same without the exe, apart from the text read from it.
- **Cause: none, it was my contact sheet.** The panel's font is ours, Share Tech Mono (mw1fonts.zrd, mw1/data/fonts), in both runs. The sheet had scaled a 4:3 shot and a 16:9 shot to the same cell, so their text looked different. Full-size crops are identical.
- **Sweep:** 15 screens (hub, options, market, travel, company, Bar, recruit, drink, office, offer, news, event, results, loading, lab) at 1080p and 4:3, MW1_SEED=7, with the exe and with an MW1 folder holding no MW_MAIN.EXE. Each pair was pixel-diffed (`outbox/2026-11-19/mw1-no-exe-sweep-77/`, diff.txt). The options screen and the lab are identical. Every other change is text read from the exe, or a feature that needs the exe:
  - **Text read from the exe:**
    - the market banner's landscape and population;
    - the travel readout's facts;
    - the office's offer title and the loading screen's mission title ("CONTRACT" without the exe);
    - the results message;
    - the Bar's pitches;
    - the Company page's commander name ("YOU" without the exe).
  - **Features that need the exe (for the lead):**
    - the Bar's ORDER DRINK and NEWS are greyed out, since their texts are MW1's;
    - the hub has no STORY button, since the story is MW1's script.
    The look is otherwise unchanged; whether those should explain themselves (a hint naming MW1's install) is a design call.

## 78. [x] Without MW1's exe, the greyed Bar buttons and the missing STORY say why *(done: mw1-no-exe-hints)*
- **Bar:** without MW_MAIN.EXE, ORDER DRINK and NEWS stay greyed. Hovering or pressing either puts "Needs MW_MAIN.EXE in your MechWarrior 1989 folder." on the status line. The player points us at the MW1 folder (the launcher, or `--mw1-game`), and the exe is looked for there. The note is 51 characters and fits the 4:3 status line.
- **Hub:** without the exe, the hub's status line says "Story off: MW1's MW_MAIN.EXE not found." once, on the first arrival. Hovering a tile or pressing anything replaces it, and later visits show the usual line.
- **Engine of the menus:** Mw1Screen/Mw1State gain a `set_greyed` hook. It is called when a disabled button is hovered or clicked; enabled buttons keep `set_hover`.
- **Tests:** the loop test checks the hub note shows on the first hub only, and never with the exe. The Bar test checks both buttons are disabled without the exe, that greying over NEWS shows the note (at most 56 characters), and that other buttons don't.
- **Dev switch:** MW1_BAR_GREYED=1 with MW1_SCREEN=bar hovers the greyed NEWS for a shot.
- **Shots:** `outbox/2026-11-19/mw1-no-exe-hints-78/` (hub and Bar, 1080p and 4:3, with and without the exe).

## 79. [x] A campaign soak for play bugs *(done: mw1-soak)*
- **The lead (2026-11-19):** Owen keeps finding things only in play. Play a long hidden headless campaign and check the invariants at each step: the money adds up, every 'Mech and pilot is accounted for, no lancemate without a slot, the log matches what happened, nothing leaves the screen.
- **The soak** (`mw1_soak_a_campaign_through_every_service_and_restarts_headless`, test_mw1_loop.cpp):
  - A campaign with the story on, played in legs over one save. Each later leg starts at MW1's menu and LOADs the last leg's save, so the game is saved and loaded between legs.
  - Every service: the office (requests, next offers, negotiating, accepting); the market (buy and sell, its questions answered yes); the Bar (hire, fire, drinks, news); the lab's lance (lancemates put on spare mechs, as the lab's DONE does); LOG, READ, the company page, travel, the hub's MAIN MENU and back, STORY.
  - Missions are won, lost or withdrawn from. Along the way the player's mech is hit or destroyed and lancemates are killed.
- **Checked every few frames, on whatever screen:**
  - the money: each change of the funds is the log's;
  - the pilots: hired means in the lance, joining and leaving only through the log's entries, the dead stay dead;
  - the mechs: the owned count changes only by the log's buys, sells and losses;
  - the log: one result per settlement, a trip or turn-away for each new world;
  - the date never goes back, and the funds never fall below 0;
  - the save round-trips and is the live company;
  - every text fits its room: MW1_FIT's check, now `fit_problems`, shared through `game_loop_fit_problems`;
  - nothing is stuck.
- **Runs:** by default, seeds 4 and 9, 3 legs of 5 settlements, in mw1tests. `MW1_SOAK_LONG=n` plays seeds 0..n-1 at 6 legs of 8; `MW1_SOAK_SEED=k` plays one seed. Ten long seeds, 480 contracts, are clean.
- **Fixed:**
  - **The story's money wasn't in the log.** A setback's fine (Ander's Moon's 10%, up to a third) and MW1's script's funds commands moved the money unlogged, and a turn-away moved the company to another world unlogged. New log kinds: `kStoryFunds` ("Paid 30,000 C-bills." / "Received ...") and `kTurnedAway` ("Turned away from X to Y.").
  - **The commander's own mech destroyed** (they take a spare) wasn't logged as lost; a lancemate's was. It is now (`kMechLost`).
  - **The market's BUY and SELL questions were always answered NO in the soaks,** so the old 20-contract soak never bought or sold. The new soak answers yes.
  - **MW1's words printed in tests:** AFTER ACTION's message, the Bar's pitches, the loading screen's lines and the hub's refusal lines. They print sizes now.
- **For a decision (sent to the lead):**
  - **Every House can come to refuse the company for good.** MW1's 8 grievances against a House is a refusal, never forgiven, and each contract adds +2 against the House fought (+1 to the employer on a retreat). In 6 of 10 long seeds every House with an office refused after 30 to 45 contracts. The company is then stranded with no way out that money can buy. The soak records it and clears the grievances to go on.
  - **A lancemate hired, or a mech bought, isn't put on anything:** only the lab's lance assigns them, and a lancemate without a mech doesn't fly. The soak assigns through the lab's apply.
  - **With the screen fade off, a press on a screen being replaced still acts.** The soak pressed travel's BACK in the frame after GO, and the queued swap replaced the arrival's event panel. The fade (on by default) ignores presses during a change, so play can't do this; a guard could ignore presses while a change is pending.

## 80. [x] A press while a screen change is queued is ignored, fade or not *(done: mw1-press-guard)*
79's soak found it: with the fade off, travel's BACK pressed in the frame after GO acted. GO's swap to the hub and the
arrival's event panel were already queued in the app, and BACK's hub replaced the panel: the panel was gone while the
game still thought it up. The fade (on by default) ignored such presses, so play couldn't do it, but nothing else did.
- Every press (`as_player`) is now ignored while the app has a screen change queued, as during a fade.
- `mw1_a_press_while_a_screen_change_is_queued_is_ignored_headless`: GO, then BACK in the same frame, to a world whose
  arrival raises a story panel. The trip runs and the panel stays up over the hub, each state once on the stack. It
  fails without the guard (the panel lost).
- The soak now presses BACK after every GO, trip or not, so it keeps exercising the guard.

## 81. [x] The log's forgiven repairs, exact *(done: mw1-repairs-forgiven)*
Found by the long soak (seed 17): a repair bill larger than the funds is paid down to 0 and the rest forgiven, but the
log kept the forgiven part in thousands, rounded down. 72,377 billed with 61,000 in the bank read "(11,000 forgiven)",
so the log's money didn't add up to the funds.
- The repairs entry keeps the forgiven amount exactly in `b`. `a` still keeps it in thousands, and an older save's
  entry (no `b`) still reads that way.
- The soak's ledger counts a repair as billed less forgiven. The log test checks both kinds of entry.

## 82. [x] The HUD's text at the window's resolution *(done: mw1-fix82-hud-text)*
- **The lead (2026-11-19), after the 4K sweep:** the HUD's words already went through TextRaster (weapon names, timer, the OP line, Heat, Coolant, KPH), so what was left was bugs and polish: (a) the numbers and letters inside PM's pictures were blocks, (b) a stray mark after "Heat" and "Coolant", (c) the speed readout "-0.0" cut on its right, (d) check gradient text.
- **(a) The labels in PM's line art** (the speed and fuel scales' 100/50/0, the coolant's F and E, the compass tape's headings and N E S W) are PM's pixel font. Above canvas size they are now drawn as text in PM's gauge face (Arial), in PM's ink colour and drop shadow, where PM put them; PM's glyph pixels are left out of the traced line art.
  - mw1_hud_art finds them in PM's pictures: the scales' by their colour (PM's brightest), grouped and read top to bottom; the tape's every 36 pixels between its major ticks, 310 first. If PM's picture isn't laid out as expected, its pixels stay.
  - The painter is the mw1 program's (mw1_hud_labels.cpp, SDL_ttf: mw1core has none): rendered big, its ink averaged down into the label's box, narrowed to fit. At canvas size, and with no painter (the tests), the pictures are PM's own.
- **(b) and (c), one cause:** a recorded hires line was clipped to its advances (x0 to x0 + w). A glyph's ink past its last advance (the t's bar, the last 0 at 4K) was cut, and the canvas's own ink past it showed beside the line as a dot. A transparent line's draw now takes its DC's rect across; an OPAQUE one's stays its cells. Engine, ours (sdl_twod_host.cpp's hires record, on only with the 2D at high resolution: MW1).
- **(d) No change needed:** no HUD text is a gradient. PM's 112 named fonts are all one colour top to bottom, and the HUD's own (gauge_font, the weapon list) set one colour. Checked with a probe in a mission, its pause and its results (not kept).
- **Tests:**
  - mw1tests `mw1_loop_hud_labels_drawn_as_text_in_place`: the labels PM's texts in order (the tape's 47, N at 0, E at 90, S, W); with a stand-in painter every PM glyph pixel gone and the painter's ink in PM's colour, the line art kept; at s 1, or a painter giving nothing, PM's own. Sabotaged (PM's glyphs kept): fails.
  - mw3pmsdltests `sdl_hires_transparent_line_takes_its_dc_rect`: "Heat" in PM's gauge font, transparent: the DC's rect; opaque: its cells; the 2D off: nothing recorded. Sabotaged (cells always): fails.
- **Shots:** `outbox/2026-11-19/mw1-hud-text-82/` (4K mission before and after: the compass, the scales, Heat and Coolant, the speed readout).

## 83. [x] The screen sweep's fixes *(done: mw1-fix83-sweep)*
From the 2026-11-19 screen sweep (the lead's calls). Item 1 (the menu title over the mech's head) is Owen's call.
- **The pause (2):** the PAUSED panel is see-through over a grab of the mission's last frame, and the HUD's marker
  line ("OP: <name> [62m]", the radar's) read across its title. Now Escape draws that frame once more, the world
  not moved, with the line left out. The grab doesn't have it, and the line's own state comes back for the next
  frame. The panel is as it was. This is an engine opt-in (`platform/pause_hud.h`, CoreState's Direct3D frame),
  MW1's alone. `mw1pausegrab` checks both shapes; with the opt-in off it fails.
- **RESOLUTION (3):** PM's list (`ResolutionCycle::rebuild`, src/mech3/options_menu.cpp) names modes 3..10 by
  PM's messages ("640 x 480") and FIX (B)'s 11..16 "1280x720". MW1 names them all "W x H", from the mode's own
  size (`platform/mode_labels.h`, an opt-in). The lead's call: the mode's size, not the window's. Every mode of a
  shape renders the 3D at the window's pixels, so the window's size would label several items the same.
- **The travel map (4):** a world's name goes above it, centred and clear of its rings, off the dots beside it.
  It goes right, left or below only where another name already has the place.
- **The intro's picture (6):** it is art now, like the other scenes: `mockups/intro.jpg` cut to 854:480 at its
  own size (1344x755, `data-src/art/intro.jpg`, art.csv's `intro`), so the 2D draws it at the window's
  resolution. `intro.bmp` (640x366, in mw1image.zbd) is gone.
- **The shot driver:** `hub_contract` and `hub_contract_read` take a contract (REQUEST, ACCEPT, OK) before
  their shot; the pause page presses Escape as a player does.

## 87. [x] The travel map's names on a dark backing *(done: mw1-fix87-name-backing)*
FIXES 83 put each name above its world, but in dense clusters (Sian, New Avalon) it still crossed the dots there.
The lead's call: a subtle backing, only as wide as the name.
- Each world's name sits on a black box at opacity 150/255. The box is the words' width as drawn (FontText's
  rect less its 3) plus 2 canvas pixels on each side, and the line's height. It draws over the marks and under the
  texts.
- The box is a mark (`mw1_marks.h`'s `b<w>x<h>o<opacity>`), so the canvas and the high-resolution draw make it from
  the same geometry, anti-aliased at the window's scale.
- Mw1Screen takes it from `TextBlock::backing`. A pool of 16 is made with the marks on the first show, before any
  text.
- `mw1namebacking` runs the travel page hidden at 16:9 and 4:3. Every name backed must fit its words. With the
  backings off it fails (0 backed). The marks test covers the box's name, canvas alpha and scaled alpha.

## 88. [x] FIXES 83's review: the tests that check the picture, one name a mode *(done: mw1-fix88-review)*
From dev 1's review of 83 (the lead's list).
- **The pause's grab, checked in the picture (1):** `mw1pausegrab` now looks at the pause's backdrop at the marker
  line's rect, not just at the counters.
  - With the high-resolution draw on, the HUD's texts reach the grab only as its copied recorded draws (FIXES 12);
    its 565 pixels hold no HUD at all. So the check is the grab's recorded texts of the line's colour (0x1fa201)
    over the rect, counted through the program's SDL host (`mw1::recorded_lines`), plus the canvas pixels.
  - It fails with the opt-in off (the frame before Escape: 1 recorded line), and when the hide is counted but the
    flag isn't set (as if the HUD ignored kHidden: the counters pass, the picture has the line).
  - A third run uses MW1_HIRES=0.
  - The software and doubled paths aren't changed: the SDL port's mission draws Direct3D's path only (the device is
    SDL GPU; docs/resolution.md, the software paths not ported).
- **The intro's picture through the real lookup (2):** the headless intro test packs `data-src/art`'s intro into an
  mw1art.zbd as the build does and installs the art.
  - In the picture phase, zimg's lookup (the image provider) gives the canvas copy (854x480, not one colour), and
    the art store gives the source (1344x755). The headless 2D host loads no layout pictures, so these are asked
    directly.
  - The generated mw1image.zbd keeps only mw1_black. `images::letterbox` and `images::cover` are gone, along with
    their test.
  - The check fails without the art archive.
- **RESOLUTION's duplicates (3):** mode 3 is "320 x 240" (it draws 320x240, doubled: 0x0040d810), not 5's
  "640 x 480". The player's own size (16) is "1920 x 1080 custom", not 13's "1920 x 1080". The test checks that no
  two items in either list read alike.
- **The names' places (4):** a new test chooses every world in turn, with its nearest world hovered and its second
  nearest the story's (" · STORY").
  - Each name must be where the rule puts it (above, right/left by the edge, below; the first clear), in the
    note font when chosen, here or the story's.
  - All four kinds of place come up (above 1002, right 158, left 69, below 34 of 1281 names), and the suffix
    every time.
  - With left and below swapped, it fails.

## 89. [x] The radar's lines at the window's scale *(done: mw1-fix89-radar-lines)*
The 4K sweep (item 12) left the mission radar's ring, sweep cone and boundaries blocky: the canvas drew them as
lines, and the high-resolution draw had only text and pictures. Per the lead and the mw3 lead, hires is MW1's path,
so this is an "mw1 (engine, ours)" change, used only with hires on.
- The SDL 2D host records its lines, line lists and circles as a third kind of draw (`Kind::kLine`), with the
  canvas under each kept as for a picture. The canvas still gets its pixels.
- The draw makes each one's texture at the frame's scale (`platform/hires_lines.h`): a canvas pixel wide, its edges
  blended over a frame pixel, round ends, in the canvas's own colour, in the canvas's order. Over the 3D, the
  canvas's copy is dropped under it, as for text.
- Dashed and noisy lines (a damaged radar's static) stay the canvas's. The cockpit dashboard is a 3D texture, left
  alone.
- Other 2D lines get the same treatment: the lab's section rules and its text caret are now smooth too. A full 4K
  sweep of every MW1_SCREEN page shows no other change, and nothing new left to the canvas.
- Test (mw3pmsdltests hires): `hires_draw_the_radar_s_lines_recorded_and_drawn_smooth` draws a ring, a cone side and
  a boundary through the host and checks three recorded lines, drawn anti-aliased in their colour over the key. With
  the host's recording off, it fails.

## 84. [x] A grudge fades *(done: mw1-grudge-fade; Owen: yes)*
The soak's decision A: MW1's 8 grievances against a House is a refusal, never forgiven, and each contract adds +2
against the House fought. In 11 of 40 long seeds every House with an office came to refuse after 26 to 48
contracts: the company was stranded with nothing money could buy. The lead's recommendation, ours not MW1's:
- Every `kGrudgeFadeMonths` (4) game months without a fight against a House, one grievance against the company fades
  (never below 0). A fight against the House starts its count again. MW1's refusal at 8 is unchanged.
- A House that refused deals with the company again once under 8, and the hub says so (the log: "Time has cooled
  House X's anger: its offices will deal with the company again.").
- The count is saved (`calm<h>`; an older save without it reads 0).
- The rate, from the soak's 11 locked-out seeds: at 3, 4 and 6 months none locked out; at most 1, 2 and 3 Houses
  refused at once. 4 keeps a refusal possible for a House fought too often, never all of them.
- Tests: `mw1_office_a_grudge_fades_without_a_fight`; the soak now fails on "locked out" and prints the peak of
  Houses refusing at once (`MW1_GRUDGE_MONTHS=n` tries another rate).

## 85. [x] A hire or a buy seats the lancemates *(done: mw1-auto-assign; Owen: yes, the Mech Lab's slots rule, see 92)*
The soak's decision B: a lancemate hired, or a 'Mech bought, wasn't put on anything; only the mech lab's lance
assigned them, and a lancemate without a 'Mech doesn't fly. The lead's recommendation, ours not MW1's:
- After a hire or a buy, each lancemate without a 'Mech takes the first spare in the hangar, in lance order
  (`seat_lancemates`). Nobody already seated is moved.
- The Bar's or the market's status line says so ("X takes the Firefly."), and the log keeps it quietly (`kSeated`).
- The lance's weight changed by a buy, the offer is drawn again, as the Bar and the lab already do.
- The mech lab can still change it all.
- Tests: `mw1_pilots_a_hire_or_a_buy_seats_the_lancemates`; the loop's Bar run checks each hire took its spare and the
  status line said so.
- Stacked on 84 only because both add a log kind (84's `kWelcomeAgain`, then `kSeated`); alone it rebases with one
  line changed.

## Order (open items, the lead's, 2026-11-17; two devs from 22:30)
**mw1 dev 1** (the hub, the lab, the market, travel), in order:
1. 23 + 27: the Company screen's mech icons, and every mech picture as PM's icon.
2. 24: hold an arrow to keep stepping.
3. 21: the travel map's world readout, and the trade loop.
4. 6: the hub's scenery by landscape (the scenes are made), and the Bar and options backdrops.
5. 25: PM's blinking buttons.
6. 26: the lab's leftovers.
7. 14: mech renders (a test render to Owen first).

**mw1 dev 2** (a mission's start and end, then the engine):
1. 18: DEPLOY straight in, with an MW1 loading screen. Owen hits PM's briefing every mission.
2. 19: MW1's results screen, with the salvage.
3. 20's rest: the offer's key facts highlighted.
4. 12: high-resolution 2D (the jagged text). The approved plan is in the lead's notes to dev 1. Step 1 shots to the lead before step 2.
5. 3 + 5: sweep every screen for anything still PM's.

## 86. [x] The menu's title clear of the 'Mech's head *(done: mw1-menu-title-up; Owen picked "up")*
On the main menu MECHWARRIOR's last letters sat on the 'Mech's head (at 16:9 the R, at 4:3 the O).
- **The two choices:** both were built as real layouts behind a development switch (branch mw1-menu-title):
  - "up": the title moved into the sky.
  - "low": the picture recut 1.25 times closer, so the head sits under the title.
- **Owen's pick, "up":** MW1MENUDIALOG and MW1MENUDIALOG_WIDE (mw1menu.zrd) now carry it. MECHWARRIOR and CRITICAL
  HIT sit 38 canvas pixels higher, in the sky above the head: TITLE at y -4, SUBTITLE at y 58. The sky over the head
  is only about 50 pixels, so the title sits near the top edge.
- **Not merged:** the switch, the "low" layouts and their picture (title_bg_low, and mw1art.py's 1.25 cut).
- **Shots:** at 1080p and 4:3, in outbox/2026-11-19/mw1-menu-title/ (menu-final-*).

## 91. [x] The lance-orders test, made solid *(done: mw1-lance-flake)*
`mw1_loop_lance_orders_reach_each_of_three_headless` failed 1 in 3 for the lead at 87's merge. Lancemate 1 was ordered
at a mech 370 m away and walked 6 m in 450 frames; the other two walked as always.
- **What was found:** the engine seeds its rand from time(). Given the seed, a run is the same to the metre: the
  same seed in 6 processes at once and under `ctest -j4` gave the same walks.
  - About 640 pinned seeds (each in its own folders), 5 full suites, 8 `ctest -j4`, ASan and UBSan over the full
    suite (clean), and malloc scribbling never showed one lancemate standing.
  - The lead's log is line for line a passing run's (same layout; the other two walked 173 and 107 m to 136 and
    231 m, as in seed 1). Only lancemate 1 differs, and the world's random draws didn't change with it. So it's
    not the seed, and not in anything the seed drives. It stays open.
- **A second flake the sweep found:** at seed 146 the enemies kill the player standing still before the 450
  frames, and the mission ends under the test (`!ended_under_us`, every walk -1). 1 seed in 300.
- **The test now:**
  - The player and the enemies are invincible while the lance walks, released before the loop ends the mission.
  - The test picks the engine's seed itself (time()), prints it, and `MW1_TEST_SEED=<n>` replays it.
  - Each lancemate's walk is kept every 50 frames: how far toward its mech, how far moved, speed, health and the gap.
    It is printed for any lancemate that didn't walk at its mech, so the next stall tells what it was.
- **Later, the walk flake (the lead, 2026-11-19; mw1-walk-flake):** `mw1_loop_lance_orders_an_awesome_walks` failed
  at nearly every merge. Its seed came from the clock, and the seed decides the layout: the enemies 340 to 1,360 m
  off. Over seeds 1 to 8 the Awesome (the third lancemate) walked 18 to 73 m. From the far layouts (seed 6, 1,359 m)
  it stood about 300 frames, its speed up but moving 0 m, then walked: 18 m in the 450 frames, under the 30 m bar.
  - **The test now:** the four lance-orders tests run at a fixed seed (kSeed, 1: the enemies 340 to 410 m off, or
    1,700 m with one enemy); `MW1_TEST_SEED=<n>` still picks another. Each lancemate must also close the gap to its
    mech by 30 m (printed as "closed"), not only walk 30 m toward it. At seed 1 the least is the Awesome's: 62 m
    walked, 135 m closed. It gave the same metres 14 times in a row.
  - **Left open:** the Awesome's slow start from a far layout (speed without movement for about 10 s) is worth its
    own look: a lancemate under orders shouldn't stand that long.

## 92. [x] The seat's status line points at the Mech Lab *(done: mw1-seat-lab)*
Owen, on 85: assignments belong to the Mech Lab's slots along the top, as in MW3. So the seat may only fill a blank
slot the lab would show, never move anyone, and the status line should say where to change it.
- **The seat, checked against the lab:** the lab's top row is you, then one slot per lancemate with their mech, or the
  empty frame without one (`lance_slots`). `seat_lancemates` only gives a lancemate who has no mech a hangar mech no
  one pilots (a spare, from the lab's store). It already met the rule: no change.
- **The note:** `seated_note` replaces the hire's "X joins the lance." and the buy's "Bought: ..." when someone was
  seated. A hire says "X joins in the Firefly", a buy "X takes the Firefly", and both end with Owen's
  "(change it in the Mech Lab)." when that fits the status line at 4:3 (`kStatusLetters`, 62). Otherwise they end
  with "; see the Mech Lab.". Several seated at once (only when lancemates had none before) is "N lancemates take
  spare 'Mechs; see the Mech Lab.". The price stays on the funds above. The log still records each seat.
- **Tests:**
  - The loop's Bar check builds the note for every pilot in every mech the market sells, with PM's titles, hired
    and bought (2016 notes). None is over the limit, every one names the Mech Lab, and 1911 are in Owen's words.
  - Its hire check now looks for " joins in the " and the lab.
  - The pilots test covers the forms.

## 94. [x] The sharp text's letters evenly spaced *(done: mw1-hires-advance)*
Owen, in a mission's F12 objectives: "tow er" and "Tow er" had a gap after the w, "Objectives" had its "ti" crowded,
and "In Progress" looked uneven.
- **The cause** (`platform/hires_text.cpp`, FIXES 12's raster for every high-resolution line):
  - Each glyph was drawn from the font opened at the frame's size, but placed at the canvas font's pen times the
    scale. That pen is the 480-line font, MONO-hinted, in whole canvas pixels: a "w" a pixel wide, a "t" a pixel
    narrow. Times 2.25 (1080p) to 4.5 (4K), a pixel of that rounding is 2 to 5 frame pixels. On Owen's Arial line
    a letter-to-letter step was up to 6.4 frame pixels off the font's own at 4K.
  - Two smaller faults made it worse:
    - A glyph with a negative left bearing (a "j") was drawn that much to the right: 3 frame pixels at 4K, into its
      neighbour. SDL_ttf starts a glyph's render at its ink, not its pen.
    - Pens taken from measured strings count the last glyph's overhang past its advance (an "f", a "j").
- **The fix:**
  - The line still starts at the canvas pen and spans what the canvas's advances span, so the layouts, fit guards
    and overlap checks still hold.
  - Each glyph sits within the line at the big font's own advances, stretched evenly to those ends; the two widths
    are within a tenth of each other.
  - The ink offset keeps a negative bearing.
  - The tracking is still the canvas's, times the scale.
  - Every high-resolution line goes through this: the HUD, the menus, MW1's screens and PM's.
  - With the opt-in off (PM, MW3, MW1_HIRES=0) nothing changes.
- **Tests** (`hires_text_on_the_canvas_pen`, mw3pmsdltests):
  - Fonts: MW1's two, PM's impact, and PM's arial at 12 and 14 bold on Owen's lines ("Destroy the tower  OP:
    Tower Primary Objectives In Progress").
  - Scales: 1080p's, 1440p's and 4K's.
  - Each glyph's ink is exactly where the rule puts it, and each step is within a frame pixel of the font's own.
  - On the old raster it fails 529 checks, 162 of them the uneven steps.
- **Shots:** `MW1_SCREEN=objectives` (development) shows F12's objectives over the mission (command 0x44 at frame
  90). Hidden shots of the objectives, the HUD, the menu and the Bar at 1080p and 4K:
  outbox/2026-11-19/mw1-hires-advance/.
- **"Destroy the tower  OP: Tower":** that's PM's and MW3's own line (Mech3.exe 0x0045e080): the objective's text,
  two spaces, then its nav point's name as "OP: <name>". It reads as the originals do.

## 97. [x] A defence's enemy stood at its post all mission *(done: mw1-defend-guard)*
Owen, defending the tower on mw3c1ia3 (4 enemies): three came; the fourth never moved, but fought when he came near.
- **Cause:** the map's Enemy_08 is MW3's Instant Action guard, in its aiv record:
  - Its net (`net_06`) is three nodes that end 56 m from where it starts, with no link on.
  - Its pilot starts at mode 0 (PM's `may_engage` wants 1).
  - It engages within 850 m. The company starts about 870 to 930 m off.
  - Its break-offs (`+0xcc` bits 0 and 1: the leash from where a fight began, and the fight's time) send it back to
    its post.
  In MW3 it waits until the player comes. A defence, though, is won only by destroying every enemy, so one left at
  its post a kilometre away stood all mission.
- **Fix (mw1_game.cpp, `enemy_launching`):** in a defence only, every enemy starts at mode 1, engages from 3 km at least
  (`kDefenceEngage`), and has no leash or time break-off, so each one comes for the company. Other contracts keep the
  map's own pilots. The log says each enemy it changed.
- **Ruled out first:** the lancemates, and the Awesome's gait warning (PM's own report: run's bounds 3 to 3). Only
  assault chassis report it (awesome, atlas, annihilator, and MW3's own daishi and supernova): each has no run
  above its trot. Of the market's nine chassis only the Awesome reports it, and it walks under orders as the
  others do (a new lance-orders test).
- **Tests:**
  - `mw1_loop_defend_every_enemy_comes_headless`: Owen's mission and force (a 55 t, a 70 t and two 75 t slots)
    with the player standing at his start. Every enemy must close 300 m on him in 2400 frames. Unfixed, the guard
    closes 56 m (and the slower Enemy_06 217 m). With mode 1 alone the guard closes about 230 m before the leash
    takes it back. Fixed, all four close 470 to 690 m.
  - `mw1_loop_lance_orders_an_awesome_walks_headless`: the third lancemate in an Awesome walks at its target. The
    harness takes the lance's mechs (`lance_mechs`).

## 93. [x] A new campaign played to its ending, headless *(done, mw1-fix93-playthrough; MW1_PLAYTHROUGH=1)*

Owen: can the game be played start to finish, the storyline working? `mw1_story_a_whole_campaign_played_to_its_ending_headless`
plays one through the real loop as a player would: the journal's lead at each hub, the lance built up for each
step's battle (contracts, the Mech Complex, the Bar's pilots, the lab), STORY's GO THERE, MW1's pages answered to
carry the trail on, the battles won. The Dark Wing three times, for the three endings: lost, withdrawn, won; then
the credits and the hub again. Long, so behind `MW1_PLAYTHROUGH=1` (`MW1_PLAYTHROUGH_SEED`, 5 by default).
- **Seed 5:** the ending reached. 65 contracts (all won), 8 story battles, 97 trips, 16 'Mechs bought, 3 pilots.
  - Steps 1 to 4 at once. Step 5's battle (rung 1, 110 t) waits from month 2 to month 62 (28 contracts): a lance
    of three lancemates, 135 t, built first. Then steps 6 to 17 by month 91; ATTACK first offered at month 108,
    after 34 contracts.
  - The Dark Wing's endings: lost (mw1_end_lost), withdrawn (mw1_end_retreat), won (mw1_end_win), each with its
    picture; the credits rolled, then the hub. The ending at month 262: the test rebuilds a 390 t lance between
    tries (a player could try again at once).
- **Nothing blocks.** The story's questions (line 372): 1 to 3 and 5 Owen's; 4 (MW1's name), 6 (one stage) and 7
  (the offers as built) stand as built for 1.0.0, the lead's call, for Owen's morning list.
- **Found, a real engine bug (for main and pm):** every Pirate's Moon world's unload leaked about 7 MB (246 MB at
  25 battles, 444 at 50; past 8 GB at 1,128). A PM node's model (pm_model) was never dropped, so its node, lists
  and data and the model stayed; and the material cycles were never freed. Fixed in the engine (its own commit and
  tests); now 95 MB at 25 battles, 108 at 50. The test fails if a campaign grows 1 MB a battle after the first 25.
- **The test's own fixes:** ready for a step's battle at 100 t a pilot at most (the Dark Wing's 635 t meets four
  assault 'Mechs at most); the endings read from the results panel (a withdrawal is a Failure with the retreat
  picture); the summary taken while the game is up.

## 113. [x] A lancemate "untouched, then dead" on c4ia1 *(chased: a fair fight lost, unseen and unsaid; the death's line now says what went)*

Owen (2026-11-19): Erin Falls's Centurion, bought new, about 25 s into a destroy-all on c4ia1, the lance bunched,
"completely untouched", then "I'm ejecting" and destroyed; no shot at her that he saw, no friendly-fire line.
- **Ruled out, in MW3's and PM's code:**
  - *The ceiling's crush* (MW3 0x00423600, PM 0x00424ba0 with 0x00424d80): the one path that kills with no hit and
    no mech's kill. A thing over the head crushes only when it moved this frame and nothing standing owns it; a
    mech's geometry sorts as a mech (0x00423010, the owner by 0x0040e9d0) first. Every boxed node of the Atlas,
    the Annihilator and the lance's mechs carries its owner's bits (43, 28, 23, 26 nodes): none can be a ceiling.
    No crush in eight seeds.
  - *Criticals* (MW3 0x00460d50; PM's ammo critical 0x004621a7 zeroes the section it hit, the centre torso's a
    death): a hit can roll one only on a part at 15% armour or less (the torso's front centre for any torso
    section). None rolled in the runs.
  - *Rams:* allies take none. *Heat:* 0 to 1. Spawn overlap: none; each walked 10 to 26 m.
- **What kills her, reproduced** (`MW1_ROWS=mw3c4ia1`, his lance, seeds 1 to 8, the commander standing): in seeds
  4, 5 and 7 the Centurion is first hit about 11 s in, from 781 m, by one enemy (a Bushwacker), and worn down, 31 to
  41 hits in volleys of four, every one on the front of the centre torso, until it goes at 35 to 42 s. The head,
  arms and legs untouched; CT 0/0, the rest near full: "untouched" but for the centre torso, which the enemy's aim
  takes after a first hit (the torso's node, damage.md). Owen's own right torso was hit before she fell in his log,
  so the enemies were firing then.
- **Why it looked sudden:** nothing told him. A lancemate under fire radios for help only with the player over 22 m
  off (PM 0x004fbc20, MW3 0x004ea520: dist2 > 500); bunched, she said nothing until the ejecting line.
- **The death's line** (106's, `log_lancemate_down`) now says which part's destruction killed the mech, how the
  damage came (a weapon's hit, the ceiling's crush, a mech's ram, or none of those: a fall, heat, a script) and every
  location's armour and structure left. The engine keeps the part and the path on the mech (`mech_death_cause`), and
  logs each ceiling crush (what was over the mech, its box, the damage) and each critical (part, section, kind, the
  section before). Logging only: nothing plays differently.
- **Test** (`mw1_loop_a_lancemate_s_death_says_what_went_headless`): his lance on c4ia1, seed 4: one death, "the
  centre torso went, by a weapon's hit", CT 0/0, head 100/100. With the hit's path unmarked it fails.
- **Open, Owen's call:** whether a lancemate should call out under fire when close (PM's own rule says no), or
  whether the HUD should show the lance's damage.
## 105. [x] Release prep for MW1 1.0.0: its own version, packaged builds, where mw1data goes *(done, mw1-fix105-release)*

The lead (2026-11-19): MW1 1.0.0 rides mw3-re v1.1.3; the launcher reads each program's version from its file.
- **Version:** mw1 compiles mw3_main.cpp with mw3pm's definitions, so its file carried the engine's describe
  (pm-v1.1.2-639-g...) as its `MW3-PROGRAM-VERSION:` tag. Now `MW1_VERSION`, `git describe --tags --match
  "mw1-v[0-9]*" --always --dirty` on every platform (was Windows only, any tag), is the one tag (mw3_main.cpp, the
  engine commit). Untagged: the bare commit. The Windows version resource takes it too.
- `mw1 --version` answers first ("mw1 (MechWarrior 1: Critical Hit) <v>"), before a write dir, an install or a
  window; it set up the user's write dir before.
- **Test** `mw1version` (ctest, every build): one tag in the file, CMake's describe, MW1's shape (mw1-v..., or the
  bare commit; never pm-v or v), and --version prints the same.
- **Package builds** (`-DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON`, target mw1, scratch worktrees): the Mac builds; WSL
  (Ubuntu, GCC 13, the release deps prefix) failed on `SoundCfg`'s defaulted `==` (C++20; the project is C++17,
  Clang only warned): written out. Both build and pass `mw1version`. MW1's own warnings left (13 to 14, other
  items' code: missing `narrow` initializers, an unused variable and function) don't stop either.
- **Where mw1data goes:** mw1 mounts `<SDL base path>/mw1data`. Probed with SDL3 itself: in a .app, a program in
  `Contents/MacOS` gets `Contents/Resources/`; on Linux, the program's folder (`AppDir/usr/bin/`, with or without
  APPIMAGE/APPDIR set); Windows, the program's folder. So: `Contents/MacOS/mw1` with
  `Contents/Resources/mw1data/`; `usr/bin/mw1` with `usr/bin/mw1data/`; `mw1.exe` with `mw1data\` beside it. The
  launcher's art goes the same way (tools/macos/make_app.sh, tools/linux/make-appimage.sh).
- **With it:** the missing-installs box looked for `mw3launcher` at SDL's base path too, which in a .app is
  Resources: never found there. It looks in mw1's own folder now (Contents/MacOS).

## 100. [x] Menu music, QUIT in the hub, the sound settings *(done: mw1-fix100-music)*
- **Owen (2026-11-19, via the lead):** a YuE2 track he picked as the menu music. QUIT outside the main menu. Music
  and effects settings in MW1's options.
- **The theme** (`data/music/menu-theme.ogg`, ours, 7.8 MB; the lead faded its last 5 s; the FLAC master is in
  keep/mw1-critical-hit/music):
  - It plays through the engine's CD music as zsnd_cd's extra track (track 100), from mw1data's music/. A numbered
    trackNN would have raised the disc's track count, and PM's mission music picks among those, so it would have
    drawn the theme.
  - It starts with the game (the title, the intro) and plays on every screen out of a mission, looping. A screen
    change never restarts it.
  - DEPLOY fades it out over a second, then stops it and gives PM back its volume for the mission's music. At the
    results it fades in again. PM's own menu music never plays over MW1's screens: the theme takes the CD back.
  - mw1_music.{h,cpp}; `music::frame(!deployed_)` each frame shown.
- **Silence:** a --nosound or --hidden run plays no music aloud. Before, SdlMusicOutput always opened the real
  device. Such a run now gets a platform::SilentMusicOutput, which keeps the calls and opens no device. It began as
  MW1's opt-in flag; the mw3 lead took it onto main for every program (--nosound or a hands-off run, 5681fc40), and
  the flag went at the merge of pm (mw1-merge-pm-2).
- **QUIT:** the hub's foot, between LOG and STORY, gone while the box shows. It asks with PM's quit box, the same one
  as the main menu's QUIT: NO stays, OK quits.
- **Settings** (the hub's OPTIONS): MUSIC: ON / OFF, then MUSIC n% and EFFECTS n% between ◀ ▶ arrows (steps of 10).
  - They drive PM's own two (its options' CD volume and sound volume: GRAPHICS & SOUND's MUSIC VOLUME and SFX sliders),
    so the mission's music and sounds follow, and the two screens never disagree.
  - Off sets PM's music volume to 0, which stops the theme and any mission music; on brings back the saved volume.
  - mw1.cfg keeps `music=`, `music_volume=` and `effects_volume=` beside the installs (mw1_start.h's SoundCfg). They're
    applied at the main state's entry, and synced from PM's sliders whenever OPTIONS shows.
- **Tests:**
  - `mw1_music_theme_screens_mission_settings_and_quit_headless` (mw1tests): the theme at the menu and the intro; the
    same play through the hub, the office and back; faded and gone in the mission, PM's volume back; again at the
    results; OFF stops it and saves; EFFECTS applied and saved; ON brings it back; QUIT asks, NO stays, OK quits.
  - `zsnd_cd_extra_track_outside_the_disc` and `quiet_music_a_hidden_run_opens_no_audio_device` (mw3pmsdltests).
- **Shots:** the hub with QUIT, and OPTIONS, at 4:3 and 1080p: outbox/2026-11-19/mw1-fix100-music/.

## 99. [x] The starport says each world's tier *(done: mw1-fix99-travel-tier)*
- **Owen (2026-11-19, via the lead):** show each world's tier in the travel page's readout, as MW1 numbers it, e.g.
  "Tier 2". Don't say what tiers do. This reverses item 64 for the starport only: the Mech Complex and the map's
  filters stay tier-free.
- **MW1's numbers:** byte 2 of the planet record, (code - 1) / 3: 0 to 3 (44 worlds tier 0, 50 tier 1, 21 tier 2, 30
  tier 3).
- **Where:** a line of its own, "Tier N", under the House line (mw1_travel_screen.cpp world_lines). It shows for the
  world under the mouse, the chosen one, and "You are here".
- **The fit:** neither the House line nor the population has room for " · Tier N" (both run to 24 of 26 columns). With
  the line, the longest readout (a long description, the trip unaffordable, not yet chosen) came to 15 of the 14
  lines above GO, so a readout past 14 drops its blank gap lines first. 14 at most, every world, either way.
- **Shots:** the readout at 4:3 and 1080p: outbox/2026-11-19/mw1-fix99-travel-tier/.
- **Tests** (`mw1_travel_screen_world_readout`): each of the 145 worlds says its own tier once, with no other tier word;
  nothing says what a tier does (no %, dealer or buy/sell lines); every line fits the panel's 26 columns, and the
  readout still fits its 14 lines above GO.

## 101. The side arrows' triangles fit their boxes (Owen, 2026-11-19)

Owen's crop of the hub's OPTIONS: each ◀ ▶ ran into its frame (the ◀'s flat side on the box's right, the ▶'s on its
left), so it looked cut off and off-centre.

- **The cause:** mw1_chrome.cpp side_arrow drew a triangle 16 rows high in the 18-row box, its base on the frame's
  edge. The up and down arrows (the office's TERMS, the lists') were already right: 20 of 28 wide, centred.
- **The fix:** the side arrow is now the up arrow's triangle turned on its side: a right angle at its point, its base
  10 rows (kSideInkH) with two rows of glass inside the frame above and below, centred both ways. The same piece
  serves every ◀ ▶ in MW1: the hub's OPTIONS and every options page (GRAPHICS & SOUND's 104).
- **Checked:** the other arrows at 4:3, 1080p and 4K. The TERMS and lists' up and down arrows were already centred;
  the lab's and the file list's small ones are bare triangles with no frame of their own, and sit centred.
- **Test:** mw1_chrome_arrow_ink_inside_its_frame_and_centred covers mw1_up, mw1_dn, mw1_lt and mw1_rt at the
  archive's scale and at 2, 3 and 4 (1080p, 4K). The ink must stay a pixel of glass inside the frame and be centred to
  a device pixel. The old side arrow fails it 8 times. The geometry pieces' golden hash moved on purpose.
- **Shots:** outbox/2026-11-19/mw1-fix101-arrows/ (hub_options, options_gfx, office_offer, lab_files at 43, 1080, 4k,
  and a 4K crop of the hub's arrows).

## 98. [x] Losses rare, and a narrow win still a win *(done: mw1-rare-losses)*
Owen lost all three lancemates and their 'Mechs in one mission (about 15 million C-bills). He wants losses much rarer,
as in MW1, and a narrow win never empty-handed.
- **The rolls (`crew::roll_losses`, once as the mission ends):**
  - On a win, every company 'Mech that went down (the commander's too) is recovered, its repair billed as usual
    (the whole repair). It's lost for good by `kMechLostPercent` (10).
  - On a loss or a retreat, a downed 'Mech is lost, as before (FIXES.md item 75).
  - A downed lancemate is KILLED by `kKilledPercent` (5) and MISSING by `kMissingPercent` (5, was 50) on any
    ending. Otherwise they come back and stay in the lance. One whose 'Mech was lost stays without one (the lab's
    blank slot). A killed or missing pilot's recovered 'Mech becomes a spare.
  - `loss_rates()` (or `MW1_LOSS_RATES=mech,killed,missing`) lets the tests pin a case and a soak tune them.
- **The floor (`settle_mission`):** a House contract won nets at least `kWinFloorPercent` (25) of its payment. The
  net is the advance + the fee + the salvage pay - the repairs paid - the 'Mechs lost at their value
  (`buy_price` at the company's tier). The House covers the shortfall, never more than the costs. Losses and
  retreats have no floor.
- **AFTER ACTION:**
  - PAYMENT: "Repairs 1,649,385; the House covers 884,385", or "Lost 3,696,000; House covers ..." for a lost
    'Mech alone, with a shorter wording for 4:3.
  - CREW STATUS: "Firefly (J. Holmaas): recovered (repairs 73,962)", "... destroyed beyond repair, KILLED", each
    the fullest wording that fits 4:3.
  - DETAILS: each repair marked "recovered", a lost lancemate's 'Mech "Lost: the Awesome (Tank Smith), destroyed
    beyond repair.", the House's part, and each downed lancemate's fate.
  - The log adds "Recovered: ..." and "The House covered ...". A lost 'Mech's line now says "destroyed beyond repair".
- **Saves:** the pending mission's bills gain ":lost" and its lance ":killed:mech_lost". Older saves read as before
  (a downed 'Mech lost, its pilot killed unless missing).
- **Tests:**
  - The rolls' rates on each ending, over 4000 rolls.
  - Downed lancemates coming back, with their 'Mech recovered, lost, or left as a spare.
  - The save, and an older save.
  - A narrow win (every 'Mech down, repairs at four times today's price) netting exactly the floor, with the House's
    PAYMENT line fitting 4:3. A lost Awesome on a win, still at the floor. A clean win netting more. A loss with
    no floor.
  - The loop: a casualty pinned (KILLED or MISSING, the 'Mech lost, AFTER ACTION's House and "beyond repair"
    lines), and the full lance's downed lancemate recovered and back.

## 102. [x] The mouse wheel scrolls every list *(done: mw1-wheel-lists)*

Owen (2026-11-19): the wheel already scrolled AFTER ACTION's salvage list; scroll every list in MW1 the same way, the
list under the pointer, with the same step and direction.
- **One handler** (`Mw1State::wheel`, mw1_screen.h's `WheelList`):
  - A screen names its lists: each with the PANEL it sits in (or anywhere on the screen) and its two arrows.
  - A notch up presses the list's up arrow; a notch down presses its down arrow. That is a row each notch (a page for
    a pager), as a click on the arrow does.
  - It presses only a shown, enabled arrow, so the wheel stops at both ends as the arrows do.
  - The list under the pointer wins. Of the anywhere lists, it takes the first whose arrows are shown.
- **The lists:**
  - The Mech Complex: FOR SALE and YOUR MECHS, each under the pointer.
  - The Bar: the recruits, and the lance on the COMPANY page.
  - The event panel's pagers: the LOG, the STORY journal and the news (PREV and NEXT). A plain event's NEXT is its
    OK, so the wheel leaves it alone.
  - AFTER ACTION keeps its own wheel (FIXES.md item 38): same step, same direction.
  - Travel has no world list any more (the map and a typed search), so there is nothing to scroll there.
- **The Mech Lab** (PM's screen, an engine change: "mw1 (engine, ours)", opt-in `platform/list_wheel.h`, MW3 and PM
  unchanged):
  - Its lists (FileList: the chassis, the weapons, the loadouts; LabItemList: the store and the parts) read zin's
    wheel each frame.
  - With the pointer over the list, a notch presses its UP or DOWN arrow, shown and enabled only.
  - A hidden list forgets the wheel, so the turns made elsewhere don't scroll it when it shows again.
- **Tests** (hidden):
  - Market (`mw1_loop_market_buy_sell_pilot_...`): over FOR SALE, to its last row and no further, then back to the
    first. Over YOUR MECHS (empty) or off both lists, nothing. A notch by zin's wheel at the list, read by the
    frame's own update, steps it down and back up.
  - The lab's chassis list, padded past its rows: to its foot (the last rows shown) and no further, then back to its
    head, a notch a frame by zin's wheel.
  - The Bar (`mw1_loop_a_full_lance_flies_on_a_pm_map_headless`): the recruits to the last and back; the COMPANY
    page's lance to its head and its foot.
  - The news (`mw1_story_drinks_and_news_in_the_bar_headless`): it pages down and back up, and up on the first page
    stays put. On a plain drink event the wheel changes nothing.
  - The LOG (`mw1_wheel_pages_the_log_headless`): 40 lines, down to the last page and no further (never back to the
    hub), up to the first.

## 103. The menu theme loops (Owen, 2026-11-19)

Owen's game (5e8bb062): the theme played once and stopped.

- **The cause:** the engine's SDL music output (sdl_music.cpp) waits for its stream's queue to empty before it posts
  a file's end, which the pump hands to zsnd_cd (MM_MCINOTIFY) to play a Loop track again. SDL3's stream resamples a
  file whose rate isn't the device's, and it keeps its last frames back until the stream is flushed. Nothing flushed
  it, so the queue never emptied and the end never came. The headless tests used the silent output, which has no end.
- **The fix:** the output flushes its stream once the file is all decoded. An "mw1 (engine, ours)" commit. It also
  mends PM's and MW3's own Loop tracks and a disc playing on, which had the same fault on main.
- **Not an end:** MW1's fade for a mission and OPTIONS' OFF stop the CD (zsnd_cd::stop), which closes the stream. No
  end is posted and nothing starts again.
- **Test (mw3pmsdltests):** zsnd_cd_extra_track_loops_through_the_sdl_output_and_pump runs the real SDL output on
  SDL's dummy driver (nothing aloud), with the real pump and app. A fifth of a second at 22050 Hz must play at least
  3 times in 2 s, then stopped, nothing more. Before the fix, once.

## 96. [x] The lab's empty lance slot shows MW3's placeholder 'Mech *(done: mw1-notmech-slot)*

Owen (2026-11-19): the Mech Lab's top row showed an empty dashed box for a lancemate without a 'Mech; MW3 shows its
placeholder 'Mech there.
- **What MW3 does:** a slot with no 'Mech asks for its 'Mech's icon by name, and `mech_name(-1)` is "notmech", so
  MechButton asks for `icon_notmech_<colour>`. PM's and MW3's image libraries both ship it (green, red and grey; no
  yellow).
- **The fix** (`mw1_mech_icons.cpp`, `make_mech_icons`): the slot's three looks (`mw1_slot_none`, `_lit`, `_off`)
  are made from `icon_notmech`, read from the install at run time, in the slot's colours, with its outline for the
  high-resolution draw, as every other slot is. The dashed frame stays only for an install without the icon.
- **Test** (`mw1_loop_market_buy_sell_pilot_...`): the made empty-slot look is lit, all amber, not the dashed
  frame, and has an outline.
- **Shots** (hidden, `MW1_SCREEN=lab MW1_LAB_LANCE=3:2`), 4:3 and 1080p: outbox/2026-11-19/mw1-notmech-slot/.
- PM's own icon still shows for the one frame a slot changes, before ours replaces it, as its red pressed icons
  do on every slot.

## 109. [x] The damage display showed only some of the 'Mech's parts at high resolution *(done, mw1-fix109-dmg-hires)*

Owen (2026-11-19, a crop of the bottom right): one arm's outline over the blue grid. The log: "8 draws left to the
canvas", the eight `dmg_<part>_<colour>` pictures.
- **Why:** the hires pass draws the display's background (`dmg_bg_*`) from PM's pixels at the window's scale, but
  not its eight part pictures (hud.cpp `kParts`). Left to the canvas, they sat under the high-resolution grid; which
  showed changed with what was drawn over them.
- **Fix:** `mw1_hud_art.cpp` reads all 32 (8 parts, blue/green/yellow/red) from PM's as the backgrounds are (PM has
  no `_blk`; the "not found" lines are PM's own). Each draws at hud.cpp's place in its order, its colour the
  part's state, as before.
- **Test** (`mw1_loop_hud_damage_parts_drawn_at_the_window_s_resolution`): in a mission, every part's four pictures
  drawn at 1080p's art scale (3) and 4K's (5), their size and lit pixels PM's times the scale: 8 of 8 at each, 64
  of 64. `mw1_loop_hud_art_is_pm_s_pictures_at_canvas_size` counts 53 read pieces (21 + 32), each lit as PM's.
- **Shots** (hidden, the mission with D to the damage display), 1080p and 4K, whole and cropped:
  outbox/2026-11-19/mw1-fix109-damage-display/. No picture of the mission is left to the canvas at 4:3, 1080p or 4K.
- **With it** (the lead, for 107): each looped weapon sound's start and stop is logged, `mw3: loop sound: <weapon>
  on <mech> [(the player's)] started|released|out of ammo|destroyed|deleted`. A loop left on shows as started and
  never stopped. Test `pm_volley_a_loop_sound_s_stop_is_noted`.

## 108. [x] A 'Mech sold dearer than it was bought *(closed: by design, MW1's own trade)*

The lead (2026-11-19), from Owen's log: a Centurion bought for 4,505,000 sold for 6,307,000.
- **Why:** buying pays the tier 0 price times the world's tier percent (100/120/130/150); selling pays 10 points
  less (90/110/120/140). That's MW1's own formula (081B:4645, 081B:32BA). Owen bought at a tier 0 world and sold at
  Skokie, tier 3.
- Within one world selling always pays less than buying; between worlds it pays to buy cheap and sell dear, as in
  MW1. Owen: keep it.
- **Test** (`mw1_market_mw1s_formula_pinned`): every chassis at every tier, buying is base × tier%, selling is
  base × (tier% − 10), floored to 1,000, a Clan 'Mech's doubled, and selling is under buying in the same world. A
  later change can't move it either way unnoticed.

## 106. [x] A lancemate ejected 25 s into a destroy-all on c4ia1 *(closed 2026-11-21: not reproduced; the log now names who downed a lancemate; reopen if it happens again)*

Owen (2026-11-19): a contract, "destroy all enemy mechs" on c4ia1. About 25 s after the launch Erin Falls said "I'm
ejecting". After the mission she was MISSING and her Centurion gone. His build predates 98: the old loss rule (his
save: the Centurion lost, the pilot missing in action).
- **Not reproduced on c4ia1:** with his lance and his contract's four, the commander standing, the enemies start
  1,000 to 1,400 m off and the first hits come near 56 s, from under 100 m. Owen moved and pressed keys in those 25 s,
  so the fight likely started early; his log couldn't say who killed her.
- **The log now says** (`mw1_game.cpp`, `log_lancemate_down`):
  - each lancemate's mech down, at what time, who had the kill (the commander, a lancemate, or an enemy by its
    chassis), who hit it last, its heat and its state;
  - a lancemate found down at the mission's end with no mech's kill (the engine reports a kill only when a mech gets
    the credit: a vehicle's, a turret's or a building's never reaches us), marked destroyed (before, its mech came
    back as if standing, the pilot unhurt);
  - at settlement each mech's fate (back, destroyed and recovered, destroyed and lost for good, with its damage) and
    each lancemate's (back, ejected and back, killed, missing in action) (`fate_lines`, `mw1_repairs.h`).
- **Tests:**
  - `mw1_loop_a_lancemate_down_is_logged_by_whom` (c4ia1): one lancemate killed by an enemy (named by its chassis),
    one with no mech's kill (found at the end), the third standing; AFTER ACTION's crew status agrees.
  - `mw1_repairs_the_fates_said_for_the_log`: every fate's line.
  - `mw1_loop_the_lance_lives_the_opening_minute` plays each of PM's Instant Action maps (12) for a minute with his
    lance and force, the commander idle, the rolls seeded: all three lancemates must stand (the lead, 2026-11-19; seeds
    106, 7 and 2026 all pass, about 70 s). MW1_ALL_MAPS=1 adds MW3's worlds, reported, not required: on mw3c1 ia1 and
    mw3c4 ia1 a lancemate can fall inside the minute (the enemies open fire from about 900 m within 10 s, then close,
    and the lance trades fire with them, two enemies down by then). Whether that's too harsh is Owen's call.
- **Looked at and left:** MW3's allied places engage at 100 m (PM's at 10,000 m). Raised to 10,000 m, the lance did
  no better (it holds by the commander and fires either way), so it's unchanged.

## 107. A weapon's looped sound stops (Owen, 2026-11-19)

Owen's c4ia1: the machine gun's sound came on and played to the mission's end.

- **The cause found:** a weapon's looped sound (U_LOOPED: the machine gun's) plays from its trigger to its release
  (weapon.cpp trigger and release). Outside group fire, a group lets go of only its current weapon (the exe's
  0x004b20b0), and the fire mode can change while the trigger is held. The mode key (group fire on to chain fire)
  selects the group again, which releases under the new mode, so the group's other weapons that had fired were
  never let go and their loops played on. The AI's volley pick changes mode the same way. The exe's own order, so a
  FIX (bug), behind SMALL FIXES: the group also lets go of each other weapon still sounding or firing.
- **Ruled out:** the effects volume (FIXES 100) only sets zSound's master level and touches no playing voice; a
  voice out of hearing range is muted, never stopped or handed to another sound; a mech's death releases every
  weapon (destroy_all). The log's DSERR_CONTROLUNAVAIL is a pitch change (set_frequency) on a sound made without that
  control, refused, and stops nothing.
- **Not proven from the log:** it has no line for a fire-mode change. This is the one path found that leaves a loop
  playing; if the sound sticks again without a mode change, the log needs a line for it.
- **The voices "outside the game's folders":** PM's sound list names ../data/common/sounds/... (eject_voice, the
  ai_* acknowledgements, ge53xx, c1m4dia7). Those files are in no install: not loose in PM's, MW3 1.0's or 1.2's,
  and not in any soundsH.zbd or soundsL.zbd. PM's own game never played them; the line is our confinement saying
  so. Not changed: they need MW1's own recordings, if wanted.
- **On the UPGRADES page:** SMALL FIXES' list gains "A weapon's loop sound stops when firing stops." (it fits
  at 4:3: outbox/2026-11-19/mw1-fix107-loop-sound/); docs/upgrades-sort.md item 21.
- **Test (mw3pmtests):** pm_volley_a_group_volley_lets_every_loop_go. A group volley's two loops playing, then the
  mode key while firing, and the trigger let go in single fire: both loops stopped and cleared. Before, the second
  played on.

## 95. [x] The Bar's recruit pitch in one quote, not each line in its own *(done: mw1-pitch-quote)*

Owen (2026-11-19): RECRUIT CREW showed each line of a recruit's words in its own quote marks, as the exe breaks them.
- **The fix** (`mw1_bar_screen.cpp`, `journal::quoted` in `mw1_journal.h`): the exe's lines joined by a space, one mark
  before the first word and one after the last, wrapped to the terminal (kPitchColumns, 20 letters at 4:3; at most
  kPitchLines, 10). MW1's words and case kept; nothing else added.
- **Drinks and news:** nothing to change. MW1's texts already open a quote on one line and close it on a later one
  (14 open, 21 close, none in a run of self-quoted lines over its 112 texts); only our pitch quoted each line.
- **Tests:** `mw1_pilots_each_pitch_is_one_quote_wrapped`, every pilot's pitch (42, sizes only): one mark opening,
  one closing, no other added, the words all there in order, each line within 20 letters, at most 10 (they take 3
  or 4). The Bar loop test shows each line of Elli Gujar's quote and never her first exe line quoted alone.
- **Shots:** outbox/2026-11-19/mw1-pitch-quote/ (4:3 and 1080p, with the terminal cropped).

## 104. [x] Every 'Mech of PM's as an enemy, in its real loadouts *(done: mw1-every-chassis)*

Owen (for 1.0.0): the enemy fields all of Pirate's Moon's 'Mechs and their real variants, not only the stand-ins for
MW1's kinds. The design the lead approved (2026-11-19), in `mw1_enemies.h/.cpp`:
- **MW1's force roll unchanged** (the budget, the classes, at most four; the ladder's rungs). Each slot then gets a
  PM chassis within kSlotTons (10 t) of its MW1 kind's, Clan by kClanPercent per tier; the 100-tonners only in the
  heaviest (85 t) slot and only late (tier ≥ 2 or record band 3). A loadout from the chassis's stock ones: Primary 60%,
  Alternates 30%, Exotics 10% (no Alternates: 75/25). Its own generator, seeded by the company and the contract, so
  MW1's draws are untouched and the same offer rolls the same 'Mechs.
- **Owen's four, each a named constant there:** kElementalsField (false), kClanPercent ({0, 10, 25, 40}), kWreckPay
  (MW1's kind price), kStoryBattlesRoll (true; the Dark Wing's last rung keeps its stand-ins).
- **Names real everywhere:** the offer ("2 Heavy, 1 Medium (Thor, Avatar, Centurion)": MW1's class words, the real
  chassis), the mission log, AFTER ACTION's kills, the wrecks and salvage, the main enemy's picture, the amber icons.
- **Saves:** optional `offer_chassis=`, `offer_loadouts=`, `pending_wreck_chassis=`; an older save's contract and
  wrecks keep their kinds' stand-ins on the Primary.
- **Tests:** `test_mw1_enemies.cpp` (the reach: 24 chassis, 112 loadouts; a 20,000-contract soak: every chassis and
  loadout seen, the Primary each one's commonest, Clan 0 / 9.8 / 25.1 / 40.2% where a Clan chassis could be, no early
  100-tonner; determinism; older saves; story battles). `mw1_loop_every_chassis_and_loadout_fields_and_salvages_headless`
  spawns four mixed (MW1_ALL_LOADOUTS=1: all 112) and checks each fields, is killed and salvaged, and the engine's stock
  config counts match the table.

## 115. [x] Clan 'Mechs priced as Inner Sphere ones *(done: mw1-clan-price)*
Owen (earlier): "clan mech worth should roughly mirror mw3". The lead's research
(`keep/mw1-critical-hit/research/clan-vs-is-2026-11-19/`) found no Clan premium in MW3 or PM. There is no price or
value field, the chassis are tech-blind, and the Clan's edge is only its equipment.
- `kClanPriceFactor` is 1 (was 2, VISION 2026-11-16). A Clan 'Mech buys, sells and pays as a wreck as an Inner
  Sphere one of its tons. Nothing else doubled Clan: salvage parts go to the stores unpriced.
- The market's header: "* Clan" marks the starred rows; "twice" is dropped. The travel screen's "Clan mechs for
  sale" stays: it's where they're sold, still rare and only at the five edge worlds.
- VISION's 2026-11-16 line says Owen changed it on 2026-11-19.
- The roster's Clan share stays flat (FIXES 112, ungated: about 50%, MW3's own 49% without the Elementals).
- **Test** (`mw1_market_prices_by_tonnage_tier_and_clan`): the Mad Cat and the Orion, the Puma and the Owens, the
  Daishi and the Atlas price the same at every tier, bought and sold, and as wrecks when they pay by chassis.
  `mw1_market_mw1s_formula_pinned` takes the factor.

## 114. [x] MW1's words name our 'Mechs; the Dark Wing fields them *(done: mw1-text-names)*
Owen (2026-11-19, through the lead, on 112's list of MW1's run-time text): "warhammers should be swapped out with
[the Thor] and battlemaster swapped out with atlas if theres scripted stuff."
- **The words, as drawn** (`mw1_text_names.h`): MW1's story texts and choices (`text_pages`, the script's captions)
  and the Bar's pitches (`pitch`) show our names for MW1's. MW1's names are read from the player's exe too (its
  kind-name table, DS:2E86, ten names in its records' order), never kept here; the exe is never changed. Any case, a
  space or hyphen inside or none, a plural (Atlas: "Atlases") and a possessive, upper case kept, whole words only.
- **The mapping** (`kOurChassis`, by MW1's kind record): Owen's, the 70 t record the **Thor**, the 85 t the
  **Atlas**, the first 20 t record the **Owens**; the 75 t record the **Mad Cat** (`kOurs75`). All four
  Owen's (final, 2026-11-19). The rest, never drawn today, the nearest of ours by tons in the same PM
  class, Inner Sphere on a tie: the other 20 t record the Firefly, 35 t the Owens, 45 t the Shadow Cat, 55 t the
  Bushwacker, 60 t the Champion. Drawn today: the Thor and the Atlas (story texts 0x2C and 0x43), the Owens (pilot
  8's pitch), the 75 t record's (pilots 13 and 35).
- **The scripted force:** the Dark Wing's rung (rung 6, the only fixed one; rungs 1 to 5 roll as contracts) fields
  the Thor and four Atlases, its three escorts the 75 t record's (`kOurs75`), on their Primary (`Rung::chassis`; the
  hardest's extra one too). It fielded its slots' stand-ins before (an Avatar, four Awesomes, Orions).
- **Tests:** `mw1_story_our_names_for_mw1s` (the swap on made-up names in the table's places);
  `mw1_story_no_mw1_mech_names_drawn` (with the exe: none of the 1,785 lines we draw names one of MW1's 'Mechs, the
  Dark Wing's texts name the Thor and the Atlas); `mw1_enemies_story_battles_roll_but_the_dark_wing` (its chassis).

## 112. [x] Our roster, not MW1's 'Mechs: the force in tonnage slots, one roster list *(done: mw1-roster)*
Owen (2026-11-19, through the lead): "we dont have any phoenix hawks in this game … we're using the entire mw3+pm
roster, not mw1 mechs … make sure the phoenix hawk, shadow hawk, marauder, etc. names are all scrubbed from the
codebase and it refers to the mechs that we actually have … if we ever want to drop back down to only 8 mechs … we
can change that list."
- **The force's slots** (`mw1_campaign.h`): MW1's ten named kinds are gone. The force rolls in eight tonnage slots,
  `kSlot20` to `kSlot85` (`kForceSlots`: the tons MW1's fielded enemies had, each with its class). MW1's roll is
  unchanged: the budget, x1.5 and x0.5, at most four, heavies first, a class's slots picked at random. The two
  kinds MW1 never fielded (its 20 t and 55 t twins) fold into those slots. The ladder's rungs are written in slots
  (the same tons: 110 to 635 t).
- **MW1's exe records stay as numbers** (`kMw1Kinds`: tons, class and price by index, no names). The tests pin them
  and the body repair prices against the exe, and the repair line is fitted through them.
- **The roster** (`mw1_enemies.h`, `kRoster`): the one list of chassis an enemy can be. It defaults to all 25 of
  MW3's and PM's; the Elemental fields only by `kElementalsField` (still false). Cut it to any subset and the rest
  follows.
- **All of them, from the first contract** (Owen: "i just tried to get a contract and only the original 8 mechs are
  showing up"; his tier-0 offer was three of the old stand-ins of four). 104's slots reached only within 10 t of
  MW1's kind, Clan was 0% at tier 0 and the 100-tonners came only at tier 2 in the 85 t slot, so tier 0 was mostly the
  old eight. Now a slot draws **uniformly from its whole weight class**, PM's own (`slot_class`: the 20 and 35 t slots
  light, 45 and 55 medium, 60 to 75 heavy, 85 assault). **Clan and Inner Sphere alike** (Owen: "the clan mechs arent
  that much better than the IS mechs"); `kClanGated` (false) puts a per-tier gate back, `kClanPercent`. The
  **100-tonners** go in any heavy or assault slot from tier 1 (`kHundredTonTier`, was 2) or record band 3. MW1's
  budget and class counts are unchanged, so the balance holds class by class. A slot with nothing on the roster
  fields its **stand-in**: the roster's nearest chassis by tons, Inner Sphere first, below 100 t first, ties to the
  lighter. With the full roster the stand-ins are exactly the
  eight the game fielded before 104 (Firefly, Owens, Strider, Bushwacker, Champion, Avatar, Orion, Awesome), so
  pre-104 contracts and wrecks field and pay as before. `PmMech` names all 25 ids.
- **Wreck pay** keeps MW1's price per slot: `kWreckPay` is now `kSlot` (the slot's stand-in at the world, as before)
  or `kChassis`.
- **Old saves** (save version 11): `offer_enemies` and a pending wreck's first number are slots now. Before 11 they
  were MW1's kind numbers and read as their slots (`slot_of_mw1_kind`).
- **Scrubbed:** the names are out of code, comments, logs ("the 55 t slot"), tests, VISION.md, the hub mockups and
  this file's own descriptions of our roster. Kept: Owen's quoted words, the history of item 20, the research notes
  (they describe MW1 itself), and VISION's BattleTech price citation.
- **Tests:**
  - `mw1_campaign_slots_and_stand_ins`: slots against the exe records; the old kind numbers to slots; the stand-ins.
  - `mw1_enemies_a_cut_roster_still_fields_every_slot`: the old eight as the roster (only they appear, all of them, a
    pre-104 wreck pays the same), and a roster of one.
  - `mw1_enemies_every_tier_fields_most_of_the_roster`: 2,000 contracts a tier. All 24 chassis appear at every tier;
    the old eight are 35 to 37% of the enemies (the light class is only three: Firefly, Owens, Puma), Clan 48 to 50%.
    Each tier's pools are printed.
  - `mw1_save_a_version_10_save_reads_mw1s_kinds_as_slots`: a version 10 file's kinds read as slots, its pre-104
    wrecks pay as their old stand-ins, and it writes back as version 11.
- **MW1's own text, read at run time from the player's MW_MAIN.EXE, that names those 'Mechs** (we can't change it;
  Owen to decide: leave it, or show a mapped name where we draw it):
  - Story texts 0x2C and 0x43 (segment 1AA5): a 70 t (both) and an 85 t (0x43), the Dark Wing's force. They show on
    the event panel.
  - The Bar's pitches (RECRUIT CREW) of pilots 8 (a 20 t), 13 and 35 (a 75 t).
  - Also in the exe but not drawn by us: MW1's kind-name tables (DS:24A5.., DS:2DB3, DS:2E70) and seven strings at
    DS:3FE5..410A and DS:6BCB.

## 110. [x] A lancemate under orders stood with its throttle up before it walked *(done: mw1-lance-stall)*
Found in FIXES 91's walk flake: on c2, seed 6 (the enemies 1,359 m off, the lance facing away), the Awesome stood
about 6 s with its speed up, moving 0 m, before it walked.
- **Cause (PM's walking context, `WalkPm::throttle`, 0x004d9550):** a pilot more than 10 degrees off its carrot
  (`steer.cos_on`) "brakes off" by throwing the throttle the other way. It judges this by the throttle alone, not by
  whether the 'Mech moves. The Awesome turns at half a light 'Mech's rate (turn rate 1.245 against the Firefly's
  2.588). It swung about 14 degrees past its carrot just after it throttled up, while still standing in the step
  waiting for a footfall. So it threw the throttle into reverse for 28 frames, stopped, turned back and set off
  again: 167 frames from its throttle up to walking.
- **Ruled out:**
  - The nav path: the way was laid at the order and never moved.
  - The gait: the Awesome's "trot max = run max" report doesn't matter; it walks.
  - A map guard: the state is PM's follow throughout.
  - The 'Mech's turn smoothing: turn_accel is 1.5 on every chassis.
- **Fix (engine, ours, opt-in: `mech3::ai::set_walk_keeps_going`, zai_walk_pm.h):** a walk under way (its throttle
  past `steer.brake`) keeps going while the pilot faces within 30 degrees of its carrot. The exe's 10 degrees still
  gate the start, and past 30 it brakes as ever. MW1's game loop turns it on (`install_game_loop`); MW3's and PM's
  programs keep the exe's rule.
- **Measured, seeds 1 to 8, a Firefly, an Owens and an Awesome:**
  - Each turns within 49 frames of the order.
  - Its gait walks 13 to 57 frames after its throttle comes up. The step waits for a footfall before it walks.
  - The Awesome on seed 6 now walks at f+189 (before: f+311), and 2 m at f+214 (before: f+322).
  - A Centurion, an Atlas and an Annihilator never stalled and are unchanged. Owen's Centurion on c4ia1 isn't this:
    its turn rate (1.205) is near the Awesome's, but it never backed off on any seed.
  - A walk of 2 m isn't timed: PM ramps a heavy 'Mech's throttle with its carrot's distance, so the Annihilator
    (9 m/s) takes about 3 s to cover 2 m once its gait walks.
- **Tests:**
  - `mw1_loop_lance_orders_set_off_on_seeds_1_to_8_headless`: each lancemate turns within 2 s of the order, and its
    gait walks within 2.5 s of its throttle coming up, unbroken until it has walked 2 m. With MW1_WALK_EXE=1 (the
    exe's rule) seed 6's Awesome fails.
  - MW1_LANCE_MECHS picks another lance and MW1_TEST_SEED one seed; MW1_WALK_TRACE prints each lancemate's state
    every 10 frames.
  - mw3pmtests `pm_walk_keeps_going_inside_thirty_degrees_only_when_asked`: the exe's brake when off; inside 30
    degrees it keeps going, and past them it brakes; standing still it needs 10 degrees to set off.

## 118. [ ] Permanent upgrades bought with money *(planned for MW1 1.1.0; not in 1.0.0)*
- **See also 122:** 1.1.0's arms dealer, the hub's other shop (weapons and equipment by world).
- **The full plan, with open questions:** mw1/research/permanent-upgrades-plan.md.
- **1.1.0's plan:** mw1/research/plan-1.1.0.md (its slices). Done so far:
  - **Slice 2, the model and save 13 (mw1-110-upgrades):** `mw1_upgrades.h`. The kits in the storehouse and on each
    'Mech (`Company::kits`, `OwnedMech::kits`), the weapon types' steps, the price ladders, buying and assigning. The
    kits go with their 'Mech when the commander changes 'Mechs (PILOT, the lance screen) and back to the storehouse when
    it is sold or lost. Save version 13 adds `kits`, `kits_bought`, `mech_kits`, `hangar<i>_kits` and `steps`; older
    saves read as nothing bought. The prices are placeholders in `mw1_upgrades.h`; `MW1_PRICE_TABLE=1` with
    `mw1_upgrades_the_ladders_climb_and_the_price_table` prints them. No screen and no engine hook yet.
  - **Slice 1, the engine's hooks (mw1-110-hooks):** `src/upgrades/kits` (docs/upgrades-kits.md), an
    "mw1 (engine, ours)" commit. Four hooks, null in MW3 and PM: extra sinks in the heat step, extra armour at a
    part's setup, a weapon's damage scale and its extra range, per weapon (never the shared WeaponDef). They are off
    in a network game, and damage is clamped to 4x and range to the weapon's own RANGE. MW1 doesn't set them yet
    (slice 3).
- **Owen (2026-11-20):** players can keep playing indefinitely without the story, like an Instant Action mod, and
  money needs something to buy. It's always on sale at the hub; nothing to hunt for. It applies to the player's side
  only, never to enemies.
  - **Kits, one unit per purchase:**
    - a permanent heat sink (+1 sink, no crits, no tonnage);
    - a permanent armour point (+1 point beyond the normal cap).
    The company owns them. Assign any number to any of your own or your lancemates' 'Mechs (10 on one, or 5 and 5)
    and move them later. A lost 'Mech's kits return to the storehouse.
  - **Weapon-type steps:** power or range in small steps (e.g. +10 m), with the heat unchanged. A step applies to
    every weapon of that type the company owns (all LRM 20s). A medium laser can climb to an ER large laser's punch
    at a medium laser's heat.
  - **Balance:** high prices and slow increments. Each repeat purchase costs more than the last.
- **Shared code (Owen):** "add the logic to the mw3/pm code as an upgrade, but let's not activate it in mw3 right
  now. Just in mw1." The mw3 lead agreed the split and set these rules (2026-11-20):
  1. It lives in `src/upgrades/kits/`, not `src/mech3/`. Ported functions get only a hook line and the comment "Ours:
     upgrade hook (src/upgrades/kits)". It is not a FIX, and CHANGES.md gets nothing until MW3 turns it on.
  2. A hook table, `upgrades::kit_hooks()`, all null by default: `extra_sinks(const Mech*)`,
     `extra_armour(const Mech*, int part)`, `damage_scale(const Weapon*)` and `range_add(const Weapon*)`. Null keeps
     the exe's path, and the probe library leaves them null. mw1 sets them and answers from its own state.
  3. Weapon steps are per weapon, never per `zwep::WeaponDef`, which enemies share. Apply them where the owner is
     known: the hit's damage, the range checks, and the AI's range and fire picks (allied AI uses the longer range;
     enemies don't). List every read of the def's damage and range in the commit.
  4. Armour and sinks are bonuses outside the Config. Apply them at mission start and in the heat dissipation, never
     in the .m3c, the lab's cap or the saved sink count, so saved loadouts still load in the original exe and in MW3.
     The lab may show "+N". Keep the HUD's heat readout consistent.
  5. In a network game the hooks answer nothing, or answer from the host, so heat and damage stay in sync.
  6. **Tests:**
     - hooks null: the differential tests (heat, damage, weapons_run, the config reader) still agree with the original;
     - hooks set: one mw3tests test per effect, on and off;
     - a seeded headless mission with kits: heat lower, armour higher, and the boosted weapon's hit bigger for the
       player and unchanged for an enemy of the same type.
  7. A plain-words `docs/upgrades-kits.md` lists the hooks and says MW3 and PM don't turn it on yet.
  - **Route:** a mw1 dev writes the engine module as a clean main-targeted commit. The mw3 lead reviews it and lands
    it on main, then merges main into pm, and mw1 takes it from pm. mw1 then adds the store, the assignment screen,
    the save and the price curve. The price table goes to Owen for approval.

## 127. [x] An Exotic loadout's id shown as its name: "Puma: MSG_CONFIG_EXOTIC_A" *(done: mw1-fix127-exotic-names, for 1.0.2)*
- **Found by Owen (2026-11-20):** an enemy's target line read "Puma: MSG_CONFIG_EXOTIC_A".
- **Cause:** PM's reader.zbd has 42 Exotic loadouts, every one marked `hidden`. Neither PM's nor MW3's Mech3Msg.dll
  has a MSG_CONFIG_EXOTIC_* message, only MSG_CONFIG_PRIMARY and _ALTERNATE_A to _D. So the loader's localize keeps
  the id. PM never shows a hidden loadout, and MW3 has none. MW1 fields them (FIXES 104: about 10% of enemies), so
  its HUD showed the id.
- **Class:** an MW1 bug, not a shared engine bug. Nothing goes upstream.
- **Fix:** MW1's stock_config gives a fielded Exotic words in the Alternates' style ("Exotic A"), once, in place. A
  loadout that already has words is left as it is. MW1 code only, no engine commit.
- **Test:** mw1_loop's every-chassis run (every chassis, every loadout fielded) checks that no fielded loadout's name
  has "MSG_" in it.
- **Not covered:** a company save made before this that holds a salvaged Exotic loadout keeps the id in its saved
  copy. A rename at load would cover it, if anyone sees one.
