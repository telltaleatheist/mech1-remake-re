# The travel map: what MW1 draws, and a plan (2026-11-17, for the mw1 lead's review)

The owner: travel goes through a star map, not the list. Clean it up and
modernize it, drawn by our code from the worlds' data (`kPlanets`), not
MW1's bitmap. Sources, read as reference (never copied): the port's
`MW_MAIN_081B_TRAVEL.c`, `MW_MAIN_081B_RESOURCES.c` and `MW_EGA_0800.c`;
and the player's MW_1PICS.BIN, decoded read-only for this note by a
throwaway script (not in the repo).

## What MW1's map is

**Picture 1 of MW_1PICS.BIN.** MW1 reads the whole file and undoes a nibble
run-length code: low nibble first, with 0xF, count−1 and the nibble for a
run. The result is a bank of 4-bit packed sprites; each has a width and
height word, then two pixels a byte, with 0 transparent. Picture 1 is
320×200, the whole travel screen. `MwOpenTravelScreenContinuation` draws it
at (0,0), then the current House's crest (MW_PICS.BIN, picture 0x25 + the
House) at (0xEF,0x21).

**Everything but the names is baked in:**

- **The frame and the panels.** This includes the "Cost" and "Wealth"
  labels and the crest's box.
- **The five Houses' territories.** Each is a solid fill with a dark
  outline and a drop shadow.
- **A dot for every world.** All 145 `kPlanets` positions land exactly on
  a dot pixel (colour 1), each inside its own House's fill (checked by
  script):

  | House | Fill (EGA index) | Worlds whose surroundings are that fill |
  |---|---|---|
  | Kurita | 9 (light blue) | 30 of 30 |
  | Steiner | 6 (brown) | 28 of 30 |
  | Marik | 15 (white) | 29 of 30 |
  | Liao | 12 (light red) | 25 of 25 |
  | Davion | 13 (light magenta) | 26 of 30 |

  The few exceptions are worlds on a border, where the box around them
  takes in the neighbour's colour.
- **A small square at each House's capital.**

**What isn't baked in:** names, the cursor, the route. The marker is
picture 13 of MW_GPICS.BIN (not decoded here; that bank loads another way).
It moves within x 6..0xDA, y 0x2A..0xBB. A world is picked when its
position falls in the marker's 4-pixel box. The route is a white line. A
name search (081B:5C5D) moves the marker to the typed world.

**Coordinates.** The positions are the picture's own pixels: x 13..215,
y 49..179, so the worlds span about 203×131 MW1 pixels. MW1's pixels are
not square: 320×200 shown at 4:3 makes each one 1.2 times as tall as wide.
The travel distance scales y by 32/20 (`kMapYScaleNum/Den`), which is a
different factor. We should draw at the display aspect (what MW1's players
saw) and keep the distance rule as it is.

**No Clan worlds.** MW1's map is the five Houses' worlds only (3025).

**PM has no star art.** Its image archive has no starfield, galaxy or map
picture. The backdrop is ours to generate, or plain.

## The plan

### Drawing it (zTwoD)

The engine's 2D layer has what we need: images by name, filled boxes, box
outlines (Frame), lines (Line2D) and polylines. One fixed part and one live
part:

- **The backdrop (fixed).** We render it at build time from `kPlanets` and
  pack it into mw1image.zbd, as the intro picture is now. mw1imgpack would
  link `mw1_travel.cpp`. It holds:
  - the territories: each pixel takes the House of its nearest world,
    within a radius, so the Houses come out as blobs like MW1's, with
    darker edges where two Houses meet;
  - a faint, seeded starfield behind them.

  It's our code and MW1's data, with no MW1 pixels. We'd add one variant
  per House with the other Houses dimmed: six 440×330 images at 565, about
  290 KB each.
- **The worlds (live).** A small House-coloured box per world, so the
  filters can change them without new art. Worlds with an office get a
  slightly larger box or a ring; a capital gets a square, as MW1's.
