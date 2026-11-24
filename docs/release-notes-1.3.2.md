# MechWarrior 3 Rebuild 1.3.2 (2026-11-24)

The launcher becomes the BattleTech launcher, with a card for each game
family, and it updates itself. Your folders move from MicroProse to
BattleTech. Fixes for a crash starting a mission on Linux, low frame rates
on Windows 10, and the MFB command.

## The BattleTech launcher

- **One card for each family, with its games as tabs:** MECHWARRIOR 3 |
  PIRATE'S MOON, MECHWARRIOR 1 ORIGINAL | REMAKE, and INCEPTION ORIGINAL |
  REMAKE. A game with no release yet says COMING SOON, and turns to DOWNLOAD
  by itself once one is out. The tab you picked last is kept.
- **UPDATE sits on the end of PLAY** when a game has one, with the new
  version beside a download arrow.
- **The launcher checks for its own update** each time it starts, quietly.
  A newer one shows UPDATE AVAILABLE in the top right; the UPDATE screen
  shows what's new, and nothing downloads until you press UPDATE.
- The buttons that fetch a game say DOWNLOAD (they said INSTALL); INSTALL
  FROM FILE is ADD FROM FILE. A disc's own setup still says INSTALL.
- **No more console windows flashing on Windows** while the launcher checks
  for updates.
- Releases come from GitHub or, if GitHub can't be reached, from a backup
  copy of the same releases.

## Your folders move to BattleTech

The games' user folders (pilots, saves, settings, mods, logs) now sit under
**BattleTech** instead of MicroProse, since the family's games come from
different studios:

- Windows: `%APPDATA%\BattleTech\MechWarrior 3\`
- Mac: `~/Library/Application Support/BattleTech/MechWarrior 3/`
- Linux: `~/.local/share/BattleTech/MechWarrior 3/`

Pirate's Moon's is `BattleTech/Pirate's Moon` beside it, and the launcher's
games folder `BattleTech/games`.

The first start of 1.3.2 moves your old folder across once. It's a rename:
nothing is copied or merged, and a note is left in the old place pointing to
the new one, so an older version of any of the games still finds your data.
If the new folder is already in use, the old one is left alone. A game
installed somewhere you chose stays where it is.

## Fixes

- **A crash starting a campaign mission or Tutorial 2** with Vulkan
  (Linux, issue #7): each texture uploaded kept its own 1.2 MB of graphics
  memory, about 4 GB in the second campaign mission, until the driver ran
  out. Texture uploads now share one buffer: the same mission takes about
  330 MB.
- **Low frame rates on Windows 10** with Direct3D 12 (issue #2): the 2D
  layer (menus, HUD, text) was uploaded in a shape Windows 10's Direct3D 12
  can't take directly, so it went through a slow extra copy every frame. It's
  now laid out the way Direct3D 12 wants it. If the game still runs slowly,
  `mw3.log` now says how many frames a second it shows and where the time
  goes; please attach it to the issue.
- **The MFB command** (issue #10): pressing M twice with no operation point
  picked sent your mobile field bases nowhere. The second press now waits
  until you pick a point with N. (Bug fixes on.)
- **Game files from an older install on another drive** (issue #10): the
  game now uses its own folder's files first. An older copy named in your
  settings is offered and copied in, never used in place or changed.
- Text looks the same on Windows, Mac and Linux (the font library is now the
  same version on all three).
- `mw3.log` names your graphics card and driver, which helps with reports.

## Versions

- Launcher and MechWarrior 3: 1.3.2.
- Pirate's Moon: 1.2.5 (the same fixes and the folder move).
- MechWarrior 1 Remake: 1.0.8 (unchanged).

### Upgrading

Install over 1.3.1, or press UPDATES in your launcher, then UPDATE on each
game. Your pilots, saves, settings, mods and game files are kept, and move
to the BattleTech folder by themselves.

## For developers

`docs/program.md`, The family folder (the move, the pointer, `MW3_USUAL_FOLDER`);
`docs/launcher.md` (family cards, the self-update, both homes);
`docs/performance.md`, Windows 10: the 256-byte rows; `MW3_GPU_DEBUG=1` asks
SDL for the graphics API's validation, `MW3_GPU_COUNT=N` counts textures,
uploads and memory.
