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
- `src/`, `tests/`, `data/`, `data-src/`, `tools/`: the `mw1` program, its
  tests, its screens' layouts and its art (below, Building).
- `FIXES.md`: what Owen's play-tests found, ticked as each is fixed.
- The engine is the rest of this repo: mw3-re's branch `mw1` is `pm`
  (Pirate's Moon) plus MW1 (VISION.md, "The code base").

## Getting it

```
git clone -b mw1 https://github.com/telltaleatheist/mw3-re.git
```

## Building

The root `CMakeLists.txt` builds the engine and Pirate's Moon, and adds `mw1/`
(`MW3_BUILD_MW1`, on), which builds `mw1` and `mw1tests` on Pirate's Moon's
game code (prerequisites: `docs/building.md`).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build mw1 mw1tests
```

`mw1` and `mw1tests` land in `build/`, beside `mw3pm` and `mw3pmtests`.
Development builds and the tests read the installs in the ignored `game/`
(`game/mw3-1.2`, `game/pm`, `game/mw1-dos/mwarrior/MECHWAR`), or the ones
named by `-DMW3_GAME_DIR=`, `-DMW3PM_GAME_DIR=` and `-DMW1_DOS_GAME_DIR=`.
On Windows, the same route as mw3pm's (`tools\windows\build.cmd mw1`).

`mw1` is Pirate's Moon's game main (`src/frontend/sdl/mw3_main.cpp`) started
by `mw1/src/mw1_main.cpp`. It has its own settings and saves folder, SDL's
app data for "MicroProse/MechWarrior 1 Critical Hit", apart from MW3's and
PM's. It takes the player's three installs; the launcher's MECHWARRIOR 1
icon passes them:

    mw1 [--install PM-DIR] [--mw3-install MW3-DIR] [--mw1-game MW1-DIR] [--write-dir DIR] [mw3pm's own flags]

- Each folder is checked: PM's and MW3's by their `zbd/reader.zbd`'s
  zArchive version, MW1's by `BTECH.EXE`. A good one is saved to `mw1.cfg`
  (`install=`, `mw3_install=`, `mw1_game=`), so a later start without the
  flags uses them again.
- Opened with any of the three missing, it says which and offers the
  launcher beside it.
- Development builds fall back to the `game/` copies above. These are never
  saved.

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