- **Markers.**
  - The current world: a steady ring.
  - The selected world: a brighter ring.
  - Between them, an optional jump line (MW1 drew one, in white).

### Scale

The whole map fits at once, so there's no zoom or pan. A map area of about
440×340 on the 640×480 screen gives about 2.1× horizontally and 2.5×
vertically (MW1's aspect). The worlds would average about 30 pixels apart,
with the dense spots closer. That's too tight to name every world, so names
show for:

- the current world;
- the selected world;
- the world under the mouse;
- the five capitals.

A zoom could come later if the owner wants all the names.

### Interaction

- **Mouse:** click a world to select it, hover to see its name. One map
  control over the map area takes hit tests and hover through the engine's
  Control (`hit`, `on_enter`/`on_leave`, `on_left_up`). A click selects the
  nearest world within a few pixels.
- **Keyboard:**
  - the arrows step to the nearest world in that direction (within a 90°
    cone, nearest by distance weighted toward the axis);
  - Enter is GO, Escape is BACK;
  - Tab steps through a short list of the nearest worlds.

  The arrows need the frame's key (`input_frame_key`) read by the travel
  state. To confirm, but it's likely no engine change.
- **The detail panel stays,** on the right: name, House, tier, office, the
  cost, the arrival date and the warnings (stranded, refusal, can't
  afford). GO and BACK stay.

### Filters (replacing HOUSE)

One FILTER button cycles through:

- **all;**
- **one House:** its territory lit and the others dimmed (the six
  backdrops);
- **offices only:** worlds without an office greyed;
- **affordable:** worlds out of reach greyed;
- **will deal with you:** a refusing House's worlds greyed.

Clan worlds don't exist on MW1's map.

### The list

The list goes. A name search replaces it, as MW1 had: typing letters jumps
the selection to the first matching world. It could use the engine's line
editor (the pilot screen's edit bar) or a simple type-ahead with no box. I
propose type-ahead: it's cheaper, and it needs no new widget.

### Tests

- **Rules (pure).**
  - Hit testing: every world is pickable at its own position, and a click
    between worlds goes to the nearest.
  - Keyboard stepping: from each world, every arrow either finds a world
    in that direction or stays put; stepping never jumps backwards.
  - The type-ahead.
  - The filter states.
- **The backdrop.** The generator puts every world inside its own House's
  colour, the same way each time (a checksum).
- **Headless.** A trip picked on the map: click a world, check that the
  panel shows it, GO, then arrive at the hub. Also the keyboard path.
- **A `--shot`** of the map (MW1_SCREEN=travel), for the owner.

### Questions for the owner (look and feel)

1. **Colours:** MW1's House colours, or BattleTech's usual ones?
   - MW1's: Kurita light blue, Steiner brown, Marik white, Liao light red,
     Davion light magenta (EGA indices, before any palette change MW1 made;
     each House's fill checked against its capital's square on MW1's picture).
   - BattleTech's: Kurita red, Davion gold, Steiner blue, Liao green,
     Marik purple.
2. **The territories:** filled blobs as MW1's, outlines only, or no
   territory (dots only)?
3. **The backdrop:** a generated starfield, or plain black like the event
   panels?
4. **Names:** on hover, for the selected world and for the capitals (my
   proposal), or always where they fit?
5. **The jump line:** on or off?
6. **Zoom:** fit-all only (my proposal), or a zoom for the dense areas?
7. **The list:** gone, with type-ahead search (my proposal), or kept as a
   second tab?

## A build order (each step mergeable)

1. The rules: hit testing, stepping, the filter states and the type-ahead,
   pure and tested.
2. The backdrop generator in mw1imgpack, plus its test.
3. The map screen: the backdrop, the worlds, the markers, the panel, GO
   and BACK, and FILTER. The list goes. Headless trip, plus a `--shot`.
4. Keyboard stepping and type-ahead in the loop, headless.
