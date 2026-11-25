## Download

- **Windows:** [MW3-Rebuild-Setup-v1.4.1.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.4.1/MW3-Rebuild-Setup-v1.4.1.exe)
- **Mac:** [MW3-Rebuild-v1.4.1-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.4.1/MW3-Rebuild-v1.4.1-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.4.1-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.4.1/MW3-Rebuild-v1.4.1-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon, MechWarrior 1 Remake and The Crescent Hawk's Inception for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading:** install over it, or press UPDATES in your launcher. Your pilots, saves, settings, mods and game files are kept.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **Inception's game files can be changed at any time.** In 1.4.0, FIND THE GAME found the copy already in use and went back to the main screen. GAME FILES, on Inception's settings page, now has PICK A FILE and PICK A FOLDER.
- **PICK A FILE on every find screen**, beside PICK A FOLDER. It opens on all files, zips too.
- **A zip of Inception given to LINK AN INSTALL** is taken as its game files instead of being refused.
- Inception's GAME FILES says which version is in use, for example DOS 1.03.

MechWarrior 3 is 1.4.1, Pirate's Moon 1.2.9, MechWarrior 1 Remake 1.1.1 and Inception 1.0.0 (unchanged).

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\BattleTech\MechWarrior 3\` on Windows, `~/Library/Application Support/BattleTech/MechWarrior 3/` on Mac, `~/.local/share/BattleTech/MechWarrior 3/` on Linux (Pirate's Moon's is in `BattleTech/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases).

Full notes: [release-notes-1.4.1.md](https://github.com/telltaleatheist/mech3-re/blob/v1.4.1/docs/release-notes-1.4.1.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
