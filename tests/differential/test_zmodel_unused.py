"""The unreferenced zModel helpers (docs/render.md, The unused zModel helpers; gmod_const, gmod_screen) against
Mech3.exe 1.2, at the x87's 24 bits:
- 0x53a740 a polygon's planarity, on flat, nearly flat and bent polygons (and degenerate ones);
- 0x53a1a0 its concavity check, on polygons around the axes (turned flat first) and elsewhere: the answer and the
  printed lines (printf stood in; the "vertex %d" lines by their index only: the exe indexes them by bytes, FIX (bug));
- 0x53b050 a light added to a model: the list's count, the record (its +0x1a, +0x2a high halves are the exe's stack:
  masked), the points made, the caller's points after (type 1 lowers two); a closing edge reads the point after the
  last in the exe, so the exe gets the first there, lowered as by then (the port's FIX (A));
- 0x53d130 one point to the screen (project_point) and 0x53d360 n of them, as test_project_point.py sets up;
- 0x53de50 / 0x53df00 the matrices under the top and at it moved.

    cmake --build build --target mw3probe && python3 tests/differential/test_zmodel_unused.py [runs]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_X87', '24')
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

F = ctypes.c_float
VIEW, FX, FY, CX, CY = 0x73fbc0, 0x73fb9c, 0x73fba0, 0x73fb88, 0x73fb8c
NEAR, CLIP_RIGHT, POINT_RECT = 0x740304, 0x740314, 0x74031c
SLOTS, TOP = 0x63d760, 0x5eda90
PRINTF, MALLOC, REALLOC = 0x5bb5f0, 0x5bb664, 0x5bb610
REALLOC_FN = None  # PM's own realloc (0x596520), stood in for as a function
FN = {'planar': 0x53a740, 'concave': 0x53a1a0, 'light': 0x53b050, 'point': 0x53d130, 'points': 0x53d360,
      'below': 0x53de50, 'two': 0x53df00}
CDECL, EXE = False, None  # test_pm_zmodel_unused sets PM's (ECX, EDX and the rest all on the stack)
f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]
FTOL_SLOT = IAT_FTOL


def fl(*v):
    return struct.pack(f'<{len(v)}f', *v)


def unf(b):
    return list(struct.unpack(f'<{len(b) // 4}f', b))


def view_matrix(r):
    """World -> camera for a camera turned by yaw and pitch (as test_project_point.py)."""
    yaw, pitch = r.uniform(-math.pi, math.pi), r.uniform(-0.6, 0.6)
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    right = (cy, 0.0, -sy)
    up = (sy * sp, cp, cy * sp)
    fwd = (sy * cp, -sp, cy * cp)
    p = [r.uniform(-500, 500), r.uniform(0, 50), r.uniform(-500, 500)]
    m = [right[0], up[0], fwd[0], right[1], up[1], fwd[1], right[2], up[2], fwd[2]]
    t = [-sum(p[i] * m[3 * i + j] for i in range(3)) for j in range(3)]
    return m + t, p, fwd, right, up



class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe=EXE) if EXE else Original()
        self.out = []
        cpu.stub_import(PRINTF, self.printf)
        cpu.stub_import(MALLOC, lambda c: c.alloc(max(1, c.arg(0))))
        if REALLOC_FN:
            cpu.stub_function(REALLOC_FN, self.realloc)
        else:
            cpu.stub_import(REALLOC, self.realloc)
        cpu.import_code(FTOL_SLOT, FTOL)
        self.buf = cpu.alloc(0x4000)
        self.buf2 = cpu.alloc(0x4000)
        self.eax = lambda: cpu.uc.reg_read(UC_X86_REG_EAX)

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.cpu.alloc(max(1, n))
        if old:
            self.cpu.write(a, self.cpu.read(old, min(n, self.sizes.get(old, n))))
        self.sizes[a] = n
        return a

    def printf(self, c):
        fmt = self.cpu.read(c.arg(0), 64).split(b'\0')[0].decode()
        args, k, i = [], 1, 0
        while i < len(fmt):
            if fmt[i] == '%':
                if fmt[i + 1] == 'd':
                    args.append(ctypes.c_int32(c.arg(k)).value)
                    k += 1
                else:
                    lo, hi = c.arg(k), c.arg(k + 1)
                    args.append(struct.unpack('<d', struct.pack('<II', lo, hi))[0])
                    k += 2
                i += 2
            else:
                i += 1
        self.out.append(fmt % tuple(args))
        return 0

    def call(self, fn, ecx, edx, stack=()):
        if CDECL:
            self.cpu.call(fn, stack_args=[ecx, edx] + list(stack))
        else:
            self.cpu.call(fn, ecx=ecx, edx=edx, stack_args=stack)

    def put(self, addr, pts):
        self.cpu.write(addr, b''.join(struct.pack('<3f', *p) for p in pts))


def rand_poly(r, kind):
    n = r.randrange(3, 9)
    if kind == 'axis':  # flat across an axis, so the concavity check turns it
        ax = r.randrange(3)
        c = r.uniform(-50, 50)
        pts = []
        for _ in range(n):
            q = [r.uniform(-50, 50) for _ in range(3)]
            q[ax] = c + r.choice([0, 0, r.uniform(-0.01, 0.01)])
            pts.append(q)
    elif kind == 'flat':
        o = [r.uniform(-50, 50) for _ in range(3)]
        u = [r.uniform(-1, 1) for _ in range(3)]
        v = [r.uniform(-1, 1) for _ in range(3)]
        pts = []
        for _ in range(n):
            a, b = r.uniform(-30, 30), r.uniform(-30, 30)
            pts.append([o[i] + a * u[i] + b * v[i] + r.choice([0, 0, r.uniform(-0.01, 0.01)]) for i in range(3)])
    else:
        pts = [[r.uniform(-50, 50) for _ in range(3)] for _ in range(n)]
    if r.random() < 0.05:
        pts[1] = list(pts[0])
    return [[f32(v) for v in p] for p in pts]


def main(lib=None):
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    lib = lib or load_probe()
    lib.probe_zu_project_points.restype = ctypes.c_uint32
    exe = Exe()
    cpu = exe.cpu
    r = random.Random(1999)
    bad = {}

    def fail(what, msg):
        bad[what] = bad.get(what, 0) + 1
        if bad[what] <= 3:
            print(f'{what}: {msg}')

    seen = {'planar': [0, 0], 'concave': [0, 0], 'light': {}, 'points': 0, 'moves': 0}
    for run in range(runs):
        kind = r.choice(['axis', 'flat', 'any'])
        pts = rand_poly(r, kind)
        n = len(pts)
        P = (F * (3 * n))(*[v for p in pts for v in p])
        # 0x53a740
        exe.put(exe.buf, pts)
        exe.call(FN['planar'], n, exe.buf)
        want = exe.eax() & 0xff
        got = lib.probe_zu_planar(n, P)
        seen['planar'][want & 1] += 1
        if bool(want) != bool(got):
            fail('planar', f'run {run} {kind} {pts}: exe {want} port {got}')
        # 0x53a1a0
        exe.put(exe.buf, pts)
        cpu.u32(exe.buf2, n)
        exe.out = []
        exe.call(FN['concave'], exe.buf2, exe.buf)
        want = exe.eax() & 0xff
        sbuf = ctypes.create_string_buffer(4096)
        got = lib.probe_zu_concave(n, P, sbuf, len(sbuf))
        seen['concave'][want & 1] += 1
        wl, gl = ''.join(exe.out).splitlines(), sbuf.value.decode().splitlines()
        cut = lambda ls: [l.split(':')[0] if l.startswith('  vertex ') and not l.startswith(('  vertex 0', '  vertex 1')) else l for l in ls]
        if bool(want) != bool(got) or cut(wl) != cut(gl):
            fail('concave', f'run {run} {kind} {pts}: exe {want} {wl} port {got} {gl}')
    # 0x53b050
    model, sizes = cpu.alloc(0x60), {}
    exe.sizes = sizes
    for run in range(runs):
        n = r.randrange(1, 7)
        pts = [[f32(r.uniform(-60, 60)) for _ in range(3)] for _ in range(n)]
        typ = r.choice([0, 1, 1, 1, 2])
        light = bytearray(r.getrandbits(8) for _ in range(0x4c))
        struct.pack_into('<I', light, 0, typ)
        struct.pack_into('<f', light, 8, f32(r.uniform(0, 10)))
        struct.pack_into('<I', light, 0xc, r.choice([0, 0, r.randrange(1, n + 1)]))
        struct.pack_into('<f', light, 0x10, f32(r.choice([r.uniform(0.5, 20), r.uniform(20, 200)])))
        existing = r.random() < 0.3
        cpu.write(model, bytes(0x60))
        if existing:
            old = cpu.alloc(0x4c)
            cpu.write(old, b'\x77' + bytes(0x4b))
            sizes[old] = 0x4c
            cpu.u32(model + 0x40, old)
            cpu.u32(model + 0x20, 1)
        # The point after the last, which the exe's closing edge reads: the first as the port's FIX (A) has it by then
        # (type 1 lowers it by 0.1 before filling).
        exe.put(exe.buf, pts + [[pts[0][0], f32(pts[0][1] - f32(0.1)) if typ == 1 and n >= 2 else pts[0][1], pts[0][2]]])
        desc = cpu.alloc(0x4c)
        cpu.write(desc, bytes(light))
        exe.call(FN['light'], model, desc, (n, exe.buf))
        cnt = struct.unpack('<i', cpu.read(model + 0x20, 4))[0]
        rec, verts = bytearray(0x4c), b''
        if cnt > int(existing):
            base = struct.unpack('<I', cpu.read(model + 0x40, 4))[0]
            rec = bytearray(cpu.read(base + 0x4c * (cnt - 1), 0x4c))
            vp = struct.unpack('<I', rec[0x2c:0x30])[0]
            k = min(64, struct.unpack('<i', rec[0xc:0x10])[0])
            verts = cpu.read(vp, 12 * k) if k > 0 else b''
            rec[0x2c:0x30] = bytes(4)
        pts_after = cpu.read(exe.buf, 12 * n)
        P = (F * (3 * n))(*[v for p in pts for v in p])
        out = (ctypes.c_uint8 * (4 + 0x4c + 64 * 12))()
        lib.probe_zu_add_light(bytes(light), n, P, int(existing), out)
        gcnt = struct.unpack('<i', bytes(out[0:4]))[0]
        grec = bytearray(out[4:4 + 0x4c])
        k = max(0, min(64, struct.unpack('<i', bytes(grec[0xc:0x10]))[0])) if gcnt > int(existing) else 0
        gverts = bytes(out[4 + 0x4c:4 + 0x4c + 12 * k])
        for off in (0x1a, 0x1b, 0x2a, 0x2b):  # the exe's stack
            rec[off] = grec[off] = 0
        key = (typ, cnt - int(existing))
        seen['light'][key] = seen['light'].get(key, 0) + 1
        if (cnt, bytes(rec), verts, pts_after) != (gcnt, bytes(grec), gverts, bytes(P)):
            fail('light', f'run {run} type {typ} n {n} existing {existing} pts {pts} edges {struct.unpack_from("<I", light, 0xc)[0]} spacing {struct.unpack_from("<f", light, 0x10)[0]}: exe {cnt} {bytes(rec).hex()} {verts.hex()} '
                          f'{pts_after.hex()}\n  port {gcnt} {bytes(grec).hex()} {gverts.hex()} {bytes(P).hex()}')
    # 0x53d130 and 0x53d360
    cpu.fpu_control(0x007f)
    world_b, out_b = cpu.alloc(16 * 12), cpu.alloc(16 * 12)
    for run in range(runs):
        m, cam, fwd, right, up = view_matrix(r)
        w, h = r.choice([(640, 480), (800, 600), (1024, 768)])
        proj = [r.uniform(200, 900), r.uniform(200, 900), w / 2 + r.uniform(-20, 20), h / 2 + r.uniform(-20, 20)]
        near = r.choice([0.5, 1.0, 2.0, 10.0])
        rect = [r.choice([0.0, 5.0]), r.choice([0.0, 5.0]), w - r.choice([1.0, 6.0]), h - r.choice([1.0, 6.0])]
        clip_right = w - r.choice([0.0, 1.0])
        npts = r.choice([1, 1, 2, 4])
        world = []
        for _ in range(npts):
            kind = r.choice(['front', 'front', 'side', 'behind', 'near'])
            d = {'front': r.uniform(near, 2000), 'side': r.uniform(near, 300), 'behind': -r.uniform(0, 2000),
                 'near': r.uniform(-near, near * 1.5)}[kind]
            sx = r.uniform(-1.5, 1.5) * d if kind != 'side' else r.uniform(-8, 8) * max(d, 1)
            sy = r.uniform(-1.2, 1.2) * d if kind != 'side' else r.uniform(-8, 8) * max(d, 1)
            world.append([cam[i] + fwd[i] * d + right[i] * sx + up[i] * sy for i in range(3)])
        cpu.write(VIEW, fl(*m))
        for a, v in ((FX, proj[0]), (FY, proj[1]), (CX, proj[2]), (CY, proj[3]), (NEAR, near), (CLIP_RIGHT, clip_right)):
            cpu.write(a, fl(v))
        cpu.write(POINT_RECT, fl(*rect))
        cpu.write(world_b, fl(*[v for p in world for v in p]))
        clip = (F * 6)(near, clip_right, *rect)
        W = (F * (3 * npts))(*[v for p in world for v in p])
        if npts == 1:
            exe.call(FN['point'], world_b, out_b)
            code_o = exe.eax()
            o3 = (F * 3)()
            lib.probe_zu_project_point.restype = ctypes.c_uint32
            code_r = lib.probe_zu_project_point((F * 12)(*m), (F * 4)(*proj), clip, W, o3)
            got_o, got_r = unf(bytes(cpu.read(out_b, 12))), list(o3)
        else:
            exe.call(FN['points'], world_b, out_b, (npts,))
            code_o = exe.eax()
            O = (F * (3 * npts))()
            code_r = lib.probe_zu_project_points((F * 12)(*m), (F * 4)(*proj), clip, W, npts, O)
            got_o, got_r = unf(bytes(cpu.read(out_b, 12 * npts))), list(O)
        seen['points'] += 1
        ok = code_o == code_r and all(abs(a - b) <= 2e-4 * max(1.0, abs(a), abs(b)) or (a != a and b != b)
                                      for a, b in zip(got_o, got_r))
        if not ok:
            fail('points', f'run {run} n {npts}: exe {code_o:#x} {got_o} port {code_r:#x} {got_r}')
    # 0x53de50 / 0x53df00
    mats = cpu.alloc(0x60)
    for run in range(runs):
        which = r.randrange(2)
        mm = [f32(r.uniform(-2, 2)) for _ in range(9)] + [f32(r.uniform(-500, 500)) for _ in range(3)]
        mm += [f32(r.uniform(-2, 2)) for _ in range(9)] + [f32(r.uniform(-500, 500)) for _ in range(3)]
        v = [f32(r.uniform(-100, 100)) for _ in range(3)]
        cpu.write(mats, fl(*mm))
        cpu.u32(SLOTS + 4, mats)
        cpu.u32(SLOTS + 8, mats + 0x30)
        cpu.u32(TOP, SLOTS + 8)
        cpu.write(exe.buf, fl(*v))
        exe.call((FN['below'], FN['two'])[which], exe.buf, 0)
        want = cpu.read(mats, 0x60)
        M = (F * 24)(*mm)
        lib.probe_zu_move(which, (F * 3)(*v), M)
        seen['moves'] += 1
        if want != bytes(M):
            fail('moves', f'run {run} {which}: exe {unf(want)} port {list(M)}')
    print(f'planar {seen["planar"]} (not, yes); concave {seen["concave"]}; lights (type, made) {sorted(seen["light"].items())}; '
          f'{seen["points"]} screen points; {seen["moves"]} moves')
    print(f'differ: {bad if bad else "none"}')
    print('OK' if not bad else 'FAIL')
    sys.stdout.flush()
    os._exit(0 if not bad else 1)


if __name__ == '__main__':
    main()
