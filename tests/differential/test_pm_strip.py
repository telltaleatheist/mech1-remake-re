"""Pirate's Moon's triangle stripper (pm::zmod, gmod_strip; docs/pm-tests.md, PM's triangle stripper) against PM's
exe, a family at a time.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_strip.py [runs]

The layer merge 0x572df0 alone, then the stripper 0x570b70 stopped where its intake ends (0x570ef7) with its edge
and point maps read out of its frame (MSVC's trees walked in order, each list from its head). Models are made at
random to meet each rule (the same loops turned, layers past 8, solid and see-through images, strips of 4 and
more), then a seeded sample of the shipped worlds' type-0 models (every Nth plus the biggest), their materials
given images at random so that layers merge. The images' load 0x5329e0, malloc / realloc / free and operator new /
delete stand in on the exe's side; MSVCP's _Lockit does nothing.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EDX

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
import original  # noqa: E402
from original import Original  # noqa: E402

MERGE, STRIP, INTAKE_END = 0x572df0, 0x570b70, 0x570ef7
LOAD_IMAGES, REALLOC, NEW, DELETE = 0x5329e0, 0x596520, 0x5ce8c6, 0x5ce8c0
IAT_FREE, IAT_MALLOC, IAT_LOCKIT, IAT_UNLOCKIT = 0x5de65c, 0x5de664, 0x5de4f8, 0x5de4fc
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
        self.state = None
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
            rec[8] = self.sized(4) if nxt() else 0
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

    def run(self, spec, mode, max_points):
        cpu = self.cpu
        cpu.heap_next, self.sizes, self.state = self.heap, {}, None
        cpu.u32(COUNT_ALL, 0), cpu.u32(COUNT_TAKEN, 0)
        for nil in (EDGE_NIL, VERTEX_NIL):  # the trees' shared nil nodes: made again on this run's heap
            cpu.u32(nil, 0), cpu.u32(nil + 4, 0)
        model, mats = self.build(spec)
        if mode == 0:
            cpu.call(MERGE, stack_args=[model])
            return self.dump(model, mats)
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


def shipped_specs(rng, every, biggest):
    """A seeded sample of the worlds' type-0 models with polygons as specs, images made up at random."""
    import test_pm_gamez as g
    import test_pm_strips as strips
    bench = g.Emulated()
    out = []
    for w in g.WORLDS:
        bench.stack([g.ZBD / w / 'gamez.zbd'], 5, 6, 5, models=True)
        typed0 = [p for p in strips.model_slots(bench) if bench.i32(p + 0x10) > 0 and bench.i32(p) == 0]
        pick = sorted(set(typed0[rng.randrange(every)::every]) |
                      set(sorted(typed0, key=lambda p: -bench.i32(p + 0x10))[:biggest]))
        for slot in pick:
            out.append((w, slot, spec_of(bench, slot, rng)))
    return out


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
    p.probe_pm_strip.argtypes = [I, ctypes.c_int, ctypes.c_int, I]
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    out = (ctypes.c_int32 * (1 << 22))()
    stats = dict.fromkeys(('runs', 'merged', 'shipped merged', 'taken', 'edges', 'uses', 'points', 'shipped'), 0)

    def check(spec, mode, max_points, what):
        ints = spec.ints()
        want = emu.run(ints, mode, max_points)
        k = p.probe_pm_strip((ctypes.c_int32 * len(ints))(*ints), mode, max_points, out)
        got = list(out[:k])
        if want != got:
            at = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), min(len(want), len(got)))
            print(f'{what} mode {mode} max {max_points}: differ at {at} (lengths {len(want)} / {len(got)})\n'
                  f'  exe  {want[max(0, at - 8):at + 12]}\n  port {got[max(0, at - 8):at + 12]}')
            raise SystemExit(1)
        stats['runs'] += 1
        if mode == 0:
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
    rng = random.Random(0x570b70)
    for w, slot, spec in shipped_specs(rng, every=int(os.environ.get('STRIP_EVERY', '40')), biggest=3):
        check(spec, 0, 0, f'{w} model {slot:#x}')
        check(spec, 1, 0x7fffffff, f'{w} model {slot:#x}')
        stats['shipped'] += 1
    print(f'pm strip: identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
