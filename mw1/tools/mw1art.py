#!/usr/bin/env python3
"""mw1art.py KEEP_DIR OUT_DIR: MW1's screen art for the repo (data-src/art/), from our generated art.

KEEP_DIR is the remake's keep folder (assets/mw3/*.png, ours from Crucible, nothing of MW1's or MW3's, and
planets.csv). Run by hand when the art changes; the build packs OUT_DIR into mw1image.zbd (tools/mw1imgpack.cpp).

The 2D canvas is 480 lines (854 at 16:9, 640 at 4:3; mw3-re docs/resolution.md, sections 9 and 12), in 16-bit 565.
Each picture is cut to its use's size and shape (cover: scaled to fill, the middle kept) and rounded to 565's levels
here, so the packed pixels are exactly these (the packer adds no loss) and PNG keeps them small. No dither: the
generated art's own grain hides 565's steps (checked on title_bg's sunset), and a dither costs a third more bytes.
- a scene (1344x768, or a 512 square used as one): 854x480, drawn centred (the middle 640 at 4:3);
- a service tile (icon_*): 100x100 (framed at pack time, mw1_chrome.h);
- a portrait or a mech picture (768x1024): 240x320, and a small one, <name>_s, 96x128 (the crew grid's);
- an emblem (story_darkwing_crest): 240x240.
The intro's unused takes (intro_0_*) are left out.

scenery.csv: each world's scenery (hub_<type>), by MW1's planet number: our classification only (MW1's descriptions,
which it was read from, stay out of the repo).
"""
import csv
import os
import sys

import numpy as np
from PIL import Image

SCENE = (854, 480)
TILE = (100, 100)
PORTRAIT, PORTRAIT_SMALL = (240, 320), (96, 128)
EMBLEM = (240, 240)
SKIP_PREFIX = ("intro_0_",)
EMBLEMS = {"story_darkwing_crest"}
PORTRAIT_PREFIX = ("recruit_", "char_", "liaison_")
PORTRAITS = {"bartender_portrait", "office_liaison_portrait"}


def cover(im, size):
    """Scaled to fill `size`, the middle kept."""
    w, h = im.size
    s = max(size[0] / w, size[1] / h)
    sw, sh = max(size[0], round(w * s)), max(size[1], round(h * s))
    im = im.resize((sw, sh), Image.LANCZOS)
    x, y = (sw - size[0]) // 2, (sh - size[1]) // 2
    return im.crop((x, y, x + size[0], y + size[1]))


def to_565(im):
    """Rounded to 565's levels (5, 6, 5 bits), as 8-bit values the packer's conversion keeps exactly."""
    a = np.asarray(im.convert("RGB"), np.float32)
    step = np.array([8, 4, 8], np.float32)
    top = np.array([248, 252, 248], np.float32)
    q = np.clip(np.floor(a / step + 0.5) * step, 0, top)
    return Image.fromarray(q.astype(np.uint8), "RGB")


def kind(name, size):
    if name in EMBLEMS:
        return "emblem"
    if name.startswith("icon_"):
        return "tile"
    if name.startswith(PORTRAIT_PREFIX) or name in PORTRAITS:
        return "portrait"
    return "scene"


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mw1art.py KEEP_DIR OUT_DIR")
    keep, out = sys.argv[1], sys.argv[2]
    src = os.path.join(keep, "assets", "mw3")
    os.makedirs(out, exist_ok=True)
    made = []
    for f in sorted(os.listdir(src)):
        if not f.endswith(".png"):
            continue
        name = f[:-4]
        if name.startswith(SKIP_PREFIX):
            continue
        im = Image.open(os.path.join(src, f)).convert("RGB")
        k = kind(name, im.size)
        sizes = {"scene": [("", SCENE)], "tile": [("", TILE)], "emblem": [("", EMBLEM)],
                 "portrait": [("", PORTRAIT), ("_s", PORTRAIT_SMALL)]}[k]
        for suffix, size in sizes:
            path = os.path.join(out, name + suffix + ".png")
            to_565(cover(im, size)).save(path, optimize=True)
            made.append(path)
    # The worlds' scenery: number and type only.
    with open(os.path.join(keep, "planets.csv"), newline="") as f, open(os.path.join(out, "scenery.csv"), "w", newline="") as o:
        w = csv.writer(o, lineterminator="\n")
        w.writerow(["n", "scenery"])
        for row in csv.DictReader(f):
            w.writerow([row["n"], row["scenery"]])
    total = sum(os.path.getsize(p) for p in made)
    print(f"{len(made)} pictures, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
