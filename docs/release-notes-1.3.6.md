# MechWarrior 3 Rebuild 1.3.6 (2026-11-24)

Fixes, for all three games.

## Fixed

- **A crash while a mission loads, on Linux with an NVIDIA card** (issue #7).
  From a player's log, the graphics driver ran out of open files while the
  mission's textures went to the card, and the crash followed. Textures now
  go in batches, the limit on open files is raised, and the log says how many
  are open. We have no such machine to prove it on, so if it still happens,
  please attach the new mw3.log.
- **Menu buttons that made no sound** (issue #19). The original loads the
  sounds its pause and salvage buttons name only with the main menu or the mech
  lab, so in the mission they can't be found. The pause, briefing, debriefing
  and salvage buttons have their hover and click sounds now, moving salvage
  makes one, and so does pointing at a salvage row. It's in SMALL FIXES on the
  Upgrades page; with SMALL FIXES off they're silent, as in 1999. (Pirate's
  Moon's pause already had its sounds.)
- **MechWarrior 1 Remake's buttons** had no sounds at all. They do now, under
  SMALL FIXES too.

## Packages

- The Linux AppImages say what their start program contains (libfuse, LGPL 2.1,
  and others: `licenses/appimage-runtime`), and are built with pinned tools.

## Versions

- Launcher and MechWarrior 3: 1.3.6.
- Pirate's Moon: 1.2.8.
- MechWarrior 1 Remake: 1.1.1.

Version numbers: features raise the second number, fixes the third, which can
go past 9.

### Upgrading

Install over 1.3.5, or press UPDATE AVAILABLE in your launcher. Your pilots,
saves, settings, mods and game files are kept.
