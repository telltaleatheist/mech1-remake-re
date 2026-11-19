#!/bin/sh
# MW1's movies (FIXES.md item 54, src/mw1_movie.h): one of our clips (an LTX take, H.264 + AAC in an .mp4) converted
# for the program, MPEG-1 video with MP2 sound in one MPEG program stream, which the engine's pl_mpeg decodes. Run by a
# developer (FFmpeg on the PATH: Homebrew's, or any); the player never needs FFmpeg.
#
#   mw1/tools/mw1movie.sh IN.mp4 [NAME]   ->  mw1/data/video/NAME.mpg (NAME: dropship_launch)
#
# 5 Mbit/s at the clip's own size (1280x704 for the launch: about 3.2 MB for 5 s, SSIM 0.989 against the .mp4);
# 24 frames a second (the takes' own); the sound at 192 kbit/s, 44.1 kHz. MW1_MOVIE_RATE and MW1_MOVIE_SIZE
# (WxH) change them.
set -eu
in=${1:?usage: mw1movie.sh IN.mp4 [NAME]}
name=${2:-dropship_launch}
rate=${MW1_MOVIE_RATE:-5M}
here=$(cd "$(dirname "$0")/.." && pwd)
out="$here/data/video/$name.mpg"
mkdir -p "$here/data/video"
scale=
if [ -n "${MW1_MOVIE_SIZE:-}" ]; then scale="-vf scale=$(echo "$MW1_MOVIE_SIZE" | tr x :)"; fi
# shellcheck disable=SC2086
ffmpeg -v error -y -i "$in" $scale -r 24 -c:v mpeg1video -b:v "$rate" -maxrate "$rate" -bufsize 4M \
  -c:a mp2 -b:a 192k -ar 44100 -f mpeg "$out"
ls -l "$out"
