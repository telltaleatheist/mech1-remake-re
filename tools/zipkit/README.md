# zipkit

Converts MechWarrior 3's files to Pirate's Moon's formats, so PM's engine (and
games built on it, such as MW1: Critical Hit) can load MW3's worlds. It reads
the player's own MW3 install and writes only under the folder it is given.
Nothing from either game is in this repository.

This is the first, rudimentary version: what MW1 needs. A fuller tool that also
reads and writes mech3ax's JSON/ZIP form for modders will come later
(`docs/worklogs/zbd-tool-research.md`, not in git).

## Use

    zipkit mw3-to-pm --install <MechWarrior 3 folder> --out <folder> --world c2:mw3c2 [--world c4b:mw3c4b]
    zipkit --version

- `--install` must be MechWarrior 3's install (its `zbd\reader.zbd` a
  version-1 archive). Pirate's Moon's folder, or anything else, is refused and
  nothing is written. 1.0 and 1.2 have the same formats; both are taken.
- `--world <mw3 world>:<new name>`: each MW3 world, under a new name. Names are
  lower-case letters, digits and `_`. The new name must not be one PM ships
  (a loose file whose name is in PM's archives makes the game refuse them).
- Progress goes to stdout, one line a step (`c2: gamez.zbd 2/19`), then `done`.
  An error is one plain line on stderr, with exit code 1 (2 for a bad command
  line).
- The library (`zipkit.h`, target `zipkitlib`) has the same run as
  `zipkit::mw3_to_pm(options, &error)`, with a progress callback. It never
  exits the program.

## What it writes

Laid out as `zutl::mount(<out>)` serves it:

| Output | From | How |
|---|---|---|
| `zbd/<new>/gamez.zbd` | `zbd/<w>/gamez.zbd` | converted, version 27 to 41 (below) |
| `zbd/<new>/anim.zbd` | `zbd/<w>/anim.zbd` | converted, version 39 to 50 (below) |
| `zbd/<new>/reader*.zbd` | the same | re-tailed: zArchive version 1 to 2 (below) |
| `zbd/<new>/texture*.zbd`, `rtexture*.zbd` | the same | copied: both games read one image-library format |
| `<new>.gs` | `<w>.gs` in `zbd/interp.zbd` | decoded to text; `set DB <w>` becomes `set DB <new>` |
| `support/<new>.gw` | `Support\<w>.gw` in `zbd/interp.zbd` | decoded to text |
| `zipkit.stamp` | | zipkit's version, each source's size and CRC, what it made |

- The other scripts a `.gs` sources (`tolerances`, `common`, `compile`,
  `commonpostload`, `write`) come from PM's own `interp.zbd`. Under `USEZBD`
  only `GameZReadZBDFile %GAMEZ%` of the world block runs.
- c2's `support\c2.gw` cycle lines give one "SetCycleTextureSpeed: Texture
  not cycled" in PM, and none in MW3. It's harmless. PM's speed setter
  (`pm::zmod::cycle_set_speed`) sets every distinct material of the model,
  where MW3's set only the first. One of c2's cycled models also has a plain
  material, and PM reports that one. The cycled material still gets its
  speed.

- The turrets' table (`aiTurrets.zrd`) is the game's own, in the top-level
  `zbd\reader.zbd`, which is searched before a world's archive, so a
  converted world runs PM's table. PM's turrets name destroy animations
  (`destroy_tur_fixed`, `destroy_tur_mobile`) that MW3's worlds don't have;
  theirs are `destroy_tur_generic`, `destroy_tur_gauss1` and so on. On
  `mw3c4b`, `tur_ult01..04` die without their animation. A game that puts
  turrets on a converted world names the world's own animations in its
  turret records (MW1, 2026-11-17: its own data, later). zipkit stays a plain
  converter.

Rules for the folder:
- Everything is built in `<out>/.zipkit-tmp` and moved into place only when
  every step has succeeded, so a failed run never leaves half a world.
- A world folder is replaced only if it holds zipkit's marker (`.zipkit`), and a
  script only if the last stamp lists it. Anything else under a name zipkit
  would write stops the run, and it is left alone.
- The same install gives the same bytes: the world's header time stamp
  (+0x08) is the source file's CRC.

## The formats

### zArchive, version 1 to 2

The entries (0x94 bytes each) and their data are unchanged. MW3's tail
`{1, count}` becomes PM's `{2, count, crc}`. The CRC is `zutl_zar::crc32` over
each entry's data in table order (PM's 0x00597a30). PM's exe checks it only on
the top-level `zbd\reader.zbd`; it is written right anyway.

### gamez.zbd, version 27 to 41

