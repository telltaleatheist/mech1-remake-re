# Converting MW3's worlds to Pirate's Moon's formats

Research for `../VISION.md` (Combat: maps; `maps.md`, blocker 7), 2026-11-16.
MW1 runs on Pirate's Moon's engine, but its snow map (MW3 `c4b`) and its green
map (MW3 `c2`, Grasslands) are MechWarrior 3 worlds. PM's readers can't read
MW3's files, so the plan (the owner, 2026-11-16) is a one-time conversion at first
run. It reads the player's MW3 install, never writes to it, and writes each
world MW1 uses into mw1's own data folder under a new name (`mw3c2`,
`mw3c4b`). The engine's readers stay as they are. This file measures how big
that converter is. It hasn't been built: the smoke test in `mw3-re`'s
`--fly` decides that (task 2).

**Sources.**
- The ported readers and writers in `external/mw3-re`: MW3's under
  `src/gamez/`, PM's under `src/pm/gamez/`.
- mech3ax (Rust, a local checkout), as a format reference only.
- A byte survey of the real files: MW3 1.2 `c2` and `c4b`, PM `c2` and `c4`.
  Each parse ended exactly at the end of the file.

Paths below without a prefix are under `external/mw3-re/`.

## Why the engine can't read them as they are

Each game's readers are picked once per run through its game table
(`src/gamez/zgame/zgame_table.h`). Under PM's table an MW3 world fails at its
first archive.

| File | MW3 1.2 | Pirate's Moon |
|---|---|---|
| `reader*.zbd` (zArchive) | version 1 | version 2, with a CRC |
| `gamez.zbd` | version 27 (0x1b) | version 41 (0x29) |
| `anim.zbd` | version 39 (0x27) | version 50 (0x32) |
| `texture*.zbd`, `rtexture*.zbd` | the same image library | the same |
| `interp.zbd` (scripts) | version 7 | the same |

## The easy parts

- **zArchive v1 → v2.**
  - The entries are the same (0x94 bytes each, data back to back).
  - The tail `{1, count}` becomes `{2, count, crc}`.
  - The CRC is MSB-first, polynomial 0x04c11db7, starting at 0, no final
    xor, run over each entry's data in table order
    (`src/pm/gamez/zutil/zutl_zar.cpp`, `archive_crc`). Checked against PM
    c2's reader.zbd (0x406051b3).
  - PM's exe checks it only on the top-level `zbd\reader.zbd`; the port
    never checks it. We write the real value anyway.
  - The writer is in-tree: `pm::zar::write_table`. About 40 lines.
- **Textures:** copied unchanged. Both games share one image library reader
  (`src/gamez/zimage/zimg_library.cpp`); PM forks only how names resolve.
- **The records** (`world.zrd`, `aiv.zrd`, `objectives.zrd`, the waynets):
  copied unchanged inside the re-tailed archives. The binary zrd format is
  shared; PM forks only the loose-text parser.
- **The build script.** `interp.zbd` is the same format, and the
  interpreter is shared.
  - PM's `interp.zbd` has no script under the new name, so we write
    `mw3c2.gs` and `support\mw3c2.gw` loose, MW3's with `DB` renamed.
  - A loose copy of a name that is in the archive makes the game refuse the
    whole archive. A new name is fine.

## gamez.zbd, 27 → 41

The sections come in the same order in both versions.

