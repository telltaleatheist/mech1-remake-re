# The setup menu inside the game

The setup kit's in-game menu (`extern/re-setup-kit`, `rekit/sdl/game_menu.h`; the kit's
`docs/plan-in-game-menu.md`) wired into MechWarrior 3 (2026-11-27), and on this branch into Pirate's Moon (the
merge from `main` the same day; its values are under [The hooks](#the-hooks)). It is ours: the
original has nothing like it. The wiring is in `src/frontend/sdl/mw3_main.cpp`
(`menu_hooks`, `MenuPause`, `menu_pass`); nothing in the ported tree changes.

## How it sits in the program

- **Events.** The pump's `overlay_event` gives every SDL event to the menu first. The
  menu keeps what is its own (its keys, everything while it is open) and hands the
  rest back through `MenuHooks::deliver`: the game editor's ImGui, then zInput, as
  before.
- **Drawing.** `SdlRender3D::before_present` runs at each present: the layer is
  cleared and `GameMenu::frame()` draws the menu, or its hint, into an RGBA image
  (`MenuHooks::draw`). `SdlRender3D::set_layer` uploads it when its serial changes
  and `draw_layer` blends it over the game's frame after the editor's overlay. This
  uses the same shaders as the game's 2D (`fs_tex` with alpha modulate), so it is the
  same on Metal, D3D12 and Vulkan. A capture (`--shot`, the checks) blends the layer
  in on the CPU (`layer_over`), so shots show what the player sees.
- **Mouse.** While the menu is open, `SdlInput::menu_up` releases relative mode and
  shows the cursor. When it closes, the game's own capture comes back.

## The pause

The menu pauses the game the way MechWarrior 3 already does when its window loses
the focus: `App::on_window_activate(0)` (0x00590c00). The state hears `on_activate`,
the app goes inactive (0x0058fa80, the game editor's Pause box uses the same call)
and input is unacquired. While the app is inactive no state updates, so no frame runs
and the mission's clock (`Missions::time`, 0x0048f470) stands still. The timer is
still read each pass, so the first frame after the menu steps a frame's time, not the
time away (the step is clamped to 0.125 s anyway). The game's sounds go quiet as
they do when the window is away (`SdlSoundOutput::set_focused`).

- While the menu is up, `SdlPump::hold_inactive` stops a focus-gained event from
  waking the game.
- When the menu closes, the game is woken only if it was active before and the
  window has the focus (or `stays_active`). Otherwise it stays as the focus says.
- While held, the game draws nothing. `menu_pass` (from the pump's `each_pass`)
  presents the held frame again about 60 times a second with the menu over it
  (`idle_wait_ms_` is 16 while the menu is up; it is the app's 100 otherwise).
- In a network game, `run_while_inactive` keeps the game running, as it does on any
  focus loss there (W45). The menu then sits over live frames, and the subtitle still
  says the game is paused. A follow-up could have the kit say otherwise there.

The pause screen (Escape) is a different thing, a visible screen of the game's own,
and the menu does not use it. The exception is UPGRADES in a mission, below.

## The hooks

| Hook | MechWarrior 3's |
|---|---|
| title, version | "MechWarrior 3", `MW3_VERSION`; the theme's header "MECHWARRIOR 3", the kit's look otherwise |
| user_folder, log_file | the write dir, its `mw3.log` |
| files_in_use | the install in use (`a.install`, made absolute) |
| install | `sdlfe::install_kind()` (sdl_update.h): a build without `MW3_PACKAGE_BUILD` is development; the Mac app (`running_package`); Linux's `$APPIMAGE`, else a folder; Windows' Setup by `unins000.exe` beside the program (under Program Files: every user, else this user's), else a folder. `MW3_INSTALL_KIND` names one in checks |
| program_dir | `sdlfe::program_dir()`: the app's `Contents`, the AppImage's `$APPDIR`, else the program's folder. The package's `rebuild-files.sha256` is there |
| check_game_files | `mech3::setup::own_check` (game_setup.h): `start_check`'s problems, then the 28 data files (`data_version.h`) against 1.0's and 1.2's size and CRC, `write_dir/patch12`'s copy first. Used for an install with no record of its files; the kit then writes the record |
| rewritten | none: MechWarrior 3 writes nothing in its install. Every write goes to the write dir (`zutl::open`: reads search the mounts, writes go to the write dir) |
| settings | `settings.reg`, `mw3.cfg` and `keys/` in the write dir, copied to `Backups/<time> settings` before a reset |
| reset_settings | `upgrades::reset::options_and_controls` (src/upgrades/settings/reset.h): every option to the loader's default (options.cpp's `load_options`) through the options' own setters, then `apply_all`; the controls are the controls screen's RESET (0x0042d210 / 0x0042d2b0) and the default map written as the game writes it. The rebuild's own `mw3.cfg` keys (`config_keys`) are removed and their switches set to the defaults at once. The mission screen mode is back to its default (used from the next mission) and so is fullscreen (`windowed`, from the next start); the install, the mods and the 3D card stay |
| restart_for_setup | `RestartTo::choose_files`: the game closes as its window's close button does, and starts again with `--choose-files`. That shows the find screen, as Shift at the start does, and then plays. `--setup` would find and then exit, which is wrong for CHANGE |
| report_url | https://github.com/telltaleatheist/mech3-re/issues |
| entries | UPGRADES (below) |

Pirate's Moon's (branch `pm`, `mw3_main.cpp` being mw3pm's there) differ in these:

| Hook | Pirate's Moon's |
|---|---|
| title | "Pirate's Moon", as its window and its launcher card say; the theme's header "PIRATE'S MOON". The version is mw3pm's `MW3_VERSION` |
| check_game_files | `mech3::setup::pm_own_check` (game_setup.h): `start_check`'s problems for Pirate's Moon, then its 1.0's 14 main data files (the table `check_install` measures the version with) by size and CRC, the write dir's copy first (ADD FROM MY DISC puts the disc's sounds there) |
| rewritten | none: Pirate's Moon writes nothing in its install either; the movie import and the disc's sounds go to the write dir too |
| report_url | https://github.com/telltaleatheist/mech3-pm-re/issues |
| entries | UPGRADES: Pirate's Moon's own page, reached the same way |

The rest are the same: its log is `mw3.log` in its write dir, its settings the same three, and RESET SETTINGS the
same code (Pirate's Moon's settings loader is MechWarrior 3's; its `settings.reg` key is its own, "MechWarrior 3 EP1").
UPDATES waits for the next merge from `main`: Pirate's Moon then needs its own `UpdateSource` (its repository, the
`pm-v` tags, its assets and keys).

The hint and the keys follow the kit: Cmd + Shift + F1 on the Mac, Shift + Alt + F1
elsewhere. The hint shows for the first three starts made without the launcher
(`RE_LAUNCHER` unset). `RE_KIT_TEST` turns the keys off and leaves the checks
in charge.

### UPGRADES

The game's own Upgrades page (`options_upgrades_state`, options_menu.h), reached the
way a player reaches it, so that its BACK goes where the player expects:

- in the main menu, it is pushed over the screen there;
- in a mission (the Core State), the pause screen comes first, then its OPTIONS is
  pressed (which makes the page), then the page is pushed. BACK returns to the
  options, then the pause screen's RESUME;
- elsewhere (a debug `--mission` run, a briefing), the log says it is not here.

## Packaging: the record of the files

The kit's `tools/files_manifest.py` writes `rebuild-files.sha256` at the program
folder's top. CHECK MY FILES compares against it. Each package writes the record
after its last file is in place and before anything seals the folder:

- **Mac** (`tools/macos/make_app.sh`): at `Contents/`, before signing, so the app's
  seal covers the record. codesign rewrites every Mach-O (its signature is inside the
  file), so a record written before signing would never match them. Every Mach-O is
  left out of the record (`--leave-out` by name, found as the kit's signer finds
  them), because the signature already checks those files.
- **Linux** (`tools/linux/make-appimage.sh`): at the AppDir's top, after the path
  check and before `appimagetool` squashes it.
- **Windows** (`tools/windows/package.cmd`): at the dist folder's top, before the zip
  and the Setup are made from it. The Windows flow has no Python, so
  `tools/windows/files_manifest.ps1` writes it instead. Its output was checked to be
  byte for byte the same as the Python script's on the same folder (2026-11-27).
  Setup's `unins000.*` are not in the record, so they are never checked.

## UPDATES: MechWarrior 3's updater and the kit's

UPDATES is the kit's step 3 (another session). MechWarrior 3 already has an updater,
built for its launcher and its find screen (`src/frontend/sdl/sdl_update.*`,
`src/upgrades/update/update.*`, used from `launcher.cpp`). Where the two meet:

| MechWarrior 3 has | What it does | For the kit |
|---|---|---|
| `release_lists`, `ask_lists`, `check_for_updates` | both homes (GitHub's API and the git mirror), asked on a thread at every launcher start; `judge` picks the newest package release for this platform | the kit's check |
| `upgrades::update::compare_versions`, `package_tag`, `plain_version`, `newest_package`, `platform_asset` | tags and assets by platform | general: they belong in the kit |
| `notes_lines` | release notes to lines (WHAT'S NEW) | the kit's notes |
| `download_checked`, `download_update` | curl as a separate program, a progress callback, the file checked against the release's `SHA256SUMS` | the kit's download |
| `install_update` | Windows: runs the new Setup and quits. Mac: `hdiutil attach`, `swap_in` of the .app, refused while translocated. Linux: swaps the AppImage | the kit's swap by `InstallKind`, which `install_kind()` already reports |
| `swap_in` | a fresh copy beside, then renames | the kit's |
| `stop_asking` | the player's "don't ask" | the kit's |

Everything there is general except the repository names and the launcher's words.
Once the kit's UPDATES lands, MechWarrior 3's copy can go: the launcher keeps only
its screens, and both the in-game menu and the launcher call the kit. Until then the
menu's UPDATES page is the kit's placeholder, and MechWarrior 3's updater runs only in
the launcher, as before.

## Checks

The program takes these environment variables (`MenuPause`):

- `MW3_MENU_AT=N[:keys]` opens the menu at the game's Nth frame and presses `keys`
  in it (`d u l r` for the arrows, `e` for Enter).
- `MW3_MENU_IN_MISSION=1` makes N count the frames of a campaign mission instead.
- `MW3_MENU_SHOT=PATH` takes a capture after `MW3_MENU_SHOT_AFTER` frames of the menu
  (10 by default).
- `MW3_MENU_CLOSE=1` closes the menu after the capture.

The log says how many game frames ran and how much mission time passed while the
menu was up, and the first step after it.

The tests are in `tests/test_sdl_frontend.cpp`, `sdl_menu_in_game_*`:

- the main menu's RESET SETTINGS applied live;
- a campaign mission held with its clock stopped (reached through `--campaign-chain`);
- the game's own check on a damaged copy (`sdl_menu_in_game_pm_own_check` in Pirate's Moon's suite);
- CHANGE's restart arguments.
