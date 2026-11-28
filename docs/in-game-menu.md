# The setup menu inside the game

The setup kit's in-game menu (`extern/re-setup-kit`, `rekit/sdl/game_menu.h`; the kit's
`docs/plan-in-game-menu.md`) wired into MechWarrior 3 (2026-11-27). It is ours: the
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
  focus loss there (W45). The menu then sits over live frames, and its subtitle says
  THE GAME IS NOT PAUSED (`MenuHooks::runs_on`).

The pause screen (Escape) is a different thing, a visible screen of the game's own,
and the menu does not use it. The exception is UPGRADES in a mission, below.

## The hooks

| Hook | MechWarrior 3's |
|---|---|
| title, version | "MechWarrior 3", `MW3_VERSION`; the theme's header "MECHWARRIOR 3", the kit's look otherwise |
| user_folder, log_file | the write dir, its `mw3.log` |
| files_in_use | the install in use (`a.install`, made absolute) |
| install | `sdlfe::install_kind()` (sdl_update.h): a build without `MW3_PACKAGE_BUILD` is development; the Mac app (`running_package`); Linux's `$APPIMAGE`, else a folder; Windows' Setup by `unins000.exe` beside the program (under Program Files: every user, else this user's), else a folder. `MW3_INSTALL_KIND` names one in checks |
| program_dir | `sdlfe::program_dir()`: the app's `Contents/Resources`, the AppImage's `$APPDIR`, else the program's folder. The package's `rebuild-files.sha256` is there, covering that folder |
| check_game_files | `mech3::setup::own_check` (game_setup.h): `start_check`'s problems, then the 28 data files (`data_version.h`) against 1.0's and 1.2's size and CRC, `write_dir/patch12`'s copy first. Used for an install with no record of its files; the kit then writes the record . The kit writes the record at every install it makes (`--setup --install`); `mw3 --install-from` writes it too (`setup::install_from_disc`, the same rules) |
| rewritten | none: MechWarrior 3 writes nothing in its install. Every write goes to the write dir (`zutl::open`: reads search the mounts, writes go to the write dir) |
| settings | `settings.reg`, `mw3.cfg` and `keys/` in the write dir, copied to `Backups/<time> settings` before a reset |
| reset_settings | `upgrades::reset::options_and_controls` (src/upgrades/settings/reset.h): every option to the loader's default (options.cpp's `load_options`) through the options' own setters, then `apply_all`; the controls are the controls screen's RESET (0x0042d210 / 0x0042d2b0) and the default map written as the game writes it. The rebuild's own `mw3.cfg` keys (`config_keys`) are removed and their switches set to the defaults at once. The mission screen mode is back to its default (used from the next mission) and so is fullscreen (`windowed`, from the next start); the install, the mods and the 3D card stay |
| restart_for_setup | `RestartTo::choose_files`: the game closes as its window's close button does, and starts again with `--choose-files`. That shows the find screen, as Shift at the start does, and then plays. `--setup` would find and then exit, which is wrong for CHANGE |
| report_url | https://github.com/telltaleatheist/mech3-re/issues |
| update | UPDATES (below) |
| quit | the window's close button (`SdlPump::post_quit`): the game's own way out, before an update is installed |
| runs_on | `run_while_inactive`: a network game runs on under the menu |
| entries | UPGRADES (below) |

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
- elsewhere (the game still starting, before the options' page exists, or a screen
  change pending), the entry is dimmed with the note "In the main menu or a mission"
  (the kit's `MenuEntry::available`, asked as the menu draws: `upgrades_reachable`),
  and it cannot be picked. A debug `--mission` run is in the mission state, so it is
  reachable there as in a campaign.

## The kit's settings file

The kit's tunable values (the hint, the sounds' volumes, CHECK MY FILES' list, REPORT A
PROBLEM's page, where the signed catalog is asked, the update check's interval, the
network time limits) are data: `assets/setup-kit-settings.json`, the kit's own file with
MechWarrior 3's `report_url` (mech3-re's issues) and `catalogs` (the three addresses
the launcher asks), the rest at the kit's defaults (`rekit/settings.h`; the kit's
DESIGN.md, Settings). Layers, each over the one before: the kit's defaults, this file
beside the program (`program_dir`), the signed catalog's live values (kept in the write
dir's `setup-kit-live.json`), then a player's own `setup-kit-settings.json` in the write
dir. The log names each file read (`mw3: settings: read ...`) and any value it
corrected.

- **Where it is:** beside `build/mw3` in a development build (CMake's
  `mw3_kit_settings`, copied again when the file changes); `Contents/Resources` on the
  Mac; the AppImage's top (`$APPDIR`, where the record is; not `usr/bin`); beside
  `mw3.exe` on Windows. Every package lists it in its record, and the package scripts
  refuse a record without it (check_dist too on Windows). The launcher alone carries
  none: it has no in-game menu.
- **The compiled values** (`h.report_url`, `h.update.catalogs` in `menu_hooks`) are
  fallbacks for a copy without the file; the file wins. The signing keys stay compiled
  in: they are never in the file.
- **Checks open nothing:** under `RE_KIT_TEST` the menu's `open_url` hook logs
  `mw3: menu: (a check) not opened: <address>` instead of opening a page or folder.
- **The kit's setup screen** (`--setup`, `--choose-files`) reads the file from
  `SDL_GetBasePath()`, its search folder. That is `program_dir` on the Mac and on
  Windows. On the AppImage it is `usr/bin`, so the setup screen uses the kit's
  defaults there; it reads only the download time limits, which the file leaves at
  the defaults.

## Packaging: the record of the files

