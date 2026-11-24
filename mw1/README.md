# mw1/: the remake's own code

The game, what it needs and how to play it are in the
[root README](../README.md). This folder holds MW1's part of the repo; the
rest is the engine and Pirate's Moon's game.

- [VISION.md](VISION.md): the design.
- [FIXES.md](FIXES.md): what the play-tests found, ticked as each is fixed.
- [research/](research/): MW1's economy, contracts, pilots and maps.
- `mockups/`: the first look at the hub and the Bar, and the New Campaign
  picture.
- `src/`, `tests/`, `data/`, `data-src/`, `tools/`: the `mw1` program, its
  tests, its screens' layouts and its art.
- [dist/](dist/): the read-me, third-party list and release notes that ship.
- [docs/RELEASING.md](docs/RELEASING.md): how a release is made.

`mw1` is Pirate's Moon's game main (`src/frontend/sdl/mw3_main.cpp`) started by
`mw1/src/mw1_main.cpp`, with its own settings and saves folder.

Credits and licence are in the [root README](../README.md#credits-and-licence).
