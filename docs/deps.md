# The bundled libraries, from source

`tools/deps/build_deps.sh mac|linux|win` builds the libraries the releases
bundle from their official release tarballs (2026-10-30: universal
arm64 + x86_64 for macOS 12 and later on the Mac, and on Linux), into
`build-deps/<platform>/prefix`. `JOBS=4` caps the parallel jobs (the
default is every core). First run on the Mac 2026-10-31:
all five downloads match their pins; 2.6 minutes at `JOBS=4`; every dylib
x86_64 + arm64, minimum macOS 12.0, named `@rpath/<file>`, loading only the
system and each other. The app (`make_app.sh --deps`) bundles SDL3,
SDL3_ttf and SDL3_sound for mw3 and the four FFmpeg dylibs for mw3import. Linux: run in WSL (Ubuntu 24.04) on 2026-10-31 at `JOBS=6`, all pins matched (docs/linux.md).

| Source | sha256 (pinned) | Checked against |
|---|---|---|
| SDL3-3.4.20.tar.gz (github.com/libsdl-org/SDL release-3.4.20; 3.4.12 until 2026-11-27) | f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f | its `.sig`: good signatures (3.4.20 checked 2026-11-27) by Sam Lantinga's DSA key 1528 635D 8053 A57F 77D1 E086 30A5 9377 A776 3BE6 and RSA key 0900 1043 63B4 C9D4 223D E149 D913 FE7D 4B61 D39B (both from keyserver.ubuntu.com; neither his GitHub profile nor libsdl.org lists a fingerprint) |
| SDL3_ttf-3.2.2.tar.gz (libsdl-org/SDL_ttf release-3.2.2) | 63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d | its `.sig`: Sam Lantinga's DSA key above |
| SDL3_sound-3.2.0.tar.gz (icculus/SDL_sound v3.2.0) | 3348f3ecc653e21f2b7ed8e409f07c67b179fbbfba3b99c85698e6e5d7b5924c | its `.sig`: Ryan C. Gordon's key 50F9 1FB5 50EC 32AB 4A71 0286 FA14 8B89 2AB4 8044, the one on his GitHub profile (github.com/icculus.gpg) |
| freetype-2.14.3.tar.xz (download.savannah.gnu.org) | 36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f | its `.sig`: Werner Lemberg's key E306 7470 7856 409F F194 8010 BE6C 3AAC 63AD 8E3F (checked 2026-11-23; 2.13.2 before it) |
| ffmpeg-8.1.2.tar.xz (ffmpeg.org/releases) | 464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c | its `.asc`: FFmpeg's release key FCF9 86EA 15E6 E293 A564 4F10 B432 2F04 D676 58D8, as ffmpeg.org's download page gives it |
| zlib-1.3.1.tar.gz (github.com/madler/zlib release v1.3.1; zlib.net now serves a newer one) | 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23 | its `.asc`: Mark Adler's DSA key 5ED4 6A67 21D3 6558 7791 E2AA 783F CD8E 58BC AFBA (keyserver.ubuntu.com) |
| xz-5.8.1.tar.xz (github.com/tukaani-project/xz release v5.8.1) | 0b54f79df85912504de0b14aec7971e3f964491af1812d83447005807513cd9e | its `.sig`: Lasse Collin's key 3690 C240 CE51 B467 0D30 AD1C 38EE 757D 6918 4620 (keyserver.ubuntu.com) |
| libarchive-3.7.9.tar.xz (libarchive.org/downloads) | ed8b5732e4cd6e30fae909fb945cad8ff9cb7be5c6cdaa3944ec96e4a200c04c | its `.asc`: Martin Matuska's key DB2C 7CF1 B4C2 65FA EF56 E3FC 5848 A18B 8F14 184B (keyserver.ubuntu.com; the key has expired since, the signature of 2025-03-30 made while it was valid) |

