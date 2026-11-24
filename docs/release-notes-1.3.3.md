# MechWarrior 3 Rebuild 1.3.3 (2026-11-24)

The launcher lists the games as panels, the same look as the download page.

## The BattleTech launcher

- **One panel for each game:** MechWarrior 3, Pirate's Moon, MechWarrior 1
  Reverse Engineered, MechWarrior 1 Remake and The Crescent Hawk's Inception.
  Each shows its badge, whether it's installed and which version, a line
  about the game, and its button: PLAY, PLAY with UPDATE on its end,
  DOWNLOAD, or COMING SOON.
- **The list scrolls** with the mouse wheel or a trackpad, by dragging the
  amber scrollbar on the right, or with the arrow keys and a gamepad. About
  three panels show at once; a shade at the top or bottom shows there is
  more.
- **The panel you point at lights up** in amber.
- **The Crescent Hawk's Inception has its own badge** (a hawk). It's one
  panel for now, and gets a second one for the remake once that has a
  release.
- The family cards and their tabs are gone.

## For modders

`docs/discoveries.md` has two new notes: a VELOCITY typed as a whole number
reads as almost zero, and a SALVO's count multiplies MASS ammo, so removing
one can make a loadout overweight.

## Versions

- Launcher and MechWarrior 3: 1.3.3.
- Pirate's Moon: 1.2.5 (unchanged).
- MechWarrior 1 Remake: 1.0.8 (unchanged).

### Upgrading

Install over 1.3.2, or press UPDATE AVAILABLE in your launcher. Your pilots,
saves, settings, mods and game files are kept.

## For developers

`docs/launcher.md`, The panels; the panels' small lines use a second font,
`assets/launcher/font_body.ttf` (Saira Condensed Medium, SIL OFL).
`MW3_LAUNCHER_LIST_Y=<px>` starts the list scrolled, for screenshots.
