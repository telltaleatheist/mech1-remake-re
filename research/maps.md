# Instant Action and multiplayer maps for MW1's contracts

Research for `../VISION.md` (open question 7), 2026-11-16. Every Instant
Action (IA) and multiplayer (MP) map in MechWarrior 3 1.2 and in Pirate's
Moon, read from the installs' data. The goal is to pick a map for each MW1
contract by climate and to find where a "reach a point" objective can go.

**Sources.** Data from `game/mw3-1.2/zbd` and `game/pm/zbd` in the mw3-re
checkout. The reader archives were read with `tools/analysis/zrd.py`. World
sizes, ground materials and textures came from mech3ax (`unzbd gamez`,
`unzbd textures`). Map names came from each install's `Mech3Msg.dll`
(`tools/analysis/msgtable.py`). The rules come from the docs cited inline.
`mw3-1.0` was only listed, to see what patch 1.2 added. `pm-disc` was not
read: everything for PM comes from the install.

Coordinates are (x, y, z) in metres, in the world's own frame, with y up.
"Start" is the player's spot in the map's `aiv.zrd`.

## How the maps are picked

**Instant Action.** The Action screen lists four worlds, the folders
`c1`..`c4` (messages 0xe33..0xe36), and five scenarios. Each scenario loads
one archive, `zbd\<world>\readeria<n>.zbd` (`external/mw3-re/docs/menus.md` section 13):

| Scenario | Archive | Mode |
|---|---|---|
| Battle | ia1 | 0 |
| Battle Waves | ia1 | 1 |
| Battle Progression | ia1 | 2 |
| Attack (destroy the enemy base) | ia2 | 3 |
| Defend (protect your base) | ia3 | 4 |

