"""Pirate's Moon's IF / ELSEIF condition (0x51fce0; MW3's 0x507010 with five conditions more and its own order)
against PM's exe: random IF records followed by ELSEIF, ELSE, ENDIF and other events, each condition (1 random,
0x20 fog, 0x40, 2 the player's distance, 8 and 0x10 the ray down, 0x2000 a node's active flag, 0x80, 0x100 a
node's height, 4 the level, 0x200 a node's orientation against the block's quaternion, 0x400 the block's value,
0x800 and 0x1000 the health) alone and two at once, on nodes with cached world matrices. Compared: the result, where
the sequence goes on and the random table's index. The ray down (0x520080) answers the same on both sides; the
distance (0x5201b0), the node's place (0x4ff250) and orientation (0x4ff2f0, 0x53ec80, 0x53f020, 0x53f310) run as
they are; the quaternion helpers also alone. At 24-bit precision, as the anim run plays.

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_if.py [cases]
"""
import ctypes
import math
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

IF, RAY = 0x51fce0, 0x520080
RANDOM, RANDOM_INDEX, FOG, LEVEL = 0x8d3be0, 0x63a35c, 0x8c4b70, 0x639f44
# g's words in the exe: block +0x24, +0x3c, PM's +0x34, +0x38, the player x y z, the quaternion, +0x60
G_AT = {2: 0x8d3f2c, 3: 0x8d3f3c, 5: 0x8d3f34, 6: 0x8d3f38, 7: 0x8d3f44, 8: 0x8d3f48, 9: 0x8d3f4c, 10: 0x8d3f50,
        11: 0x8d3f54, 12: 0x8d3f58, 13: 0x8d3f5c, 14: 0x8d3f60}
