# MechWarrior 3 Rebuild 1.3.1 (2026-11-22)

Each game goes where you choose, and each game's setup names its game.

## Each game goes where you choose

The request: games can be installed anywhere, on any drive, from the
launcher or from a game's own setup, with "everything installed in the same
location the user picks".

- **The launcher's INSTALL** asks where the first time: INSTALL puts the game
  in the launcher's own games folder as before, and **CHANGE FOLDER** picks
  another, on any drive. The game gets a folder of its own there
  ("MechWarrior 3", "Pirate's Moon", "MechWarrior 1 Remake"), and its updates
  go to the same place.
- **Each game's setup** (copying your game files from your install, disc or
  disc image) has **CHANGE FOLDER** too. The screen shows where the files go
  before anything is copied.
- **Music and converted movies** from your disc go into the same folder, and
  the game looks for them there. Music you already ripped into your user
  folder still plays.
- **Your pilots, saves, settings and mods stay in your user folder,** where
  mods expect them, wherever the game is.
- A folder that already holds someone else's files is refused: pick an empty
  one, or the game's own folder from before.

## Each game's setup names its game

The setup screens said "MW3 Rebuild" for every game. Each one now shows the
game's own name and badge: MECHWARRIOR 3, MECHWARRIOR 3: PIRATE'S MOON,
MECHWARRIOR 1 REMAKE, MECHWARRIOR 1.

## Versions

- Launcher and MechWarrior 3: 1.3.1 (MechWarrior 3's game is unchanged; its
  setup has the new header and folder choice).
- Pirate's Moon: 1.2.4 (the same; the game is unchanged).
- MechWarrior 1 Remake: 1.0.8 (unchanged; its next release picks this up).

### Upgrading

Install over 1.3.0, or press UPDATES in your launcher, then UPDATE on each
game. Installed games stay where they are and keep everything of yours.

## For developers

`docs/launcher.md`, Where a game goes: the game folder's mark
(`rebuild-game-folder.txt`), `games_<key>` in the launcher's `mw3.cfg`,
`sdlfe::media_dir`, and the check hooks `MW3_LAUNCHER_INSTALL_TO` and
`MW3_LAUNCHER_FILES_TO`.
