"""The unused frustum clippers (docs/render.md, The unused frustum clippers; gamez/zmodel/gmod_clipfrustum) against
Mech3.exe 1.2: 0x534bf0 (positions), 0x530d90 (and the first three corners, its texture coordinates near-cut only), 0x531d90 (and texture coordinates),
0x5331e0 (and colours), on random polygons around the view (the near and far planes on or off, random cotangents),
at the x87's 24 bits: the answer, the count, every coordinate, texture coordinate and colour as bits, the three
corners. The probe library runs with the always-on fixes off (the near plane alone: the exe's arrays). 0x530d90's
texture floats past a near cut to three or four corners are the exe's stack: counted, not compared; a NaN
(0/0, an edge lying in a plane) is the x87's 0xffc00000 there and 0x7fc00000 here: counted as equal.

    cmake --build build --target mw3probe && python3 tests/differential/test_clipfrustum.py [runs]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_X87', '24')  # the draw runs with the game up (CLAUDE.md)
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

CLIP = 0x7402f8  # the clip struct
COT, INV = 0x63d750, 0x63d740  # cot h, v; their inverses
CDECL, EXE = False, None  # test_pm_clipfrustum sets PM's (every argument on the stack, the caller pops)
FUNCS = [(0x534bf0, ''), (0x530d90, 'tu'), (0x531d90, 'tu'), (0x5331e0, 'tuc')]
MAX = 64


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def rand_case(rng):
    n = rng.randrange(3, 9)
    near = f32(rng.uniform(0.5, 8))
    far = f32(rng.uniform(20, 120))
    mask = rng.choice([0x30, 0x30, 0x10, 0x20, 0x00, 0x3f])
    clip = struct.pack('<IIIfIIfII', mask, 0, 0, near, 0, 0, far, 0, 0)
    ch, cv = (1.0, 1.0) if rng.random() < 0.25 else (rng.uniform(0.4, 2.5), rng.uniform(0.4, 2.5))  # 1: corners stay on a plane
    cot = [f32(ch), f32(cv), f32(1 / ch), f32(1 / cv)]
    span = rng.choice([10, 40, 150])
    pos = [[f32(rng.uniform(-span, span)), f32(rng.uniform(-span, span)), f32(rng.uniform(-15, 140))]
           for _ in range(n)]
    for _ in range(rng.choice([0, 0, 1, 2])):  # corners on a plane exactly
        k = rng.randrange(n)
        pos[k][rng.randrange(2)] = pos[k][2] * rng.choice([-1, 1])
    uv = [[f32(rng.uniform(-2, 2)) for _ in range(2)] for _ in range(n)]
    rgb = [[f32(rng.uniform(0, 255)) for _ in range(3)] for _ in range(n)]
    return n, clip, cot, pos, uv, rgb


class Exe:
    def __init__(self):
        self.cpu = Original(exe=EXE) if EXE else Original()
        self.pos, self.uv, self.rgb = self.cpu.alloc(MAX * 12), self.cpu.alloc(MAX * 8), self.cpu.alloc(MAX * 12)
        self.tri, self.count = self.cpu.alloc(0x40), self.cpu.alloc(4)

    def run(self, which, n, clip, cot, pos, uv, rgb, with_tri):
        cpu = self.cpu
        cpu.write(CLIP, clip)
        cpu.write(INV, struct.pack('<2f', cot[2], cot[3]))
        cpu.write(COT, struct.pack('<2f', cot[0], cot[1]))
        cpu.write(self.pos, b''.join(struct.pack('<3f', *p) for p in pos) + bytes(MAX * 12 - 12 * n))
        cpu.write(self.uv, b''.join(struct.pack('<2f', *p) for p in uv) + bytes(MAX * 8 - 8 * n))
        cpu.write(self.rgb, b''.join(struct.pack('<3f', *p) for p in rgb) + bytes(MAX * 12 - 12 * n))
        cpu.write(self.tri, b'\xab' * 0x40)
        cpu.u32(self.count, n)
        addr, args = FUNCS[which]
        stack = []
        for a in args:
            stack.append({'t': self.tri if with_tri else 0, 'u': self.uv, 'c': self.rgb}[a])
        stack.append(self.count)
        if CDECL:
            cpu.call(addr, stack_args=[CLIP, self.pos] + stack)
        else:
            cpu.call(addr, ecx=CLIP, edx=self.pos, stack_args=stack)
        r = ctypes.c_int32(cpu.uc.reg_read(UC_X86_REG_EAX)).value
        m = struct.unpack('<i', cpu.read(self.count, 4))[0]
        k = max(0, min(m, MAX))
        return (r, m, cpu.read(self.pos, 12 * k), cpu.read(self.uv, 8 * k) if 'u' in args else b'',
                cpu.read(self.rgb, 12 * k) if 'c' in args else b'', cpu.read(self.tri, 0x24))


def port(lib, which, n, clip, cot, pos, uv, rgb, with_tri):
    P = (ctypes.c_float * (MAX * 3))(*[v for p in pos for v in p])
    U = (ctypes.c_float * (MAX * 2))(*[v for p in uv for v in p])
    C = (ctypes.c_float * (MAX * 3))(*[v for p in rgb for v in p])
    T = (ctypes.c_uint8 * 0x40)(*([0xab] * 0x40))
    cnt = ctypes.c_int32(n)
    r = lib.probe_clipfrustum(which, clip, (ctypes.c_float * 4)(*cot), P, T if with_tri else None, U, C,
                              ctypes.byref(cnt))
    m = cnt.value
    k = max(0, min(m, MAX))
    args = FUNCS[which][1]
    return (r, m, bytes(P)[:12 * k], bytes(U)[:8 * k] if 'u' in args else b'', bytes(C)[:12 * k] if 'c' in args else b'',
            bytes(T)[:0x24])


X87_53 = os.environ.get('MW3_X87') == '53'


def close(want, got):
    """Same answer and count, every float within 1e-4 relative (or 1e-4 absolute)."""
    if want[:2] != got[:2]:
        return False
    for a, b in zip(want[2:], got[2:]):
        if len(a) != len(b):
            return False
        fa = struct.unpack(f'<{len(a) // 4}f', a[:len(a) // 4 * 4])
        fb = struct.unpack(f'<{len(b) // 4}f', b[:len(b) // 4 * 4])
        if any(abs(x - y) > 1e-4 * max(1.0, abs(x)) for x, y in zip(fa, fb)):
            return False
    return True


def canon(r):
    """Every NaN as one: the x87's default NaN (0xffc00000, a 0/0 cut along an edge in a plane) and the port's
    (0x7fc00000) differ in sign alone."""
    out = []
    for b in r[2:]:
        w = list(struct.unpack(f'<{len(b) // 4}I', b[:len(b) // 4 * 4]))
        out.append(struct.pack(f'<{len(w)}I', *[0x7fc00000 if (x & 0x7f800000) == 0x7f800000 and x & 0x7fffff else x
                                                for x in w]))
    return tuple(r[:2]) + tuple(out)


def leftover_only(want, got, case):
    """0x530d90's 9 floats back after a near cut: past 2 * count of them the exe copies its stack; the port leaves
    the caller's values there. At 53 bits the rest is compared within close()'s tolerance."""
    same = (lambda a, b: close((0, 0, a), (0, 0, b))) if X87_53 else (lambda a, b: a == b)
    if want[:2] != got[:2] or not same(want[2], got[2]) or not all(same(a, b) for a, b in zip(want[4:], got[4:])):
        return False
    w = struct.unpack(f'<{len(want[3]) // 4}f', want[3])
    g = struct.unpack(f'<{len(got[3]) // 4}f', got[3])
    orig = [v for p in case[4] for v in p] + [0.0] * 128
    first = next((k for k in range(len(w)) if not same(struct.pack('<f', w[k]), struct.pack('<f', g[k]))), None)
    return first in (6, 8) and all(struct.pack('<f', g[k]) == struct.pack('<f', orig[k]) for k in range(first, min(9, len(g)))) \
        and same(want[3][36:], got[3][36:])

def main(lib=None):
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    lib = lib or load_probe()
    lib.probe_clipfrustum.restype = ctypes.c_int32
    exe = Exe()
    rng = random.Random(1999)
    bad = [0] * 4
    ulps = [0] * 4
    leftovers = 0
    nans = 0
    paths = [dict() for _ in range(4)]
    for run in range(runs):
        case = rand_case(rng)
        with_tri = rng.random() < 0.8
        for which in range(4):
            want = exe.run(which, *case, with_tri)
            got = port(lib, which, *case, with_tri)
            if want != got and canon(want) == canon(got):
                nans += 1
                continue
            key = (want[0], want[1] != case[0])
            paths[which][key] = paths[which].get(key, 0) + 1
            if which == 1 and want != got and leftover_only(want, got, case):
                leftovers += 1  # FIX (A): the exe's floats past a three- or four-corner near cut are its stack
                continue
            if want != got and X87_53 and close(want, got):
                ulps[which] += 1  # the x87's wider intermediates at 53 bits: within 1e-4 relative, the same counts
                continue
            if want != got:
                bad[which] += 1
                if bad[which] <= 2:
                    print(f'run {run} {FUNCS[which][0]:#x}: case {case}\n exe:  {want[:2]} port: {got[:2]}')
                    for i, nm in ((2, 'pos'), (3, 'uv'), (4, 'rgb'), (5, 'tri')):
                        if want[i] != got[i]:
                            print(f'  {nm} exe  {want[i].hex()}\n  {nm} port {got[i].hex()}')
    for which in range(4):
        print(f'{FUNCS[which][0]:#x}: {runs} polygons, {bad[which]} differ'
              + (f' ({ulps[which]} by rounding alone)' if X87_53 else '') + f'; answers and count changes {paths[which]}')
    print(f'{FUNCS[1][0]:#x}: {leftovers} near cuts to three or four corners, the exe copying its stack past them (FIX (A))')
    print(f'{nans} results equal but for the NaN sign (a 0/0 cut along an edge lying in a plane)')
    ok = not any(bad)
    print('OK' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