# _CIasin: st0 = atan2(x, sqrt(1 - x*x)); _copysign (test_matrix.py's): the matrix to Euler angles uses both.
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')
COPYSIGN = bytes.fromhex('8b442410 2500000080 8b4c2408 81e1ffffff7f 0bc8 894c2408 dd442404 c3')
FLAGS = (1, 0x20, 0x40, 2, 8, 0x10, 0x2000, 0x80, 0x100, 4, 0x200, 0x400, 0x800, 0x1000)


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def rotation(r):
    a, b, c = (r.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    m = [cb * cc, cb * sc, -sb, sa * sb * cc - ca * sc, sa * sb * sc + ca * cc, sa * cb,
         ca * sb * cc + sa * sc, ca * sb * sc - sa * cc, ca * cb]
    return [struct.unpack('<f', struct.pack('<f', x))[0] for x in m] + [r.uniform(-50, 50) for _ in range(3)]


def condition(r, f):
    """An IF / ELSEIF's flags, value and upper bound."""
    first = next(x for x in FLAGS if f & x)
    if first == 4:
        value = r.randint(0, 5)
    elif first == 0x200:
        value = fb(r.choice((0.01, 0.5, 2.0, 9.0, r.uniform(0, 12))))
    else:
        value = fb(r.choice((0.0, 0.25, 0.5, 1.0, 3.0, 50.0, 2500.0, r.uniform(-5, 3000), -1.0)))
    return f, value, fb(r.choice((0.5, 2.0, 60.0, 3000.0, r.uniform(0, 3000))))


def case(r):
    def flags():
        f = r.choice(FLAGS)
        if r.random() < 0.3:
            f |= r.choice(FLAGS)
        return f
    f0 = flags()
    index = r.choice((0, 0, 1) if f0 & 0x300 else (0, 0, 1, -1))
    ev = bytearray()

    def rec(kind, f=0, v=0, top=0, size=0x1c):
        b = bytearray(size)
        b[0] = kind
        struct.pack_into('<i', b, 4, size)
        if kind in (31, 33):
            struct.pack_into('<IiII', b, 0xc, f, index, v, top)
        return b
    ev += rec(31, *condition(r, f0))
    for _ in range(r.randint(0, 4)):
        kind = r.choice((33, 33, 6, 32, 34))
        if kind == 33:
            f = flags()
            while f & 0x300 and index == -1:
                f = flags()
            ev += rec(33, *condition(r, f))
        else:
            ev += rec(kind, size=0x10)
    if r.random() < 0.7:
        ev += rec(34, size=0xc)
    q = [r.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q)) or 1
    g = [r.randint(0, 199), r.choice((0, 1)), r.choice((0, 1)), r.choice((0, 1)), r.randint(0, 5), r.choice((0, 1)),
         r.choice((0, 1))]
    g += [fb(r.uniform(-60, 60)) for _ in range(3)] + [fb(x / n) for x in q] + [fb(r.uniform(-10, 3000))]
    g += [fb(r.choice((0.0, 1.0, 40.0, r.uniform(-10, 3000)))), r.choice((0, 4, 0x10, 4 | 0x10))]
    return dict(ev=bytes(ev), g=g, random=[r.random() for _ in range(200)], ma=rotation(r), mb=rotation(r))


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.calls = 0

        def ray(c):  # cdecl: node, pos, *height, *hit
            h = struct.unpack('<I', cpu.read(c.arg(2), 4))[0]
            cpu.u32(c.arg(3), h & 1)
            self.calls += 1
            return (h >> 1) & 1
        cpu.stub_function(RAY, ray)
        cpu.import_code(IAT['_CIasin'], ASIN)
        cpu.import_code(IAT['_copysign'], COPYSIGN)

    def alloc(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def node(self, flags, m):
        n, o = self.alloc(0xd4), self.alloc(0x90)
        self.cpu.u32(n + 0x34, 5)
        self.cpu.u32(n + 0x24, flags | 0x80000)
        self.cpu.u32(n + 0x38, o)
        self.cpu.write(o + 0x60, struct.pack('<12f', *m))
        return n

    def run(self, c):
        cpu, g = self.cpu, c['g']
        a, b = self.node(g[16], c['ma']), self.node(0, c['mb'])
        refs = self.alloc(0x58)
        cpu.u32(refs + 0x28, a)
        d = self.alloc(0x110)
        cpu.u32(d + 0xe8, refs)
        cpu.u32(d + 0x6c, b)
        cpu.u32(d + 0xb8, g[15])
        ev = self.alloc(len(c['ev']) + 0x20)
        cpu.write(ev, c['ev'])
        s = self.alloc(0x40)
        cpu.u32(s + 0x34, ev)
        cpu.u32(s + 0x38, ev)
        cpu.u32(s + 0x3c, len(c['ev']))
        cpu.write(RANDOM, struct.pack('<200f', *c['random']))
        cpu.u32(RANDOM_INDEX, g[0])
        cpu.u32(FOG, g[1])
        cpu.u32(LEVEL, g[4])
        for k, at in G_AT.items():
            cpu.u32(at, g[k])
        cpu.call(IF, stack_args=(d, s, ev))
        res = cpu._uc.reg_read(UC_X86_REG_EAX)
        cur = struct.unpack('<I', cpu.read(s + 0x34, 4))[0]
        return [res, cur - ev, struct.unpack('<i', cpu.read(RANDOM_INDEX, 4))[0]]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_if.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_uint32),
                              ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                              ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_int32)]
    exe = Exe()
    r = random.Random(0x51fce0)
    bad = 0
    seen = {}
    for k in range(cases):
        c = case(r)
        want = exe.run(c)
        out = (ctypes.c_int32 * 3)()
        p.probe_pm_if(ctypes.create_string_buffer(c['ev'], len(c['ev']) + 0x20), len(c['ev']),
                      (ctypes.c_uint32 * 17)(*c['g']), (ctypes.c_float * 200)(*c['random']),
                      (ctypes.c_float * 12)(*c['ma']), (ctypes.c_float * 12)(*c['mb']), out)
        got = list(out)
        f0 = struct.unpack_from('<I', c['ev'], 0xc)[0]
        first = next(x for x in FLAGS if f0 & x)
        key = f'{first:#x} {"true" if want[1] == 0 else "on"}'
        seen[key] = seen.get(key, 0) + 1
        if got != want:
            bad += 1
            if bad <= 5:
                print(f'case {k}: flags {f0:#x} exe {want} port {got}')
    # The quaternion helpers alone, bit for bit: the product (0x53f020) and the log (0x53f310).
    fp = ctypes.POINTER(ctypes.c_float)
    p.probe_pm_quat.argtypes = [fp, fp, fp, fp]
    qbad = 0
    for k in range(3000):
        a = [r.uniform(-1, 1) for _ in range(4)]
        b = [r.uniform(-1, 1) for _ in range(4)]
        if k % 10 == 0:
            a[1:] = [x * r.choice((1e-3, 1e-6, 0.0)) for x in a[1:]]
        qa, qb = struct.pack('<4f', *a), struct.pack('<4f', *b)
        pa, pb, po = exe.alloc(16), exe.alloc(16), exe.alloc(16)
        exe.cpu.write(pa, qa), exe.cpu.write(pb, qb)
        exe.cpu.call(0x53f020, stack_args=(pa, pb, po))
        want = exe.cpu.read(po, 16)
        exe.cpu.call(0x53f310, stack_args=(pa, po))
        want += exe.cpu.read(po, 12)
        rel, lg = (ctypes.c_float * 4)(), (ctypes.c_float * 3)()
        p.probe_pm_quat((ctypes.c_float * 4).from_buffer_copy(qa), (ctypes.c_float * 4).from_buffer_copy(qb), rel, lg)
        if bytes(rel) + bytes(lg) != want:
            qbad += 1
            if qbad <= 3:
                print(f'quaternion {k}: exe {want.hex()} port {(bytes(rel) + bytes(lg)).hex()}')
    bad += qbad
    print(f'{cases} cases, {bad} differ ({qbad} of 3000 quaternions); {dict(sorted(seen.items()))}; ray calls {exe.calls}')
    missing = [f'{x:#x}' for x in FLAGS if not (seen.get(f'{x:#x} true') and seen.get(f'{x:#x} on'))]
    if bad or missing:
        raise SystemExit(f'failed (conditions never both ways: {missing})' if missing else 'failed')
    print(f'ok: PM\'s IF / ELSEIF as PM\'s exe ({cases} cases, every condition true and false)')


if __name__ == '__main__':
    main()
