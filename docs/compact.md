# The compact original (2026-11-27)

Ours, not the original's. The game's files as a disc install leaves them,
made small for hosting: the second source after the Internet Archive's disc
image for the launcher's DOWNLOAD (the catalog's `"compact"` list, the hub's
docs/launcher.md). The setup takes it as an installed copy, checks every file,
and the first start converts the movies the same way it converts a disc's.

## What is in it

`tools/package/make_compact.py make` builds it from the disc image's zip (the
Internet Archive's, checked by its SHA-256) with the game's own programs:

1. `mw3 --setup --install <zip>`: the game's own setup of the image, as the
   launcher asks for it. That gives the install and the music
   (`music/trackNN.wav`).
2. The install, less what the rebuild never reads (below).
3. Each movie `video/<name>.avi` becomes three files:
   - `<name>.webm`: VP9 of the Indeo 5 pictures that mw3import decodes. It
     takes them from `mw3import --frames-out` (y4m: the same decode, the same
     black lead-in and held frames as the import), encoded with libvpx-vp9
     `-crf 20 -b:v 0`, one thread, `-g 150`, bit-exact muxing.
   - `<name>.flac`: the AVI's PCM, lossless (8-bit sound is held as 16-bit,
     the low byte zero).
   - `<name>.fmvh`: the `.fmv` mw3import writes from the AVI (its headers),
     kept as it is.
4. The music as `music/trackNN.flac`.
5. `mw3-compact.json`: `"format": "compact 1"`, the game, its data version,
   its name, who made it, the source zip's name and SHA-256, the encoders'
   versions, the list of what was left out, and every file's path, size and
   SHA-256.

The archive is a .tar.xz: the manifest first, then every file in path order,
times and owners zero, written by Python's lzma (preset 9e). The same input
and the same encoders give the same bytes. The tool prints the catalog entry
(url, sha256, size, name, note, game_min). `--game-min` has no default: pass the
game's own version, the first release that reads compacts (MechWarrior 3
1.7.0, Pirate's Moon 1.3.0; each game numbers its releases on its own, so
one value never serves both). A launcher skips an entry whose game_min the
installed game does not reach.

### Left out

Each is something the port never opens (traced 2026-11-27):

| Left out | Why |
|---|---|
| `Ir50_32.dll` | the Indeo codec: only the Windows Indeo helper loads it, and only beside the AVIs, which are not there |
| `Mfc42.dll`, `Msvcrt.dll`, `msvcp60.dll`, `msvcirt.dll`, `IFORCE2.dll`, `Uninst.dll` (and MFC40/MSVCRT40/MSVCP50 where present) | Mech3.exe's own runtime, its force-feedback library and the uninstaller; the port loads none |
| `zbd/<world>/texture.zbd`, `texture1.zbd`, `texture2.zbd` (c1 to c4b, t1) | the software renderer's world textures: `new_library("texture", level, hw)` names `rtexture<level>.zbd` when `hw`, and the port's `hw` is always 1 (install options 0x070707, no software renderer: `restore_video`) |
| `MSN Gaming Zone.url` | a web shortcut |
| `zbd/rmechtexs.zbd` | the software renderer's mech textures |
| `video/*.avi` | replaced by the three files above |

