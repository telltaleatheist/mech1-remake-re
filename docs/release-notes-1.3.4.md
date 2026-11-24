# MechWarrior 3 Rebuild 1.3.4 (2026-11-24)

You choose which copy of your game files each game uses, and you can see
what each copy is missing.

## Choosing your game files

- **GAME FILES lists every copy of the game the launcher finds** on your
  computer: the one in use (marked IN USE) first, then any others, such as
  an older install or a second drive. Each says which version of the data
  it is and what the game would be missing with it:
  - MechWarrior 3: the music, the movies, the 1.2 patch.
  - Pirate's Moon: the music, the full-quality sounds.

  Music and movies the game already has from somewhere else count for
  every copy, so a copy is only marked missing what you would really lose.
  Pick one and it's copied into the game's folder with its music and
  movies. PICK ANOTHER still takes a disc, a disc image, a download (.zip,
  .iso) or Mech3.exe.
- The same list appears when you hold Shift while a game starts.
- **The copy you set up last is the one used.** Pirate's Moon and
  MechWarrior 1 Remake now take the copy in the launcher's own folder before
  an older one your settings still name, as MechWarrior 3 already did
  (issue #10).

## The launcher

- **Every launcher screen says BATTLETECH at the top**, with the game the
  screen is for underneath. A game started on its own still shows its own
  name.
- **Every button's words fit inside it.** A long folder name is shortened in
  the middle, so you see both where it starts and where it ends.

## Versions

- Launcher and MechWarrior 3: 1.3.4.
- Pirate's Moon: 1.2.6 (the same game-files changes).
- MechWarrior 1 Remake: 1.0.8 (unchanged).

### Upgrading

Install over 1.3.3, or press UPDATE AVAILABLE in your launcher. Your pilots,
saves, settings, mods and game files are kept.

## For developers

`docs/launcher.md`, The copies on this computer. `--launcher-screen copies`
(or `pmcopies`) draws the list; mw3launcher.log has a `copy <folder>: <version>, missing ...`
line for each copy.
