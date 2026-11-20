#!/bin/bash
# MechWarrior (1989), Elthial's SDL port, built for the launcher's PLAY ORIGINAL (MW1 FIXES 124).
# The port, Munt and Nuked-OPL3 at pinned commits, our build patches applied, built against SDL3, then staged in OUT
# ready to go in MechWarrior1989/ beside the launcher: the program, its libraries, MechWarrior.ini and licences/.
#
#   build_port.sh fetch                      clone the three sources at their pins into $MW1PORT_SRC
#                                            (build-deps/mw1port/src by default; once; network)
#   build_port.sh mac   OUT [PREFIX]         PREFIX: build-deps/mac/prefix (universal, macOS 12+); none: Homebrew's
#                                            SDL3 (arm64 only; not for release)
#   build_port.sh linux OUT PREFIX           PREFIX: make-appimage.sh's (build-deps/linux/prefix)
#   ... --proposed                           also apply patches/proposed (read-only opens, case-insensitive opens)
#
# The build never downloads: FetchContent is off and Munt and Nuked-OPL3 come from $MW1PORT_SRC.
# Windows: build_port.cmd.
set -euo pipefail
KIT="$(cd "$(dirname "$0")" && pwd)"
. "$KIT/pins.txt"
SRC="${MW1PORT_SRC:-$KIT/../../../build-deps/mw1port/src}"  # git-ignored, beside build_deps.sh's
JOBS="${JOBS:-4}"

fetch_one() {  # dir url commit
  local dir="$SRC/$1"
  [ -d "$dir/.git" ] || git clone -q "$2" "$dir"
  git -C "$dir" cat-file -e "$3^{commit}" 2>/dev/null || git -C "$dir" fetch -q origin
  git -C "$dir" checkout -q --detach "$3"
  [ "$(git -C "$dir" rev-parse HEAD)" = "$3" ] || { echo "$1 is not at $3" >&2; exit 1; }
  echo "$1 $3"
}
if [ "${1:-}" = fetch ]; then
  mkdir -p "$SRC"
  fetch_one port "$PORT_URL" "$PORT_COMMIT"
  fetch_one munt "$MUNT_URL" "$MUNT_COMMIT"
  fetch_one nuked-opl3 "$NUKED_URL" "$NUKED_COMMIT"
  exit 0
fi

PLATFORM="${1:?mac, linux or fetch}"; OUT="${2:?OUT directory}"; shift 2
mkdir -p "$(dirname "$OUT")" && OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"  # absolute: the path maps
PREFIX="" PROPOSED=0
for a in "$@"; do case "$a" in --proposed) PROPOSED=1 ;; *) PREFIX="$(cd "$a" && pwd)" ;; esac; done
case "$PLATFORM" in mac|linux) ;; *) echo "mac or linux, not $PLATFORM" >&2; exit 2 ;; esac
[ "$PLATFORM" = mac ] || [ -n "$PREFIX" ] || { echo "linux needs PREFIX" >&2; exit 2; }
for d in port munt nuked-opl3; do [ -d "$SRC/$d/.git" ] || { echo "no $SRC/$d: run build_port.sh fetch" >&2; exit 1; }; done
[ "$(git -C "$SRC/port" rev-parse HEAD)" = "$PORT_COMMIT" ] || { echo "port is not at $PORT_COMMIT" >&2; exit 1; }

