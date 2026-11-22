#!/usr/bin/env python3
"""Writes data/mw1bounds.zrd's mission areas (FIXES 138, src/mw1_bounds.h) from the boundary survey.

  mw1bounds.py <survey.tsv.detail> [more.detail ...] [-o data/mw1bounds.zrd]
  mw1bounds.py --repeat <survey.tsv.detail> ...     the file made twice (the second from the logs and their rows in
                                                     reverse order) is byte for byte the same; nothing written
  mw1bounds.py --launched <run.tsv.detail> ... [-o data/mw1bounds.zrd]
                                                     every enemy and ally a run made (its own random draw) lies at
                                                     least MARGIN inside the file's yellow, on the maps it covers

The survey (tests/test_mw1_loop.cpp, mw1_loop_boundary_survey_headless, with MW1_BOUNDARY_TABLE) logs, for each row
of the map table, the player's start, the row's structure, and every candidate: each record of the map's aiv.zrd the
launch may pick, on its side. The launch picks among the candidates at random (seeded from the clock), so the
enemies and allies a run made (its "enemy"/"ally" lines) are one draw and differ run to run; they are not read. The
places are the start, the structures and the candidates, so the same survey data gives the same file. A map whose
regions are its own (a reg_inside_ that isn't MW1's) keeps them; every other map gets an area here:

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
PLACE = re.compile(r'^(start|candidate enemy \S+|candidate ally \S+|structure) \((-?\d+), (-?\d+)\)')


def survey_rows(paths, reverse=False):
    """Each survey row's lines, in the logs' order (`reverse`: the logs and their rows backwards)."""
    rows = []
    for path in (reversed(paths) if reverse else paths):
        mine = []
        for line in open(path):
            if line.startswith('BOUNDARY'):
                mine.append([])
            if mine:
                mine[-1].append(line)
        rows += mine[::-1] if reverse else mine
    return rows


def read_places(paths, reverse=False):
    places, own = {}, set()
    for row in survey_rows(paths, reverse):
        key = None
        for line in row:
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
                pad = EXTRA if m.group(1).startswith('candidate enemy') else 0.0
                places[key].add((int(m.group(2)), int(m.group(3)), pad))
    places = {k: v for k, v in places.items() if k not in own and v}
    for k, v in places.items():
        if not any(pad for _, _, pad in v):
            sys.exit(f'{k[0]} ia{k[1]}: no candidate enemies in the survey (a log from before the candidates?)')
    return places


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


def make_text(paths, out, reverse=False):
    """The file's text from the survey logs (the MOVE part kept from `out`), and its counts."""
    places = read_places(paths, reverse)
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
        '; The MW1 remake\'s mission areas (FIXES 138, src/mw1_bounds.h; research/boundaries.md).',
        ';',
        '; BOUNDS: a map without an area of its own (PM\'s c1..c4, MW1\'s arctic mw3c4b) gets this one, which plays as',
        '; MW3\'s maps\' own: the radar draws the YELLOW and the RED; past the yellow the "out_of_bounds" voice; past the',
        '; red the mission is lost. Written by tools/mw1bounds.py from the boundary survey (the start, every record the',
        f'; launch may pick, an enemy\'s with {EXTRA:.0f} m round it, and every row\'s structure): {SIDES} sides each {MARGIN:.0f} m past the farthest, the red',
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
    return '\n'.join(lines) + '\n', len(places), sum(m.startswith('MOVE ') for m in moves)


LAUNCHED = re.compile(r'^(enemy|ally) (\S+) \((-?\d+), (-?\d+)\)')
YELLOWS = re.compile(r'BOUNDS \(WORLD \("([^"]+)"\) IA \((\d+)\)\s*YELLOW \(((?:\(\S+ \S+\)\s*)+)\)')


def depth_inside(poly, x, z):
    """How far (x, z) lies inside the convex outline: the nearest side's distance (negative outside)."""
    best = None
    for i, (ax, az) in enumerate(poly):
        bx, bz = poly[(i + 1) % len(poly)]
        ex, ez = bx - ax, bz - az
        d = ((x - ax) * ez - (z - az) * ex) / math.hypot(ex, ez)  # MW3's (clockwise) order: inside is +
        best = d if best is None else min(best, d)
    return best


def check_launched(paths, out):
    yellows = {}
    for m in YELLOWS.finditer(open(out).read()):
        yellows[(m.group(1), int(m.group(2)))] = [tuple(map(float, c.split())) for c in re.findall(r'\((\S+ \S+)\)', m.group(3))]
    seen, bad, shallowest = 0, 0, None
    for row in survey_rows(paths):
        w = row[0].split()
        key = (w[1], int(w[2][2:]))
        if key not in yellows:
            continue
        for line in row:
            m = LAUNCHED.match(line.strip())
            if not m:
                continue
            seen += 1
            d = depth_inside(yellows[key], int(m.group(3)), int(m.group(4)))
            shallowest = d if shallowest is None else min(shallowest, d)
            if d < MARGIN - 1.0:  # the log's whole metres
                bad += 1
                print(f'{key[0]} ia{key[1]}: {m.group(1)} {m.group(2)} only {d:.0f} m inside the yellow')
    print(f'{out}: {seen} launched places on {len(yellows)} areas, {bad} less than {MARGIN:.0f} m inside'
          f'{"" if shallowest is None else f" (the shallowest {shallowest:.0f} m)"}')
    return bad == 0 and seen > 0


def main():
    args = sys.argv[1:]
    out = 'data/mw1bounds.zrd'
    if '-o' in args:
        i = args.index('-o')
        out = args[i + 1]
        del args[i:i + 2]
    if args and args[0] == '--repeat':
        a, n, moves = make_text(args[1:], out)
        b, _, _ = make_text(args[1:], out, reverse=True)
        print(f'{n} areas, {moves} moves: {"the same, byte for byte" if a == b else "DIFFERENT"}')
        sys.exit(0 if a == b else 1)
    if args and args[0] == '--launched':
        sys.exit(0 if check_launched(args[1:], out) else 1)
    text, n, moves = make_text(args, out)
    with open(out, 'w') as f:
        f.write(text)
    print(f'{out}: {n} areas, {moves} moves')


if __name__ == '__main__':
    main()