Kept: `Mech3.exe` (the setup and the status recognize the copy by it),
`Mech3Msg.dll` (the game's strings), `force_eff.ifr`, the fonts, both sound
sets (`soundsH`, `soundsL`: SoundLOD in the options picks one), every hardware
texture set, and the rest of `zbd/`.

## How the game takes it

- **--look** (the setup's look at a pick): a kit "installed copy" whose
  archive's first file is `mw3-compact.json` is named from it, for example
  "MechWarrior 3 1.0 (compact, reverse-engineered.com)." (`mech3::compact::peek`,
  `source::Found::compact`, `game_words`).
- **--install**: the setup copies it like any installed copy, then
  (`install_from_source`) checks every file listed against its size and
  SHA-256 (`mech3::compact::verify`; a missing or changed file abandons the
  install and changes nothing). It moves `music/` into the music folder as a
  disc's music is moved, and records `compact=` in `mw3-install.txt`. The
  movies stay in `video\` as .webm/.flac/.fmvh.
- **The first start** (`first_run_import`, mw3import): a `video\<name>.webm`
  counts as a movie to convert (`movies_missing`, the launcher's notes). The
  importer reads its kept header (`<name>.fmvh`), decodes its sound from
  `<name>.flac` into the `.wav` the AVI would give (the same bytes), encodes the
  pictures to `.m1v` as from the AVI, and writes the `.fmv` last. A folder
  holding both a movie's .avi and its .webm converts the .avi.

So a compact copy ends where a disc install ends: the same install files
(less those left out), the same `.wav` and `.fmv` bytes, the same music as
sound, and `.m1v` pictures within the measured difference below.

FFmpeg in the deps (tools/deps/build_deps.sh) reads it: the Matroska/WebM and
FLAC demuxers and the VP9 and FLAC decoders and parsers, all FFmpeg's own, LGPL.
The encoding needs an FFmpeg with libvpx-vp9 (Homebrew's), only where the tool
runs.

## Checked

`make_compact.py check` installs the disc zip and the compact, each through
the game's own setup (`--setup --install`), runs the first start's import on
both, and compares:

- the install trees: every kept file byte for byte, nothing extra but the
  manifest and the movies' three files;
- every `.wav` and `.fmv` byte for byte;
- every `.m1v`'s pictures, frame by frame by count: PSNR (mean and worst
  frame) and SSIM, with frame grabs (disc import left, compact import right)
  for a look;
- the music, decoded to PCM and hashed;
- `--status` on both: ready, the same data version, nothing pending.

How much the pictures lose, measured on c1m2 (475 frames) against the Indeo
decode itself (YUV PSNR, frame by frame):

| | size | PSNR mean | worst frame |
|---|---|---|---|
| the AVI (Indeo 5) | 6.24 MB | | |
| today's import (MPEG-1 q2, its pictures) | | 48.2 dB | 45.8 dB |
| VP9 crf 10 | 5.67 MB | 50.6 dB | 46.4 dB |
| VP9 crf 20 (the default) | 3.18 MB | 46.6 dB | 42.3 dB |
| VP9 crf 26 | 2.22 MB | 44.6 dB | 40.3 dB |
| VP9 crf 30 | 1.61 MB | 43.1 dB | 38.9 dB |

At crf 20 the VP9 step loses about as much as the MPEG-1 import already does,
which is itself above the 45 dB usually taken as visually lossless. `--crf`
picks another point on the curve.

### MechWarrior 3 1.0 (2026-11-27)

From `MechWarrior 3 (USA).zip` (the Internet Archive's, 624,970,304 bytes,
sha256 00f365a4...2eb5): `mw3-compact-1.0.tar.xz`, 310,194,616 bytes, 156
files (the movies 168 MB of them, the music 28 MB), 29 files left out. Made
in 26 minutes on the Mac Studio, most of it the single-threaded VP9.

`check` passed with 0 failed: 97 install files identical, all 38 `.wav` and
`.fmv` identical, both music tracks the same sound, and `--status` on both
ready, data 1.0, nothing pending. The pictures, the disc's import against the
compact's (19 movies, every frame):

| | PSNR mean | worst frame | SSIM |
|---|---|---|---|
| all 19 | 45.25 dB | 38.38 dB (campaign) | 0.9878 worst movie |
| c1m2 | 45.10 dB | 41.89 dB | 0.9879 |
| intro | 46.70 dB | 43.67 dB | 0.9891 |

These compare two MPEG-1 encodes of nearly the same pictures, so they are
lower than VP9 against the Indeo decode above. The frame grabs (disc left,
compact right) show no visible difference.

### Pirate's Moon 1.0 (2026-11-27, branch pm)

Pirate's Moon has no movies (its install's `video` folder is empty), so its
compact is the install less the same left-out files, with its one CD track
as `music/track02.flac`. `mw3pm`'s look names it from the manifest (its
recipe's `describe`), and its setup checks it as MechWarrior 3's does, for
`"game": "pm"`.

From `MechWarrior 3 - Pirate's Moon (Expansion Pack) (USA).zip`
(323,446,564 bytes, sha256 1b7a6b8f...935d): `pm-compact-1.0.tar.xz`,
167,214,200 bytes, 85 files, 19 left out, made in 4 minutes. `check` passed,
0 failed: 84 install files identical, the music track the same sound, and
`--status` on both ready, data 1.0, nothing pending.