The signatures were checked once, when the pins were made (2026-10-30, a
throwaway GnuPG home; the three archive libraries' 2026-10-31); the script then checks each download's sha256 and
refuses a mismatch.

What is built:
- **zlib, liblzma (xz) and libarchive** (2026-10-31, for SETUP's "Point to
  your game", docs/setup-sources.md): all three static and
  position-independent, linked into the programs, so the packages carry no
  more libraries (only their licences: libarchive BSD-2, zlib's zlib
  licence, liblzma 0BSD). libarchive's readers for zip, 7z, RAR4/5 (its own
  clean-room code, no unRAR), tar and ISO 9660, with zlib and liblzma and
  nothing else (no crypto, XML, bzip2, zstd, LZ4, iconv; no tools). On
  Windows the same pinned sources are built by
  `tools\windows\build_deps_archive.cmd` (clang-cl, CMake, Ninja) into
  `deps\archive`. A development Mac may use Homebrew's libarchive instead
  (CMakeLists.txt finds either; without one SETUP takes folders, drives,
  .iso and .cue only). Both scripts copy the three licences into the
  prefix's `share/licenses/<name>-<version>/` (libarchive's COPYING, zlib's
  LICENSE, xz's COPYING and COPYING.0BSD), from where each package takes
  them: package.cmd (check_dist.ps1 refuses a package without them),
  make_app.sh --deps and make-appimage.sh.
- **SDL3**: shared, no tests or examples.
- **FreeType** 2.14.3, static and
  position-independent, inside SDL3_ttf and, for the launcher's RmlUi
  (`third_party/rmlui`, 2026-11-24), linked into the programs, with none of its optional
  libraries (zlib, bzip2, PNG, HarfBuzz, Brotli): the game's TrueType fonts
  need none. TTF text depends on FreeType's version: 2.13.2 (the version
  SDL3_ttf 3.2.2 vendors, pinned here until 2026-11-23) differs from 2.14.3
  by about 11 pixels at glyph edges (MW1's office_offer canvas, fdd0ffb9
  where the golden is 0eee21a1). 2.14.3 is the version the Mac's Homebrew
  SDL3_ttf loads. HarfBuzz makes no difference to the game's text (the 2D
  host draws with mono hinting and no kerning: the same pixels with and
  without it, at both FreeType versions), but with SDL_ttf's defaults (the
  launcher's: kerning on, Blended) it moves proportional text, so a
  development build on Homebrew's SDL3_ttf draws the launcher's text a
  little apart from the packages, which have none. On Windows,
  `tools\windows\build_deps_ttf.cmd` builds the same SDL3_ttf and FreeType
  into `deps\ttf`, which package.cmd requires: the `-VC.zip` release's DLL
  carries FreeType 2.13.2 and HarfBuzz 8.5.0. Checked 2026-11-23 by
  drawing four strings in the game's three fonts and our Liberation Sans,
  Liberation Mono and Anton at eight sizes through each build: the Windows package's SDL3_ttf, the
  AppImage's and a FreeType 2.14.3 + HarfBuzz 14.6.0 build (Homebrew's
  versions) the same with the host's settings; the old Windows DLL the
  same as Linux's vendored 2.13.2. SDL3_ttf's release tarball does not carry its vendored
  libraries (its `external/download.sh` clones them unpinned), so the
  signed FreeType release stands in.
- **SDL3_ttf**: shared, without HarfBuzz (complex-script shaping) or
  PlutoSVG (SVG fonts). Instead of Homebrew's seven extra libraries
  (FreeType, HarfBuzz, GLib, libintl, PCRE2, graphite2, libpng; two LGPL),
  none.
- **SDL3_sound**: shared, every default decoder.
- **FFmpeg**: LGPL 2.1, shared, only what mw3import uses (the AVI demuxer,
  the Indeo 5 decoder, the MPEG-1 video encoder and muxer, swscale, the file
  protocol; since 2026-11-27 also the Matroska/WebM and FLAC demuxers and the
  VP9 and FLAC decoders and parsers, FFmpeg's own, for the compact original's movies,
  docs/compact.md). On the Mac each architecture is configured and built apart
  (x86_64 cross-compiled, without its hand-written assembly) and the dylibs
  joined with `lipo`; install names `@rpath`.
  Configured with `--prefix=/ffbuild` and installed through `DESTDIR`
  (2026-11-02): FFmpeg keeps its whole configure line in each library
  (`avutil_configuration()`), and a real prefix put the build folder, with
  the user name, into the release packages (release.md, Privacy).
- **Windows** (`build_deps.sh win`, 2026-11-02): FFmpeg only, the same
  tarball and options, cross-built for x86_64 with Homebrew's `mingw-w64`
  14.0.0_3 (GCC 16.2.0, binutils' `dlltool` for the MSVC import `.lib`s),
  targeting the UCRT (the DLLs import the `api-ms-win-crt-*` sets, KERNEL32
  and bcrypt; no msvcrt). Windows threads (`--enable-w32threads`), libgcc
  linked in (`-static-libgcc`), no x86 assembly (no nasm). mingw-w64 keeps
  `clock_gettime` and `nanosleep` in `libwinpthread-1.dll`, so the script
  turns both off in FFmpeg's `config.h` after configure: avutil then uses
  Windows' clock (`GetSystemTimeAsFileTime`) and `Sleep`. The script
  refuses a DLL that imports anything else. Replaces BtbN's "latest"
  LGPL v3 build (docs/windows.md).
