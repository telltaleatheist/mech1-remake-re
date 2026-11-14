"""Pirate's Moon's own unused zModel functions (docs/render.md, The unused zModel helpers; pm-tests.md, S19;
pm gmod_unused_pm) against Mech3.exe PM 1.0, at the x87's 24 bits:
- 0x53ee40 a turn from two vectors (dot, cross);
- 0x5415b0 what a model needs drawn with: its sphere through the top of the matrix stack or not, the fog (0x576ff0,
  stood in by a few bytes of x86 that keep its arguments and load the answer) and light (0x55ebf0, stood in) queries
  asked or not, the bits;
- 0x543010 four points through the top into the arena (0x91cbc0);
- 0x5432b0 a node's box as six faces into the arena (the real push, model-view and pop; 0x542a70 stood in, popping
  when it says 0 as the real one does, else setting the top to the view): the arena, the planes put back, the stack's depth; the exe's second pop
  after a 0 is counted, not compared (FIX (bug));
- 0x55bc30 every polygon on one material (0x596520, PM's realloc, stood in).

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_zmodel_unused_own.py
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
os.environ.setdefault('MW3_PM_MOVED', '1')
from original import Original  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

F, I = ctypes.c_float, ctypes.c_int32
TOP, IDENT, SLOTS, FLAGS = 0x611220, 0x61121c, 0x66f4d0, 0x66f558
VIEW, ARENA, PLANES, HARDWARE = 0x8d43c0, 0x91cbc0, 0x8d4b08, 0x8c4b70
FOG_ON, MASK_PTR, ANY, TINT = 0xa4e038, 0xa4e0d8, 0xa4d014, 0xa4e07c
REALLOC, FREE = 0x596520, 0x5de65c
f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]
fl = lambda *v: struct.pack(f'<{len(v)}f', *v)
bits = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]


def rmat(r):
    return [f32(r.uniform(-1.5, 1.5)) for _ in range(9)] + [f32(r.uniform(-300, 300)) for _ in range(3)]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.log = ''
        self.sizes = {}
        self.fog_log, self.fog_ans = cpu.alloc(0x20), cpu.alloc(4)
        # 0x576ff0: keep the point's three words and the radius, load the answer
        code = (b'\x8b\x44\x24\x04'  # mov eax, [esp+4]
                b'\x8b\x08\x89\x0d' + struct.pack('<I', self.fog_log) +       # mov ecx,[eax]; mov [log],ecx
                b'\x8b\x48\x04\x89\x0d' + struct.pack('<I', self.fog_log + 4) +
                b'\x8b\x48\x08\x89\x0d' + struct.pack('<I', self.fog_log + 8) +
                b'\x8b\x4c\x24\x08\x89\x0d' + struct.pack('<I', self.fog_log + 12) +
                b'\xff\x05' + struct.pack('<I', self.fog_log + 16) +            # inc dword [log+16]
                b'\xd9\x05' + struct.pack('<I', self.fog_ans) + b'\xc3')       # fld [ans]; ret
        cpu.write(0x576ff0, code)
        cpu.stub_function(0x55ebf0, self.lights)
        cpu.stub_function(0x542a70, self.prepare)
        cpu.stub_function(REALLOC, self.realloc)
        cpu.stub_import(FREE, lambda c: 0)
        self.mask = cpu.alloc(4)
        self.ident = cpu.alloc(4)

    def lights(self, c):
        p = c.arg(0)
        w = struct.unpack('<3I', self.cpu.read(p, 12))
        self.log += 'L %08x %08x %08x %08x;' % (*w, c.arg(1))
        return self.light_ans & 0xffffffff

    def prepare(self, c):
        self.log += 'P;'
        cpu = self.cpu
        if not self.prepare_ans:  # the real one pops when it says 0
            for a in (TOP, IDENT):
                cpu.u32(a, struct.unpack('<I', cpu.read(a, 4))[0] - 4)
        else:  # it sets the top: the view here, as the port's stand-in
            slot = struct.unpack('<I', cpu.read(TOP, 4))[0]
            cpu.write(struct.unpack('<I', cpu.read(slot, 4))[0], cpu.read(VIEW, 48))
            cpu.u32(struct.unpack('<I', cpu.read(IDENT, 4))[0], 0)
        return self.prepare_ans

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.cpu.alloc(max(1, n))
        if old:
            self.cpu.write(a, self.cpu.read(old, min(n, self.sizes.get(old, n))))
        self.sizes[a] = n
        return a

    def eax(self):
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def top(self, m12):
        """One matrix on the stack: slot 0, identity or not."""
        cpu = self.cpu
        mat = cpu.alloc(0x30)
        cpu.write(mat, fl(*(m12 or [0.0] * 12)))
        cpu.u32(SLOTS, mat)
        cpu.u32(FLAGS, 0 if m12 else 1)
        cpu.u32(TOP, SLOTS)
        cpu.u32(IDENT, FLAGS)


def main():
    lib = load_pm_probe()
    lib.probe_pmzu_needs.restype = ctypes.c_uint32
    exe = Exe()
    cpu = exe.cpu
    r = random.Random(77)
    bad = {}
    seen = {}

    def tally(k):
        seen[k] = seen.get(k, 0) + 1

    def fail(what, msg):
        bad[what] = bad.get(what, 0) + 1
        if bad[what] <= 3:
            print(f'{what}: {msg}')

    buf = cpu.alloc(0x400)
    for run in range(2000):
        # 0x53ee40
        a = [f32(r.uniform(-10, 10)) for _ in range(3)]
        b = [f32(r.uniform(-10, 10)) for _ in range(3)]
        cpu.write(buf, fl(*a, *b))
        cpu.call(0x53ee40, stack_args=(buf, buf + 12, buf + 24))
        want = cpu.read(buf + 24, 16)
        out = (F * 4)()
        lib.probe_pmzu_turn((F * 3)(*a), (F * 3)(*b), out)
        if want != bytes(out):
            fail('turn', f'run {run}: exe {want.hex()} port {bytes(out).hex()}')
        tally('turn')
        # 0x5415b0
        m12 = rmat(r) if r.random() < 0.7 else None
        hdr = bytearray(0x64)
        struct.pack_into('<I', hdr, 8, r.randrange(4))
        struct.pack_into('<4f', hdr, 0x48, *[f32(r.uniform(-100, 100)) for _ in range(3)], f32(r.uniform(0, 30)))
        s = [r.choice([0, 1, 2]), r.choice([0, 1, 3]), r.choice([0, 1]), r.choice([0, 1]),
             bits(f32(r.choice([0.0, 0.002, 0.0039, 0.004, 0.5]))), r.choice([0, 1, 3])]
        exe.top(m12)
        model = cpu.alloc(0x68)
        cpu.write(model, bytes(hdr))
        cpu.u32(FOG_ON, s[0])
        cpu.u32(exe.mask, s[1])
        cpu.u32(MASK_PTR, exe.mask)
        cpu.u32(ANY, s[2])
        cpu.u32(TINT, s[3])
        cpu.u32(exe.fog_ans, s[4])
        cpu.write(exe.fog_log, bytes(0x20))
        exe.log, exe.light_ans = '', s[5]
        cpu.call(0x5415b0, stack_args=(model,))
        want_r = exe.eax()
        fw = struct.unpack('<5I', cpu.read(exe.fog_log, 20))
        want_log = ('F %08x %08x %08x %08x;' % fw[:4] if fw[4] else '') + exe.log
        sb = ctypes.create_string_buffer(512)
        got_r = lib.probe_pmzu_needs((F * 12)(*m12) if m12 else None, bytes(hdr), (I * 6)(*s), sb, len(sb))
        if (want_r, want_log) != (got_r, sb.value.decode()):
            fail('needs', f'run {run} s {s}: exe {want_r} {want_log} port {got_r} {sb.value.decode()}')
        tally(('needs', want_r))
        # 0x543010
        m12 = rmat(r) if r.random() < 0.7 else None
        exe.top(m12)
        pts = [f32(r.uniform(-50, 50)) for _ in range(12)]
        cpu.write(buf, fl(*pts))
        cpu.call(0x543010, stack_args=(buf, buf + 12, buf + 24, buf + 36))
        want = cpu.read(ARENA, 48)
        out = (F * 12)()
        lib.probe_pmzu_quad((F * 12)(*m12) if m12 else None, (F * 12)(*pts), out)
        if want != bytes(out):
            fail('quad', f'run {run}: exe {want.hex()} port {bytes(out).hex()}')
        tally('quad')
        # 0x5432b0
        parent, view = rmat(r), rmat(r)
        box = [f32(r.uniform(-20, 0)) for _ in range(3)] + [f32(r.uniform(0, 20)) for _ in range(3)]
        s = [r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([0, 1, 1]), r.choice([0, 0x3f, 0x13])]
        exe.top(parent)
        cpu.write(VIEW, fl(*view))
        cpu.u32(HARDWARE, s[0])
        node = cpu.alloc(0x100)
        cpu.write(node, bytes(0x100))
        boxp = cpu.alloc(24)
        cpu.write(boxp, fl(*box))
        cpu.u32(node + 0x70, boxp)
        cpu.u32(node + 0x3c, cpu.alloc(0x68) if s[1] else 0)
        cpu.u32(PLANES, s[3])
        cpu.write(ARENA, bytes(48))
        exe.log, exe.prepare_ans = '', s[2]
        cpu.call(0x5432b0, stack_args=(node,))
        depth = (struct.unpack('<I', cpu.read(TOP, 4))[0] - SLOTS) // 4
        want = (cpu.read(ARENA, 48), struct.unpack('<I', cpu.read(PLANES, 4))[0], depth, exe.log)
        out = (F * 14)()
        sb = ctypes.create_string_buffer(64)
        lib.probe_pmzu_box((F * 12)(*parent), (F * 12)(*view), (F * 6)(*box), (I * 4)(*s), out, sb, len(sb))
        got = (bytes(out)[:48], int(out[12]), int(out[13]), sb.value.decode())
        if s[0] and s[1] and not s[2] and want[2] == -1 and got[2] == 0 and want[0] == got[0] and want[3] == got[3]:
            tally('box: the exe pops twice (FIX (bug))')
        elif want != got:
            fail('box', f'run {run} s {s}: exe {want[1:]} port {got[1:]} arena same {want[0] == got[0]}')
        else:
            tally(('box', s[0], s[1], s[2]))
        # 0x55bc30
        n = r.randrange(1, 6)
        counts = [r.choice([1, 1, 2, 3]) for _ in range(n)]
        uvs = [r.choice([0, 1]) for _ in range(n)]
        refs = r.choice([1, 2, 4])
        mdl = cpu.alloc(0x68)
        cpu.write(mdl, bytes(0x68))
        polys = cpu.alloc(0x28 * n)
        cpu.write(polys, bytes(0x28 * n))
        for i in range(n):
            mats = cpu.alloc(4 * counts[i])
            exe.sizes[mats] = 4 * counts[i]
            cpu.write(mats, struct.pack(f'<{counts[i]}I', *[0x1000 + k for k in range(counts[i])]))
            cpu.u32(polys + 0x28 * i + 0x10, counts[i])
            cpu.u32(polys + 0x28 * i + 0x14, mats)
            if uvs[i]:
                u = cpu.alloc(4 * counts[i])
                exe.sizes[u] = 4 * counts[i]
                cpu.write(u, struct.pack(f'<{counts[i]}I', *[cpu.alloc(8) for _ in range(counts[i])]))
                cpu.u32(polys + 0x28 * i + 0x18, u)
        refp = cpu.alloc(0xc * refs)
        exe.sizes[refp] = 0xc * refs
        cpu.write(refp, bytes(0xc * refs))
        cpu.u32(mdl + 0x10, n)
        cpu.u32(mdl + 0x34, polys)
        cpu.u32(mdl + 0x5c, refs)
        cpu.u32(mdl + 0x60, refp)
        MAT = 0x7777
        cpu.call(0x55bc30, stack_args=(mdl, MAT))
        want = []
        for i in range(n):
            mats = struct.unpack('<I', cpu.read(polys + 0x28 * i + 0x14, 4))[0]
            want += [struct.unpack('<i', cpu.read(polys + 0x28 * i + 0x10, 4))[0],
                     99 if struct.unpack('<I', cpu.read(mats, 4))[0] == MAT else -1]
        rp = struct.unpack('<I', cpu.read(mdl + 0x60, 4))[0]
        rf = struct.unpack('<3I', cpu.read(rp, 12))
        want += [struct.unpack('<i', cpu.read(mdl + 0x5c, 4))[0]]
        want += [99 if rf[0] == MAT else -1, rf[1], int(rf[2] == polys)] if refs != 1 else [-1, 0, 0]
        out = (I * (2 * n + 4))()
        lib.probe_pmzu_one_material(n, (I * n)(*counts), (I * n)(*uvs), refs, out)
        got = list(out)
        if refs == 1:  # the list left as it was: the probe's calloc'd entry
            got[2 * n + 1:] = [-1, 0, 0]
        if want != got:
            fail('one material', f'run {run} counts {counts} refs {refs}: exe {want} port {got}')
        tally('one material')
    print(f'cases: {dict(sorted(seen.items(), key=str))}')
    print(f'differ: {bad if bad else "none"}')
    print('OK' if not bad else 'FAIL')
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
