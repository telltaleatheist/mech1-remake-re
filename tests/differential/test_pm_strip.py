"""Pirate's Moon's triangle stripper (pm::zmod, gmod_strip; docs/pm-tests.md, PM's triangle stripper) against PM's
exe.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_strip.py [runs]
    STRIP_ALL=1 ...   every shipped type-0 model with polygons, the four worlds' and mechlib's (slow)

Run alone in the exe and compared with the port: the layer merge 0x572df0; the stripper 0x570b70 stopped where its
intake ends (0x570ef7), its edge and point maps read out of its frame (MSVC's trees walked in order); the join test
0x573f50 and the join's shape 0x56b980 on two polygons made to share an edge; and the whole stripper 0x570b70 on
grids of ground made to join (triangles and quads on shared corners, coordinates from the grid now and then moved by
whole repeats, rows or parts of rows already strips of either parity, polygons twice, a few off), the polygons, the
material refs, flag 0x80 and the counts [0x8c1504..0x8c151c] compared after. Then a seeded sample of the worlds'
type-0 models (every Nth plus the biggest), their materials given images at random, through each, the whole
stripper as the crater re-read calls it (4 points) and as the others do. 53 bits on both sides. The images' load
0x5329e0, malloc / realloc / free, _ftol and operator new / delete stand in on the exe's side; MSVCP's _Lockit does
nothing.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EAX

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
import original  # noqa: E402
from original import Original  # noqa: E402

MERGE, STRIP, INTAKE_END, TRY_JOIN, JOIN_SHAPE = 0x572df0, 0x570b70, 0x570ef7, 0x573f50, 0x56b980
MODEL, ARG, PASS, SLACK = 0x8c14fc, 0x8c14f4, 0x8c1500, 0x8c14f8
LOAD_IMAGES, REALLOC, NEW, DELETE = 0x5329e0, 0x596520, 0x5ce8c6, 0x5ce8c0
IAT_FREE, IAT_MALLOC, IAT_LOCKIT, IAT_UNLOCKIT, IAT_FTOL = 0x5de65c, 0x5de664, 0x5de4f8, 0x5de4fc, 0x5de644
COUNT_ALL, COUNT_TAKEN = 0x8c1504, 0x8c1508
EDGE_NIL, VERTEX_NIL = 0x8c1524, 0x8c152c


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        self.sizes = {}
        cpu.stub_function(LOAD_IMAGES, lambda c: 0)
        cpu.stub_function(REALLOC, self.realloc)
        cpu.stub_function(NEW, lambda c: self.sized(c.arg(0)))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_MALLOC, lambda c: self.sized(c.arg(0)))
        cpu.stub_import(IAT_LOCKIT, lambda c: 0)
        cpu.stub_import(IAT_UNLOCKIT, lambda c: 0)
        cpu.import_code(IAT_FTOL, original.FTOL)  # MSVCRT's _ftol: the x87 top cut toward zero
        self.state, self.whole = None, False
        cpu._uc.hook_add(UC_HOOK_CODE, self.intake_end, begin=INTAKE_END, end=INTAKE_END)
        self.heap = cpu.heap_next

    def sized(self, n):
        a = self.cpu.alloc(max(n, 4))
        self.sizes[a] = n
        return a

    def realloc(self, c):
        old, size = c.arg(0), c.arg(1)
        new = self.sized(size)
        if old:
            self.cpu.write(new, self.cpu.read(old, min(self.sizes.get(old, 0), size)))
        return new

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def p32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def tree(self, head, nil):
        out = []

        def walk(n):
            if n == nil or n == head:
                return
            walk(self.p32(n))
            out.append(n)
            walk(self.p32(n + 8))
        walk(self.p32(head + 4))
        return out

    def intake_end(self, uc, address, size, user):
        if self.whole:  # the whole stripper: on past the intake
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        st = [self.i32(COUNT_ALL), self.i32(COUNT_TAKEN)]
        nodes = self.tree(self.p32(esp + 0x54), self.p32(EDGE_NIL))
        st.append(len(nodes))
        for n in nodes:
            e = self.p32(n + 0x14)
            assert (self.i32(e), self.i32(e + 4)) == (self.i32(n + 0xc), self.i32(n + 0x10))
            head, uses = self.p32(e + 0xc), []
            k = self.p32(head)
            while k != head:
                uses += [self.i32(k + 8), self.cpu.read(k + 0xc, 1)[0]]
                k = self.p32(k)
            assert len(uses) == 2 * self.i32(e + 0x10)
            st += [self.i32(e), self.i32(e + 4), len(uses) // 2] + uses
        nodes = self.tree(self.p32(esp + 0x70), self.p32(VERTEX_NIL))
        st.append(len(nodes))
        for n in nodes:
            v = self.p32(n + 0x10)
            head, polys = self.p32(v + 8), []
            k = self.p32(head)
            while k != head:
                polys.append(self.i32(k + 8))
                k = self.p32(k)
            st += [self.i32(v), len(polys)] + polys
        self.state = st
        uc.emu_stop()

    def build(self, spec):
        """The model of `spec` (probe_pm_strip's form) in the exe's memory; returns it and its materials."""
        cpu, it = self.cpu, iter(spec)
        nxt = lambda: next(it)
        mats = []
        for _ in range(nxt()):
            m = self.sized(0x2c)
            cpu.write(m, bytes(0x2c))
            alpha, flags, has_texture, has_image, image_flags = (nxt() for _ in range(5))
            cpu.write(m, bytes([alpha & 0xff, flags & 0xff]))
            if has_texture:
                e = self.sized(0x2c)
                cpu.write(e, bytes(0x2c))
                if has_image:
                    im = self.sized(0x38)
                    cpu.write(im, bytes(0x38))
                    cpu.write(im + 9, bytes([image_flags & 0xff]))
                    cpu.u32(e, im)
                cpu.u32(m + 0x10, e)
            mats.append(m)
        arr = lambda vals: (lambda a: (cpu.write(a, struct.pack(f'<{len(vals)}i', *vals)) if vals else None, a)[1])(
            self.sized(4 * len(vals)))
        np = nxt()
        model = self.sized(0x68)
        cpu.write(model, bytes(0x68))
        cpu.u32(model + 0x10, np)
        polys = self.sized(0x28 * max(np, 1))
        cpu.u32(model + 0x34, polys)
        for i in range(np):
            q = polys + 0x28 * i
            flags, prio = nxt(), nxt()
            n = flags & 0x3ff
            rec = [flags, prio, arr([nxt() for _ in range(n)]), 0, 0, 0, 0, 0, 0, 0]
            if flags & 0x1000:
                rec[3] = arr([nxt() for _ in range(n)])
            mc = nxt()
            rec[4] = mc
            rec[5] = arr([mats[nxt()] for _ in range(mc)])
            if nxt():
                rec[6] = arr([arr([nxt() for _ in range(2 * n)]) for _ in range(mc)])
            rec[7] = arr([nxt() for _ in range(3 * n)])
            rec[8] = self.sized(4 * n) if nxt() else 0  # +0x20, one word a corner, cleared
            rec[9] = nxt()
            cpu.write(q, struct.pack('<10I', *[v & 0xffffffff for v in rec]))
        return model, mats

    def dump(self, model, mats):
        ids = {m: k for k, m in enumerate(mats)}
        np, polys = self.i32(model + 0x10), self.p32(model + 0x34)
        out = [np]
        for i in range(np):
            q = polys + 0x28 * i
            flags, prio, verts, norms, mc, marr, uvs, cols, _, zone = struct.unpack('<IiIIiIIIII', self.cpu.read(q, 0x28))
            n = flags & 0x3ff
            out += [struct.unpack('<i', struct.pack('<I', flags))[0], prio, mc, 1 if uvs else 0,
                    struct.unpack('<i', struct.pack('<I', zone))[0]]
            out += [self.i32(verts + 4 * k) for k in range(n)]
            if flags & 0x1000:
                out += [self.i32(norms + 4 * k) for k in range(n)]
            for j in range(mc):
                out.append(ids.get(self.p32(marr + 4 * j), -1))
                if uvs:
                    u = self.p32(uvs + 4 * j)
                    out += [self.i32(u + 4 * k) for k in range(2 * n)]
            out += [self.i32(cols + 4 * k) for k in range(3 * n)]
        return out

    def run(self, spec, mode, max_points, join=None):
        cpu = self.cpu
        cpu.heap_next, self.sizes, self.state = self.heap, {}, None
        for k in range(7):  # 0x8c1504..0x8c151c: the stripper's counts
            cpu.u32(COUNT_ALL + 4 * k, 0)
        for nil in (EDGE_NIL, VERTEX_NIL):  # the trees' shared nil nodes: made again on this run's heap
            cpu.u32(nil, 0), cpu.u32(nil + 4, 0)
        model, mats = self.build(spec)
        if 2 <= mode <= 3:
            use_a, use_b, key, outs = self.sized(8), self.sized(8), self.sized(8), self.sized(16)
            cpu.write(use_a, struct.pack('<iI', join[0], join[1]))
            cpu.write(use_b, struct.pack('<iI', join[2], join[3]))
            cpu.write(key, struct.pack('<ii', join[4], join[5]))
            cpu.write(outs, struct.pack('<4i', 0x5555, 0x5555, 0x5555, 0x5555))
            cpu.u32(MODEL, model), cpu.u32(ARG, join[6]), cpu.u32(PASS, join[7]), cpu.u32(SLACK, 0x7777)
            if mode == 2:
                cpu.call(TRY_JOIN, ecx=use_a, stack_args=[use_b, key, outs, outs + 4, outs + 8])
                first, second, strip, end = struct.unpack('<4i', cpu.read(outs, 16))
            else:
                cpu.call(JOIN_SHAPE, ecx=use_a, stack_args=[use_b, key, join[8], join[9], join[10], outs + 8, outs + 12])
                first, second = join[8], join[9]
                strip, end = struct.unpack('<2i', cpu.read(outs + 8, 8))
            ok = cpu._uc.reg_read(UC_X86_REG_EAX) & 0xff
            answer = [ok, first, second, strip, self.i32(SLACK), end if mode == 3 and ok and strip == 1 else 0]
            return answer + self.dump(model, mats)
        if mode == 0:
            cpu.call(MERGE, stack_args=[model])
            return self.dump(model, mats)
        if mode == 4:
            self.whole = True
            try:
                cpu.call(STRIP, stack_args=[model, join[0], max_points])
            finally:
                self.whole = False
            counts = [self.i32(COUNT_ALL + 4 * k) for k in range(7)]
            ids = {m: k for k, m in enumerate(mats)}
            polys, refs = self.p32(model + 0x34), []
            for k in range(self.i32(model + 0x5c)):
                mat, use, poly = struct.unpack('<IiI', cpu.read(self.p32(model + 0x60) + 12 * k, 12))
                refs += [ids.get(mat, -1), use, (poly - polys) // 0x28 if poly else -1]
            return counts + [self.i32(model + 8)] + self.dump(model, mats) + [self.i32(model + 0x5c)] + refs
        esp = original.STACK_TOP - 0x1000
        for v in reversed([original.RETURN, model, 0x7fffffff, max_points]):
            esp -= 4
            cpu.u32(esp, v)
        uc = cpu._uc
        uc.reg_write(UC_X86_REG_ESP, esp), uc.reg_write(UC_X86_REG_ECX, 0), uc.reg_write(UC_X86_REG_EDX, 0)
        uc.emu_start(STRIP, original.RETURN, count=50_000_000)
        dumped = self.dump(model, mats)
        if self.state is None:  # refused (the exe returned before its maps)
            return dumped
        return dumped + self.state


class Spec:
    """A model as probe_pm_strip reads it."""

    def __init__(self):
        self.mats, self.polys = [], []

    def ints(self):
        out = [len(self.mats)]
        for m in self.mats:
            out += m
        out.append(len(self.polys))
        for p in self.polys:
            out += p
        return out


def random_spec(rng):
    s = Spec()
    for _ in range(rng.randint(1, 7)):
        textured = rng.random() < 0.8
        flags = (1 if textured else 0) | rng.choice((0, 0, 4, 0x10))
        has_image = 1 if rng.random() < 0.85 else 0
        s.mats.append([rng.choice((0xff, 0xff, 0x80, 0)), flags, 1 if textured or rng.random() < 0.5 else 0,
                       has_image, rng.choice((0, 0, 2, 4, 6))])
    pool = rng.randint(4, 14)
    loops = []
    for _ in range(rng.randint(1, 14)):
        if loops and rng.random() < 0.45:  # a layer: a loop already made, turned
            verts, flags = rng.choice(loops)
            t = rng.randrange(len(verts))
            verts = verts[t:] + verts[:t]
            if rng.random() < 0.1:
                flags ^= 0x400
            if rng.random() < 0.08:
                verts = verts[:]
                verts[rng.randrange(len(verts))] = rng.randrange(pool)
        else:
            strip = rng.random() < 0.3
            n = rng.choice((3, 3, 4, 4, 5, 6, 8)) if strip else rng.choice((2, 3, 3, 4, 4, 5, 6, 9))
            verts = [rng.randrange(pool) for _ in range(n)]
            flags = n | (0x2000 if strip else 0) | (0x400 if rng.random() < 0.3 else 0)
            flags |= 0x1000 if rng.random() < 0.85 else 0
            flags |= 0x800 if rng.random() < 0.2 else 0
            loops.append((verts, flags))
        n = len(verts)
        mc = rng.choice((1, 1, 1, 2, 2, 3, 4, 5))
        mids = [rng.randrange(len(s.mats)) for _ in range(mc)]
        all_textured = all(s.mats[m][1] & 1 for m in mids)
        uvs = 1 if all_textured or rng.random() < 0.7 else 0
        p = [flags, rng.randint(-3, 3)] + verts
        if flags & 0x1000:
            p += [rng.randrange(pool) for _ in range(n)]
        p.append(mc)
        for m in mids:
            p.append(m)
        p.append(uvs)
        if uvs:
            for m in mids:
                p += [bits(rng.uniform(-2, 2)) for _ in range(2 * n)]
        p += [bits(rng.random()) for _ in range(3 * n)]
        p += [1 if rng.random() < 0.5 else 0, rng.getrandbits(31)]
        s.polys.append(p)
    rng.shuffle(s.polys)
    return s


def join_spec(rng):
    """Two polygons a and b on a shared edge (b's points run it the other way, mostly), the rest of the model at
    random; their corners alike up to whole texture repeats more often than not. Returns the spec and the join."""
    s = Spec()
    for _ in range(rng.randint(1, 4)):
        s.mats.append([0xff, 1 | rng.choice((0, 4)), 1, 1, rng.choice((0,) * 6 + (1, 2, 3))])
    pool = rng.randint(6, 12)

    def poly(n, strip, verts=None):
        verts = verts or [rng.randrange(pool) for _ in range(n)]
        return {'flags': len(verts) | (0x2000 if strip else 0) | 0x1000 | (0x400 if rng.random() < 0.05 else 0) |
                (0x800 if rng.random() < 0.1 else 0),
                'prio': rng.choice((0, 0, 0, 1)), 'verts': verts,
                'norms': [rng.randrange(4) for _ in verts], 'mids': None,
                'uvs': None, 'cols': [[rng.choice((0.0, 100.0, 255.0)) for _ in range(3)] for _ in verts],
                'zone': rng.choice((0, 0, 0, 0x01000000))}
    a = poly(rng.choice((3, 3, 4, 4, 5, 6)), rng.random() < 0.4)
    na = len(a['verts'])
    if a['flags'] & 0x2000 and rng.random() < 0.7:  # a strip end: its first, last or (a quad's) a diagonal
        pairs = [(0, 1), (na - 2, na - 1)] + ([(0, 2), (1, 3)] if na == 4 else [])
        i, j = rng.choice(pairs)
    else:
        i = rng.randrange(na)
        j = (i + 1) % na
    x, y = a['verts'][i], a['verts'][j]
    if x == y:
        a['verts'][j] = y = (x + 1) % pool
    nb = rng.choice((3, 3, 4, 4, 5, 6))
    b_strip = rng.random() < 0.4
    rest = [rng.randrange(pool) for _ in range(nb - 2)]
    if b_strip and rng.random() < 0.6:
        at = rng.choice((0, nb - 2))
        if nb == 4 and rng.random() < 0.3:  # across b's diagonal
            vb = rest[:]
            vb.insert(0, y), vb.insert(2, x)
        else:
            vb = rest[:at] + ([y, x] if rng.random() < 0.8 else [x, y]) + rest[at:]
    else:
        at = rng.randrange(nb - 1)
        vb = rest[:at] + ([y, x] if rng.random() < 0.8 else [x, y]) + rest[at:]
    b = poly(nb, b_strip, vb)
    if rng.random() < 0.9:
        b['flags'] = (b['flags'] & ~0xc00) | (a['flags'] & 0xc00)
        b['prio'], b['zone'] = a['prio'], a['zone']
    mc = rng.randint(1, len(s.mats))
    a['mids'] = rng.sample(range(len(s.mats)), mc)
    b['mids'] = a['mids'][:] if rng.random() < 0.93 else [rng.randrange(len(s.mats)) for _ in range(mc)]
    a['uvs'] = [[round(rng.uniform(-3, 3), rng.choice((1, 2, 5))) for _ in range(2 * na)] for _ in range(mc)]
    b['uvs'] = [[round(rng.uniform(-3, 3), 2) for _ in range(2 * nb)] for _ in range(len(b['mids']))]
    for v in (x, y):  # the shared corners: a's, moved by whole repeats, now and then a little off
        if rng.random() < 0.85:
            ka, kb = a['verts'].index(v), b['verts'].index(v)
            b['norms'][kb] = a['norms'][ka] if rng.random() < 0.95 else 9
            b['cols'][kb] = [c + rng.choice((0,) * 12 + (1.5, 3)) for c in a['cols'][ka]]
            for jj in range(mc):
                for q in range(2):
                    off = rng.choice((0, 0, 1, -1, 2, -3)) + rng.choice((0,) * 24 + (0.04, 0.06, -0.02, 0.5))
                    b['uvs'][jj][2 * kb + q] = a['uvs'][jj][2 * ka + q] + off
    others = [poly(rng.choice((3, 4)), False) for _ in range(rng.randint(0, 3))]
    for o in others:
        o['mids'] = [rng.randrange(len(s.mats))]
        o['uvs'] = [[rng.uniform(-1, 1) for _ in range(2 * len(o['verts']))]]
    order = [a, b] + others
    rng.shuffle(order)
    for q in order:
        n = len(q['verts'])
        rec = [q['flags'], q['prio']] + q['verts'] + q['norms'] + [len(q['mids'])] + q['mids'] + [1]
        for uv in q['uvs']:
            rec += [bits(f) for f in uv]
        rec += [bits(c) for col in q['cols'] for c in col] + [0, q['zone']]
        s.polys.append(rec)
    ia, ib = order.index(a), order.index(b)
    key = (min(x, y), max(x, y))
    ra = rng.choice((0, 1)) if rng.random() < 0.2 else int(y < x)
    rb = 1 - ra if rng.random() < 0.85 else ra
    return s, [ia, ra, ib, rb, key[0], key[1], rng.choice((0x7fffffff, 0x7fffffff, 8, 6)), rng.choice((0, 1, 2, 2))]


def mesh_spec(rng):
    """A patch of ground as a grid: triangles (either diagonal) and quads sharing corners, their texture
    coordinates from the grid (now and then moved by whole repeats), a row now and then already a strip; a few
    polygons on another material, turned, or off by a little. Made to join."""
    s = Spec()
    for _ in range(rng.randint(1, 3)):
        s.mats.append([0xff, 1 | rng.choice((0, 4)), 1, 1, rng.choice((0,) * 5 + (1, 2))])
    tiny = rng.random() < 0.15  # one odd strip and the triangle beside its last edge, nothing else
    w, h = (rng.randint(3, 5), 2) if tiny else (rng.randint(2, 6), rng.randint(2, 5))
    su, sv = rng.choice((0.25, 0.5, 1.0)), rng.choice((0.25, 0.5, 1.0))
    cols = [[rng.choice((0.0, 128.0, 255.0)) for _ in range(3)] for _ in range(w * h)]
    norms = [rng.randrange(3) if rng.random() < 0.3 else i for i in range(w * h)]
    mats = len(s.mats)

    def add(verts, strip=False):
        if rng.random() < 0.05:
            verts = verts[::-1]
        n = len(verts)
        mid = 0 if rng.random() < 0.85 else rng.randrange(mats)
        shift = (rng.choice((0, 0, 0, 1, -1)), rng.choice((0, 0, 0, 2)))
        uv = []
        for v in verts:
            x, y = v % w, v // w
            uv += [x * su + shift[0], y * sv + shift[1]]
        if rng.random() < 0.04:
            uv[0] += 0.3
        flags = n | 0x1000 | (0x2000 if strip else 0) | (0x800 if rng.random() < 0.03 else 0)
        rec = [flags, 0] + verts + [norms[v] for v in verts] + [1, mid, 1] + [bits(f) for f in uv]
        rec += [bits(c) for v in verts for c in cols[v]] + [1, 0]
        s.polys.append(rec)

    def cell(x, y):
        a, b, c, d = y * w + x, y * w + x + 1, (y + 1) * w + x + 1, (y + 1) * w + x
        r = rng.random()
        if r < 0.25:
            add([a, b, c, d])
        elif r < 0.6:
            add([a, b, c]), add([a, c, d])
        else:
            add([a, b, d]), add([b, c, d])

    for y in range(h - 1):
        covered = set()
        if tiny or (rng.random() < 0.3 and w >= 2):  # part of the row as a strip already, either way, either row
            top, bottom = (y * w, (y + 1) * w) if rng.random() < 0.5 else ((y + 1) * w, y * w)
            k = (w - rng.randint(0, 1)) if tiny else rng.randint(2, w)
            x0 = rng.randint(0, w - k)
            xs = list(range(x0, x0 + k)) if rng.random() < 0.5 else list(range(x0 + k - 1, x0 - 1, -1))
            run = [v for x in xs for v in (top + x, bottom + x)]
            beside = None
            if tiny and xs[0] < xs[-1] and xs[-1] < w - 1 and rng.random() < 0.5:
                # an odd count from its second point, ending on an upright edge a quad shares, run the other way
                run = run[1:]
                beside = [bottom + xs[-1], top + xs[-1], top + xs[-1] + 1, bottom + xs[-1] + 1]
            elif len(run) > 3 and (tiny or rng.random() < 0.5):  # an odd count: the last cell's other half plain
                run = run[:-1]
                add([top + xs[-1], bottom + xs[-2], bottom + xs[-1]])  # the shared edge run the other way
            add(run, strip=True)
            if beside:
                add(beside)
            covered = set(range(w)) if tiny else set(range(min(xs), max(xs)))
        for x in range(w - 1):
            if x not in covered:
                cell(x, y)
    for q in list(s.polys):  # now and then a polygon twice: an edge three or four polygons share
        if rng.random() < 0.06:
            s.polys.append(q[:])
    rng.shuffle(s.polys)
    return s


def shipped_specs(rng, every, biggest, mechlib=False):
    """A seeded sample of the worlds' type-0 models with polygons as specs (with `mechlib`, mechlib's too), images
    made up at random; every one with `every` 1."""
    import test_pm_gamez as g
    import test_pm_strips as strips
    benches = []
    for w in g.WORLDS:
        bench = g.Emulated()
        bench.stack([g.ZBD / w / 'gamez.zbd'], 5, 6, 5, models=True)
        benches.append((w, bench))
    if mechlib:
        import test_pm_mechlib as lib
        bench = lib.Emulated()
        data = lib.MECHLIB.read_bytes()
        bench.native_stub(0x5329e0, lambda c: 0)
        bench.run(None, lib.MECHLIB, [n for n, _, _ in lib.entries(data) if n not in ('version', 'format', 'materials')])
        benches.append(('mechlib', bench))
    for w, bench in benches:
        typed0 = [p for p in strips.model_slots(bench) if bench.i32(p + 0x10) > 0 and bench.i32(p) == 0]
        pick = sorted(set(typed0[rng.randrange(every)::every]) |
                      set(sorted(typed0, key=lambda p: -bench.i32(p + 0x10))[:biggest]))
        for slot in pick:
            yield w, slot, spec_of(bench, slot, rng)


def spec_of(bench, slot, rng):
    s, mid = Spec(), {}
    u32 = lambda a: bench.i32(a) & 0xffffffff
    for i in range(bench.i32(slot + 0x10)):
        q = u32(slot + 0x34) + 0x28 * i
        flags, prio, verts, norms, mc, marr, uvs, cols, scratch, zone = struct.unpack('<IiIIiIIIII', bench.read(q, 0x28))
        n = flags & 0x3ff
        p = [struct.unpack('<i', struct.pack('<I', flags))[0], prio]
        p += list(struct.unpack(f'<{n}i', bench.read(verts, 4 * n))) if n else []
        if flags & 0x1000:
            p += list(struct.unpack(f'<{n}i', bench.read(norms, 4 * n))) if n else []
        p.append(mc)
        for j in range(mc):
            m = u32(marr + 4 * j)
            if m not in mid:
                mid[m] = len(s.mats)
                mat = bench.read(m, 0x14)
                textured = mat[1] & 1
                s.mats.append([mat[0], mat[1], 1 if textured else 0, 1 if rng.random() < 0.9 else 0,
                               rng.choice((0, 0, 2))])
            p.append(mid[m])
        p.append(1 if uvs else 0)
        if uvs:
            for j in range(mc):
                p += list(struct.unpack(f'<{2 * n}i', bench.read(u32(uvs + 4 * j), 8 * n))) if n else []
        p += list(struct.unpack(f'<{3 * n}i', bench.read(cols, 12 * n))) if n else []
        p += [1 if scratch else 0, struct.unpack('<i', struct.pack('<I', zone))[0]]
        s.polys.append(p)
    return s


def main():
    p = test_pm_zar.load_pm_probe()
    I = ctypes.POINTER(ctypes.c_int32)
    p.probe_pm_strip.argtypes = [I, ctypes.c_int, ctypes.c_int, I, I]
    p.probe_pm_set_fpu53(1)
    emu = Emulated()
    emu.cpu.fpu_control(0x027f)  # the reference runtime's 53 bits (CLAUDE.md), on both sides
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    out = (ctypes.c_int32 * (1 << 22))()
    stats = dict.fromkeys(('runs', 'merged', 'shipped merged', 'taken', 'edges', 'uses', 'points', 'shipped',
                           'joins', 'shapes', 'strips', 'stripped', 'strips left', 'polygons before',
                           'polygons after'), 0)

    def check(spec, mode, max_points, what, join=(0,) * 11):
        ints = spec.ints()
        want = emu.run(ints, mode, max_points, join)
        k = p.probe_pm_strip((ctypes.c_int32 * len(ints))(*ints), mode, max_points, (ctypes.c_int32 * 11)(*join), out)
        got = list(out[:k])
        if want != got:
            at = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), min(len(want), len(got)))
            print(f'{what} mode {mode} max {max_points}: differ at {at} (lengths {len(want)} / {len(got)})\n'
                  f'  exe  {want[max(0, at - 8):at + 12]}\n  port {got[max(0, at - 8):at + 12]}')
            raise SystemExit(1)
        stats['runs'] += 1
        if mode == 4:
            stats['stripped'] += 1
            stats['strips left'] += want[2]
            stats['polygons before'] += len(spec.polys)
            stats['polygons after'] += want[8]
        elif mode >= 2:
            stats['joins' if mode == 2 else 'shapes'] += want[0]
            stats['strips'] += want[0] and want[3] == 1
        elif mode == 0:
            stats['shipped merged' if 'model' in what else 'merged'] += len(spec.polys) - want[0]
        elif emu.state:
            st = emu.state
            stats['taken'] += st[1]
            at, edges = 3, st[2]
            for _ in range(edges):
                stats['uses'] += st[at + 2]
                at += 3 + 2 * st[at + 2]
            stats['edges'] += edges
            stats['points'] += st[at]

    rng = random.Random(0x572df0)
    for run in range(runs):
        spec = random_spec(rng)
        check(spec, 0, 0, f'run {run}')
        check(spec, 1, rng.choice((0x7fffffff, 4, 3)), f'run {run}')
    rng = random.Random(0x573f50)
    for run in range(runs * 3):
        spec, join = join_spec(rng)
        check(spec, 2, 0, f'join {run}', join)
        shape = join + [rng.choice((join[4], join[5], -1, 7)), rng.choice((join[5], join[4], -1)), rng.choice((0, 1))]
        check(spec, 3, 0, f'shape {run}', shape)
    rng = random.Random(0x56d260)
    for run in range(runs):
        spec = mesh_spec(rng)
        check(spec, 4, rng.choice((0x7fffffff, 4)), f'mesh {run}', [rng.choice((0x7fffffff, 0x7fffffff, 40, 12, 6))] + [0] * 10)
    rng = random.Random(0x570b70)
    every_one = os.environ.get('STRIP_ALL') == '1'  # the whole shipped set, mechlib's too (sitting 3; slow)
    for w, slot, spec in shipped_specs(rng, every=1 if every_one else int(os.environ.get('STRIP_EVERY', '40')),
                                       biggest=0 if every_one else 3, mechlib=every_one):
        check(spec, 0, 0, f'{w} model {slot:#x}')
        check(spec, 1, 0x7fffffff, f'{w} model {slot:#x}')
        for max_points in (4, 0x7fffffff):  # the crater re-read's, and the other callers'
            check(spec, 4, max_points, f'{w} model {slot:#x}', [0x7fffffff] + [0] * 10)
        stats['shipped'] += 1
        if every_one and stats['shipped'] % 500 == 0:
            print(f'{stats["shipped"]} shipped models identical so far', flush=True)
    print(f'pm strip: identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