| Section | MW3 → PM | Mapping |
|---|---|---|
| Header | 0x24 → 0x28: a time stamp added at +8 | Direct. PM never reads the node free head. |
| Textures | 0x28 → 0x2c each, 4 bytes in front | Direct |
| Materials | the same 0x2c records | Direct, but the cycles go in index order (MW3: used-list order). MW3 c2 has 33 materials with flag 0x02, which PM's files never use; its meaning is unknown. |
| Model | 0x60 → 0x68: a ref count and refs pointer added | Type-0 models with polygons get flag 0x80: PM sets it on 1,696 of 1,696 in c2. Without it PM's Direct3D path strips the model. MW3's clouds flag 0x20 never appears in PM's files. |
| Polygon | 0x24 → 0x28 | The flag bits are remapped: count 0xff → 0x3ff, back face 0x100 → 0x400, normals 0x200 → 0x1000. Material count 1; the UV flag set when the material is textured. PM's 0x4000 (741 untextured polygons in c2) and 0x800 are not understood. |
| Polygon data | adds per-polygon material indices, UVs per material, then 12-byte material refs per model | The refs need the polygons grouped by material, which reorders them, so the draw order changes. |
| Node | 0xd4, same size | Counts become 16-bit (MW3's maximum is 21 children). The area cell becomes i16. The virtual cell = the area cell when one is set. The active-box index comes from flag bits 8–10: 0b011 → 1, 0b101 → 2, else 0, which holds on every PM node. The data offset moves to +0x38. MW3's ~14,000 free slots are compacted out. |
| World | 0xbc → 0xcc: area2 added | area2 = the area ±3 cells. Both PM files match, and mech3ax asserts it. |
| Partition | 4-byte node entries → 12 (node, y min, y max) | The world-space y of each node's box. 615 of 725 entries in c2 are the node's own box; the rest are children in local space and need their parents' transforms. Fallback: the cell's own y range. |
| Light | 0xd0 → 0x100: 0x30 of colours | The colours can be computed (diffuse × colour, ambient × ambient colour, their sum). **The flags don't map cleanly:** MW3's sun is 0x38b, PM's suns 0x98b, and a plain bit shift gives 0x70b. Each world has one light, so write 0x98b with MW3's values. |
| LoD | 0x50 → 0x5c: 12 bytes inserted | Zeros, as in every PM file |
| Camera, window, display, object3d, sound | the same | Direct |

**Writers in the tree.**
- `pm::zcls::write_zbd` and its section writers
  (`src/pm/gamez/zclass/cls_zbd.cpp`) write v41. They are byte-identical to
  PM's exe (`tests/differential/test_pm_write_zbd.py`) and already handle
  compacting the nodes, the remap, the cycle order and the refs.
- MW3's `zcls::read_zbd` reads v27.
- So the converter is: read MW3's, fill PM's side fields (`pm_box`,
  `pm_virtual`, `pm_area2`, `pm_colors`, `pm_y`, `Lod::pm_tail`), convert
  each model, and write PM's.

## anim.zbd, 39 → 50

Nothing writes version 50 in the tree.

- **Header:** PM adds a time stamp, read and dropped. The 0x54-byte source
  records are unchanged.
- **Block, 0x44 → 0x6c:** a direct mapping (the inverse of PM's `kFields`).
  PM-only fields are 0; only PM's new IF flags 0x200/0x400 read them.
- **Def, 0x13c → 0x10c:**
  - MW3 +0x20..+0xc3 → PM +0x28; counts +0x108 → +0xd8.
  - Path count 0, a non-zero reset pointer, second sequence 0.
  - MW3's in-record copy of the reset sequence is dropped.
- **ObjectRef, 0x60 → 0x5c:** MW3's +0x28..+0x2f are junk in shipped files
  (string remnants, stale pointers) in 657–1,084 objects per world. PM reads
  them as the parent index and refuses the whole file when one points past
  the list. So write parent = 0, and the local bit from the def's +0x94 bit 19.
- **NodeRef, 0x28 → 0x2c:** 4 bytes added in front.

| Event | MW3 → PM size | Mapping |
|---|---|---|
| 4 LIGHT_STATE | 132 → 152 | Fields move, new ones 0 (flags 0x800/0x1000/0x2000 clear) |
| 12 OBJECT_MOTION_SI_SCRIPT | n → 36 | The key frames become one of PM's SI scripts, after all the defs. Checked byte for byte: MW3 c2's `hq_close` against PM c2's script 0. The script's name and object strings are made up. |
| 25 STOP_ANIMATION | 48 | −1/−1 at +0x2c (left at 0, PM stops `anim_refs[0]`) |
| 31 IF, 33 ELSEIF | 24 → 28 | 4 bytes of 0 appended |
| 42 PUFFER_STATE | 592 → 596 | A 4-byte priority inserted at +0xc0 |
| 28 FOG_STATE | 80 | MW3 only (c2 has 3, c4b 4). Not checked against PM's exe. |
| all others | the same | Direct; each sequence's event size is recomputed |

## mech3ax

- **Licence: EUPL-1.2. This repository is MIT.** Reading mech3ax as a
  description of the formats is fine, and so is running its binaries as a
  separate tool. Porting its code here is not.
- It reads every gamez into one shared intermediate and writes it per game.
  So an MW3-read, PM-write path exists by construction, but it has no
  conversion logic.
- Smoke test on MW3 c2's gamez:
  - The straight PM write fails mech3ax's own PM reader.
  - After nine hand fixes to the intermediate it reads back as PM and
    rewrites identically: dummy PM pointers, node indices, Empty →
    Object3d, active boxes, the light header, the Lod fields, partition y
    ranges, and two others.
  - Not yet loaded in the engine.
  - mech3ax's PM anim writer refuses MW3's anim ("Invalid mission MwC2V10
    for PM").
- **Best use:** its PM reader as a pass/fail gate on what our converter writes.

## Size and risk

| Piece | Lines | Risk |
|---|---|---|
| Archives, textures, scripts | about 100 | Low |
| gamez | about 500–800 | Medium: the models (regrouping, flags), the light flags, the partition y |
| anim | about 400–500 | High: no writer exists, the SI scripts, FOG_STATE |
| Tests (source checksums unchanged, read-back through PM's readers) | about 300+ | |

Roughly **1.5–2 weeks**. These can only be settled by running a world in
PM's engine:
- the partition y ranges;
- the light flags;
- PM's polygon flags 0x4000 and 0x800;
- PM's mission code reading MW3's records: PM keeps its waynets in the
  mission archives under other names, and has a `defaults` block in
  world.zrd, ai.zrd and templates.zrd, which MW3's worlds lack.
