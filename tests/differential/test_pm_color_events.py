"""Pirate's Moon's own anim events 44 and 45 (0x51e750, 0x51e890, through PM's handler table 0x521c70; with the node
ref 0x51e720 and the colour walk 0x50cb90 -> 0x55b8f0 -> 0x55b850) against PM's exe: random events run over several
frames on a node tree whose models' polygons have colours or none yet, the result, the time left, the event's state
and every polygon's colours compared each frame. No shipped anim.zbd has either event (docs/pm-tests.md); the run
dispatches them as PM's table does. At 24-bit precision, as the anim run plays (CLAUDE.md, FPU precision).

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_color_events.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
os.environ['MW3_X87'] = '24'
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402
from twod_world import IAT  # noqa: E402

HANDLER = {44: 0x51e750, 45: 0x51e890}
TIME_LEFT = 0x8d3f68


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def case(r):
    kind = r.choice((44, 45))
    ev = bytearray(0x78)
    ev[0] = kind
    index = r.choice((0, 0, 0, -100, -100, 1, 5, -1, -7))
    struct.pack_into('<i', ev, 0x2c, index)
    ev[0x30] = r.choice((0, 1, 3, 2))
    for off in range(0x34, 0x70, 4):
        struct.pack_into('<f', ev, off, r.uniform(-2, 2))
    polys = r.randint(1, 6)
    struct.pack_into('<hh', ev, 0x70, r.choice((-1, 0, 1, 2, polys - 1, polys + 2)), r.choice((-1, 0, 1, polys - 1, polys + 3)))
    struct.pack_into('<f', ev, 0x74, r.choice((0.0, 0.5, 1.0, 3.0, r.uniform(0, 4))))
    verts = [r.randint(0, 9) for _ in range(polys)]
    preset = [r.random() < 0.5 for _ in range(polys)]
    frames = r.randint(1, 8)
    times, t = [], 0.0
    lefts = []
    for _ in range(frames):
        step = r.choice((0.033, 0.1, 0.5, r.uniform(0, 1)))
        t += step
        times.append(f32(t))
        lefts.append(f32(r.choice((step, step * 2, 0.033, 1.0))))
    colors = [f32(r.uniform(0, 1)) for _ in range(21)]
    return dict(ev=ev, children=r.randint(0, 3), polys=polys, verts=verts, preset=preset, times=times, lefts=lefts,
                colors=colors)


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(max(c.arg(0), 4)))

    def alloc(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, c):
        cpu = self.cpu
        models = []
        nodes = []
        for i in range(c['children'] + 1):
            polys = self.alloc(0x28 * c['polys'])
            for k in range(c['polys']):
                p = polys + 0x28 * k
                cpu.u32(p, c['verts'][k])
                if c['preset'][k]:
                    col = self.alloc(12 * c['verts'][k] + 1)
                    for v in range(c['verts'][k]):
                        j = (k + v) % 7
                        cpu.write(col + 12 * v, struct.pack('<3f', *c['colors'][3 * j:3 * j + 3]))
                    cpu.u32(p + 0x1c, col)
            m = self.alloc(0x68)
            cpu.u32(m + 0x10, c['polys'])
            cpu.u32(m + 0x34, polys)
            n = self.alloc(0xd4)
            cpu.u32(n + 0x3c, m)
            models.append(m)
            nodes.append(n)
        kids = self.alloc(4 * max(1, c['children']))
        for i, n in enumerate(nodes[1:]):
            cpu.u32(kids + 4 * i, n)
        cpu.write(nodes[0] + 0x56, struct.pack('<h', c['children']))
        cpu.u32(nodes[0] + 0x5c, kids)
        refs = self.alloc(0x2c)
        cpu.u32(refs + 0x28, nodes[0])
        d = self.alloc(0x110)
        cpu.u32(d + 0xe8, refs)
        cpu.u32(d + 0x48, nodes[0])
        s = self.alloc(0x40)
        ev = self.alloc(0x78)
        cpu.write(ev, bytes(c['ev']))
        out = bytearray()
        for f in range(len(c['times'])):
            cpu.write(s + 0x20, bytes([0 if f == 0 else 1]))
            cpu.write(s + 0x28, struct.pack('<f', c['times'][f]))
            cpu.write(TIME_LEFT, struct.pack('<f', c['lefts'][f]))
            cpu.call(HANDLER[c['ev'][0]], stack_args=(d, s, ev))
            r = cpu._uc.reg_read(UC_X86_REG_EAX)
            out += struct.pack('<i', r) + cpu.read(TIME_LEFT, 4) + cpu.read(ev + 0x34, 0x78 - 0x34)
            for m in models:
                polys = self.r32(m + 0x34)
                for k in range(c['polys']):
                    col = self.r32(polys + 0x28 * k + 0x1c)
                    out += cpu.read(col, 12 * c['verts'][k]) if col else bytes(12 * c['verts'][k])
        return bytes(out)


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    p = test_pm_zar.load_pm_probe()
    f = ctypes.POINTER(ctypes.c_float)
    i = ctypes.POINTER(ctypes.c_int32)
    p.probe_pm_color_event.argtypes = [ctypes.c_char_p, ctypes.c_int, f, f, ctypes.c_int, ctypes.c_int, i, i, f,
                                       ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_color_event.restype = ctypes.c_int
    exe = Exe()
    r = random.Random(0x51e750)
    buf = ctypes.create_string_buffer(1 << 20)
    bad = 0
    seen = {'44': 0, '45': 0, 'done': 0, 'no node': 0, 'colours made': 0, 'children': 0}
    for k in range(cases):
        c = case(r)
        want = exe.run(c)
        n = len(c['times'])
        ev = ctypes.create_string_buffer(bytes(c['ev']), 0x78)
        got_n = p.probe_pm_color_event(ev, n, (ctypes.c_float * n)(*c['times']), (ctypes.c_float * n)(*c['lefts']),
                                       c['children'], c['polys'], (ctypes.c_int32 * c['polys'])(*c['verts']),
                                       (ctypes.c_int32 * c['polys'])(*c['preset']), (ctypes.c_float * 21)(*c['colors']),
                                       buf, len(buf))
        got = buf.raw[:got_n]
        seen[str(c['ev'][0])] += 1
        r0 = struct.unpack_from('<i', want)[0]
        index = struct.unpack_from('<i', c['ev'], 0x2c)[0]
        seen['no node'] += index not in (0, -100)
        seen['done'] += r0 == 2 and index in (0, -100)
        seen['colours made'] += not all(c['preset'])
        seen['children'] += c['children'] > 0 and c['ev'][0x30] & 1
        if got != want:
            bad += 1
            if bad <= 3:
                at = next((j for j in range(min(len(got), len(want))) if got[j] != want[j]), min(len(got), len(want)))
                print(f'case {k}: differ at byte {at} of {len(want)}/{len(got)}; {c["ev"][0]} index {index}')
    print(f'{cases} cases, {bad} differ; {seen}')
    if bad or min(seen.values()) == 0:
        raise SystemExit(1)
    print(f'ok: PM\'s events 44 and 45 as PM\'s exe ({cases} cases)')


if __name__ == '__main__':
    main()
