# MW1: Critical Hit

A fan remake of **MechWarrior** (Dynamix and Activision, 1989) on the engine
of the [MechWarrior 3 rebuild](https://github.com/telltaleatheist/mw3-re).
MW1's mercenary campaign (the worlds, the Houses, the contracts, the bars,
the mech markets, the long road to clearing the founder's name) brought to
Pirate's Moon's game: its mechs and weapons, MW3's combat, mech lab and
salvage, and new art for every screen between missions.

It is not a decompile of MW1, and not a copy of either game: MW1's design is
where it starts, and it changes what plays better. It is a personal project:
free, not sold, and not affiliated with or endorsed by Microsoft, MicroProse,
Activision or Catalyst Game Labs.

**Status: design, and the first code.** What the game is meant to be is in
[VISION.md](VISION.md); what the original does, with its numbers, in
[research/](research/).

**No game files are included.** You'll need your own copies of MechWarrior
(1989), MechWarrior 3 and its expansion, Pirate's Moon. It can ship on its
own or with the MechWarrior 3 rebuild, as a third game on its launcher.

## What's here

- `VISION.md`: the design, the goal everything works toward.
- `research/`: MW1's economy, contracts, pilots, and the maps we fight on.
- `mockups/`: `intro.jpg`, the picture New Campaign opens on, and the first
  look at the hub and the Bar (`mw1-hub.html`, made by `build.py` from
  `mw1hub.template.html` and the images here: new art from a local image
  model).
- `src/`, `tests/`: the `mw1` program and its tests (below, Building).
- `external/mw3-re`: the engine, a submodule on mw3-re's `mw1-engine` branch
  (VISION.md, "The code base").

## Getting it

```
git clone --recurse-submodules https://github.com/telltaleatheist/mw1-critical-hit.git
```

In a clone without it: `git submodule update --init`. The engine's build
notes are in `external/mw3-re/docs/building.md`.

## Building

The root `CMakeLists.txt` adds the engine as a subdirectory, with MW3's own
programs off, and builds `mw1` and `mw1tests` on Pirate's Moon's game code
(the engine's prerequisites: `external/mw3-re/docs/building.md`).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build mw1 mw1tests
```

`mw1` lands in `build/external/mw3-re/`, beside `mw3pm` and the launcher.
Development builds and the tests read the installs in the ignored `game/`
(`game/mw3-1.2`, `game/pm`, `game/mw1-dos/mwarrior/MECHWAR`), or the ones
named by `-DMW3_GAME_DIR=`, `-DMW3PM_GAME_DIR=` and `-DMW1_DOS_GAME_DIR=`.

`mw1` is Pirate's Moon's game main (`external/mw3-re/src/frontend/sdl/mw3_main.cpp`)
started by `src/mw1_main.cpp`. It has its own settings and saves folder,
SDL's app data for "MicroProse/MechWarrior 1 Critical Hit", apart from MW3's
and PM's. It also takes the player's three installs:

    mw1 [--install PM-DIR] [--mw3-install MW3-DIR] [--mw1-game MW1-DIR] [--write-dir DIR] [mw3pm's own flags]

- Each folder is checked: PM's and MW3's by their `zbd/reader.zbd`'s
  zArchive version, MW1's by `BTECH.EXE`. A good one is saved to `mw1.cfg`
  (`install=`, `mw3_install=`, `mw1_game=`), so a later start without the
  flags (the launcher's or a plain one) uses them again.
- Development builds fall back to the `game/` copies above. These are never
  saved.
- For now it starts into Pirate's Moon's frontend as it is. MW3's worlds
  aren't loaded yet.
- `mw1tests` (`tests/`) covers the start: the flags, `mw1.cfg`, and the
  install checks.

## Credits and licence

- **MechWarrior** (1989) was made by Dynamix and Activision, under licence
  from FASA. **MechWarrior 3** and **Pirate's Moon** were made by Zipper
  Interactive and published by MicroProse in 1999. All the games' content is
  theirs and comes from your own copies.
- The engine is [mw3-re](https://github.com/telltaleatheist/mw3-re), the
  MechWarrior 3 rebuild by Owen Morgan.
- MW1's rules and data were studied through
  [Elthial's MechWarrior SDL port](https://github.com/Elthial/Mechwarrior-sdl-port),
  a reconstruction of the 1989 game (MIT licence).
- MW1: Critical Hit is by Owen Morgan, under the [MIT licence](LICENSE).

MechWarrior and BattleTech are trademarks of their owners. This project is
not affiliated with or endorsed by them.
