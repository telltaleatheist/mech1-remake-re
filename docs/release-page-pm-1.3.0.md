Pirate's Moon on its own (mw3pm), without the launcher. The BattleTech launcher installs and updates it from this page.

**Most players want the launcher** (it installs MechWarrior 3, Pirate's Moon, MechWarrior 1 Remake and The Crescent Hawk's Inception and keeps them up to date): [the BattleTech launcher 1.10.0](https://github.com/telltaleatheist/mw3-re/releases/tag/v1.10.0).

## Download

- **Windows:** `PM-Rebuild-Setup-pm-v1.3.0.exe` (or `mw3pm-pm-v1.3.0-win64.zip`)
- **Mac:** `PM-Rebuild-pm-v1.3.0-mac.dmg`
- **Linux:** `PM-Rebuild-pm-v1.3.0-x86_64.AppImage`

You need your own Pirate's Moon: no game files are included. Installing over an earlier version keeps your pilots, saves, settings and mods.

## What's new

- **Started without its game files, Pirate's Moon offers to download them:** started on its own (not from the launcher) with no game files, it looks for a copy at once, and if it finds none asks whether to download Pirate's Moon's original files, with DOWNLOAD THE GAME FILES, PICK MY OWN COPY and PICK A FOLDER. The download is checked, installed, then deleted.
- **A Pirate's Moon disc in a drive is found** when no installed copy is, by the game's search and by the launcher, so nothing is downloaded that your disc already has. A MechWarrior 3 disc is not offered for Pirate's Moon.
- **A compact copy of the original game files:** reverse-engineered.com hosts a smaller copy of Pirate's Moon's original files (167 MB against the disc image's 323 MB), downloaded when the Internet Archive cannot be reached. The setup checks every file before it installs anything, and a game installed from it is the same as one installed from the disc, file for file, music included.
- **A dropped download picks up where it stopped** instead of starting over.
- **A crash at quit after the sound output changed** (headphones or a headset plugged in or out while playing) is fixed by SDL 3.4.20.
- Nothing in play changes.

Full notes: [release-notes-1.7.0.md](https://github.com/telltaleatheist/mech3-re/blob/v1.7.0/docs/release-notes-1.7.0.md). `SHA256SUMS` and `SHA256SUMS.sig` are under Assets.
