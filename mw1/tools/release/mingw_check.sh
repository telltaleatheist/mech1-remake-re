#!/bin/sh
# A Windows compile check of one file from a Mac or Linux, from the repo root: prints "<errors> <file>". Read the count:
# a file that doesn't compile prints more than 0 and the exit code is 0 either way.
# Usage: mw1/tools/release/mingw_check.sh <file>
f=$1
n=$(nice -n 10 x86_64-w64-mingw32-g++ -std=c++20 -fsyntax-only -Isrc -Imw1/src -Imw1/tests -Itests -Itools/zipkit \
    -Imw1/third_party -Ithird_party -idirafter "${SDL_INCLUDE:-/opt/homebrew/include}" -DMW1_DOS_GAME_DIR='"x"' \
    -DMW3PM_GAME_DIR='"x"' -DMW1_DATA_DIR='"x"' -DMW3_TEST_TMP_ROOT='"x"' -DMW1_MW3_GAME_DIR='"x"' -DMW3_GAME_DIR='"x"' \
    -I"$PWD/extern/re-setup-kit/include" -I"$PWD/third_party/imgui" -DMW3_BIN='"x"' -DMW3_LAUNCHER_BIN='"x"' \
    -DMW3_FAKE_INDEO='"x"' -x c++ "$f" 2>&1 | grep -c "error")
echo "$n $f"
