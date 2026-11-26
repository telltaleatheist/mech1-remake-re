## Download

- **Windows:** [MW3-Rebuild-Setup-v1.6.0.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.6.0/MW3-Rebuild-Setup-v1.6.0.exe)
- **Mac:** [MW3-Rebuild-v1.6.0-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.6.0/MW3-Rebuild-v1.6.0-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.6.0-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.6.0/MW3-Rebuild-v1.6.0-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon, MechWarrior 1 Remake, MechWarrior 1 Rebuild and The Crescent Hawk's Inception for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading:** install over it, or press UPDATE AVAILABLE in your launcher. Your pilots, saves, settings, mods and game files are kept.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **The mouse wheel scrolls the menus' lists** (issue #20): the Mech Lab's, the pilots, the saved games and the lobby's lists, and the commands on the controls screen. Under SMALL FIXES.
- **Each game sets itself up.** MechWarrior 3 and Pirate's Moon now find, install and complete their own files, as the other games already did; the launcher shows their screens as before, holds no game code any more, and is less than half the size to download.
- **The launcher has its own menu sounds,** and new games can appear in it without a launcher update.
- **PLAY and a start on its own open the same copy** of MechWarrior 3 when you have more than one.

**After updating the launcher, press UPDATE ALL:** it sets up only games of this version or newer.

MechWarrior 3 is 1.6.0. Pirate's Moon 1.2.11 and MechWarrior 1 Remake 1.1.3 have the same changes, and the Remake keeps its menus 4:3 on screens narrower than 16:9 with WIDE MENUS on (issue #1); the launcher also offers MechWarrior 1 Rebuild and Inception.

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\BattleTech\MechWarrior 3\` on Windows, `~/Library/Application Support/BattleTech/MechWarrior 3/` on Mac, `~/.local/share/BattleTech/MechWarrior 3/` on Linux (Pirate's Moon's is in `BattleTech/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases), [MechWarrior 1 Rebuild](https://github.com/telltaleatheist/mech1-re/releases), [Inception](https://github.com/telltaleatheist/inception-re/releases).

Full notes: [release-notes-1.6.0.md](https://github.com/telltaleatheist/mech3-re/blob/v1.6.0/docs/release-notes-1.6.0.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
