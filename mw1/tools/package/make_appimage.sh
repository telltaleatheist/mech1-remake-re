#!/bin/sh
# MechWarrior 1 Remake alone, for Linux: the AppImage the launcher's INSTALL / UPDATE downloads from this repo's
# Releases (DEPLOY.md). usr/bin/mw1 with usr/bin/mw1data/ beside it (where SDL_GetBasePath points mw1) and the
# engine's stand-in fonts in usr/bin/assets/fonts, the SDL libraries it loads from PREFIX, linuxdeploy's usual
# rpath. The launcher unpacks it with --appimage-extract and runs usr/bin/mw1 itself; mw1 is also the AppImage's
# entry. Vulkan, X11, Wayland and the audio servers are the system's. A package build (MW3_PACKAGE). Nothing from
# any game. PM's own tools/linux/make-appimage.sh is left as it is, so merges from mech3-pm-re never meet this file.
# Usage: mw1/tools/package/make_appimage.sh [PREFIX] [TOOLS]   (from the repo root, in WSL or on Linux)
#   PREFIX: tools/deps/build_deps.sh linux's prefix (default ../prefix);
#   TOOLS: the folder whose appimage-tools/ holds the AppImage tools, pinned and fetched by
#   tools/linux/get_appimage_tools.sh (default ../deps).
# Out: build-mw1-linux/dist/MW1-Remake-<mw1-vX.Y.Z>-x86_64.AppImage
set -e
cd "$(dirname "$0")/../../.."
PREFIX=$(cd "${1:-../prefix}" && pwd)
TOOLS="${2:-../deps}"; mkdir -p "$TOOLS"; TOOLS=$(cd "$TOOLS" && pwd)
TOOLS="$TOOLS/appimage-tools"  # pinned by checksum; downloaded here when missing
sh tools/linux/get_appimage_tools.sh "$TOOLS"
export APPIMAGE_EXTRACT_AND_RUN=1  # no FUSE needed (WSL, containers)
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

BUILD=build-mw1-linux/build
rm -rf build-mw1-linux
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build "$BUILD" --target mw1
[ -x "$BUILD/mw1" ] && [ -f "$BUILD/mw1data/zbd/mw1image.zbd" ] || { echo "mw1 or its mw1data was not built in $BUILD" >&2; exit 1; }

VER=$(git describe --tags --match 'mw1-v[0-9]*' --always --dirty 2>/dev/null || echo dev)
APPDIR=build-mw1-linux/AppDir
DOC="$APPDIR/usr/share/doc/mw1"
mkdir -p "$APPDIR/usr/bin" "$DOC/licenses"
cp "$BUILD/mw1" "$APPDIR/usr/bin/"
cp -R "$BUILD/mw1data" "$APPDIR/usr/bin/mw1data"
# The read-me and the notices: MW1's own, and the engine's (its libraries and fonts).
cp mw1/dist/README-MW1.txt "$DOC/README.txt"
cp mw1/dist/THIRD-PARTY-MW1.txt "$DOC/"
cp tools/windows/dist/THIRD-PARTY.txt "$DOC/THIRD-PARTY.txt"
[ -f LICENSE ] && cp LICENSE "$DOC/LICENSE.txt"
mkdir -p "$APPDIR/usr/bin/assets/fonts" "$DOC/licenses/liberation" "$DOC/licenses/anton"
cp assets/fonts/*.ttf "$APPDIR/usr/bin/assets/fonts/"
cp assets/fonts/Liberation-OFL.txt "$DOC/licenses/liberation/OFL.txt"
cp assets/fonts/Anton-OFL.txt "$DOC/licenses/anton/OFL.txt"
# Each library's licence files: from its source tree when the prefix is build_deps.sh's, else what it installed.
if [ -d "$PREFIX/../work/src" ]; then
  for d in "$PREFIX"/../work/src/*/; do
    n=$(basename "$d"); mkdir -p "$DOC/licenses/$n"
    for f in "$d"LICENSE* "$d"COPYING* "$d"LICENSE.txt "$d"docs/LICENSE.TXT "$d"docs/FTL.TXT "$d"docs/GPLv2.TXT; do
      [ -f "$f" ] && cp "$f" "$DOC/licenses/$n/"
    done
  done
else
  for lib in SDL3 SDL3_ttf SDL3_sound; do
    mkdir -p "$DOC/licenses/$lib"
    find "$PREFIX/share" -ipath "*$lib*" \( -iname "LICENSE*" -o -iname "COPYING*" \) -exec cp {} "$DOC/licenses/$lib/" \; 2>/dev/null || true
  done
fi
# The AppImage's own start program and the libfuse in it (LGPL): its notice and licences.
cp -R tools/linux/appimage-runtime "$DOC/licenses/appimage-runtime"
# libarchive, zlib and liblzma are linked in: build_deps.sh keeps their licences in the prefix.
[ -d "$PREFIX/share/licenses" ] && cp -R "$PREFIX/share/licenses/." "$DOC/licenses/"
LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH" "$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" \
  -e "$APPDIR/usr/bin/mw1" -d mw1/tools/package/mw1.desktop -i mw1/tools/package/mw1.png
[ -x "$APPDIR/usr/bin/mw1" ] || { echo "no mw1 in the AppDir" >&2; exit 1; }
[ -f "$APPDIR/usr/bin/mw1data/zbd/mw1image.zbd" ] || { echo "no mw1data beside mw1" >&2; exit 1; }
grep -qx 'Exec=mw1' "$APPDIR"/*.desktop || { echo "the AppImage does not start mw1" >&2; exit 1; }
case "$VER" in *-dirty) echo "warning: $VER: a dirty tree, not for release" >&2 ;; esac
# No local path or user name in anything shipped.
python3 -I tools/deps/check_paths.py "$APPDIR" || exit 1
mkdir -p build-mw1-linux/dist
OUT="build-mw1-linux/dist/MW1-Remake-$VER-x86_64.AppImage"
ARCH=x86_64 "$TOOLS/appimagetool-x86_64.AppImage" --runtime-file "$TOOLS/runtime-x86_64" "$APPDIR" "$OUT"
echo "$OUT"
