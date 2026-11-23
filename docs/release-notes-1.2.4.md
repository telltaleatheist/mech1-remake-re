# MechWarrior 3 Rebuild 1.2.4

**A small fix release.** Standing up from a crouch now obeys the sound volume, in MechWarrior 3 and Pirate's Moon (now pm-v1.2.3). MechWarrior 1 Remake stays at 1.0.6. Install it over any earlier version: your pilots, saves, settings and mods are kept.

## Downloads

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.2.4.exe` (or `MW3-Rebuild-v1.2.4-win64.zip`) | `MW3-alone-Setup-v1.2.4.exe` (or `MW3-alone-v1.2.4-win64.zip`) | `PM-Rebuild-Setup-pm-v1.2.3.exe` (or `mw3pm-pm-v1.2.3-win64.zip`) |
| Mac | `MW3-Rebuild-v1.2.4-mac.dmg` | `MW3-alone-v1.2.4-mac.dmg` | `PM-Rebuild-pm-v1.2.3-mac.dmg` |
| Linux | `MW3-Rebuild-v1.2.4-x86_64.AppImage` | `MW3-alone-v1.2.4-x86_64.AppImage` | `PM-Rebuild-pm-v1.2.3-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads.

## What's new: MechWarrior 3 and Pirate's Moon

- **Standing up from a crouch obeys the sound volume.** The original game plays that sound at your 'Mech's position without making it a 3D sound, and in that case its sound code never sets the volume, so it played at full volume however low the sound was set. Other sounds played the same way are fixed too (reported in issue #10).

Checked in Pirate's Moon's mission 5 while preparing this release, and unchanged because it already works as intended: a lancemate with jump jets jumps the chasm, whether it's over 50 tons or not, and whether the bridge is still up or not.

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log`. It's in the game's data folder, not the folder you installed to:

| | MechWarrior 3 | Pirate's Moon |
|---|---|---|
| Windows | `%APPDATA%\MicroProse\MechWarrior 3\` | `%APPDATA%\MicroProse\Pirate's Moon\` |
| Mac | `~/Library/Application Support/MicroProse/MechWarrior 3/` | `~/Library/Application Support/MicroProse/Pirate's Moon/` |
| Linux | `~/.local/share/MicroProse/MechWarrior 3/` | `~/.local/share/MicroProse/Pirate's Moon/` |

On Windows, paste the path into Explorer's address bar. If you started the game again since the problem, send `mw3.old.log` as well.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
