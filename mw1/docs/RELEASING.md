# Releasing MechWarrior 1 Remake

What a release took for 1.1.0 (2026-11-24), in the order it was done, and the
traps. `DEPLOY.md` has the family's release; this is the remake's part. Version
numbers: the second is a feature update, the third is bug fixes.

## Steps

1. **The branch.** A release branch off `main`, with everything the release
   holds merged or cherry-picked, the notes in `mw1/dist/RELEASE-NOTES-x.y.z.md`
   (players' words), and the new `mw1-*` tests.
2. **Take the engine's release.** Merge the Pirate's Moon release's tag
   (`pm-vX.Y.Z`). Check the merge traps below.
3. **Local annotated tag `mw1-vX.Y.Z`** on the commit to build. The version
   comes from `git describe --tags --match "mw1-v[0-9]*"`, so the tag must be
   there before anything is configured. It is only pushed by whoever publishes.
   A commit after the tag means deleting and re-tagging. Only changes that no
   package contains (the notes) can be left without a rebuild.
4. **Targeted tests** for what changed since the last release, on Mac, Windows
   and Linux, and `mw1/tools/release/mingw_check.sh` on each changed `.h` and
   `.cpp` (it prints a count; read it). Full suites only when the release calls
   for them. Run one suite at a time, nice'd, at most `-j4`, all `--hidden`.
   A list of test names goes in `MW3_TEST`, comma-separated (substrings match).
   Run them on Windows too: 1.1.0's Windows run found a save bug that the Mac's
   could not.
5. **Packages** (`mw1/README.md` has no build steps; these are the scripts):

   | Package | Command, from the repo root |
   |---|---|
   | Mac dmg | `tools/deps/build_deps.sh mac`, then `CMAKE_BUILD_PARALLEL_LEVEL=4 mw1/tools/package/make_app.sh --deps build-deps/mac/prefix` |
   | Windows zip and Setup | `mw1\tools\package\package.cmd` (needs Visual Studio's clang-cl, the Inno Setup compiler) |
   | Linux AppImage | `tools/deps/build_deps.sh linux`, then `mw1/tools/package/make_appimage.sh PREFIX TOOLS` |

   Each script checks the package for game files and local paths, and Windows
   for missing DLLs (`check_dist.ps1`).
6. **Check each package:** a hidden smoke start, `--version`, `--status` (exit 0,
   empty stderr, nothing made), and the record. Windows:
   `mw1/tools/release/setup_test.ps1`. Linux:
   `mw1/tools/release/linux_gate.sh` (the record and the ready file come from one
   start on SDL's offscreen driver). Mac: a start under a temporary home
   (`CFFIXED_USER_HOME`), `SDL_VIDEO_DRIVER=dummy`, stopped as soon as
   `rebuild-program.txt` is there. Prove first that the redirect works, or the
   real folder is the one moved.
7. **SHA256SUMS** for all four, in the same folder. The launcher installs
   nothing without it.
8. **Publish** (the person who releases): `main` and the tag at the release commit,
   then the release with the four packages and `SHA256SUMS`.

## The PC

- Windows GPU runs need the desktop session: over ssh there is no swapchain. Use
  a one-off scheduled task (`schtasks /create ... /it`) that starts a hidden
  `wscript` wrapper, and delete it afterwards.
- `setup_test.ps1` redirects `%USERPROFILE%` to a scratch folder for the record
  step, after proving with `SHGetFolderPathW` that the redirect works. A
  non-`--write-dir` start without it would move the player's own folder.
- PowerShell variable names ignore case (`$p` and `$P` are one variable).

## Traps when merging from Pirate's Moon

- **The shared `src/frontend/sdl/mw3_main.cpp`** is also MW1's main. Anything it
  does for "the program" (recording itself, its name in the log) must ask
  whether MW1 named itself: `record_self` is gated on `named.id.empty()`, or MW1
  records itself as Pirate's Moon.
- **MW1 writes under its own folder**, `BattleTech/MechWarrior 1 Remake`. A
  check that opens, makes or moves a folder before `--status` is answered
  breaks the "nothing left behind" rule.
- **`identify()` in the launcher** needs the kit id `mech1-remake`, exactly.
- **`--ready-file`** is passed through to the game and written at the first frame
  presented. A `--hidden` run presents none, so it never writes it.
- **The launcher and its tests don't build here** (`if(FALSE)` in the root
  `CMakeLists.txt`): check linking with mech3-re's own build.
- **Windows renames no open file.** Close a save before setting it aside.
- **Paths in tests:** compare as `std::filesystem::path`, not text.

## Not tested at 1.1.0, or only partly

- **The two scripts in this folder's `../tools/release/`** are what ran for 1.1.0,
  with the machine's own paths taken out and the Linux steps put in one file.
  Their pieces ran; these copies themselves have not been run.
- **An elevated (all users) Windows install.** `installer.iss` asks for admin by
  default, so a default install should put its uninstall key,
  `...\Uninstall\{C33F5AC5-1EFA-4952-81EA-A76A9A48AF28}_is1`, in HKLM (64-bit
  view). Only a per-user install was tested; there it is in HKCU.
- **The real GPU's memory** was sampled on the menu and an office screen (90 s
  each, flat). No mission was run.
- **The launcher's own UI tests** (`sdl_program_launcher`) failed or timed out in
  a WSL machine with software rendering. The cause is not established, and they
  were not judged blocking.
- **`--ready-file` on Windows and the Mac**, and pressing PLAY in the launcher on
  a linked copy, were not tried.
- **check_dist's allow-list** takes any `/repos/` path. Tighten it to
  `github.com/`, `api.github.com/repos/` and `/api/v1/repos/`.