MW3's reader (`zcls::read_zbd`, 0x004ff520) fills the shared pools, and PM's
writer (`pm::zcls::write_zbd`, PM's 0x005168e0) writes them. In between, the
converter fills what PM's files have and MW3's do not. Each rule was checked
on PM's shipped worlds.

- **Models.** Each MW3 model is copied into PM's model pool at the same index.
  The polygons' flags are remapped: vertex count 0xff to 0x3ff, back face
  0x100 to 0x400, normals 0x200 to 0x1000. Each polygon gets one material and
  its UVs. A type-0 model with polygons gets flag 0x80, as all 1,696 such
  models in PM's c2 have; without it PM's Direct3D path would strip the model.
  The polygons keep MW3's order. PM's writer would sort them by material
  (0x0055c470), but the ground query takes a model's first polygon under a
  point, so a sorted model stands mechs on other faces: 43 of 133,356 points in
  c2 and c4b did, one of them a building's side wall at 13.0 m instead of its
  roof at 18.85 m. zipkit gives the writer its refs as runs in MW3's order
  (`pm::zmod::write_hooks().refs`, ours; unset, the writer sorts as the exe
  does). A material can own several runs, and a cycled material's polygon is a
  run of its own, as the sort makes it.
- **Nodes.**
  - The live bit PM's writer writes by (0x2000000) is set, and the model
    becomes PM's slot.
  - The box index (+0x70) comes from flags 0x700: 0b011 is the model's box,
    0b101 the children's, otherwise the node's own. This holds on every node of
    PM's c2 and c4.
  - The virtual cell is the area cell when one is set, else 0, 0, as on every
    node of PM's c2. Four nodes of PM's c4 differ; MW3's worlds have no
    equivalent.
  - The free slots (about 14,000 in c2) are compacted out by the writer.
- **World.** PM's second area is the area widened by the margins (+0x80,
  +0x84) each way (cls_world, set_area2).
- **Partition y ranges.** Each entry's range is the chosen box's y, raised by
  its own and its parents' translations (object3d flag 0x20, +0x58). This
  holds on all 1,207 entries of PM's c2 and c4.
- **Light.**
  - The flags map MW3's 0x200 bit to PM's 0x800. MW3's sun 0x38b becomes PM's
    0x98b, as PM's c2 has for the same script lines. A light other than MW3's
    sun is counted and reported.
  - The colours are computed: the colour, diffuse times colour, ambient times
    colour, and the last two summed.
- **Materials.** A coloured material's packed colour (+0x02) goes back to 0,
  as both games' files hold it; MW3's reader computes it at load. The texture
  entries' +0x24 is 0, as PM's files have it.
- **Still open:**
  - PM's polygon flags 0x4000 and 0x800 are left clear. PM's c2 sets 0x4000 on
    741 untextured polygons, and what reads it is not known yet.
  - MW3's material flag 0x02 (33 in c2) is kept. PM's files never set it.

### anim.zbd, version 39 to 50

Converted byte to byte (`anim.cpp`): MW3's file walked as docs/anim.md has
it, PM's written as PM's reader takes it (`pm/gamez/zeffect/zeff_anim_init.cpp`).
Each rule was checked on both games' shipped files.

- **Header and block.** PM's header adds a time stamp (the source's CRC here).
  Its 0x6c block holds MW3's 0x44 fields where the reader's field table puts
  them. PM's own fields are 0, as in every shipped PM file, and the script
  count is set.
- **Defs.** 0x13c bytes to 0x10c: MW3's +0x20..+0xc3 at +0x28, the counts
  at +0xd8, +0x138 at +0x108. There is no node path and no second sequence. The
  reset sequence is written only when it has events, and its pointer (+0xd0)
  says so, as in PM's files.
- **Object and node refs** (0x60 to 0x5c, 0x28 to 0x2c). They gain PM's flags
  and parent. The parent is 0, as everywhere in PM's files. The local bit
  (objects 2, nodes 1) is the def's own "local nodes only" (MW3's def flag
  0x80000, which MW3's lookup reads). MW3's two object flag words are dropped,
  as PM's record has no room for them.
- **Events.** Five records changed between the games; every other one is
  copied:
  - 4 LIGHT_STATE (0x84 to 0x98): PM's light map, own colours and ambient
    colour are added, and the rest moves.
  - 12 OBJECT_MOTION_SI_SCRIPT: MW3's key frames move into the file's scripts
    (shared when identical), and the event keeps the script's index.
  - 25 STOP_ANIMATION: PM's two cached indices start at -1.
  - 31 IF and 33 ELSEIF (0x18 to 0x1c): PM's upper bound is 0.
  - 42 PUFFER_STATE (0x250 to 0x254): PM's priority is 0, inserted at +0xc0.

  None of PM's new flags appear in MW3's data. IF, ELSE and the sequence walk
  go by each event's size, so the size changes are safe.

### mech3ax

mech3ax's PM reader rejects any world PM's writer writes, PM's own included.
PM's exe wrote its memory's pointers into the files (the polygons' arrays, the
material refs, the cycles' texture lists). Neither game reads them back, and
our writer writes 0 there, but mech3ax checks that they are set. So it is not
used as the gate. PM's own readers are, in the tests.

## Tests

`tests/test_zipkit.cpp` in `mw3pmtests` (`MW3_TEST=zipkit`):
- every reader archive of both games re-tailed and checked by PM's table
  reader and CRC;
- c2 and c4b converted twice (the same bytes) and read back by PM's reader,
  with the rules above checked on what it read;
- anim.zbd on all six MW3 worlds (92,557 events): each changed event holds,
  where PM's handler reads it, what MW3's handler read; the key frames moved
  whole; every file walked to its last byte;
- the ground query every 20 m over c2 and c4b, PM's reading of the converted
  world against MW3's of its own: the same nodes, heights and normals at all
  133,356 points;
- the run's folder rules, with the install's files unchanged;
- `mw3c2` mounted as a mod world (`mod.txt`: `world=`, `ia_world=`) and played
  headless as Instant Action in wave mode (`test_pm_mission.cpp`): 600 frames,
  the wave's enemy made, the player on the ground, no full hit list, all
  1,422 animation defs read with none corrupt, and the 19 that start
  themselves running.
