#!/bin/bash
# The Linux half of a release, in a checkout at the local tag: deps, a fresh development build, the filtered mw1tests,
# the AppImage, and its checks (--version, --status in an empty home, a hidden smoke, the record and the ready file).
# Usage: linux_gate.sh CHECKOUT BUILD_DIR TOOLS_DIR MW3_DIR PM_DIR MW1_DIR TEST_NAMES (test names: a file, one per line;
# TOOLS_DIR is make_appimage.sh's). Nothing is shown: --hidden, or SDL's offscreen driver for the one shown start.
set -u
SRC=$1; B=$2; TOOLS=$3; MW3=$4; PM=$5; MW1=$6; NAMES=$7
L=$B/logs; mkdir -p "$B" "$L"; say() { echo "$(date +%H:%M:%S) $*"; }
(cd "$SRC" && git submodule update --init --recursive --quiet)
[ -d "$SRC/build-deps/linux/prefix" ] || (cd "$SRC" && JOBS=4 nice -n 10 tools/deps/build_deps.sh linux) || exit 1
PREFIX=$SRC/build-deps/linux/prefix
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_BUILD_MW1=ON "-DCMAKE_PREFIX_PATH=$PREFIX" \
  "-DMW1_DOS_GAME_DIR=$MW1" "-DMW3PM_GAME_DIR=$PM" "-DMW3_GAME_DIR=$MW3" > "$L/cmake.log" || exit 1
nice -n 10 ninja -C "$B" -j4 > "$L/build.log" 2>&1 || { tail -20 "$L/build.log"; exit 1; }
(cd "$B" && MW3_TEST=$(paste -sd, "$NAMES") nice -n 10 ./mw1tests --hidden < /dev/null > "$L/mw1tests.log" 2>&1; say "mw1tests rc $?: $(tail -1 "$L/mw1tests.log")"; LC_ALL=C grep -a '^FAIL' "$L/mw1tests.log")
(cd "$B" && ctest -R mw1version --output-on-failure | tail -3)
(cd "$SRC" && CMAKE_BUILD_PARALLEL_LEVEL=4 nice -n 10 sh mw1/tools/package/make_appimage.sh "$PREFIX" "$TOOLS" > "$L/appimage.log" 2>&1) || { tail -15 "$L/appimage.log"; exit 1; }
A=$(ls "$SRC"/build-mw1-linux/dist/*.AppImage); sha256sum "$A"
for mode in direct extract; do  # --status: silent, rc 0, nothing left behind
  H=$L/home-$mode; rm -rf "${H:?}"; mkdir -p "$H"; E=X_=1; [ $mode = extract ] && E=APPIMAGE_EXTRACT_AND_RUN=1
  env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_DATA_HOME HOME="$H" $E "$A" --status > "$L/status.out" 2> "$L/status.err"
  say "--status ($mode) rc $?, stderr $(wc -c < "$L/status.err") bytes, files made $(($(find "$H" | wc -l) - 1)): $(cat "$L/status.out")"
done
H=$L/home-start; rm -rf "${H:?}"; mkdir -p "$H"; READY=$H/ready
# A real, shown start: the record and --ready-file (a --hidden run never writes it: nothing is presented).
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_DATA_HOME HOME="$H" SDL_VIDEO_DRIVER=offscreen setsid nice -n 10 "$A" --install "$PM" --mw3-install "$MW3" \
  --mw1-game "$MW1" --nosound --ready-file "$READY" > "$L/start.log" 2>&1 < /dev/null &
P=$!; for i in $(seq 1 300); do [ -e "$READY" ] && break; kill -0 $P 2>/dev/null || break; sleep 1; done
say "ready file $([ -e "$READY" ] && echo written || echo NOT written)"; kill -TERM -- -$P 2>/dev/null; sleep 2; kill -KILL -- -$P 2>/dev/null
cat "$H/.local/share/BattleTech/MechWarrior 1 Remake/rebuild-program.txt"
H=$L/home-smoke; rm -rf "${H:?}"; mkdir -p "$H/wd"
env -u DISPLAY -u WAYLAND_DISPLAY HOME="$H" SDL_VIDEO_DRIVER=offscreen APPIMAGE_EXTRACT_AND_RUN=1 timeout 300 nice -n 10 "$A" --hidden --write-dir "$H/wd" \
  --install "$PM" --mw3-install "$MW3" --mw1-game "$MW1" --nosound --frames 120 > "$L/smoke.log" 2>&1
say "hidden smoke rc $?"
