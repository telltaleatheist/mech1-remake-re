# MechWarrior 3 Rebuild 1.2.2

**Fixes from your bug reports, for all three games.** MechWarrior 3 and Pirate's Moon get a raw mouse and the fixes below; MechWarrior 1 Remake (formerly MW1: Critical Hit) is now 1.0.5. Install it over any earlier version: your pilots, saves, settings and mods are kept.

## Downloads

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.2.2.exe` (or `MW3-Rebuild-v1.2.2-win64.zip`) | `MW3-alone-Setup-v1.2.2.exe` (or `MW3-alone-v1.2.2-win64.zip`) | `PM-Rebuild-Setup-pm-v1.2.1.exe` (or `mw3pm-pm-v1.2.1-win64.zip`) |
| Mac | `MW3-Rebuild-v1.2.2-mac.dmg` | `MW3-alone-v1.2.2-mac.dmg` | `PM-Rebuild-pm-v1.2.1-mac.dmg` |
| Linux | `MW3-Rebuild-v1.2.2-x86_64.AppImage` | `MW3-alone-v1.2.2-x86_64.AppImage` | `PM-Rebuild-pm-v1.2.1-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads.

## What's new: MechWarrior 3 and Pirate's Moon

- **Raw mouse.** The mouse now moves the torso by exactly how far you move it, as the original's DirectInput did, without your system's pointer acceleration on top. It is on by default; RAW MOUSE on the Upgrades page turns it off. MOUSE SENSITIVITY now goes up to 5.0 (was 3.0), for the extra range raw movement needs. (#15)
- **The power key works.** A key bound to SHUTDOWN (or any command that had no key before) during a mission now works at once, not only after restarting the game. (#4)
- **Rumble pads bind.** A pad with force feedback (DualSense, Xbox pads) could rumble but take no bindings when the game started without focus or lost it; it now comes back with the window's focus. (#16)
- **Free look on the middle mouse button** works when that command has no key. (#10)
- **Torso auto return with the mouse** no longer swings the torso past its limit and round. Under Small fixes. (#10)
- **Linux: no crash or freeze** at a mission's start or in the mech lab on graphics drivers whose screen format the fade and pause pictures did not expect. (#5, #7)
- **A mouse line in mw3.log** at each mission's end, so a report about mouse feel shows where the movement goes.
- **Pirate's Moon started on its own** without a Pirate's Moon install now asks for Pirate's Moon's folder, not MechWarrior 3's disc. (#13)
- **Updates** are now checked on both this page and MechWarrior 3's own new page (github.com/telltaleatheist/mech3-re), ready for the games' move to their own pages.

## What's new: MechWarrior 1 Remake 1.0.5

The game is now called MechWarrior 1 Remake (it was MW1: Critical Hit); your saves and settings stay where they were.

- **'Mech prices vary a little** at each market restock, within 5% of the usual; buying and selling at a world use the same price, so you can't buy and resell for profit.
- **Out of bounds on every map:** every contract map now has MechWarrior 3's yellow and red lines on the radar; crossing the red one loses the mission.
- **Arctic defence missions can be lost:** the structure you defend falling now ends the mission, as it should.
- The same fixes as MechWarrior 3 above (raw mouse, rumble pads, new key bindings, the middle mouse button, torso return, the Linux crash), and new credits.

Its full notes are in [mw1/dist/RELEASE-NOTES-1.0.5.md](https://github.com/telltaleatheist/mw3-re/blob/mw1-v1.0.5/mw1/dist/RELEASE-NOTES-1.0.5.md).

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
