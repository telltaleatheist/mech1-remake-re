# MechWarrior 3 Rebuild 1.3.0 (2026-11-22)

The launcher now downloads and updates the games itself. Its package is the
launcher alone (named BattleTech); each game comes from its own release.

## The launcher downloads the games

The request: "make the launcher download the games when the player wants
to play them, with an update button", and players choose when to update.

- **INSTALL.** A game that isn't installed shows INSTALL on its card, with
  the version and the download's size. The launcher gets it from the game's
  own release on GitHub: MechWarrior 3 from
  [mech3-re](https://github.com/telltaleatheist/mech3-re/releases), Pirate's
  Moon from [mech3-pm-re](https://github.com/telltaleatheist/mech3-pm-re/releases),
  MechWarrior 1 Remake from
  [mech1-remake-re](https://github.com/telltaleatheist/mech1-remake-re/releases).
- **UPDATE TO x.y.z.** When a game has a newer version, its card shows
  UPDATE TO with the version. Nothing is downloaded or changed until you
  press it.
- **Checked before anything is installed.** Every download is checked
  against its release's `SHA256SUMS`; a download that doesn't match, or a
  release without one, installs nothing.
- **Your files stay.** The games go in the launcher's own folder
  (`%LOCALAPPDATA%\MicroProse\BattleTech\games` on Windows,
  `~/Library/Application Support/MicroProse/BattleTech/games` on Mac,
  `~/.local/share/MicroProse/BattleTech/games` on Linux). Your game files,
  settings, pilots, saves and mods stay in each game's own folder as before,
  so an update never asks for your original copy again.
- **Offline.** The launcher asks GitHub when it starts. If it can't reach
  it, installed games just play, and a game not installed says it can't
  reach the server.
- **INSTALL FROM FILE** (bottom row) installs a game's download you already
  have: from a game's Releases page, or a modder's build. A `SHA256SUMS`
  beside the file is checked; without one, the launcher tells you it can't
  check it and asks before installing. On Windows it takes the game's zip,
  on the Mac its `.dmg`, on Linux its `.AppImage`.
- **Each game still comes on its own** and runs without the launcher.
  MechWarrior 3 alone carries its own launcher, which starts that copy and
  can install the other two.

### Upgrading from 1.2.4 or earlier

Install the new package over the old one, or press UPDATES in your launcher.
The old package's games are removed with it (on Windows the installer
removes them from the program's folder), so press INSTALL once on each game
you play. Everything of yours is kept.

## MechWarrior 1 Remake has its own download

`MW1-Remake-mw1-v1.0.7-...` on
[mech1-remake-re](https://github.com/telltaleatheist/mech1-remake-re/releases):
a zip on Windows, a disk image on the Mac, an AppImage on Linux. It still
needs MechWarrior 3 and Pirate's Moon set up, and your MechWarrior (1989).

## Versions

- Launcher and MechWarrior 3: 1.3.0 (MechWarrior 3 itself is unchanged
  from 1.2.4).
- Pirate's Moon: 1.2.3 (unchanged).
- MechWarrior 1 Remake: 1.0.7 (its own package; the game is unchanged).

## For developers

`docs/launcher.md`, Games on demand: the check, the file names, the games
folder and its markers, INSTALL FROM FILE, and the check hooks.
`DEPLOY.md`: the launcher's package is the launcher alone; each game repo's
release (its tag, its file names, its `SHA256SUMS`) is now read by every
launcher, and Pirate's Moon or MW1 Remake can be released on their own.
