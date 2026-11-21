# MechWarrior 3 Rebuild 1.2.0

**Ultrawide screens, better joysticks, and a new launcher.** MechWarrior 3 and Pirate's Moon fill 21:9 and 32:9 screens, joysticks with more than ten buttons can use all of them, and a stick plugged in while the game is running now works. The launcher shows each game on a card of its own, is named BattleTech, and opens in a window. MW1: Critical Hit is updated to 1.0.3. Install it over any earlier version: your pilots, saves, settings and mods are kept.

## Downloads

| | All three games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.2.0.exe` (or `MW3-Rebuild-v1.2.0-win64.zip`) | `MW3-alone-Setup-v1.2.0.exe` (or `MW3-alone-v1.2.0-win64.zip`) | `PM-Rebuild-Setup-pm-v1.2.0.exe` (or `mw3pm-pm-v1.2.0-win64.zip`) |
| Mac | `MW3-Rebuild-v1.2.0-mac.dmg` | `MW3-alone-v1.2.0-mac.dmg` | `PM-Rebuild-pm-v1.2.0-mac.dmg` |
| Linux | `MW3-Rebuild-v1.2.0-x86_64.AppImage` | `MW3-alone-v1.2.0-x86_64.AppImage` | `PM-Rebuild-pm-v1.2.0-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads. The [1.1.0 notes](release-notes-1.1.0.md) say how the launcher sets MechWarrior 3 and Pirate's Moon up.

## What's new

### Both games

- **Ultrawide screens.** On a screen wider than 16:9 (21:9, 32:9) the game fills the whole screen, showing more to the sides without stretching, and the graphics options offer 2560x1080, 3440x1440, 3840x1080 and 5120x1440. The cockpits were drawn for 4:3, so at those widths their frames run off the sides or stop short. Thanks to roswellien for the pull request.
- **Joystick buttons 11 to 15 can be bound.** The original read only the first ten buttons, so the extra buttons on today's pads and sticks did nothing.
- **A joystick plugged in after the game started works.** The original looked for a joystick only when it started; now one plugged in later is taken at once, with your joystick setting and axes as you saved them.

### The launcher

Each game has its own card with its badge and its PLAY button. MechWarrior 1's card has PLAY REMAKE for MW1: Critical Hit; its PLAY ORIGINAL, for MechWarrior (1989) as it was released, is greyed out and marked "coming later".

- **Named BattleTech.** The package holds more than MechWarrior 3, so its app (Mac), shortcuts (Windows) and menu entry (Linux) are now called BattleTech. Searching for "MechWarrior 3" still finds it, and MechWarrior 3 alone keeps its name.
- **A window, not fullscreen.** On a Mac, a fullscreen launcher counted as a fullscreen game, which turns on macOS's Game Mode, and Game Mode slowed the file picker to 7 to 12 seconds. The launcher now opens in a window, and its pickers open in about a second. The games still go fullscreen.

### MW1: Critical Hit 1.0.3

Three save slots, a new SETTINGS screen (Game, Display, Sound, Controls, Enhancements), key bindings and joysticks as in MechWarrior 3, Pirate's Moon's Exotic loadouts named, the ultrawide resolutions, the same seed playing the same campaign on every system, and a House that short-pays or voids a contract now named on the results screen with what it kept back. Its full notes are in [mw1/dist/RELEASE-NOTES-1.0.3.md](https://github.com/telltaleatheist/mw3-re/blob/mw1-v1.0.3/mw1/dist/RELEASE-NOTES-1.0.3.md).

### For people building from source

On a Mac, `tools/macos/install_app.sh` builds the app and puts it in Applications in one command, with the app's icon taken from your own Mech3.exe, plus three build fixes. Thanks to dj0nes for the pull request.

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
