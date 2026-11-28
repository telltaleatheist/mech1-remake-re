"""Pirate's Moon's multiplayer stations, 0x4938e0 (the port's program_mp_stations with the table's
mp_stations_mark, tests/probe_pm_mp_stations.cpp), against PM's exe. On an MCB waynet of dock points, each
not yet marked (+0x0d, a static base's) gets a copy of the world's mfb_station01, named mfb_station_04 on,
added to the world, placed at the point, shown, and the point labelled with the name and marked. Compared
per point: its mark after, its station's number, the station's place, shown, in the world. The exe's calls
are stubbed (the list find, the tree copy, the world's add, the node setters, the label); the port runs on
real nodes. Cases: 0 to 24 points, some marked, the station model missing.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_mp_stations.py [cases]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

STATIONS = 0x4938e0
ELEM, LIST_FIND, TREE_COPY, SET_POS, SET_NAME, SET_ACTIVE, LABEL = (
    0x5acad0, 0x50b920, 0x50d7c0, 0x510300, 0x4fcbe0, 0x4fca50, 0x47c800)
CSTRING_CTOR, CSTRING_FORMAT, CSTRING_DTOR = 0x5ce8e4, 0x5ce8de, 0x5ce8d8
WORLD = 0x62ca90  # the world's object (its vt+4 adds a child)
STATION, COPY_BASE = 0x7000, 0x7100


def f32(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


class Emu(test_pm_zar.Emulated):
    def __init__(self, case):
        super().__init__()
        self.case = case
        n = len(case['pos'])
        self.points = []
        for i in range(n):
            p = self.alloc(0x38)
            vt = self.alloc(0x40)
            at = self.alloc(12)
            self.cpu._uc.mem_write(at, struct.pack('<3f', *case['pos'][i]))
            self.w32(vt + 0x18, self.fn_ptr(lambda c, at=at: at))
            self.w32(p, vt)
            self.cpu._uc.mem_write(p + 0xd, bytes([case['marks'][i]]))
            self.points.append(p)
        self.net = self.alloc(0x40)
        nvt = self.alloc(0x40)
        self.w32(nvt + 0x10, self.fn_ptr(lambda c: n))
        self.w32(self.net, nvt)
        world = self.alloc(8)
        wvt = self.alloc(0x40)
        self.w32(wvt + 4, self.fn_ptr(self.world_add, 4))
        self.w32(world, wvt)
        self.w32(WORLD, world)
        self.copies = 0
        self.nodes = {}  # copy -> {'at', 'name', 'shown', 'in_world'}
        self.labels = {}  # point index -> label
        self.native_stub(ELEM, lambda c: self.points[c.arg(0)] if 0 <= c.arg(0) < n else 0, 4)
        self.native_stub(LIST_FIND, self.list_find)
        self.native_stub(TREE_COPY, self.tree_copy)
        self.native_stub(SET_POS, lambda c: self.node(c.arg(0)).update(at=(f32(c.arg(1)), f32(c.arg(2)), f32(c.arg(3)))) or 0)
        self.native_stub(SET_NAME, lambda c: self.node(c.arg(0)).update(name=self.cstr(c.arg(1)).decode()) or 0)
        self.native_stub(SET_ACTIVE, lambda c: self.node(c.arg(0)).update(shown=c.arg(1) != 0) or 0)
        self.native_stub(LABEL, self.label, 4)
        self.native_stub(CSTRING_CTOR, self.cs_ctor)
        self.native_stub(CSTRING_DTOR, lambda c: 0)
        self.native_stub(CSTRING_FORMAT, self.cs_format)

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def fn_ptr(self, fn, pop=0):
        at = self.cpu.code(b'\xc3' * 16)
        self.native_stub(at, fn, pop)
        return at

    def keep(self, s):
        p = self.alloc(len(s) + 1)
        self.cpu._uc.mem_write(p, s.encode('latin-1') + b'\0')
        return p

    def node(self, h):
        return self.nodes.setdefault(h, {'at': (0.0, 0.0, 0.0), 'name': '', 'shown': False, 'in_world': False})

    def list_find(self, c):
        return STATION if self.case['station'] and c.arg(0) == 6 and self.cstr(c.arg(1)) == b'mfb_station01' else 0

    def tree_copy(self, c):
        if c.arg(0) != STATION:
            return 0
        self.copies += 1
        h = COPY_BASE + self.copies
        self.node(h)
        return h

    def world_add(self, c):
        self.node(c.arg(0))['in_world'] = True
        return 0

    def label(self, c):
        self.labels[self.points.index(c.ecx)] = self.cstr(c.arg(0)).decode()
        return 0

    def cs_ctor(self, c):
        self.w32(c.ecx, self.keep(''))
        return c.ecx

    def cs_format(self, c):
        fmt = self.cstr(c.arg(1)).decode('latin-1')
        assert fmt == 'mfb_station_%02d', fmt
        self.w32(c.arg(0), self.keep(fmt % c.arg(2)))
        return 0


def run_exe(case):
    e = Emu(case)
    e.call(STATIONS, args=[e.net])
    out = []
    for i, p in enumerate(e.points):
        mark = bytes(e.cpu._uc.mem_read(p + 0xd, 1))[0]
        lab = e.labels.get(i)
        row = [mark, int(lab[len('mfb_station_'):]) if lab else -1, (0.0, 0.0, 0.0), False, False]
        if lab:
            node = next((v for v in e.nodes.values() if v['name'] == lab), None)
            if node:
                row[2:] = [node['at'], node['shown'], node['in_world']]
        out.append(row)
    return len(e.labels), out


def run_port(probe, case):
    n = len(case['pos'])
    pos = (ctypes.c_float * max(1, 3 * n))(*[v for p in case['pos'] for v in p])
    marks = (ctypes.c_uint8 * max(1, n))(*case['marks'])
    om = (ctypes.c_uint8 * max(1, n))()
    num = (ctypes.c_int32 * max(1, n))()
    at = (ctypes.c_float * max(1, 3 * n))()
    shown = (ctypes.c_int32 * max(1, n))()
    inw = (ctypes.c_int32 * max(1, n))()
    made = probe.probe_pm_mp_stations(n, pos, marks, case['station'], om, num, at, shown, inw)
    out = [[om[i], num[i], (at[3 * i], at[3 * i + 1], at[3 * i + 2]), bool(shown[i]), bool(inw[i])] for i in range(n)]
    return made, out


def make_case(seed):
    r = random.Random(seed)
    n = r.choice([0, 1, 2, 3, 5, 8, r.randrange(1, 25)])
    f = lambda: struct.unpack('<f', struct.pack('<f', r.uniform(-4000, 4000)))[0]
    return {'pos': [(f(), f(), f()) for _ in range(n)],
            'marks': [r.choice([0, 0, 0, 1, 2, 0xff]) for _ in range(n)],
            'station': 0 if r.random() < 0.1 else 1}


def main():
    probe = test_pm_zar.load_pm_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    made_total = skipped = 0
    for seed in range(count):
        case = make_case(seed)
        a, b = run_exe(case), run_port(probe, case)
        if a != b:
            for i, (x, y) in enumerate(zip(a[1], b[1])):
                if x != y:
                    raise SystemExit(f'seed {seed} point {i}: exe {x}, port {y} (made {a[0]} / {b[0]})')
            raise SystemExit(f'seed {seed}: made {a[0]} (exe), {b[0]} (port)')
        made_total += a[0]
        skipped += sum(1 for m in case['marks'] if m) if case['station'] else 0
    pm_floors.floors(cases=(count, 300), stations=(made_total, 500), passed_over=(skipped, 100))
    print(f'ok: {count} cases, {made_total} stations made, {skipped} marked points passed over, identical')


if __name__ == '__main__':
    main()
