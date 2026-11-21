#!/usr/bin/env python3
"""Writes data/mw1bounds.zrd's mission areas (FIXES 138, src/mw1_bounds.h) from the boundary survey.

  mw1bounds.py <survey.tsv.detail> [more.detail ...] [-o data/mw1bounds.zrd]

The survey (tests/test_mw1_loop.cpp, mw1_loop_boundary_survey_headless, with MW1_BOUNDARY_TABLE) logs, for each row
of the map table, the player's start, every enemy and ally as the launch made them, and the row's structure. A map
whose regions are its own (a reg_inside_ that isn't MW1's) keeps them; every other map gets an area here:

- YELLOW: the 16 sides whose outward normals are 22.5 degrees apart, each as far out as the farthest of the map's
  places in its direction plus 600 m; an enemy's place counts as 60 m round it, where the launch may make another
  beside it. Every place then lies at least 600 m inside every side (and the corners at most
  12 m past the hull's 600 m round), and the radar's outline (20 corners at most, ztwod's PolyLine) holds it.
- RED: each of those sides 500 m further out.

Corners go in MW3's region order (as reg_inside_ia1.zrd's: clockwise in x, z), to a tenth of a metre. The file's
MOVE entries (enemy places moved inside a map's own area), after the MARK line, are kept as they are.
"""
import math
import re
import sys

SIDES = 16
MARGIN = 600.0
RED = 500.0
MARK = '; ---- MOVE: written by hand, kept by tools/mw1bounds.py ----'
EXTRA = 60.0  # an enemy past the map's places is made 30-60 m beside one (mech3/player.cpp, extra_enemy_place)
OWN_REGION = re.compile(r'region loaded (reg_inside_\S+):')
PLACE = re.compile(r'^(start|enemy \S+|ally \S+|structure) \((-?\d+), (-?\d+)\)')


def read_places(paths):
    places, own = {}, set()
    for path in paths:
        key = None
        for line in open(path):
            if line.startswith('BOUNDARY'):
                w = line.split()
                key = (w[1], int(w[2][2:]))
                places.setdefault(key, set())
                continue
            line = line.strip()
            m = OWN_REGION.match(line)
            if m and not m.group(1).endswith('_mw1'):
                own.add(key)
            m = PLACE.match(line)
            if m and key:
                pad = EXTRA if m.group(1).startswith('enemy') else 0.0
                places[key].add((int(m.group(2)), int(m.group(3)), pad))
    return {k: v for k, v in places.items() if k not in own and v}


def outline(points, out):
    """The SIDES-sided polygon: side k's normal at k * 360 / SIDES degrees, `out` past the farthest point."""
    normals = [(math.cos(2 * math.pi * k / SIDES), math.sin(2 * math.pi * k / SIDES)) for k in range(SIDES)]
    reach = [max(x * nx + z * nz + pad for x, z, pad in points) + out for nx, nz in normals]
    corners = []
    for k in range(SIDES):
        (ax, az), (bx, bz) = normals[k], normals[(k + 1) % SIDES]
        ha, hb = reach[k], reach[(k + 1) % SIDES]
        det = ax * bz - az * bx
        corners.append(((ha * bz - hb * az) / det, (ax * hb - bx * ha) / det))
    return corners[::-1], reach  # counter-clockwise as made: reversed into MW3's order


def pairs(corners):
    return ' '.join(f'({x:.1f} {z:.1f})' for x, z in corners)


def main():
    args = sys.argv[1:]
    out = 'data/mw1bounds.zrd'
    if '-o' in args:
        i = args.index('-o')
        out = args[i + 1]
        del args[i:i + 2]
    places = read_places(args)
    moves = []
    try:
        text = open(out).read().split('\n')
        if MARK in text:  # the hand-written part: from the mark to the file's closing parenthesis
            moves = text[text.index(MARK):]
            while moves and moves[-1].strip() in ('', ')'):
                moves.pop()
    except FileNotFoundError:
        pass
    lines = [
        '; MW1: Critical Hit\'s mission areas (FIXES 138, src/mw1_bounds.h; research/boundaries.md).',
        ';',
        '; BOUNDS: a map without an area of its own (PM\'s c1..c4, MW1\'s arctic mw3c4b) gets this one, which plays as',
        '; MW3\'s maps\' own: the radar draws the YELLOW and the RED; past the yellow the "out_of_bounds" voice; past the',
        '; red the mission is lost. Written by tools/mw1bounds.py from the boundary survey (the start, every enemy',
        f'; place (and {EXTRA:.0f} m round it) and every row\'s structure): {SIDES} sides each {MARGIN:.0f} m past the farthest, the red',
        f'; {RED:.0f} m past those. Corners (x z), in MW3\'s region order. Regenerate rather than edit.',
        ';',
        '; MOVE: an enemy place a map puts outside its own area, moved inside it (an enemy made within 80 m of FROM is',
        '; made as far from TO, on the ground there). Written by hand, kept by the tool.',
        '(',
    ]
    for (world, ia) in sorted(places, key=lambda k: (k[0].startswith('mw3'), k)):
        pts = sorted(places[(world, ia)])
        yellow, reach = outline(pts, MARGIN)
        red, _ = outline(pts, MARGIN + RED)
        lines.append(f'; {world} ia{ia}: {len(pts)} places')
        lines.append(f'BOUNDS (WORLD ("{world}") IA ({ia})')
        lines.append(f'  YELLOW ({pairs(yellow)})')
        lines.append(f'  RED ({pairs(red)}))')
    lines += moves or [MARK]
    lines.append(')')
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'{out}: {len(places)} areas, {sum(m.startswith("MOVE ") for m in moves)} moves')


if __name__ == '__main__':
    main()
