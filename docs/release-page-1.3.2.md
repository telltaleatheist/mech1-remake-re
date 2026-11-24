## Download

- **Windows:** [MW3-Rebuild-Setup-v1.3.2.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.2/MW3-Rebuild-Setup-v1.3.2.exe)
- **Mac:** [MW3-Rebuild-v1.3.2-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.2/MW3-Rebuild-v1.3.2-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.3.2-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.2/MW3-Rebuild-v1.3.2-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon and MechWarrior 1 Remake for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading:** install over it, or press UPDATES in your launcher. Your pilots, saves, settings, mods and game files are kept.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **The BattleTech launcher:** one card for each game family, with its games as tabs. It checks for its own updates and shows UPDATE AVAILABLE in the top right when there is one.
- **Your folders move from MicroProse to BattleTech** by themselves on the first start: a rename, nothing copied.
- **Fixes:** a crash starting a campaign mission on Linux (Vulkan running out of memory), low frame rates on Windows 10, the MFB command with no operation point picked, and game files from an older install on another drive.

MechWarrior 3 is 1.3.2, Pirate's Moon 1.2.5, MechWarrior 1 Remake 1.0.8.

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\BattleTech\MechWarrior 3\` on Windows, `~/Library/Application Support/BattleTech/MechWarrior 3/` on Mac, `~/.local/share/BattleTech/MechWarrior 3/` on Linux (Pirate's Moon's is in `BattleTech/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases).

Full notes: [release-notes-1.3.2.md](https://github.com/telltaleatheist/mech3-re/blob/v1.3.2/docs/release-notes-1.3.2.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
