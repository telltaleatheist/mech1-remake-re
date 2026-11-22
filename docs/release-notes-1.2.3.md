# MechWarrior 3 Rebuild 1.2.3

**Setting up your games is simpler, and always your choice.** All three games now copy what they need from your own copy into their own folder, so the zip, disc image or folder you gave them can be deleted afterwards. A copy the game finds by itself is only offered, never used without asking. MechWarrior 1 Remake is now 1.0.6. Install it over any earlier version: your pilots, saves, settings and mods are kept.

## Downloads

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.2.3.exe` (or `MW3-Rebuild-v1.2.3-win64.zip`) | `MW3-alone-Setup-v1.2.3.exe` (or `MW3-alone-v1.2.3-win64.zip`) | `PM-Rebuild-Setup-pm-v1.2.2.exe` (or `mw3pm-pm-v1.2.2-win64.zip`) |
| Mac | `MW3-Rebuild-v1.2.3-mac.dmg` | `MW3-alone-v1.2.3-mac.dmg` | `PM-Rebuild-pm-v1.2.2-mac.dmg` |
| Linux | `MW3-Rebuild-v1.2.3-x86_64.AppImage` | `MW3-alone-v1.2.3-x86_64.AppImage` | `PM-Rebuild-pm-v1.2.2-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads.

## What's new: MechWarrior 3 and Pirate's Moon

- **Your files are copied in.** An installed copy you point the game at is now copied into the game's own folder, as a disc image or a download already was, so you can delete your copy afterwards. Your own files are only read.
- **A game folder next to the program is found**, whatever it is called: put your MechWarrior 3 (or Pirate's Moon) folder beside the program and start it.
- **What the game finds is only offered.** "USE THESE GAME FILES?" shows the copy it found on one button: INSTALL copies it in, and clicking the button lets you pick other files instead. Nothing is used without your say.
- **Choose again at any time.** GAME FILES in the launcher always opens the picker, and holding **Shift** while a game starts (without the launcher) opens the same screen: PLAY goes on with the files you have, FIND THE GAME picks others. (On Linux this needs X11; under Wayland alone, start the game with `--setup`.)
- **Pirate's Moon is set up the same way**, in the launcher and when started on its own, always on its own screen, never MechWarrior 3's.
- **MechWarrior (1989)**, for MechWarrior 1 Remake, is taken the same way: copied in, and a copy the launcher finds only offered.
- Fixed: after setting up new files, the launcher could start the game from the old copy when your settings named a different one.

## What's new: MechWarrior 1 Remake 1.0.6

The new name on the window and the save folder, and contracts you can win the way their briefings say.

- **The new name everywhere:** the window, the log and the game's message boxes say MechWarrior 1 Remake, and settings and saves live in a "MechWarrior 1 Remake" folder. On the first start the old "MechWarrior 1 Critical Hit" folder is moved there once (renamed, or copied, checked byte for byte and only then removed); nothing is merged or overwritten, and if the move can't be done that run plays from the old folder and tries again next time.
- **Reach contracts can be reached around the enemy:** every contract that asks you to reach a place has a walkable way there that keeps clear of where the enemy starts, each driven to in testing with the enemy alive. Three MechWarrior 3 maps now send you to a barracks, warehouses or a dock instead of a defended tower; one more, and one Pirate's Moon map, now offer only their attack contracts.
- **The first arctic map has a defence contract again**, with the start moved south of the base and one patrol moved out of the way to the depot.
- **Out-of-bounds lines are drawn around every place an enemy can start**, on Pirate's Moon's and the arctic maps.
- The same setup changes as MechWarrior 3 above: your files copied in, a found copy only offered.

Its full notes are in [mw1/dist/RELEASE-NOTES-1.0.6.md](https://github.com/telltaleatheist/mech1-remake-re/blob/mw1-v1.0.6/mw1/dist/RELEASE-NOTES-1.0.6.md).

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
