#!/bin/bash
# Writes mw1/tools/package/mw1.ico, MechWarrior 1 Remake's Windows icon (mw1.exe's, its installer's and the
# uninstaller's), from mw1.png beside it: the same badge as the Mac app's and the AppImage's. PNG images inside the
# .ico (Windows Vista and later) at 16, 32, 48 and 256 pixels. Run on the Mac (sips) from the repo root; the .ico is
# committed, so a Windows build never runs this.
#   mw1/tools/package/make_ico.sh
set -euo pipefail
cd "$(dirname "$0")"
TMP="$(mktemp -d)"
trap '/bin/rm -rf "${TMP:?}"' EXIT
for n in 16 32 48 256; do
  sips -z "$n" "$n" mw1.png --out "$TMP/$n.png" >/dev/null
done
python3 - "$TMP" mw1.ico <<'EOF'
import pathlib
import struct
import sys

src, out = pathlib.Path(sys.argv[1]), sys.argv[2]
sizes = (16, 32, 48, 256)
images = [(src / f"{n}.png").read_bytes() for n in sizes]
head = struct.pack("<HHH", 0, 1, len(sizes))
offset = len(head) + 16 * len(sizes)
entries = b""
for n, data in zip(sizes, images):
    entries += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
pathlib.Path(out).write_bytes(head + entries + b"".join(images))
EOF
echo "mw1.ico: $(wc -c < mw1.ico) bytes"