# A clean tree of the pinned commit, our patches on it (ours, replaced each run).
WORK="$OUT.work"
rm -rf "${WORK:?}" && mkdir -p "$WORK/tree"
git -C "$SRC/port" archive "$PORT_COMMIT" | tar -x -C "$WORK/tree"
PATCHES=("$KIT"/patches/build/*.patch)
[ "$PROPOSED" = 0 ] || PATCHES+=("$KIT"/patches/proposed/*.patch)
for p in "${PATCHES[@]}"; do git -C "$WORK/tree" apply --whitespace=nowarn "$p"; echo "patch: $(basename "$p")"; done

CONF=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DMECHWARRIOR_MT32_SOURCE_DIR="$SRC/munt" -DMECHWARRIOR_NUKED_OPL_SOURCE_DIR="$SRC/nuked-opl3"
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
  # No build machine's paths in what ships (__FILE__ in asserts and logs; tools/deps/check_paths.py).
  -DCMAKE_C_FLAGS="-ffile-prefix-map=$WORK/tree=port -ffile-prefix-map=$SRC=src"
  -DCMAKE_CXX_FLAGS="-ffile-prefix-map=$WORK/tree=port -ffile-prefix-map=$SRC=src")
if [ "$PLATFORM" = mac ]; then
  CONF+=(-DCMAKE_INSTALL_RPATH=@executable_path -DCMAKE_INSTALL_NAME_DIR=@rpath)
  if [ -n "$PREFIX" ]; then CONF+=(-DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
  else echo "note: Homebrew's SDL3 (arm64 only): not for release" >&2; CONF+=(-DCMAKE_PREFIX_PATH=/opt/homebrew); fi
else
  CONF+=(-DCMAKE_INSTALL_RPATH='$ORIGIN' -DCMAKE_PREFIX_PATH="$PREFIX")
fi
cmake -S "$WORK/tree" -B "$WORK/build" "${CONF[@]}" > "$WORK/cmake.log"
cmake --build "$WORK/build" -j "$JOBS" > "$WORK/build.log" || { tail -30 "$WORK/build.log" >&2; exit 1; }
B="$WORK/build"

# Stage: the program and its libraries side by side, found by rpath (@executable_path, $ORIGIN).
rm -rf "${OUT:?}" && mkdir -p "$OUT/licences"
cp "$B/mechwarrior" "$OUT/"
cp "$WORK/tree/MechWarrior.ini" "$OUT/"
if [ "$PLATFORM" = mac ]; then
  for f in "$B"/libmt32emu.*.dylib "$B/libnukedopl.dylib"; do  # under the name the program loads
    cp "$f" "$OUT/$(basename "$(otool -D "$f" | tail -1)")"; done
  sdl="$(otool -L "$B/mechwarrior" | awk '/libSDL3\.0\.dylib/{print $1; exit}')"
  case "$sdl" in
    @rpath/*) cp "$PREFIX/lib/libSDL3.0.dylib" "$OUT/" ;;
    *) cp "$sdl" "$OUT/libSDL3.0.dylib"; chmod u+w "$OUT/libSDL3.0.dylib"
       install_name_tool -change "$sdl" @rpath/libSDL3.0.dylib "$OUT/mechwarrior"
       install_name_tool -id @rpath/libSDL3.0.dylib "$OUT/libSDL3.0.dylib" ;;
  esac
  for f in "$OUT"/mechwarrior "$OUT"/*.dylib; do [ -L "$f" ] || codesign -f -s - "$f" 2>/dev/null; done
  left="$(for f in "$OUT"/mechwarrior "$OUT"/*.dylib; do otool -L "$f" | grep -v ':$' | awk '{print $1}'; done \
          | grep -vE '^(@rpath/|/usr/lib/|/System/)' || true)"
  for d in $(otool -L "$OUT/mechwarrior" | grep -v ':$' | awk '$1 ~ /^@rpath\//{print substr($1,8)}' | sort -u); do
    [ -f "$OUT/$d" ] || left="$left $d"; done
else
  for f in "$(ls "$B"/libmt32emu.so.*.* | head -1)" "$B/libnukedopl.so"; do  # under its soname, if any
    n="$(objdump -p "$f" | awk '/SONAME/{print $2}')"; cp "$f" "$OUT/${n:-$(basename "$f")}"; done
  cp -P "$PREFIX"/lib/libSDL3.so.0* "$OUT/"
  left="$(LD_LIBRARY_PATH= ldd "$OUT/mechwarrior" | awk '/not found/{print $1}')"
fi
[ -z "$left" ] || { echo "libraries outside the bundle: $left" >&2; exit 1; }

# Licences: the port's (its scope notice first), the libraries', and where every source is.
L="$OUT/licences"
cp "$WORK/tree/LICENSE" "$L/MechWarrior-sdl-port-LICENSE.txt"
cp "$WORK/tree/docs/LICENSES/MIT.txt" "$L/MechWarrior-sdl-port-MIT.txt"
cp "$WORK/tree/docs/LICENSES/README.md" "$L/MechWarrior-sdl-port-LICENSES.md"
cp "$WORK/tree/docs/NOTICE.md" "$L/MechWarrior-sdl-port-NOTICE.md"
cp "$WORK/tree/docs/THIRD_PARTY_NOTICES.md" "$L/MechWarrior-sdl-port-THIRD_PARTY_NOTICES.md"
cp "$SRC/munt/mt32emu/COPYING.LESSER.txt" "$L/libmt32emu-LICENSE.txt"
cp "$SRC/nuked-opl3/LICENSE" "$L/Nuked-OPL3-LICENSE.txt"
sdl_lic="$(ls "$PREFIX"/share/licenses/SDL3/LICENSE.txt 2>/dev/null || ls /opt/homebrew/opt/sdl3/LICENSE.txt 2>/dev/null || true)"
[ -n "$sdl_lic" ] && cp "$sdl_lic" "$L/SDL3-LICENSE.txt" || { echo "no SDL3 LICENSE.txt found" >&2; exit 1; }
{
  echo "MechWarrior (1989) SDL port, as built for MechWarrior 3 Rebuild's launcher (PLAY ORIGINAL)."
  echo "A legally obtained MechWarrior (1989) installation is required; none is included."
  echo
  echo "Sources (corresponding source for the LGPL libraries libmt32emu and nukedopl):"
  echo "  port        $PORT_URL @ $PORT_COMMIT"
  echo "  libmt32emu  $MUNT_URL @ $MUNT_COMMIT (mt32emu/)"
  echo "  nukedopl    $NUKED_URL @ $NUKED_COMMIT (opl3.c)"
  echo "  SDL3        https://github.com/libsdl-org/SDL ($(basename "$(dirname "$(dirname "$sdl_lic")")") prefix)"
  echo "Patches applied to the port:"
  for p in "${PATCHES[@]}"; do echo "  $(basename "$p")"; done
  echo "The two LGPL libraries are separate shared libraries beside the program and may be replaced."
} > "$L/SOURCES.txt"
echo "staged $OUT:"; ls -l "$OUT"
