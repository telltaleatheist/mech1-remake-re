#!/bin/bash
# A release's packages for this platform (docs/release.md): MechWarrior 3 and the launcher from a vX.Y.Z tag on
# main, Pirate's Moon from a pm-vX.Y.Z tag on pm, MechWarrior 1 Remake from an mw1-vX.Y.Z tag on mw1, each built in a
# clean worktree of its own, all in build-pkg/dist/<platform>/:
#   MW3-Rebuild-<v>-mac.dmg / -x86_64.AppImage   the launcher alone (what the launcher's updater takes); it downloads
#                                               each game from the game's own release (2026-11-22;
#                                               docs/launcher.md, Games on demand)
#   MW3-alone-<v>-...                           the launcher and mw3: MechWarrior 3's own download
#   PM-Rebuild-<pm v>-...                       mw3pm alone (branch pm's own package)
#   MW1-Remake-<mw1 v>-...                      mw1 and its data (branch mw1's mw1/tools/package), with --mw1
# Then tools/package/sums.py writes their SHA256SUMS (and again over every platform's files once collected).
#
#   tools/package/combined.sh mac   <main ref> <pm ref> [--deps] [--mw1 <mw1 ref>]  (on the Mac; --deps for a release)
#   tools/package/combined.sh linux <main ref> <pm ref> PREFIX TOOLS [--mw1 <mw1 ref>]  (in WSL; make-appimage.sh's)
#
# Refs: the tags for a release (v1.1.0 pm-v1.0.2); any commit for a check (origin/main origin/pm). The worktrees are
# build-pkg/main, build-pkg/pm and build-pkg/mw1 (ours, replaced each run); the deps (build-deps/, ../prefix) are
# linked or passed.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$PWD"
PLATFORM="${1:?mac or linux}"
MAIN_REF="${2:?the main ref (a vX.Y.Z tag)}"
PM_REF="${3:?the pm ref (a pm-vX.Y.Z tag)}"
shift 3
case "$PLATFORM" in mac|linux) ;; *) echo "mac or linux, not $PLATFORM" >&2; exit 2 ;; esac
EXTRA=() MW1_REF=""
while [ $# -gt 0 ]; do
  if [ "$1" = --mw1 ]; then MW1_REF="${2:?--mw1 needs a ref}"; shift 2
  else EXTRA+=("$1"); shift; fi
done
if [ "$PLATFORM" = linux ]; then  # absolute, for the worktrees
  [ ${#EXTRA[@]} -ge 2 ] || { echo "linux needs PREFIX and TOOLS (make-appimage.sh's)" >&2; exit 2; }
  EXTRA=("$(cd "${EXTRA[0]}" && pwd)" "$(cd "${EXTRA[1]}" && pwd)")
fi
W="$ROOT/build-pkg"
OUT="$W/dist/$PLATFORM"
mkdir -p "$W"

# A clean worktree at `ref` in build-pkg/<name>: an earlier one of ours removed first (only these paths).
worktree() {
  local name="$1" ref="$2" dir="$W/$1"
  if [ -e "$dir" ]; then
    git worktree remove --force "$dir" 2>/dev/null || { echo "$dir is not a worktree of ours: remove it yourself" >&2; exit 1; }
  fi
  git worktree prune
  git worktree add --detach "$dir" "$ref" >/dev/null
  # extern/re-setup-kit; a checkout whose .git/config points it at a local clone (the kit's developers) is allowed
  # that clone (git refuses the file transport for submodules by default).
  git -C "$dir" -c protocol.file.allow=always submodule update --init --quiet
  # The deps the package scripts read from the checkout (build_deps.sh's), linked in.
  [ -d "$ROOT/build-deps" ] && ln -s "$ROOT/build-deps" "$dir/build-deps"
  echo "$name: $(git -C "$dir" describe --tags --always --dirty) ($(git -C "$dir" rev-parse --short HEAD))"
}
worktree main "$MAIN_REF"
worktree pm "$PM_REF"
[ -z "$MW1_REF" ] || worktree mw1 "$MW1_REF"
rm -rf "$OUT" && mkdir -p "$OUT"

if [ "$PLATFORM" = mac ]; then
  # Pirate's Moon alone (pm's own script).
  (cd "$W/pm" && tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/pm/build-mac/dist/PM-Rebuild-*-mac.dmg "$OUT/"
  # MechWarrior 3 alone, then the launcher alone from the same build.
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-mac/dist/MW3-alone-*-mac.dmg "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_LAUNCHER_ONLY=1 tools/macos/make_app.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-mac/dist/MW3-Rebuild-*-mac.dmg "$OUT/"
  if [ -n "$MW1_REF" ]; then  # its own script (on mw1, beside pm's untouched ones), with the release's libraries
    MW1_ARGS=()
    [[ " ${EXTRA[*]-} " == *" --deps "* ]] && MW1_ARGS=(--deps "$ROOT/build-deps/mac/prefix")
    (cd "$W/mw1" && mw1/tools/package/make_app.sh ${MW1_ARGS[@]+"${MW1_ARGS[@]}"})
    cp "$W"/mw1/build-mw1-mac/dist/MW1-Remake-*-mac.dmg "$OUT/"
  fi
else
  (cd "$W/pm" && tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/pm/build-linux-pkg/dist/PM-Rebuild-*-x86_64.AppImage "$OUT/"
  (cd "$W/main" && MW3_PKG_STEM=MW3-alone tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-linux-pkg/dist/MW3-alone-*-x86_64.AppImage "$OUT/"
  (cd "$W/main" && MW3_PKG_KEEP_BUILD=1 MW3_PKG_LAUNCHER_ONLY=1 tools/linux/make-appimage.sh ${EXTRA[@]+"${EXTRA[@]}"})
  cp "$W"/main/build-linux-pkg/dist/MW3-Rebuild-*-x86_64.AppImage "$OUT/"
  if [ -n "$MW1_REF" ]; then
    (cd "$W/mw1" && mw1/tools/package/make_appimage.sh "${EXTRA[0]}" "${EXTRA[1]}")
    cp "$W"/mw1/build-mw1-linux/dist/MW1-Remake-*-x86_64.AppImage "$OUT/"
  fi
fi
python3 -I tools/package/sums.py "$OUT"
ls -l "$OUT"
