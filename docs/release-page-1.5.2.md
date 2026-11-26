## Download

- **Windows:** [MW3-Rebuild-Setup-v1.5.2.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.2/MW3-Rebuild-Setup-v1.5.2.exe)
- **Mac:** [MW3-Rebuild-v1.5.2-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.2/MW3-Rebuild-v1.5.2-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.5.2-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.5.2/MW3-Rebuild-v1.5.2-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon, MechWarrior 1 Remake, MechWarrior 1 Rebuild and The Crescent Hawk's Inception for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading:** install over it, or press UPDATE AVAILABLE in your launcher. Your pilots, saves, settings, mods and game files are kept.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **RAW MOUSE is on the Game Options screen, under MOUSE SENSITIVITY** (it was on the Upgrades page). On the Mac it is off to start, and this update switches it off once there: a Magic Mouse was several times too fast with it. Turn it back on if you liked it; your choice is kept from then on.
- **MOUSE SENSITIVITY goes down to 0.1.**
- **Fixed: on a Mac the Dock could show the launcher as a folder** ("Contents") after it updated itself. This takes effect from the update after this one; if your Dock shows a folder, open the launcher from Applications once.

MechWarrior 3 is 1.5.2, with the mouse changes. Pirate's Moon 1.2.10 and MechWarrior 1 Remake 1.1.2 have them too, and the Remake's enemies on the two ice maps no longer stand still; the launcher also offers MechWarrior 1 Rebuild 1.0.1 and Inception 1.0.1.

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\BattleTech\MechWarrior 3\` on Windows, `~/Library/Application Support/BattleTech/MechWarrior 3/` on Mac, `~/.local/share/BattleTech/MechWarrior 3/` on Linux (Pirate's Moon's is in `BattleTech/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases), [MechWarrior 1 Rebuild](https://github.com/telltaleatheist/mech1-re/releases), [Inception](https://github.com/telltaleatheist/inception-re/releases).

Full notes: [release-notes-1.5.2.md](https://github.com/telltaleatheist/mech3-re/blob/v1.5.2/docs/release-notes-1.5.2.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
