# MechWarrior 3 Rebuild 1.1.2

**Both games are now ported whole.** Every function in MechWarrior 3's and Pirate's Moon's programs is in the rebuild: the parts that run in play, and the developers' own tools and leftover code, kept as the original has them even where the released game never reaches them. Pirate's Moon gets a long list of its own behaviours that 1.1.1 still played MechWarrior 3's way. Both games get sharper distant textures and a handful of fixes. Install it over 1.1.1, 1.1.0 or any 1.0.x: your pilots, saves, settings and mods are kept.

## Downloads

The same packages as 1.1.1. The combined one holds the launcher, MechWarrior 3 and Pirate's Moon, and it's the one the launcher's UPDATES button finds.

| | Both games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.1.2.exe` (or `MW3-Rebuild-v1.1.2-win64.zip`) | `MW3-alone-Setup-v1.1.2.exe` (or `MW3-alone-v1.1.2-win64.zip`) | `PM-Rebuild-Setup-pm-v1.1.2.exe` (or `mw3pm-pm-v1.1.2-win64.zip`) |
| Mac | `MW3-Rebuild-v1.1.2-mac.dmg` | `MW3-alone-v1.1.2-mac.dmg` | `PM-Rebuild-pm-v1.1.2-mac.dmg` |
| Linux | `MW3-Rebuild-v1.1.2-x86_64.AppImage` | `MW3-alone-v1.1.2-x86_64.AppImage` | `PM-Rebuild-pm-v1.1.2-x86_64.AppImage` |

You still need your own games: none of the original games' files are in these downloads. The [1.1.0 notes](release-notes-1.1.0.md) say how the launcher sets them up.

## What's new

### Both games

- **Distant ground and walls no longer shimmer.** The original's textures came in one size, so far away their detail broke into flickering speckles as you moved. Each texture now has smaller copies that the graphics card blends between at a distance, with anisotropic filtering so ground seen at a slant keeps its grain. Trees, shrubs and fences keep their outline. The menus, the HUD and the 2D screens are drawn as before. This one is ours, not the original's.
- **The whole original is in.** The developers' debug menu and dialogs, the Battlesport editor's remaining pages, the debug keys, the screenshot command, the unused model, texture and AI code, the script commands no shipped mission uses: each is ported and checked against the original program in an emulator. The released game never reaches most of them, so neither does the rebuild. They're kept so the rebuild is a complete record of what the original holds.
- **Mods can add motions as loose `.zan` files**, as the original's own path for them allows ([modding notes](modding.md)).

### Pirate's Moon

Each of these was read in the expansion's program and checked against it in an emulator.

- **Its menus, briefings and screens behave like its own.** The intro plays INTRO alone. The main menu's idle minute just restarts with no attract movie, and P opens Pirate Missions. The menu backgrounds and title scroll by time. Skipping a briefing finishes every step it had left. Escape in the pause menu resumes like RESUME.
- **Instant Action keeps your line-up.** Enemies are kept by their place in the list, a world you pick goes straight to your pilot's game, and mechs from a line-up go into the setup when you come back from the mech lab.
- **Ctrl+P saves a screenshot**, as in the expansion, with no dialog: `mw3` plus a number, `.bmp`, in the game's settings folder.
- **The look of a mission.** It fades in from black. Underwater fog uses the expansion's own fog mode. The camera's ground check starts 1024 m up. Low object detail swaps to the low models sooner. The top-down cameras are tilted as in the expansion. Light nodes and per-node lighting work. Textures are as sharp as Pirate's Moon's own MipBias setting asks, now that there are smaller texture copies to choose from.
- **Mechs and weapons.** An Elemental's footfall leaves no footprint or step sound. The headlight follows the torso when its detail changes. Weapon groups step past ones with nothing to fire. The fire mode is spoken a second after the key. A mech's weapons are let go properly when it dies. Shots start and end on the expansion's timing.
- **Sensors and the AI.** A C3 master senses its C3 slaves at any range. The AI aims from the weapon's own firepoint, with the expansion's spread. In multiplayer, the wide radar needs the lobby's RADAR option.
- **Multiplayer and field bases.** Field bases dock in every game. The last base's death cancels a repair in progress. The chat line lets go of keys held while you type. Salvage loads the mech lab's world when none is up.
- **Screen sizes up to 1600x1200** are listed, and Shift+V skips a size the card refuses.
- **Music** plays a disc with six or more tracks to its end, and the movie scripts use the expansion's CD commands.

### MechWarrior 3

- **An older-form `sounds.zrd`** (SYNTAX 1, or with no SYNTAX key) loads its sounds, as MechWarrior 3's program does. Before, only the shipped form did. (Pirate's Moon's program ignores the SYNTAX key, and so does the rebuild there.)
- **The Vulture's DESERT camo has its cockpit canopy.** One of its textures is missing from the game's data, and the developers' special case for it had the name misspelt, so the canopy's top showed red and green checks. It now takes the texture they meant. It's one of the small fixes on the UPGRADES page. Pirate's Moon's data has the texture, so it needs no fix.

## Fixed

- Leaving the pause menu with Escape no longer silences the mission (the wind, the ocean, your mech's hum). Escape brings the sounds back as RESUME does.
- A missile locked on a mech that leaves the game (a network player quitting) no longer follows a ghost or crashes the game; it loses the lock and flies straight on.
- A damaged or hand-edited world or model file can no longer eat all the memory or crash the game. A model that points past its own data is left out, and the log names it.
- Checking a mod (the MODS screen's add and import, its report, `--check-mod`) while a world is loaded no longer crashes or adds the mod's lights and sounds to that world.
- Pirate's Moon: a vehicle bumping only into a mech or another vehicle no longer risks a crash, and Shift+V can no longer hang the game on a card that refuses every size.
- Linux: converting the movies no longer seems to stop halfway, and Cancel works throughout. Without Vulkan, the launcher says what to install before PLAY.

## Still known

As in [1.1.0](release-notes-1.1.0.md#still-known).

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.