Pirate's Moon's exe holds the same five-row table (file offset 0x2096b8,
read for this survey), so PM also loads only ia1..ia3. **PM's `readeria4`
archives (one per world) are not reachable from the Action screen.** They
are scripted missions with their own objectives (for example c1's "Capture
Enemy Convoy"). What, if anything, loads them is unconfirmed.

**Multiplayer.** Each world's `readermp1` and `readermp2` are the "shipped
eight", named by messages 0x320 + 2 × world + (n − 1)
(`src/mech3/pilot_games.cpp`, MW3 1.2 at 0x00437920). Any `readermp3` and up
is an add-on, named by its `mpinfo.zrd` `db_name`. Only MW3's c3 has these
(`mp3` and `mp4`). Patch 1.2 added them: `mw3-1.0/zbd/c3` stops at
`readermp2`.

**Not reachable.** MW3's `t1` (training) has `readeria1` and `readermp1`.
Each holds only a player spot and stealth, with no enemies. Neither menu
offers `t1`: the Action screen lists c1..c4, and the lobby skips t1
(`src/mech3/mp_lobby.cpp`). `c4b` has no IA or MP archives.

**Counts.**

| | Instant Action | Multiplayer | Unreachable extras |
|---|---|---|---|
| MW3 1.2 | 12 (4 worlds × ia1..ia3) | 10 (8 shipped + c3 mp3, mp4) | t1 ia1, t1 mp1 |
| Pirate's Moon | 12 (4 worlds × ia1..ia3) | 8 | ia4 on each world (4) |

These maps sit on eight different terrains: MW3's c1..c4 and PM's c1..c4.
PM's folders use MW3's names, but they hold different worlds
(`external/mw3-re/docs/pirates-moon.md`).

## The eight worlds and their climate

There is no "climate" field in the data. Each call below rests on four
things:

- the world's ground materials, by soil type, with their textures looked at;
- `world.zrd`'s ambient temperature, weather and fog colour;
- the world's scenery names;
- the IA names.

Size is the world node's area rectangle in `gamez.zbd`. The space you can
actually play in is smaller: the boundary regions are listed per map below.

| Game | Folder | IA name | MP names | Size (m) | Climate call | Evidence |
|---|---|---|---|---|---|---|
| MW3 | c1 | Big Lake | Beachhead (mp1), River Valley (mp2) | 4864 × 5632 | **Desert coast** (hot, dry, sandy) | Ambient 40, the hottest of MW3's IA and MP worlds. Ground: sand, "dksand", beach and orange-brown dirt, plus a river and the sea. Pale grey haze, no weather. Scenery: fishing village, docks, city plazas, power plant. |
| MW3 | c2 | Grasslands | Lake Bed (mp1), The Villa (mp2) | 6144 × 3328 | **Temperate green** (the greenest map, the nearest to tropical) | Ambient 20. Ground: green grass, moss and mud, with caves and pools. Grey-green haze. Pine and shanty scenery. Not a jungle: no palms or jungle textures were found. |
| MW3 | c3 | Rolling Hills | Dropship (mp1), Pyramids (mp2), Durgan City (mp3), Spaceport (mp4) | 4608 × 10496 | **Temperate, at night**, partly urban | Ambient 18. Dark blue-black fog (night). Ground: tan-green grass, beach and concrete. Scenery: city, spaceport, river town, domes. Rain in mp2. |
| MW3 | c4 | Gorge | Train Station (mp1), The Gorge (mp2) | 5376 × 7680 | **Arid rocky canyon** | Ambient 31. Orange-brown haze, and rain in every IA and MP block. Ground: brown dirt, cliff walls and mountain rock, with one snow-capped mountain texture (`mount03`, soil Snow). Scenery: river, village, airfield, train station. |
| PM | c1 | Canyon's Edge | Canyon's Edge East (mp1), West (mp2) | 5376 × 5376 | **Volcanic badlands** (rocky moon) | Ambient 20. Black fog with a cyan cloud layer below ground level. Ground: red-brown rocky dirt, sand, lava craters (soil Lava). Scenery: pirate base, dome base, starbase. |
| PM | c2 | Road To Madre | Road To Madre East / West | 3072 × 4096 | **Desert** (the best desert overall) | Ambient 30. Sandy haze (0.8, 0.7, 0.5) and a tan cloud layer. Ground: orange dirt and gravel, and some water. Scenery: portal base, tents, tanks. |
| PM | c3 | Desolation Ridge | Desolation Ridge East / West | 3328 × 4096 | **Volcanic, rocky** | Ambient 20. Black fog and an orange cloud glow below. Ground: grey cracked rock, red dirt, lava craters. Scenery: crashed dropship, pirate base, power generators. |
| PM | c4 | Madre, the Crater | Madre, the Crater East / West | 4096 × 4096 | **Arid crater** (desert or volcanic) | Ambient 30. Dark brown fog. Ground: red-orange dirt, gravel and pebbles, with water. Scenery: dome base, warehouses, tank farm. |

**No arctic map exists in Instant Action or multiplayer.** The game's only
snow world is MW3's `c4b`: operation 4's missions 4 to 6. Its `world.zrd`
has SNOW weather, ambient 2 in m4 and 10 in m6, and snow ground textures
(`gter01`, `cratersnow`, soil Snow). It is 5120 × 6400 m. It has no
`readeria`/`readermp` archives, and its m5 block is the lava mission
(ambient 40, red fog). See the blockers below for what an arctic IA map on
c4b would need.

## MW3 1.2 maps

What all of MW3's reachable IA maps share:

- **Player side.** `aiv.zrd` holds the player, three lancemates (Dominic
  Paine, Epona Rhi, Alan Mattila; c4 ia3 calls them Lancemate_01..03), and
  three mobile field-base spots, `mobilehq_01..03`, on team 1.
- **Enemy side.** `Enemy_01..08` (or 09 or 12) are on team 2. Each has its
  own AI net and a mech in its record.
- **Who appears.** The mechs you pick on the Action screen take these
  spots. Each pick takes an unused spot on its side whose record mech is
  the same weight class, else the nearest class. The spot is random at the
  start, and later waves take the spots farthest from you
  (`external/mw3-re/docs/discoveries.md`, "Instant Action matches each mech to a start
  point by weight class").
- **Nav points.** These are in the world's `reader.zbd` as
  `waynet_ia1..3.zrd`. ia1 has only Start. ia2 and ia3 add Able, at the
  tower.
- **Objectives.** These are in the world's `reader.zbd` `objectives.zrd`,
  blocks ia1..ia3:
  - every block fails you for leaving the boundary (`outside_ia*`);
  - ia2's primary is "destroy the tower" (`valve_change` iatowv2, with nav
    Able);
  - ia3 fails you if the tower falls.
- **Static field bases.** MW3 has no `static_mfb` (it is a PM key).

The tower is `iatower` in `targets.zrd`: team 2 in ia2, team 1 in ia3, with
200 or 300 health. It is the one `destroy_entity` target in MW3's IA data.
`destroy_entity` can also name any AI mech or vehicle record by name. The
Battle archives (ia1) have no `targets.zrd`.

MP maps have no AI and no targets:

- 17 spawn spots (`player`, `remote_0..15`);
- two MFB stations as nav points (`mfb_station_01/02` or `_03/04`);
- c1 mp1 also has `mobilehq_01..03`;
- objectives are only a hidden boundary fail.

### c1, Big Lake (Beachhead, River Valley)

| Map | Mode | Start | Nav points | Enemy spots (team 2) | Targets | IA boundary |
|---|---|---|---|---|---|---|
| ia1 | Battle | (−389, 1, −1626) | Start | 9: two on high ground NE (133..201, 32, −886..−856), three S (−744..−236, 3, −2438..−2294), two SE by the river (518..524, −4, −2208..−2137), two far W (−1408..−1354, 3..8, −1799..−1754) | none | x −2325..3133, z −4132..300 |
| ia2 | Attack | (−389, 1, −1626) | Start, Able (392, −7, −2176) | 8 | iatower (enemy) | same |
| ia3 | Defend | (252, −7, −2166) | Start, Able (392, −7, −2176) | 8, around the tower | iatower (yours) | same |
| mp1 Beachhead | MP | (−3201, −36, −1225) | mfb_station_01 (−3104, −31, −1502), mfb_station_02 (−1600, 3, −1600) | 17 spawns, x −3346..−1211 | none | |
| mp2 River Valley | MP | (265, 33, −825) | mfb_station_03 (317, 19, −1219), mfb_station_04 (−448, 4, 64) | 17 spawns | none | |

Field bases in IA: (−505, 3, −1660), (−486, 3, −1712), (−536, 2, −1660);
in ia3, near (415, 0, −2363). Named AI regions in ia1: `reg_base` (centre
242, −2107), `reg_outpost` (−866, −1535), `reg_water`.

**Reach points:**

- the tower base, nav Able (392, −7, −2176), 955 m from the Start;
- the NE high ground (167, 32, −871), 940 m away. This is a ridge spot.

### c2, Grasslands (Lake Bed, The Villa)

| Map | Mode | Start | Nav points | Enemy spots | Targets | IA boundary |
|---|---|---|---|---|---|---|
| ia1 | Battle | (−730, 3, −1086) | Start | 8: NW pair (−1666..−1611, 2, −264..−198), SW pair (−1823..−1743, 0..2, −1945..−1874), N (−427, 3, −530), W (−1777, 0, −310), S (−405, 9, −1642), one on a rise (−321, 26, −1267) | none | x −3351..1250, z −4185..1415 |
| ia2 | Attack | (−291, 9, −1693) | Start, Able (−904, −3, −1156) | 8 | iatower | same |
| ia3 | Defend | (−755, 4, −1088) | Start, Able (−904, −3, −1156) | 8 | iatower (yours) | same |
| mp1 Lake Bed | MP | (987, 0, −446) | mfb_station_01 (832, 0, −448), mfb_station_02 (−64, 0, −960) | 17 spawns | none | |
| mp2 The Villa | MP | (−581, 4, −215) | mfb_station_03 (−832, −1, −64), mfb_station_04 (−2368, 3, −1344) | 17 spawns | none | |

Field bases in IA: about (−475, 3, −1130); in ia2, (−360, 8, −1850).
Region `reg_base` (centre −899, −1179) is the tower's base, which is only
190 m from ia1's Start.

**Reach points:** for ia1, the NW pair's spot (−1638, 2, −231), 1250 m; the
SW spot (−1783, 1, −1910), 1340 m. For ia2, Able, 815 m.

### c3, Rolling Hills (Dropship, Pyramids, Durgan City, Spaceport)

| Map | Mode | Start | Nav points | Enemy spots | Targets | IA boundary |
|---|---|---|---|---|---|---|
| ia1 | Battle | player (−1588, 0, −840); nav Start (−1483, 32, −801) | Start | 8: E (−616..−490, 0..4, −1255..−959), S (−1481..−1093, 0..2, −1766..−1594), others | none | region `reg_inside_c3m3` x −3438..2577, z −2759..1946 (ia1 reuses mission 3's regions) |
| ia2 | Attack | (−12, 4, −2931) | Start, Able (−975, 7, −3281) | 8 | iatower | `reg_inside_ia3` x −3742..1728, z −5280..−832 |
| ia3 | Defend | (−939, 6, −3067) | Start, Able (−975, 7, −3281) | 8 | iatower (yours) | same |
| mp1 Dropship | MP | (305, 0, −815) | mfb_station_01 (−1088, 7, −320), mfb_station_02 (448, 3, −704) | 17 | none | |
| mp2 Pyramids | MP | (−1090, 0, −2313) | mfb_station_03 (1088, 3, −3136), mfb_station_04 (−320, 8, −3008) | 17 | none | |
| mp3 Durgan City (1.2 only) | MP | (−692, 38, −4297) | mfb_station_01 (−553, 0, −5075), mfb_station_02 (164, 7, −4786) | 17 | none | |
| mp4 Spaceport (1.2 only) | MP | (1151, 10, −7546) | mfb_station_01 (959, 8, −7118), mfb_station_02 (1368, 8, −6858) | 17 | none | |

Field bases in IA: near (−1230, 22, −970) in ia1; (−100, 9, −2790) in ia2;
(−680, 4, −3270) in ia3.

**Reach points:** for ia1, the east spot (−490, 0, −1167), 1060 m; the
south spot (−1361, 2, −1724), 930 m. For ia2, Able, 1025 m.

### c4, Gorge (Train Station, The Gorge)

| Map | Mode | Start | Nav points | Enemy spots | Targets | IA boundary |
|---|---|---|---|---|---|---|
| ia1 | Battle | (264, 2, 1885) | Start | 12, spread over the map, two on high ground: (1580, 52, 1235) and (−934, 39, 1938) | none | x −2298..2378, z 126..2785 |
| ia2 | Attack | (264, 2, 1885) | Start, Able (1163, −5, 887) | 12 | iatower | same |
| ia3 | Defend | (1020, −1, 934) | Start, Able (1163, −5, 887) | 12, including a pair on high ground (2069..2164, 50..65, 1445..1632) | iatower (yours) | same |
| mp1 Train Station | MP | (−1193, −20, 3716) | mfb_station_01 (−1472, −26, 4032), mfb_station_02 (1472, 2, 2112) | 17 | none | |
| mp2 The Gorge | MP | (−654, 2, 155) | mfb_station_03 (1619, 0, −824), mfb_station_04 (−960, −5, −1600) | 17 | none | |

Field bases in IA: (178..200, 0, 1985..2034); (−190, −4, 1600) in ia2;
(1000, −2, 850) in ia3.

Named regions:

- `reg_village`, centre (388, 1485);
- `reg_upriver`, centre (−152, 1065);
- `reg_base`, centre (1112, 873), the tower's base.

Two IA enemies' order lists are missing their `vlist` label
(`external/mw3-re/docs/discoveries.md`, "Two Instant Action enemies forgot their orders").

**Reach points:** for ia1:

- the base at nav Able (1163, −5, 887), 1340 m;
- the east ridge (1580, 52, 1235), 1470 m;
- the west ridge (−934, 39, 1938), 1200 m.

### t1 (training, not reachable)

`readeria1` and `readermp1` hold only the player and stealth at
(1408, 16, −893). The world is 3584 × 2816 m, coastal, with rain and
ambient 31.

## Pirate's Moon maps

What PM's maps share:

- **Player side.** `aiv.zrd` holds the player, `pirate_mate_1..3` and
  `mobilehq_01..03`.
- **Enemy side.** ia1..ia3 have `Enemy_01..12`, whose records name the
  mech `notmech`, so the Action screen fills them. ia4 names its own
  vehicles and mechs.
- **Nav points.** These are in the IA archive itself, as `waynet_ia<n>.zrd`,
  not in the world's `reader.zbd` as in MW3.
- **Objectives.** These are in the world's `reader.zbd` `objectives.zrd`.
  PM has blocks ia2, ia3 and ia4 only, **no ia1 block**:
  - ia2's primary is "Destroy Enemy Base" (nav Able);
  - ia3 fails on "Failed to Protect Allied Base" (nav Able).
- **Boundaries.** PM's IA archives hold no boundary region files and no
  boundary objective. PM's in/out areas cover IA (`external/mw3-re/docs/pm-map.md`), but
  the port's in/out is still MW3's (same doc). Whether a PM IA map has a
  working boundary in the engine is unconfirmed.
- **`world.zrd`.** It has only a `defaults` block on c2..c4 (c1 adds m2,
  m5 and sa4). The reader takes `defaults`, then any mission block
  (`external/mw3-re/docs/pm-map.md`).
- **Targets.** Unlike MW3, most PM IA maps have `targets.zrd` buildings, in
  Battle too.

**Static field bases.** All 12 of PM's `static_mfb` nodes are on c1. One
node, "Static MFB" (auxname `starbase_refit_activate`), sits at
(4733, 28, −4200). It appears in:

- the net files of ia1..ia4 and mp1..mp2;
- the campaign's m1, m2 and m5;
- standalone missions sa1..sa3.

It is the starbase refit point (`external/mw3-re/docs/pm-tests.md`, "The starbase refit").
No other PM world has one.

MP maps:

- 41 spawns each;
- an Able and a Baker base, each a `targets.zrd` building with
  `mpoption` 1 or 2 and 500 health;
- nav points Able and Baker (c1 adds Bridge);
- no field-base spots.

### c1, Canyon's Edge

| Map | Mode | Start | Nav points | Enemy spots | Targets (destroy_entity) |
|---|---|---|---|---|---|
| ia1 | Battle | (3642, 22, −1796) | Start | 12, spread W and SW to (323, 23, −3579) and (2295, 24, −4327) | none |
| ia2 | Attack | (3842, 16, −1068) | Start, Able (4477, 25, −4486) | 12, clustered around Able | `hitme_twr` (dome core), `dprt01..` (dome shields) |
| ia3 | Defend | (728, 18, −2629) | Start, Able (567, 2, −2466) | 12 | `pship_kill` (the pirate base, yours) |
| ia4 (unreachable) | scripted | (2195, 20, −1970) | Start, Able (1676, 1, −1431), Baker (2820, 0, −3099), Charlie (567, 2, −2466) | convoy trucks, bulldogs, a Centurion, mechs | `sbrg01`, `sbrg02` (canyon bridges), `pirate_ship` |
| mp1 East | MP | (4403, 28, −4263) | Able (4477, 25, −4486), Bridge (3184, 23, −3429), Baker (533, 1, −2515) | 41 spawns, x 489..4837 | hitme_twr, pship_kill as bases |
| mp2 West | MP | (4441, 28, −4351) | Able and Baker swapped, Bridge | 41 | same, swapped |

Field bases in IA: ia1 (3160..3237, 23, −1773); ia2 (3730..3781, 3..7,
−876..−841); ia3 (850..903, 2..12, −2557..−2531).

**Reach points:** for ia1:

- the bridge, nav Bridge from MP (3184, 23, −3429), 1700 m;
- the starbase refit point (4733, 28, −4200), 2640 m;
- the dome base at ia2's Able (4477, 25, −4486), 2820 m.

### c2, Road To Madre

| Map | Mode | Start | Nav points | Enemy spots | Targets |
|---|---|---|---|---|---|
| ia1 | Battle | (907, 12, −661) | Start | 12, mostly E and S | `3twr1` (pump complex) |
| ia2 | Attack | (907, 12, −661) | Start, Able (1153, 6, −3434) | 12, around Able | `portalgate` |
| ia3 | Defend | (1792, 8, −734) | Start, Able (1821, 8, −646) | 12, in a ring to the S and W | `tcab05`, `tcab06` (germanium transports), `mp_tent_campb` (HQ) |
| ia4 (unreachable) | scripted | (612, 16, −1300) | Start, Able (1834, 8, −759), Baker (1938, 6, −2174), Charlie (1153, 6, −3434) | Awesomes, Centurions, Masakari, Daishi, Mad Cats | `tcab03`, `portalgate` |
| mp1 East | MP | (931, 6, −3450) | Able (1804, 8, −687), Baker (1119, 6, −3473) | 41 | mp_tent_campb, portalgate |
| mp2 West | MP | (931, 6, −3450) | swapped | 41 | swapped |

Field bases in IA: about (920..960, 9..11, −472..−437); ia3 (1845..1914,
9..12, −517..−507).

**Reach points:** for ia1:

- the portal gate, nav Able from ia2 (1153, 6, −3434), 2780 m;
- the south camp, nav Baker from ia4 (1938, 6, −2174), 1830 m.

### c3, Desolation Ridge

| Map | Mode | Start | Nav points | Enemy spots | Targets |
|---|---|---|---|---|---|
| ia1 | Battle | (1684, 8, −1016) | Start | 12, wide: E high ground to (3451, 41, −1654), SW to (378, 31, −2925) | `xslid01..03` (land bridges) |
| ia2 | Attack | (1771, −4, −899) | Start, Able (683, 10, −3202) | 12, around Able | `mpbbase`, `pspwrsrc01` (power generators) |
| ia3 | Defend | (687, 10, −3159) | Start, Able (683, 10, −3202) | 12 | same, as yours |
| ia4 (unreachable) | scripted | (755, 10, −3161) | Start, Able (1696, −4, −960) | Sunders, Orions, Ryoken, Puma, Daishi | `mpbbase`, `pspwrsrc` |
| mp1 East | MP | (1778, −4, −870) | Able (682, 10, −3193), Baker (1708, −4, −956) | 41 | pspwrsrc01, mpbbase |
| mp2 West | MP | (1778, −4, −870) | swapped | 41 | swapped |

Field bases in IA: (1725..1782, 1..4, −1049..−1040); ia3 (647..752, 27..30,
−2778..−2711).

**Reach points:** for ia1:

- the generator base, nav Able from ia2 (683, 10, −3202), 2400 m;
- the east ridge (3451, 41, −1654), 1880 m.

### c4, Madre, the Crater

| Map | Mode | Start | Nav points | Enemy spots | Targets |
|---|---|---|---|---|---|
| ia1 | Battle | (2544, 0, −3024) | Start | 12, W and N, one on high ground (3530, 45, −2381) | `modA140`, `modA141`, `modA340`, `modA341`... (support buildings) |
| ia2 | Attack | (1276, 34, −818) | Start, Able (2432, 0, −3005) | 12, around Able | `warehouse_target40`, `warehouse_target41` |
| ia3 | Defend | (2544, 0, −3024) | Start, Able (2432, 0, −3005) | 12 | same, as yours |
| ia4 (unreachable) | scripted | (1276, 34, −818) | Start, Able (925, 0, −2061), Baker (2432, 0, −3005) | Awesomes, Avatar, Thor, Cauldron-Borns, Mad Cats, Atlas | `warehouse40`, `warehouse41`, `warehouse30`, `warehouse31` |
| mp1 East | MP | (2166, 20, −3319) | Able (598, 0, −2388), Baker (2232, 0, −3441) | 41 | modA141, modA130 |
| mp2 West | MP | (2166, 20, −3319) | swapped | 41 | swapped |

Field bases in IA: (2173..2248, 0, −3505..−3496); ia2 (1232..1260,
24..25, −1142..−1100).

**Reach points:** for ia1:

- the west HQ, nav Able from ia4 (925, 0, −2061), 1880 m;
- the east high ground (3530, 45, −2381), 1180 m.

For ia2, the warehouses at Able (2432, 0, −3005), 2470 m.

## MW1's climates and the best maps

| MW1 climate | Best fit | Next best | Notes |
|---|---|---|---|
| **Desert** | PM c2, Road To Madre (orange dirt, sandy haze, ambient 30) | MW3 c1, Big Lake (sand and beaches, ambient 40); PM c4, Madre, the Crater; MW3 c4, Gorge (arid canyon) | Several maps fit. PM's are PM's own worlds and need nothing from MW3. |
| **Tropical** | MW3 c2, Grasslands (green grass, moss, mud, haze) | MW3 c3, Rolling Hills (green, but night) | **No real jungle.** PM has no green world at all. MW3 c2 is the only green daytime map. |
| **Arctic** | none | MW3 c4b (the only snow world) after an IA map is made for it; MW3 c4's snowcaps are not enough | **No IA or MP map is arctic.** |

PM c1 and c3 (volcanic badlands) fit none of MW1's three climates well.
They could pass as desert at a stretch, or serve as extra variety.

## What could block a nav point and a reach_area in Instant Action

1. **The objectives are there: IA reads `objectives.zrd`.** The mission
   load reads block `ia<n>` of the world's `objectives.zrd`
   (`external/mw3-re/docs/mission-load.md` section 3; `external/mw3-re/docs/objectives.md`). Every MW3
   world has ia1..ia3. MW3's ia2 already uses a nav point (Able) in a
   `valve_change`. PM has ia2..ia4 but no ia1, and a block can simply be
   added. So **IA worlds are not missing the campaign's objective files.**
   - The block lives in the world's `reader.zbd`, which the campaign
     missions on that world share. An edit or a mod's override replaces
     the whole `objectives.zrd`, every block in it.
   - Whether a copy in `readeria<n>.zbd` would win over `reader.zbd`'s is
     unconfirmed. The objectives are found by the reader's depth-first
     search over the mounted archives.
2. **Nav points are per map.** MW3 keeps them in the world's `reader.zbd`
   (`waynet_ia<n>.zrd`); PM keeps them in `readeria<n>.zbd`. Battle (ia1)
   has only Start, so a new nav point means editing that waynet.
   - `reach_area` also takes `point (x y z)` or a world `node` with no nav
     point at all, but then the HUD shows no nav marker.
   - `external/mw3-re/docs/objectives.md` still says nav points aren't wired into the
     objectives. That is out of date: `src/mech3/app_mission.cpp`
     (nav_find, nav_show, nav_position) wires them. The campaign chain
     walks to c2m2's reach_area nav point (`src/frontend/sdl/campaign_chain.cpp`).
   - reach_area by nav has not been run in Instant Action itself
     (unconfirmed).
3. **Reach on its own cannot win an IA mission as the data stands.** In
   every IA mode, the engine adds its own "destroy the enemies" group to
   the primary objectives (0x0045c4a0, `add_enemy_objectives` in
   `src/mech3/mech_objective.cpp`). A win needs every primary objective
   done, and then the `final` condition (`external/mw3-re/docs/mission-frame.md`). So a
   reach_area in primary or final is *added to* "kill them all". It does
   not replace it.
   - VISION's "reach a point to get out" needs an engine change, for
     example a per-mission flag that skips that group.
   - Even after a win, the mission runs on until Exit Mission
     (`external/mw3-re/docs/discoveries.md`, "Winning doesn't end the mission by itself").
4. **Wave modes reload the block.** Battle Waves and Progression free and
   reload `ia<n>` after each wave. Use plain Battle, Attack or Defend for
   contracts.
5. **Boundaries.**
   - MW3's IA blocks fail you outside `reg_inside_*`, so keep reach points
     inside the boundaries listed above.
   - On PM's IA maps, whether a boundary works in the engine is
     unconfirmed (see above).
6. **`destroy_entity` finds only named entities:** AI mechs and vehicles by
   their `aiv.zrd` names, turrets, and `targets.zrd` buildings by node name.
   A name it can't find counts as destroyed at once (`external/mw3-re/docs/objectives.md`).
   MW3's Battle archives have no `targets.zrd`, so a building objective
   there needs a new `targets.zrd` entry. Most PM IA maps already have
   some.
7. **MW3's maps under a PM-based engine (unconfirmed).** The remake runs on
   PM's code and data.
   - MW3's c1..c4 have the same folder names as PM's c1..c4, which are
     different worlds. A mod world cannot reuse one of the game's nine
     names (`external/mw3-re/docs/modding.md`, "New worlds and words"), so an MW3 world
     would need a new folder name and its own `.gs` script.
   - The mechs those worlds' `aiv.zrd` records name are MW3's.
   - Nobody has tried an MW3 world under the `mw3pm` target.
   - Since the tropical stand-in (MW3 c2) is an MW3 world, this matters.
8. **An arctic map means building an IA map for c4b.**
   - `ia_world=` can put any world on Instant Action's list
     (`external/mw3-re/docs/modding.md`).
   - c4b has no `readeria<n>.zbd`, and its `reader.zbd` has no `ia` blocks
     in `objectives.zrd` or `world.zrd`. All of these would need writing:
     `aiv.zrd` spots, nets, a waynet, valves, regions, and a `world.zrd`
     block with the snow weather.
   - c4b is an MW3 world, so point 7 applies too.
9. **PM's `readeria4` maps are not on the Action screen.** They are
   scripted missions with their own objectives and nav points. If one
   looks right for a contract, its objectives would have to move into a
   reachable ia1..ia3 block, or the scenario table would need a new row.
