# MechWarrior 3 Rebuild 1.2.1

**A fix release for MW1: Critical Hit, now 1.0.4.** MechWarrior 3 and Pirate's Moon are as in [1.2.0](release-notes-1.2.0.md). Install it over any earlier version: your pilots, saves, settings and mods are kept.

## Downloads

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.2.1.exe` (or `MW3-Rebuild-v1.2.1-win64.zip`) | `MW3-alone-Setup-v1.2.1.exe` (or `MW3-alone-v1.2.1-win64.zip`) | `PM-Rebuild-Setup-pm-v1.2.0.exe` (or `mw3pm-pm-v1.2.0-win64.zip`) |
| Mac | `MW3-Rebuild-v1.2.1-mac.dmg` | `MW3-alone-v1.2.1-mac.dmg` | `PM-Rebuild-pm-v1.2.0-mac.dmg` |
| Linux | `MW3-Rebuild-v1.2.1-x86_64.AppImage` | `MW3-alone-v1.2.1-x86_64.AppImage` | `PM-Rebuild-pm-v1.2.0-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads.

## What's new: MW1: Critical Hit 1.0.4

- **Contract targets where the fight is.** "Destroy the pirate base" on Desolation Ridge put the base beside your start, far from the enemies, so the mission ran on after they were dead. Every contract map is now checked for where its target stands; the contracts that failed were removed or had their sides swapped. A contract you have already accepted keeps its target and can still be won.
- **No freeze on D.** Switching the damage display in combat no longer stalls for a second or two on high-resolution screens.
- **Ice worlds cool you.** The arctic maps now shed heat faster, as MechWarrior 3's snow missions do.
- **Sharper HUD shapes.** Filled shapes and dashed lines are drawn smooth at high resolution, like the rest of the 2D.

Its full notes are in [mw1/dist/RELEASE-NOTES-1.0.4.md](https://github.com/telltaleatheist/mw3-re/blob/mw1-v1.0.4/mw1/dist/RELEASE-NOTES-1.0.4.md).

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
