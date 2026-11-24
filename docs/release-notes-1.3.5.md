# MechWarrior 3 Rebuild 1.3.5 (2026-11-24)

The launcher finds the games you installed without it, and every game has a
page of its own: where it's installed, linking another copy, and uninstalling.

## Games installed without the launcher

- **The launcher finds them by itself.** If you installed MechWarrior 3 or
  Pirate's Moon from its own Setup, disk image or AppImage, the launcher
  links it when it starts, and PLAY starts it. Nothing is downloaded twice.
  (A copy is found once it has been started at least once with 1.3.5 or
  later; on Windows a copy from its Setup, and on a Mac one in Applications,
  is found straight away.)
- **UPDATE on a linked copy** installs the new version into the launcher's
  own folder, and that one plays from then on. The launcher never writes
  into a folder it didn't make, so your own install is left as it was.

## A page for each game

- **Each game's panel has a gear** to the left of its button. It opens the
  game's page:
  - where the game is installed, and which version;
  - **LINK AN INSTALL**: point at a copy you installed yourself (its
    program, its folder, the Mac app or the AppImage);
  - **UNLINK**: the launcher forgets a linked copy; its folder isn't
    touched, and it isn't linked again by itself;
  - **UNINSTALL**, for a copy the launcher installed: the program alone, or
    the program and the game files it copied from your original. Game files
    are only offered when nothing else uses them (MechWarrior 1 Remake plays
    MechWarrior 3's and Pirate's Moon's). Your pilots, saves, settings and
    mods always stay.
- **Words in buttons shrink to fit** instead of being cut short.

## Fixed

- On GAME FILES' list of copies, the second copy opened the file picker
  instead of being used.
- Inception's download screens could draw a wrong header or close the
  launcher.

## Versions

- Launcher and MechWarrior 3: 1.3.5.
- Pirate's Moon: 1.2.7 (records where it's installed, for the launcher).
- MechWarrior 1 Remake: 1.0.9.

### Upgrading

Install over 1.3.4, or press UPDATE AVAILABLE in your launcher. Your pilots,
saves, settings, mods and game files are kept.

## For developers

`docs/launcher.md`, Copies installed without the launcher and A game's page.
Each released program writes `rebuild-program.txt` (game, program, version)
into its user folder at start; the launcher's links are `link_<game>=` and
`unlinked_<game>=` in its mw3.cfg. `--launcher-screen game:mw3` (or
`game:pm:uninstall`) draws a page.
