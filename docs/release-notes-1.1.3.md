# MechWarrior 3 Rebuild 1.1.3

**Three games on one launcher.** MW1: Critical Hit 1.0.1, a fan remake of MechWarrior (1989) on the rebuild's engine, joins MechWarrior 3 and Pirate's Moon in the combined package, on the launcher's MECHWARRIOR 1 line. Both rebuilt games get a music fix, a stuck weapon sound fixed, and memory that no longer grows mission after mission. Install it over 1.1.2, 1.1.1, 1.1.0 or any 1.0.x: your pilots, saves, settings and mods are kept.

## Downloads

The combined package now holds the launcher, MechWarrior 3, Pirate's Moon and MW1: Critical Hit, and it's the one the launcher's UPDATES button finds. MechWarrior 3 alone and Pirate's Moon alone are as before; neither carries MW1.

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.1.3.exe` (or `MW3-Rebuild-v1.1.3-win64.zip`) | `MW3-alone-Setup-v1.1.3.exe` (or `MW3-alone-v1.1.3-win64.zip`) | `PM-Rebuild-Setup-pm-v1.1.3.exe` (or `mw3pm-pm-v1.1.3-win64.zip`) |
| Mac | `MW3-Rebuild-v1.1.3-mac.dmg` | `MW3-alone-v1.1.3-mac.dmg` | `PM-Rebuild-pm-v1.1.3-mac.dmg` |
| Linux | `MW3-Rebuild-v1.1.3-x86_64.AppImage` | `MW3-alone-v1.1.3-x86_64.AppImage` | `PM-Rebuild-pm-v1.1.3-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads. The [1.1.0 notes](release-notes-1.1.0.md) say how the launcher sets MechWarrior 3 and Pirate's Moon up.

## What's new

### MW1: Critical Hit 1.0.1

MechWarrior (1989)'s mercenary campaign on MechWarrior 3's combat: the five Houses, the contracts, the Bar, the 'Mech markets and the story that clears your family's name, fought in 3D on MechWarrior 3's and Pirate's Moon's worlds with their Mech Lab, salvage and every one of their 24 'Mechs. New art for every screen between missions and an original soundtrack. It needs your own MechWarrior (1989), MechWarrior 3 and Pirate's Moon; the MECHWARRIOR 1 line is ready once the launcher has found the other two. The launcher looks for your MechWarrior (1989) in the usual places; if it isn't there, a click takes its BTECH.EXE, its folder or the download it came in (a .zip is unpacked into the launcher's own folder, nothing of yours is changed). Its own read-me and notices are beside it in the package (`README-MW1.txt`, `THIRD-PARTY-MW1.txt`). MW1's version shows on its launcher line.

It's a free fan project, not a decompile of MechWarrior and not affiliated with or endorsed by the game's owners.

### Both games

- **A music track that should repeat now repeats, and the music plays on to the next track.** A menu's theme played once and went quiet, and a disc never went on, whenever the ripped track's sample rate wasn't the sound card's (our music player never noticed the file had ended). This one is ours, not the original's.
- **A machine gun's sound stops when you stop firing.** Firing a group together, then changing the fire mode while still holding the trigger, left the group's other weapons sounding to the end of the mission. This one is the original's; the fix is on the UPGRADES page's small fixes.

## Fixed

- Memory no longer grows with every mission played in one sitting: each mission's unload now frees its animated textures, and in Pirate's Moon its models (about 7 MB a mission before).
- Pirate's Moon: with more than one lancemate, lance orders go to the one you picked; they still went to the first.

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
