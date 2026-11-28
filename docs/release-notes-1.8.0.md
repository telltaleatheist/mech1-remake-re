# MechWarrior 3 Rebuild 1.8.0 (2026-11-28)

A setup menu inside the game, updates from inside it, a switch for a pad's
rumble, and a gamepad's throttle fixed.

## New

- **A setup menu inside the game.** Press Cmd + Shift + F1 on the Mac, or
  Shift + Alt + F1 elsewhere. It opens over any screen, a mission included,
  and pauses the game the way the original pauses when its window loses the
  focus: nothing moves and no time passes until you return (in a multiplayer
  game it can't pause, and says so). From it:
  - GAME FILES shows which game files are in use, and CHANGE restarts the
    game on its setup screen to pick others;
  - REPORT A PROBLEM opens the report page, with the log at hand;
  - CHECK MY FILES checks the program and game files against the record
    made when they were installed. An older install, with no record, gets
    the game's own check of its data files, and the record is kept from then
    on;
  - RESET SETTINGS puts every setting back to its default at once, a copy of
    the old settings kept first;
  - UPGRADES opens the Upgrades page. It is dimmed, with a note, while the
    game is still starting;
  - OPEN MY DATA FOLDER opens the folder with your pilots, settings and log.

  For the first three starts made without the launcher, a small reminder of
  the keys shows in the corner for ten seconds. Its × closes it.
- **Updates from inside the game.** The menu's UPDATES page checks for a
  newer MechWarrior 3, shows what is new in it, and updates in place, then
  starts the game again. Your pilots and settings are backed up first.
  Started on its own, the game checks once a day and asks over its own
  screen when a new version is out; you can turn that off on the page.
  Started from the launcher, it never asks, because the launcher updates it.
  If an update could not finish, the game says so once at its next start.
- **RUMBLE on the Game Options screen,** under RAW MOUSE, on to start. Turn
  it off if you play on mouse and keyboard and a plugged-in pad would
  otherwise rumble (issue #22). Off, nothing rumbles, and an effect that is
  playing stops at once.

## Fixed

- **A gamepad's throttle keeps Assign Axis's zero point** (issue #10). Bound
  at rest, the stick now stops the mech at rest and reaches full reverse
  pulled back, as in the original. Before, a pad's stick ran at half throttle
  at rest and reached only stop. A joystick's throttle lever is unchanged:
  full reverse at the bottom to full ahead at the top.
- **The salvage and debriefing lists' scroll arrows click** as the salvage
  rows do.
- **FREELOOK or TORSO MODE on the middle mouse button with no key works**
  (issue #10). The fix has been in since 1.2.2 and is now covered by the
  tests; update if it does nothing for you.

## Versions

- MechWarrior 3: 1.8.0. Your pilots, saves, settings, mods and game files are
  kept.
- The launcher 2.2.0 shows what is new in each version on MechWarrior 3's
  page (WHAT'S NEW).
