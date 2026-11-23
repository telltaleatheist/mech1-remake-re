## Download

- **Windows:** [MW3-Rebuild-Setup-v1.3.0.exe](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.0/MW3-Rebuild-Setup-v1.3.0.exe)
- **Mac:** [MW3-Rebuild-v1.3.0-mac.dmg](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.0/MW3-Rebuild-v1.3.0-mac.dmg)
- **Linux:** [MW3-Rebuild-v1.3.0-x86_64.AppImage](https://github.com/telltaleatheist/mw3-re/releases/download/v1.3.0/MW3-Rebuild-v1.3.0-x86_64.AppImage)

This is the launcher (named BattleTech). It installs MechWarrior 3, Pirate's Moon and MechWarrior 1 Remake for you and keeps them up to date.

## Before you install

- **You need your own games.** No game files are included: each game sets itself up from your install, disc or disc image, and copies what it needs.
- **Upgrading from any earlier version:** install over it, or press UPDATES in your launcher. Your pilots, saves, settings, mods and game files are kept. Then press INSTALL on each game you play; that's a one-time download.
- **Mac:** the first time, macOS stops the app because it isn't signed. Open System Settings > Privacy & Security and click Open Anyway.
- **Linux:** make the AppImage executable, then run it.

## What's new

- **The launcher downloads the games.** Each game's card shows **INSTALL**, or **UPDATE TO x.y.z** when a newer version is out, and gets the game from that game's own release. Nothing downloads until you press it, and every download is checked against its release's checksums before anything is installed.
- **Offline,** your installed games just play. **INSTALL FROM FILE** installs a game's download you already have: one from a game's Releases page, or a modder's build.
- **MechWarrior 1 Remake has a download of its own** (1.0.7).

MechWarrior 3 is 1.3.0 (the same game as 1.2.4) and Pirate's Moon stays at 1.2.3.

**Reporting a bug:** attach `mw3.log` from the game's data folder: `%APPDATA%\MicroProse\MechWarrior 3\` on Windows, `~/Library/Application Support/MicroProse/MechWarrior 3/` on Mac, `~/.local/share/MicroProse/MechWarrior 3/` on Linux (Pirate's Moon's is in `MicroProse/Pirate's Moon` beside it). The launcher's own log is `mw3launcher.log` in the MechWarrior 3 folder.

Each game also has its own page: [MechWarrior 3](https://github.com/telltaleatheist/mech3-re/releases), [Pirate's Moon](https://github.com/telltaleatheist/mech3-pm-re/releases), [MechWarrior 1 Remake](https://github.com/telltaleatheist/mech1-remake-re/releases).

Full notes: [release-notes-1.3.0.md](https://github.com/telltaleatheist/mech3-re/blob/v1.3.0/docs/release-notes-1.3.0.md). Each game on its own, a zip instead of the Windows installer, and `SHA256SUMS` are under Assets below.