The kit's `tools/files_manifest.py` writes `rebuild-files.sha256` at the program
folder's top. CHECK MY FILES compares against it. Each package writes the record
after its last file is in place and before anything seals the folder:

- **Mac** (`tools/macos/make_app.sh`): at `Contents/Resources/`, covering Resources
  only, before signing, so the app's seal covers the record. codesign refuses a plain
  file at the top of `Contents/` ("code object is not signed at all"), and the
  signature already seals `Info.plist` and the Mach-Os. codesign rewrites every
  Mach-O (its signature is inside the file), so a record could never match them; any
  Mach-O in Resources is left out (`--leave-out` by name).
- **Linux** (`tools/linux/make-appimage.sh`): at the AppDir's top, after the path
  check and before `appimagetool` squashes it.
- **Windows** (`tools/windows/package.cmd`): at the dist folder's top, before the zip
  and the Setup are made from it. The Windows flow has no Python, so the
  kit's `tools/files_manifest.ps1` writes it instead. Its output was checked to be
  byte for byte the same as the Python script's on the same folder (2026-11-27).
  Setup's `unins000.*` are not in the record, so they are never checked.

## UPDATES

UPDATES and the start-up prompt are the kit's update engine (`rekit/sdl/update.h`,
the kit's DESIGN.md, Updates). MechWarrior 3 fills `MenuHooks::update`:

- **Its releases:** the recipe is `mw3`, repository `mech3-re`, tags `v<version>`,
  this copy `MW3_VERSION`. The release file by install kind is
  `sdlfe::update_file` (sdl_update.h). Windows' Setup installs take
  `MW3-alone-Setup-<tag>.exe` and an unpacked Windows folder the `-win64.zip`. The
  Mac takes the `-mac.dmg` and Linux the `-x86_64.AppImage`. These are MechWarrior
  3's own downloads, the same the launcher's catalog names.
- **Where to look:** the signed catalog at the three addresses the launcher asks
  (the hub's `assets/launcher/sources.json`), checked against the same key. The
  catalog lists the homes (GitHub, then the mirror) and the count address. The
  count goes out as the kit sends it (`game=mw3`), so the hub's tally counts it with
  the launcher's updates.
- **What is kept:** `pilots`, `settings.reg`, `mw3.cfg` and `keys/` in the write
  dir, backed up before an update.
- **The restart:** this start's own arguments again (`sdlfe::restart_args`).
- **A development build** (no `MW3_PACKAGE_BUILD`) gets no update source. It cannot
  be updated, so it never checks or prompts, and the page says it has no source.

The kit never prompts when `RE_LAUNCHER` is set: the launcher updates the games it
starts. Started alone, the game checks once a day and prompts over its own screen
when a newer version is out. The player can untick that on the page.

MechWarrior 3's older updater (`src/frontend/sdl/sdl_update.*`,
`src/upgrades/update/*`) stays for the launcher, which still uses it: its self
update and its game downloads (`launcher.cpp`, `sdl_games.cpp`). Moving the
launcher onto the kit is a later item.

## Checks

The program takes these environment variables (`MenuPause`):

- `MW3_MENU_AT=N[:keys]` opens the menu at the game's Nth frame and presses `keys`
  in it (`d u l r` for the arrows, `e` for Enter).
- `MW3_MENU_IN_MISSION=1` makes N count the frames of a campaign mission instead.
- `MW3_MENU_KEYS=keys@F[;keys@F]` presses keys at the menu's Fth frame, however it
  opened (the update prompt opens it by itself).
- `MW3_MENU_SHOT=PATH[@F][;PATH@F]` takes captures at the menu's Fth frames
  (without `@F`, after `MW3_MENU_SHOT_AFTER` frames, 10 by default).
- `MW3_MENU_CLOSE=1` closes the menu after the last capture.
- `MW3_UPDATE_WORLD=DIR` and `MW3_UPDATE_KEY=HEX` give UPDATES a stand-in world of
  `file:///` addresses: `DIR/catalog.json`, signed by that key. Only `DIR/install`
  can be replaced and there is no restart.

The test programs set `RE_KIT_TEST=1` and `SDL_AUDIO_DRIVER=dummy` for themselves
and every program they start (`tests/main.cpp`), unless already set. So no test run
checks for updates online, prompts, shows the hint or plays a sound. A test that
wants the kit's own behavior unsets them for its run (`ingame::kUnset`).

The log says how many game frames ran and how much mission time passed while the
menu was up, and the first step after it.

The tests are in `tests/test_sdl_frontend.cpp`, `sdl_menu_in_game_*`:

- the main menu's RESET SETTINGS applied live;
- a campaign mission held with its clock stopped (reached through `--campaign-chain`);
- MechWarrior 3's own check on a damaged copy;
- CHANGE's restart arguments;
- UPDATES started alone: the kit's start-up prompt finds a newer MechWarrior 3 in the
  stand-in world, and its UPDATE NOW reaches the close question (left unanswered);
- UPDATES under the launcher (`RE_LAUNCHER`): no prompt, and CHECK NOW shows the
  newer release with its notes.
- UPGRADES at the game's first frame: dimmed with its note, and Enter leaves the menu
  open.
- REPORT A PROBLEM from the settings file: a release-like copy (mw3 in a folder of its
  own with the file beside it, its address changed) opens the file's address, and a
  development run reads the copy beside `build/mw3`
  (`sdl_menu_in_game_report_page_from_the_shipped_settings`);
- a player's copy in the write dir read after the shipped one, its address winning
  (`sdl_menu_in_game_player_settings_override_the_shipped`).
