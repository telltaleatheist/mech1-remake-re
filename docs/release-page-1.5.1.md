## Download

- **Windows:** [MW3-Rebuild-Setup-v1.5.1.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.1/MW3-Rebuild-Setup-v1.5.1.exe)
- **Mac:** [MW3-Rebuild-v1.5.1-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.1/MW3-Rebuild-v1.5.1-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.5.1-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.1/MW3-Rebuild-v1.5.1-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon, MechWarrior 1 Remake, MechWarrior 1 Rebuild and The Crescent Hawk's Inception for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading:** install over it, or press UPDATE AVAILABLE in your launcher. Your pilots, saves, settings, mods and game files are kept.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **Fixed: on a Mac the launcher could use tens of gigabytes of memory** while a full-screen game covered its window. It now keeps to the display's pace, and slower while covered or minimised.
- MechWarrior 1 Rebuild and Inception are found when installed by their own Windows Setups, and the Rebuild when it was started on its own. PLAY sets either one up on the launcher's own screens.

MechWarrior 3 plays as in 1.5.0.

MechWarrior 3 is 1.5.1. Pirate's Moon 1.2.9 and MechWarrior 1 Remake 1.1.1 are unchanged; the launcher also offers MechWarrior 1 Rebuild 1.0.0 and Inception 1.0.1.

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\BattleTech\MechWarrior 3\` on Windows, `~/Library/Application Support/BattleTech/MechWarrior 3/` on Mac, `~/.local/share/BattleTech/MechWarrior 3/` on Linux (Pirate's Moon's is in `BattleTech/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases), [MechWarrior 1 Rebuild](https://github.com/telltaleatheist/mech1-re/releases), [Inception](https://github.com/telltaleatheist/inception-re/releases).

Full notes: [release-notes-1.5.1.md](https://github.com/telltaleatheist/mech3-re/blob/v1.5.1/docs/release-notes-1.5.1.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
