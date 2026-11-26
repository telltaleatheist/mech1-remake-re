#!/bin/bash
# shots.sh <launcher> <out dir> <setup dir>: every --launcher-shot screen in two setups, for a byte-for-byte
# before/after compare of a launcher change (docs/launcher-split.md: b0 and on). Run it for both builds with the same
# <setup dir> (the logs hold its paths), then compare:
#   for f in before/*.bmp; do cmp -s "$f" after/$(basename "$f") || echo "differs: $f"; done
# Each build draws its version on screen: build both with it pinned, e.g.
#   GIT_DIR=/nonexistent cmake -S . -B build-shot && make -C build-shot mw3launcher   (the version says "unknown")
# Setup A asks stand-in releases (games on demand); B starts from an empty write dir. macOS names (-mac.dmg).
# Only folders this script made are cleared (its marker in them); a folder of anything else is refused.
set -u
L=$1; O=$2; S=$3
clear_ours() {  # an empty or missing folder, or one with our marker: cleared and made again with the marker
  if [ -e "$1" ] && [ -n "$(ls -A "$1" 2>/dev/null)" ] && [ ! -e "$1/.launcher-shots" ]; then
    echo "shots.sh: $1 holds files this script did not make: not cleared" >&2; exit 1
  fi
  rm -rf "$1"; mkdir -p "$1"; : > "$1/.launcher-shots"
}
clear_ours "$O"; clear_ours "$S"
SCREENS="main setup pick files notes disc pmnotes pmdisc data found place nomusic mw1found mw1origfound update updatebox chifiles copies pmcopies work"
for k in mw3 pm mw1 mw1r chi chirm; do SCREENS="$SCREENS game:$k game:$k:uninstall"; done
# Setup A: games on demand against stand-in releases (an update for PM, MW1 Rebuild out, Inception not, its remake out).
A=$S/A; mkdir -p $A/api $A/games/pm $A/games/mw1 $A/wd/music; printf x > $A/wd/music/track02.wav
for kt in pm:mw3pm:pm-v1.2.5 mw1:mw1:mw1-v1.0.9; do IFS=: read key prog tag <<<"$kt"
  printf '#!/bin/sh\nexit 0\n' > $A/games/$key/$prog; chmod 755 $A/games/$key/$prog
  printf '# x\ngame=%s\ntag=%s\nfile=x\nsha256=x\nfrom=github\n' $key $tag > $A/games/$key/rebuild-game.txt; done
rel() { printf '[{"tag_name": "%s", "draft": false, "prerelease": false, "body": "x", "assets": [{"name": "%s%s-mac.dmg", "size": 1000, "browser_download_url": "file://%s/x"}, {"name": "SHA256SUMS", "size": 10, "browser_download_url": "file://%s/s"}]}]' $2 $3 $2 $A $A > $A/api/$1.json; }
rel mech3-pm-re pm-v1.2.6 PM-Rebuild-; rel mech1-remake-re mw1-v1.0.9 MW1-Remake-; rel mech1-re mw1r-v1.0.0 MW1-Rebuild-; rel inception-remake-re chirm-v0.1.0 CHI-Remake-; rel mech3-re v9.9.9 MW3-alone-
printf '[]' > $A/api/inception-re.json
n=0
for s in $SCREENS; do f=$(echo $s | tr : _)
  env MW3_GAMES_API="file://$A/api/{repo}.json" MW3_GAMES_DIR=$A/games "$L" --write-dir $A/wd --launcher-shot $O/A_$f.bmp --launcher-screen $s > $O/A_$f.log 2>&1; n=$((n+1)); done
env MW3_GAMES_API="file://$A/api/{repo}.json" MW3_GAMES_DIR=$A/games MW3_LAUNCHER_LIST_Y=200 "$L" --write-dir $A/wd --launcher-shot $O/A_main_scrolled.bmp --launcher-screen main > $O/A_main_scrolled.log 2>&1
env MW3_GAMES_API="file://$A/api/{repo}.json" MW3_GAMES_DIR=$A/games MW3_LAUNCHER_SHOT_SIZE=1708x960 "$L" --write-dir $A/wd --launcher-shot $O/A_main_big.bmp --launcher-screen main > $O/A_main_big.log 2>&1
# Setup B: a fresh, empty write dir; no game found, no repo asked.
B=$S/B; mkdir -p $B
for s in $SCREENS; do f=$(echo $s | tr : _); rm -rf $B/wd; mkdir -p $B/wd
  env MW3_FIND_INSTALL=1 "$L" --write-dir $B/wd --launcher-shot $O/B_$f.bmp --launcher-screen $s > $O/B_$f.log 2>&1; n=$((n+1)); done
echo "$n screens, $(ls $O/*.bmp | wc -l | tr -d ' ') shots"
