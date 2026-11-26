# MechWarrior 3 Rebuild 1.6.0 (2026-11-26)

The launcher on its own: each game now sets itself up, and the launcher only
asks it. Plus the mouse wheel in the menus' lists.

## New

- **The mouse wheel scrolls the menus' lists** (issue #20): the Mech Lab's,
  the pilots, the saved games and the multiplayer lobby's lists move a line a
  notch, as their arrows do, and on the controls screen a notch steps through
  the commands as NEXT and PREV do. The original's menus ignored the wheel.
  Under SMALL FIXES.
- **The launcher has its own menu sounds**, a soft click on a press and a
  quieter tick on a hover. Before, it borrowed MechWarrior 3's menu sounds
  from your game files, and was silent until it found them.
- **New games can appear in the launcher without a launcher update.** It
  reads the list of games from this project's page at the start (nothing
  about you is sent), so a new release or a new game shows up on its own.

## Changed

- **Each game sets itself up.** Finding your copy, installing it from a disc,
  an image or a folder, adding the music and movies, and converting the
  movies are now done by MechWarrior 3's and Pirate's Moon's own programs,
  as MechWarrior 1 Rebuild, the Remake and Inception already did; the
  launcher shows their screens as before. The launcher itself holds no game
  code any more, and its download is less than half the size.
- **PLAY and a start on its own open the same copy.** MechWarrior 3 started
  without the launcher now picks the copy the launcher shows for it: the
  game's own folder first, then the one in mw3.cfg, the settings folder's,
  then the installer's places. Before, with more than one copy on the
  computer, the two could open different ones.

## Versions

- Launcher and MechWarrior 3: 1.6.0.
- Pirate's Moon 1.2.11 and MechWarrior 1 Remake 1.1.3, with the same
  changes. The Remake 1.1.3 also keeps its menus 4:3 on a screen narrower
  than 16:9 with WIDE MENUS on (issue #1: at 1680x1050 the hub ran past the
  right edge), and no longer can crash as it closes. The launcher also offers
  MechWarrior 1 Rebuild and The Crescent Hawk's Inception.

### Upgrading

Install over 1.5.2, or press UPDATE AVAILABLE in your launcher, then UPDATE
ALL for the games: this launcher sets up only games of this version or newer,
and asks you to update an older one first. Your pilots, saves, settings, mods
and game files are kept.
