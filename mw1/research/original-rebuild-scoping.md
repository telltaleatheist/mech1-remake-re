# MW1 Original: the scoping report (FIXES 130)

Status: **scoping only, no code yet.** Written 2026-11-20 by mw1 dev 2 for the mw1 lead and the owner.

The project rebuilds MechWarrior (1989) as our own C++ program, the way mw3-re rebuilt MW3.
- **The base:** the owner's revised rule makes Elthial's SDL port the base. We fork it with its history and keep its LICENSE, NOTICE and MIT text as they are. Then we convert it to clean C++20, with classes, namespaces, plain names, no goto, and files organized by what they do. Each function notes the original's address.
- **The content:** we pull as little from the exes as we can. Only MW1's creative content is extracted from the player's files at first run. Everything else is our code or our own data files.

Inputs examined:
- Elthial's port at e119a72a (`/Volumes/Callisto/claude-files/scratch/mw1-sdl-port/src`, 11 commits);
- crescent-hawks-rebuild, the Inception rebuild (MIT, the owner's, C++20 and SDL3): its import, text, renderer and DOSBox-X reference rig, which its lead pointed us to;
- The owner's install (`game/mw1-dos/mwarrior/MECHWAR` in the mw1-critical-hit checkout);
- mw1's own exe readers (`mw1/src/mw1_story.h` and its neighbours).

No MW1 words are quoted here.

---

## 0. Decide first: the licence of the base

The new rule makes the port's reconstructed game the base, and that part was never licensed to us.
- The port's `LICENSE` and `docs/LICENSES/README.md` grant MIT only for "project-authored host/platform code, tools, tests and documentation".
- They exclude "reconstructed game procedures, recovered text and tables, annotations": that is `src/Original/**`, 30,319 of its 34,436 lines.
- They also say "No rights in excluded material are granted."

**What this means for the fork:**
- Converting that code to C++, renaming it and removing the gotos still leaves our work derived from code we have no grant for.
- A fork with history makes that lineage explicit, which is right for credit but doesn't add a licence.
- The MIT host layer (`src/SDL`, `src/Support`, the build files, about 3,000 lines) is fine to fork as it is.

**Recommendation.** Before any conversion work is published, the owner (or the lead) asks Elthial for written permission to use `src/Original` as the base of a derived, publicly released project. MIT or a short explicit grant would do.
- Until then, the fork stays private and nothing from it ships.
- If Elthial declines, we fall back to the earlier rule: their reconstruction is a reference only, and our code is written fresh against the exe addresses. The plan below works for either route; only phase 1 differs.

---

## 1. Elthial's code, mapped

Totals: 251 files in `src/`, about 34,400 lines, 1.8 MB of source.
- The lines are dense: the reconstruction alone has about 22,700 statements in about 934 functions.
- **Gotos: 265 in all.** 218 are in the reconstruction; 47 are in the MIT host code (24 in `src/SDL`, 23 in `src/Support`).
- Only about 517 of the functions carry a `SEG:OFF` address note so far.

| Area | Files | Lines | goto | Licence | What it does |
|---|---|---|---|---|---|
| `Original/Campaign` | 22 | 7,350 | 81 | reconstructed | MW_MAIN.EXE's game (segment 081B): startup, hub, market, repair, pilots, contracts, economy, travel, news, events, options, the save format |
| `Original/Combat` | 53 | 7,981 | 23 | reconstructed | BTECH.EXE's battle (segments 0810, 17FB): world, movement, AI, weapons, damage, HUD, cockpit, radar, orders, results, the hand-back to the campaign |
| `Original/Memory` | 27 | 4,602 | 0 | reconstructed | **The importers:** each reads one exe data segment by offset at load into typed globals (MW_MAIN's 1090, 1AA5 and 28BE; BTECH's 304E; MW_EGA's) |
| `Original/Video` | 21 | 3,131 | 40 | reconstructed | EGA drawing (BTECH 1BF5 and 2A36, MW_EGA.EXE): planes, spans, polygons, ellipses, bitmaps, page flips |
| `Original/Runtime` | 17 | 2,959 | 41 | reconstructed | The DOS C runtime and services: near heap, streams, LZW archives, keyboard ISR, PIT timer, the campaign's MIDI driver and synth (0F41) |
| `Original/Geometry` | 14 | 2,244 | 11 | reconstructed | The 3D pipeline (22B6): camera, projection, clipping, horizon, polygon colours, resource rendering |
| `Original/Audio` | 7 | 1,622 | 4 | reconstructed | Combat sound (1ADF, 2134, 2176, 21CA, 229C): speaker, Tandy, AdLib and MPU drivers, the sound bytecode |
| `Original/Launcher` | 1 | 117 | 18 | reconstructed | MW.EXE: picks the graphics driver, runs MW_MAIN, which runs BTECH |
| `Original/campaign_graphics.c` | 1 | 117 | 0 | reconstructed | Campaign picture helpers |
| `include/mechwarrior` | 59 | 4,736 | 0 | mixed: reconstructed record layouts and the host's API | Shared types, enums and record layouts |
| `SDL` | 14 | 2,306 | 24 | **MIT** | Host: window, EGA buffer to texture, audio queue, input, files, time, the combat "child process" (setjmp and longjmp for DOS EXEC) |
| `Support` | 8 | 773 | 23 | **MIT** | Host: BTECH image loader (EXEPACK decoder, revision pin), combat near heap, native addressing, pilot and diagnostics |
| `MT32`, `Adlib` | 5 | 362 | 0 | MIT glue, **LGPL libraries** | Thin wrappers over Munt and Nuked-OPL3; both go |

How the port works today:
- **Native C, no CPU emulation.** The original's segmented memory model is kept as 64 KB byte views, with `MwNativeByteView` and `MwReadNativeWord` used about 1,076 times in the reconstruction.
- **The original's process model is kept too:** MW.EXE runs MW_MAIN, which runs BTECH. Combat's "child process" exit is a longjmp.
- **The speed is paced by a CPU-work budget,** `cpu_cycles_per_ms=2500` in MechWarrior.ini.
- **Status by its own docs:** experimentally playable on Windows. Full-campaign acceptance is incomplete, and Linux and macOS aren't qualified.

## 2. The exes and data files

The owner's install has 129 files. The exes, against the hashes Elthial's `docs/SUPPORTED_VERSIONS.md` reviews:

| File | Size | Format | the owner's vs Elthial's | Role |
|---|---|---|---|---|
| MW.EXE | 4,096 | MZ, 4 relocs, 189-byte tail | same hash | Launcher: driver pick, runs MW_MAIN |
| MW_MAIN.EXE | 137,871 | MZ, **not packed**, 125 relocs | same hash | The campaign: all screens, rules, the story script and its text |
| BTECH.EXE | **174,533** | MZ, **EXEPACK** (B0/B2 backwards, 16 reloc groups) | **different** (Elthial's is 174,267, CRC 0x36A13E1A; the owner's CRC 0x7E06BB28) | The battle |
| MW_EGA.EXE, MW_TANDY.EXE, MW_CPICS.EXE | 5,599, 5,009, 3,566 | MZ drivers loaded as overlays (entry 0:0) | first two same, CPICS not listed | Graphics drivers (EGA, Tandy, CGA pictures) |
| NEV_UNIV.EXE | 356,700 | MZ plus a 254 KB overlay | not listed | Not referenced by the game or the port; unidentified, ignore |

**The BTECH revisions.**
- I unpacked the owner's BTECH with a scratch EXEPACK decoder (the format is public). The rebuild itself will reuse Inception's
  `src/formats/exepack.{h,cpp}` (`chr::formats::unexepack`, checked against its `tools/unexepack.py`) rather than a
  third decoder. The image is 0x2D3D0 bytes against Elthial's 0x2D2C0, so it's 0x110 (272) bytes larger.
- From its far calls, every code segment after the first sits exactly 0x11 paragraphs later: 17FB→180D, 1ADF→1AF0, 1BF5→1C06, 2134→2145, 2176→2187 and 22B6→22C7.
- The data segment moves the same way, 304E→305F (image offset 0x284F0 against 0x283E0). It has the same size (0x4EE0 bytes before BSS, matching Elthial's `BTECH_BSS_START` 0x4EDC), and its tables sit at the same offsets: four of the port's string pointer tables at DS:02B6..02FA resolve to text in the owner's copy.
- **So the revisions differ only inside segment 0810, BTECH's main combat code, by about 272 bytes.** It is probably a bug-fix patch. What it changes needs a Ghidra diff of segment 0810 against the 174,267-byte BTECH, and **no copy of that revision is on this Mac** (searched all of Callisto). Elthial or another owner would need to supply one.
- MW_MAIN, MW.EXE and the drivers are identical, so the campaign is the same.
- **The port refuses the owner's BTECH at run time** (dev 1, FIXES 124, run in WSL). `BtechHostLoadInitialData` checks the exact packed size, 174,267, before the CRC. The owner's 174,533 fails and falls into the original's insert-disk prompt, printed to stdout only. So the port runs the campaign with the owner's files but **no combat**. Golden traces of combat need the 174,267 revision. Otherwise combat's oracle is DOSBox-X alone.

**Data files:**

| Kind | Files | Format (port's notes and names) |
|---|---|---|
| Pictures | MW_PICS.BIN, MW_1/2/A/C/F/G/TPICS.BIN, *.BMP, *.SCR | Indexed picture containers and nibble-packed EGA screens (`pictureReadHighNibbleNext` and the rest) |
| Fonts | 6X6.FNT, 6X6B.FNT, 8X8B.FNT, FOX88.FNT | Bitmap fonts |
| Palettes | ARCTIC/DESERT/TROPIC/DMGPAL.PAL | EGA palette and dither selection per landscape |
| 3D models | *PCK.TBL (8 chassis), OTHPCK.TBL, TERPCK.TBL, TERPCK.GI | LZW-packed resource banks (polygon parts) |
| Maps | TILE*.GRD (height and collision), TILE*.WLD (object placement), SNARIO.DAT | Four tiles make a scenario |
| Music | DEATH, LAUNCH, MECH, MECHBAR, VICTORY .MUS | Standard MIDI, format 0 |
| Sound effects | BTECHSND.SND | Sequenced tone bytecode (speaker, Tandy, AdLib), not samples |

**Where the content sits:**
- **MW_MAIN, data segment 1090:** the story script (event lists DS:6067, records DS:6121..655C), choice captions (DS:5CF4), planet records, pilot definitions and their dialogue, contract narratives and mission descriptions, after-action texts, intro panels, every menu label, and all the rules tables (prices, wages, tonnages, rolls). mw1's `mw1_story.h` already reads the script at these offsets.
- **MW_MAIN, segment 1AA5:** the event text bank, 0x77 records.
- **MW_MAIN, segment 28BE:** the music driver's state and the AdLib instrument patches.
- **BTECH, data segment 304E (305F in the owner's):** mech templates, weapon definitions, combat tables, mission titles, enemy pilot names, cockpit and inspection labels, animation maps.

## 3. Sorting every embedded string and table into three groups

The port has almost no content literally in its source: there are no large array initializers, and its string literals are diagnostics and file names. It reads everything from the exes at load through `Original/Memory`. The sort below covers what those importers read, which is what the C++ game will need, by the owner's rule.

### Extracted (the original's creative content only)

Extracted at first run into the story file in the player's write dir; never in the repo.
1. **The story:** the event script (records, conditions, actions), the event text bank (1AA5), the choice captions, and the six story-planet pools.
2. **Narrative text:**
   - the intro narration and its seven panels;
   - the contract narratives and mission descriptions;
   - the per-House after-action texts and the settlement explanations;
   - the Dark Wing texts and the "uneventful deployment" text;
   - the news articles;
   - the combat mission titles and briefings.
3. **MW1's own names and writing:**
   - pilot definitions, with names, portraits' ids and their dialogue;
   - enemy pilot names;
   - the commander's default name;
   - planet names;
   - mech kind display names, as MW1 spells them.
4. **Art:** pictures, screens, bitmaps and fonts.
5. **3D content:** chassis and terrain models, plus the animation maps tied to them (pose keyframes, frame maps, the chassis animation table, which only index those models).
6. **Maps:** TILE*.GRD and .WLD, and SNARIO.DAT.
7. **Sound:** the music (.MUS as MIDI), the sound-effect bytecode, and the AdLib instrument patches (MW1's sound design).

### Ours: written fresh, in our own data files in the repo

These are plain-text, versioned files. I propose `.zrd`, the format mw1 already uses, e.g. `mw1slots.zrd`.
- **UI words:** every menu, panel and button label (company, Bar, crew, contract office, ammo shop, repair, market, travel browser, options, save/load), headings, the after-action column heads, cockpit lamp and inspection labels, body location and condition labels, ZOOM, AUTO, PAUSED, the retreat question, radar range labels, ammo family names, weapon names, month names, number and date formats, attitude, reputation and skill labels.
- **System messages:** "no contracts", "contract accepted", "insufficient funds", "disk full", save and load errors, and full-roster messages, with our own wording.
- **Rules and constants:**
  - money: mech base prices and tier multipliers, repair prices by kind, ammo prices and freight;
  - pilots: wages, pay and deduction rolls, gunnery and piloting tables;
  - the market: generation ranges and the starting Jenner;
  - contracts: mission force weights;
  - the map: planet coordinates, Houses and tiers;
  - combat: mech templates (armour, internals, mounts, heat sinks, speed, jump), weapon definitions, hit location, cluster, heat and transfer tables, and AI thresholds;
  - layout: cockpit layouts and HUD colour schemes.

  These are transcribed from the original's values, since that is what makes it play the same, but they're data we own and can tune. A test checks each table against the player's exe when one is present.

### Code (neither: logic, or generated at start)

- EGA scanline, byte and mask tables, dither masks, glyph masks, ellipse insets: our renderer's own.
- Trig and projection tables (orientationCosines, headingRatioAngles, coefficientAngles): computed at start.
- LZW, heap, stream, timer, PIT, MPU, OPL register and interrupt-vector state: replaced by our platform layer.

### Dropped

- Copy protection: its pictures, callsigns, answers and prompt.
- Disk-swap prompts, the DOS launcher's strings, memory-tier errors and the driver-pick prompt.

**The extracted list is items 1 to 7 above.** Of MW_MAIN's roughly 300 named data items, about 40 are extracted; the rest are ours, code, or dropped.

## 4. The story file and first-run extraction

**The shape follows Inception's import** (crescent-hawks-rebuild `docs/import.md`, `docs/text.md`,
`src/formats/import.cpp`, `exe_data.cpp`):
- **The repo ships an import table, never the bytes:** each piece is an ID, a `SEG:OFF` address, a length, and the first 16 hex digits of the piece's SHA-256.
- **The player's copy is accepted piece by piece:** every piece's hash must match. A copy differing only in bytes we don't import still imports, and one that fails is refused, naming the pieces.
- **Re-import:** the import's manifest is written last, recording the exe's SHA-256, an import version and a hash of the import table itself, so a piece added to the table re-imports without a version bump.
- **An override file per text ID** (`--text FILE`) replaces any text, for a translation or a rewrite.
- **Our UI words live in the repo** as Inception's `data/text/interface.json` does.
- **Plain files in the player's data folder,** as JSON with named fields: one place for every ID, readable and moddable.

**Two departures from Inception:**
- We import far less. Inception imports its rule tables (`rules/mechs.json` and the rest); by the owner's MW1 rule ours are our own data files (section 3).
- Its step 1 accepts only one layout: header and relocations hash to one revision. For MW1 that step becomes a per-revision row of segment bases (section 2: the owner's BTECH has the same data layout 0x11 paragraphs later), so both known revisions pass the same piece hashes.

**Format.** I first sketched a single chunked file, below. Following Inception's shape instead (plain JSON files under an `import.json` manifest) is better, for one tool chain across the two projects. The chunk list still names what goes in: It's a small chunked container (a 4-byte tag, a version and a length per chunk), so it can be checked and partly reloaded:
- `META`: the source exes' hashes, the revision name, and the extractor's version (a new extractor re-extracts);
- `EVNT`: event records (id, class, conditions and actions as typed rows, not raw bytes);
- `TEXT`: every extracted text by stable key (`event.0x2a`, `pilot.7.bio`, `contract.desc.3.1`), UTF-8 with the original's colour codes mapped to our markup;
- `NAME`: planets, pilots and mech display names;
- `ROST`: pilot definitions (portrait id, starting skills), story-planet pools;
- `ANIM`: animation maps per chassis;
- `MIDI`, `SNDB`, `OPLP`: music, the sound bytecode, the AdLib patches.

Art, models and maps stay in the player's install and are read in place by our decoders. Copying them gains nothing, and the install is needed anyway.

**Finding the content in each revision:**
- **A layout table, in our repo, by exe hash:** segment bases plus each item's DS offset and count. Both known BTECH revisions share one data layout with different bases, so it's two rows.
- **A signature fallback for unknown revisions.** Find the data segment by the Microsoft C runtime banner, which sits at DS:0008: this located the owner's 305F correctly. Find MW_MAIN's text bank by its 0x77-word directory. Then check each table structurally (pointers land on text, counts in range) before accepting it.
- **Revisions that fail the checks** are named in the launcher's message, as mw1 does today.
- The extraction runs once, under a second, and is redone when the exes' hashes change.

## 5. The engine: original against ours

**The split, as Inception's:** game logic speaks only in IDs (picture, sprite, model part, text). An asset provider decodes the player's files, and a renderer draws them with SDL3, with per-picture palettes. Mods can replace any ID. There's no EGA buffer anywhere.

**Drawing.**
- **Original:** EGA 320x200, 16 colours, four bit planes, page flipping. Polygons are filled with two-colour dither patterns per palette. MW_MAIN draws through the MW_EGA.EXE driver, while BTECH has its own EGA code (1BF5, 2A36). There are Tandy and CGA paths too.
- **The port:** it emulates the planes and copies them to an SDL texture.
- **Ours** (under the split above):
  - an 8-bit indexed 320x200 framebuffer in plain C++, with dithers as pattern fills, presented through an SDL3 texture with integer scaling;
  - no plane emulation;
  - Tandy and CGA dropped;
  - later, an optional high-resolution 3D view that draws the same polygons at window resolution with the same projection, for crisp combat. The 2D screens stay 320x200.

**Sound structure.** The SDL3 output layer (device, mixing, stream) is kept separate from the synths (speaker and tone voices, FM). Inception has no sound yet beyond PC speaker and Tandy, and can then take the same layer.

**Sound effects.**
- **Original:** BTECHSND.SND bytecode drives PC speaker tones with channel priorities, or Tandy's three voices, or AdLib.
- **Ours:** a tiny bytecode player with our own square and noise voices, exact to the bytecode.

**Music.** The original plays .MUS (MIDI format 0) through MW_MAIN's driver to the speaker, AdLib (MW1's own patches) or MT-32 (MPU-401). The options:
- **(a) Our own small FM synth** (OPL2-style two operators, 9 channels, about 400 lines) driven by MW1's AdLib patches. It sounds like the AdLib version without being bit-exact. **Recommended default.**
- **(b) The speaker rendition:** square waves from the same MIDI, the most "1989" sound. A cheap option alongside (a).
- **(c) Our own sample player** with an MT-32-to-GM patch map and a soundfont the player supplies. It's closer to the MT-32 mood, but more work and a user-supplied asset.
- **(d) The player's own recordings** (OGG per track), as MW3's CD music works. Cheap, and worth adding with (a).

**Input.**
- **Original:** a keyboard ISR on scan codes, a game-port joystick read by timing loops with calibration, and a mouse in the campaign.
- **Ours:** SDL3 scancodes through one table to the original's command codes, SDL3 gamepads and joysticks with no calibration, the mouse, and rebinding in our data file.

**Timing.**
- **Original:** the PIT (18.2 Hz BIOS tick, reprogrammed for music), with combat partly CPU-bound.
- **The port:** paces with a CPU-work budget.
- **Ours:** a fixed simulation tick from SDL's clock, with its rate measured once against DOSBox at a reference speed.

## 6. Where it lives

**Recommended: its own repo, a GitHub fork of Elthial's**, e.g. `mw1-original`.
- Elthial's authorship and history show natively.
- Their licence files stay at its root, untouched.
- The not-yet-granted reconstruction (section 0) stays isolated from mw3-re's code.
- It shares little of MW3's engine.
- mw3-re reaches it the way it reaches the port today: the packaging kit `tools/package/mw1port` (its `pins.txt`) builds it, and the launcher's MECHWARRIOR 1 entry gets a PLAY ORIGINAL action.

**No shared library with Inception for now.** We copy its MIT code (EXEPACK, SHA-256, the import table, JSON, the text provider) with its notice. At this size, copying beats coupling; the owner decides if that changes.

The second choice is `git subtree add --prefix=mw1o/` into mw3-re's mw1 branch. It keeps history too, but mixes the licence question into mw3-re.

## 7. The conversion plan

**A behaviour check at every step:**
- **Golden traces from the unmodified fork, recorded once before any change.** Each is a seed plus a scripted input (the port already has combat input replay and a seeded RNG), giving per-tick hashes of the campaign records, the combat mech states and the indexed framebuffer, plus a log of sound and music events.
- Every later commit must reproduce them bit-exact. Changes meant to differ (a bug fixed, our UI wording) re-record the affected trace in the same commit, with the reason.
- **Against the real original: adopt Inception's DOSBox-X reference rig** (`tools/capture/snap.asm`, `tools/reference/*.py`, 858 cases). It runs the player's own game headless from a key script, snaps the screen and a memory dump at marked steps, and compares screen and state with the rebuild pixel by pixel. It fits the keep-it-running rule exactly, and nothing from the game enters the repo. MW1 needs two changes:
  - **A different key feeder.** Inception's original reads keys only through int 16h and int 21h, which SNAP hooks. MW1's BTECH installs its own int 9 keyboard handler (the port's `savedKeyboardVector`), and MW_MAIN keeps a keyboard-interrupt state, so the feeder must inject at the int 9 and port 60h level.
  - **Combat checkpoints by timer tick, not by keyboard poll,** since combat runs on the PIT rather than waiting on keys.

  The campaign's screens and save files compare as they are.
- **Also against the original:** the save format is the original's, so a scripted campaign run in DOSBox (a test tool only, never shipped) and in ours should produce byte-equal save files at each checkpoint.
- Combat checkpoints compare the mech states DOSBox's debugger dumps.
- This is mw3-re's differential idea without a probe DLL, since 16-bit code can't be linked. DOSBox-X is a test tool only: Inception already uses it this way, so the no-emulation rule (which is about what ships) isn't touched.
- **Pure functions** (geometry, clipping, division, ellipses, LZW, picture decoding) get unit differentials: while each is converted, the old C version stays in the test build and both are fed the same random inputs, as mw3-re's probe tests do.

**The port's own headless oracle** (dev 1, FIXES 124) is the cheapest way to record the golden traces:
- **Running it:** set `MECHWARRIOR_DEBUG_CONTROL_DIRECTORY` and it takes scripted key steps from `command.txt`.
- **status.json:** each step writes the current original menu (each entry's handler offset in the original, its label and its navigation links), plus the campaign fields (planet, contract, payment, scenario, result) and, in combat, the units and targets.
- **Captures:** each step also writes `.indices`, the indexed EGA frame.
- **Headless:** with `SDL_VIDEO_DRIVER=offscreen`, `SDL_AUDIO_DRIVER=dummy` and `MECHWARRIOR_SAVE_DIRECTORY`, it writes nothing into the game folder.
- **Uses:** its handler offsets map the menu tree onto the original's functions, and phase 0 records traces through it.
- **Where:** a built copy and a driver script are on the PC (WSL `~/mw1port`).

**The phases:**
- **Phase 0. Fork and harness** (about 1-2 weeks): fork with history; CMake on Mac, Linux and Windows; record the golden traces; a headless test runner in mw3-re's style (filters, `--hidden`); Inception's DOSBox-X rig brought over with the int 9 feeder.
- **Phase 1. C to C++20, unchanged in behaviour** (about 1 week): rename to .cpp and fix the C-isms (compound literals, implicit `void*`, designated-initializer order). Traces equal.
- **Phase 2. Content out** (2 weeks):
  - `Original/Memory` becomes the first-run extractor writing the story file (section 4);
  - our `.zrd` files take the UI words and rules tables (section 3);
  - copy protection, disk prompts and the DOS launcher go.

  Traces equal, except for our new wording.
- **Phase 3. Restructure by function** (the bulk, 5-7 weeks for two devs):
  - **The order:** platform and runtime, then render, then campaign, then combat.
  - **The namespaces:** `mw1o::platform`, `render`, `audio`, `input`, `campaign`, `market`, `travel`, `pilots`, `story`, `combat`, `ai`.
  - **The globals** become owning classes (`CampaignState`, `CombatState`, `World`).
  - **The native byte views** become typed records parsed at load. This is the biggest single cost: about 1,076 uses.
  - **Names:** plain names throughout, with each function's original `SEG:OFF` in its comment. The roughly 400 functions without one get it from Ghidra as they're touched.
- **Phase 4. The gotos, all 265, one function per commit** (inside phase 3), each covered by its trace or unit differential:
  - **About 70 are error exits** (`failed`, `cleanup`, `allocationFailed`, `finished`, `done`): RAII and early returns.
  - **About 20 are restart loops** (`initializeCampaign`, 10 of them, in startup): a loop or an explicit state enum.
  - **The rest are shared tails and far jumps in geometry and video** (`farDeflection`, `returnPolygon`): extracted helper functions.
  - A CI check fails the build on any `goto`.
- **Phase 5. Platform** (2-3 weeks):
  - indexed renderer in place of the EGA planes (traces compare indexed pixels, so it must match);
  - Munt and Nuked-OPL3 removed;
  - the speaker and bytecode player and the FM synth (section 5), compared by event logs since audio can't be bit-compared;
  - the fixed tick in place of the CPU budget.
- **Phase 6. Revisions and acceptance** (2 weeks): the owner's BTECH (a Ghidra diff of segment 0810 against the 174,267 revision, any behaviour change as a revision switch), the signature fallback, a full campaign playthrough script, the launcher entry, and the high-resolution 3D option if wanted.

**Size:**
- about 14-17 weeks of developer time, so roughly 2 to 2.5 months for two devs;
- Phase 3 dominates;
- it is longer if Elthial's licence answer forces the fresh-code route, by about +50%, since we'd write rather than convert.

**The risks:**
1. **The licence of the base** (section 0): it must be settled first.
2. **We inherit the port's gaps:** its own status says campaign acceptance is incomplete. The golden traces lock in its bugs as well as its behaviour, so the DOSBox save-file checks against the original are what catch them.
3. **The owner's BTECH revision isn't the one the port reconstructs.** The 272 changed bytes in segment 0810 are unknown until we have the other exe to diff.
4. **Timing:** the original's combat speed was partly CPU-bound. The fixed tick needs measuring and the owner's ear.
5. **The native addressing is everywhere:** replacing it is where behaviour can slip, and the traces must cover enough to notice.
6. **Music without MT-32 emulation won't sound like an MT-32.** That's accepted by the rule; the (c) and (d) options soften it.
7. **No MW1 words in logs or tests:** the traces store hashes, never text.

## 8. MW1 against Inception: formats and code (the owner's ask)

Inception is Westwood's (1988, Infocom) and MW1 is Dynamix's (1989, Activision); both main exes are BTECH.EXE. I checked crescent-hawks-rebuild's `docs/formats/` and `docs/original.md` against the owner's MW1 files, and compared the two unpacked BTECH images (and MW_MAIN) byte for byte. Inception's exe was unpacked with its own `tools/unexepack.py` and matches its documented SHA-256.

**They share nothing beyond EXEPACK and the Microsoft C 1988 runtime.**

| | Inception (Westwood) | MW1 (Dynamix) | Match? |
|---|---|---|---|
| **Container** | No tags: .CMP and .ICN are a u16 length, a type byte, then the stream | Tagged chunks: a 4-char tag and a u32 size with the top bit set, nested (`SCR:`, then `BIN:`; `BMP:`, then `INF:`; `PAL:`, then `EGA:`; `SND:`, then `INF:`; `FNT:`) | No. MW1's is Dynamix's resource-chunk scheme |
| **Full screens** | .CMP: 320x200, two pixels a byte (high nibble left), rows or columns | .SCR (`SCR:` / `BIN:`) and the MW_*PICS.BIN banks, nibble-packed EGA | Only the 4-bit pixel idea, which is generic EGA |
| **Tiles and sprites** | .ICN: 250 16x16 tiles in a .CMP container | .BMP (`BMP:` / `INF:`) sprites and SM_MECHS sheets | No |
| **Animation** | .ANM: XOR-delta frames of runs over an 88x88 picture | Polygon models in *PCK.TBL banks with exe-held animation maps | No |
| **Maps** | .MTP: cell maps with townspeople's names | TILE*.GRD heights and TILE*.WLD objects, four tiles per scenario, SNARIO.DAT | No |
| **Compression** | Signed-byte RLE (1-127 literal, -1 to -128 repeat, 0 then a u16 count) | LZW for the model and terrain banks (variable code width, clear code); nibble runs in the pictures | No |
| **Font** | 1,024 bytes inside BTECH.EXE: 128 glyphs, 8x8 | Separate .FNT files (`FNT:` chunks; 6x6, 6x6 bold, 8x8 bold, FOX88) | No |
| **Palettes** | None in the files: tables in the exe (37e8:0000, plus an MCGA copy), picked by the caller | .PAL files per landscape (`PAL:` / `EGA:`, 314 bytes), plus dither patterns | No |
| **Sound** | PC speaker and Tandy in code, no sound files | .SND tone bytecode (speaker, Tandy, AdLib) and .MUS standard MIDI, with AdLib patches and an MPU-401 driver | No |
| **Exe packing** | Microsoft EXEPACK | Microsoft EXEPACK, in BTECH only (MW_MAIN isn't packed) | **Yes:** same packer, so Inception's `exepack.cpp` serves both, as section 2 proposes |
| **Compiler and runtime** | Microsoft C 1988 | Microsoft C 1988, with the same banner | **Yes:** identical runtime code (below) |
| **Display drivers** | All four (CGA, Tandy, EGA/VGA, MCGA) in the one exe | Separate overlay exes (MW_EGA, MW_TANDY, MW_CPICS), and BTECH's own EGA code | No |

**The byte comparison.** I looked for byte runs of 24 bytes or more present in both exes, ignoring runs of nearly constant bytes.
- **Inception against MW1's BTECH:** 21 shared runs, 1,277 bytes.
- **Inception against MW_MAIN:** 10 shared runs, 731 bytes.

Every one is Microsoft C's runtime:
- command-line parsing;
- the near heap;
- int 21h file I/O;
- the runtime-error strings (R6000, stack overflow) and `;_C_FILE_INFO`;
- the runtime banner at DS:0008, which also locates MW1's data segment in section 4.

The only other match is 25 bytes of a counting table of words (0007, 0008, 0008, 0009 and so on). It sits in data in Inception and code-adjacent in MW_MAIN, so it's a coincidence. No game code, format decoder or table is shared.

**For the Inception lead:**
- The exe-side tools carry over: EXEPACK, the import-table design, and a runtime-banner check to find the data segment.
- No decoder carries over.
- MW1's chunk container is Dynamix's, also used in their later games, so a decoder for it would serve Dynamix titles, not Westwood's.
