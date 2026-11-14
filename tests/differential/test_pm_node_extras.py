"""Three of Pirate's Moon's own small pieces against PM's exe:
- the scripts' NodeSetOverrideAmbient / ...AmbientValue / ...Diffuse / ...DiffuseValue (0x4fcff0, 0x4fd080,
  0x4fd110, 0x4fd1a0; no shipped script uses them, ported unwired): random node flags and values, and no node;
- the LoD fade values (0x515c00, which the LoD copy 0x50d4e0 runs on every copy): random LoDs, the switches either
  side of 0.01, the one-for-all flag (+0x50), compared bit for bit at 24 bits;
- the texture library's file name (0x532150, every library's first open): random names, variants, r-prefixes and
  sets of files there, the r-name, the plain name of the same variant and none, and the shipped install's own files.

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_node_extras.py [cases]
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
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402
from twod_world import IAT  # noqa: E402

OVERRIDES = (0x4fcff0, 0x4fd080, 0x4fd110, 0x4fd1a0)
LOD_FADES, RESOLVE, EXISTS, LOG_ERROR = 0x515c00, 0x532150, 0x531eb0, 0x406cb0


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.reports = []
        self.files = set()
        cpu.stub_function(LOG_ERROR, lambda c: self.reports.extend((c.arg(0), c.arg(2))) or 0)
        cpu.stub_function(EXISTS, lambda c: int(self.cstr(c.arg(0)) in self.files), pop_bytes=4)  # ret 4
        cpu.stub_import(IAT['sprintf'], self.sprintf)

    def sprintf(self, c):  # %s and %d only, as the library name's formats
        fmt, out, k, i = self.cstr(c.arg(1)), '', 2, 0
        while i < len(fmt):
            if fmt[i] == '%' and fmt[i + 1] in 'sd':
                out += self.cstr(c.arg(k)) if fmt[i + 1] == 's' else str(c.arg(k))
                k, i = k + 1, i + 2
            else:
                out, i = out + fmt[i], i + 1
        self.cpu.write(c.arg(0), out.encode('latin-1') + b'\0')
        return len(out)

    def alloc(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def cstr(self, a):
        b = bytes(self.cpu._uc.mem_read(a, 0x80))
        return b[:b.index(b'\0')].decode('latin-1')

    def call(self, addr, *args, ecx=0):
        self.cpu.call(addr, ecx=ecx, stack_args=args)
        return self.cpu._uc.reg_read(UC_X86_REG_EAX)


def overrides(exe, p, r, cases):
    p.probe_pm_override.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_int32)]
    bad = 0
    for _ in range(cases):
        which, null = r.randrange(4), r.random() < 0.1
        flags, value = r.getrandbits(32), r.choice((0, 1, 2, 0xff, 0x100, r.getrandbits(32)))
        n = 0 if null else exe.alloc(0xd4)
        if n:
            exe.cpu.u32(n + 0x2c, flags)
        exe.reports = []
        res = exe.call(OVERRIDES[which], n, value) & 0xffffffff
        after = struct.unpack('<I', exe.cpu.read(n + 0x2c, 4))[0] if n else flags
        want = [res, after, len(exe.reports)] + exe.reports
        out = (ctypes.c_int32 * 8)()
        p.probe_pm_override(which, int(null), flags, value, out)
        got = [x & 0xffffffff for x in out[:3 + out[2]]]
        bad += got != want
    return bad


def lod(r):
    raw = bytearray(0x5c)
    struct.pack_into('<I', raw, 0, r.randint(0, 3))
    rng = r.uniform(10, 900)
    struct.pack_into('<ff', raw, 4, r.choice((0.0, -1.0, r.uniform(0, 1e6), (rng * r.uniform(0.5, 1.2)) ** 2)), rng)
    for k in range(3):
        struct.pack_into('<f', raw, 0x10 + 4 * k, r.choice((r.uniform(1, rng), rng, rng * 2, 0.5)))
        struct.pack_into('<f', raw, 0x1c + 4 * k, r.choice((0.0, 0.01, 0.0100001, 1.0, r.uniform(0, 2))))
        struct.pack_into('<f', raw, 0x28 + 4 * k, r.uniform(0, 1))
    struct.pack_into('<I', raw, 0x50, r.choice((0, 0, 1)))
    return bytes(raw)


def fades(exe, p, r, cases):
    p.probe_pm_lod_fades.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    bad, live = 0, 0
    for _ in range(cases):
        raw = lod(r)
        d, n = exe.alloc(0x5c), exe.alloc(0xd4)
        exe.cpu.write(d, raw)
        exe.cpu.u32(n + 0x38, d)
        exe.call(LOD_FADES, n)
        want = exe.cpu.read(d + 0x34, 12)
        out = ctypes.create_string_buffer(12)
        p.probe_pm_lod_fades(raw, out)
        bad += out.raw != want
        live += want != bytes(12)
    return bad, live


def resolve(exe, p, r, cases):
    p.probe_pm_resolve.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_char_p,
                                   ctypes.c_char_p]
    shipped = {f.name.lower() for f in (ROOT / 'game' / 'pm' / 'zbd').glob('*.zbd')}
    bad, kinds = 0, {'r-name': 0, 'plain': 0, 'missing': 0}
    for k in range(cases):
        base = r.choice(('texture', 'mech', 'image', 'tex'))
        variant, rp, needs = r.choice((0, 0, 1, 2, 3, 8)), r.random() < 0.7, r.random() < 0.95
        if k % 4 == 0:
            files = set(shipped)
        else:
            files = set()
            for v in range(9):
                tail = f'{v}.zbd' if v else '.zbd'
                if r.random() < 0.25:
                    files.add(base + tail)
                if r.random() < 0.25:
                    files.add('r' + base + tail)
        exe.files = files
        lib = exe.alloc(0xb4)
        exe.cpu.write(lib, base.encode() + b'\0')
        exe.cpu.u32(lib + 0xa8, variant)
        exe.cpu.write(lib + 0xac, bytes([int(needs), int(rp)]))
        exe.call(RESOLVE, ecx=lib)
        want = (exe.cstr(lib), exe.cpu.read(lib + 0xae, 1)[0])
        out = ctypes.create_string_buffer(0x80)
        missing = p.probe_pm_resolve(base.encode(), variant, int(rp), int(needs), '|'.join(sorted(files)).encode(), out)
        got = (out.value.decode('latin-1'), missing)
        if got != want:
            bad += 1
            if bad <= 3:
                print(f'library {base} v{variant} r{int(rp)}: exe {want} port {got}')
        if want[1]:
            kinds['missing'] += 1
        elif want[0].startswith('r') and needs:
            kinds['r-name'] += 1
        elif needs:
            kinds['plain'] += 1
    return bad, kinds


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    p = test_pm_zar.load_pm_probe()
    exe = Exe()
    r = random.Random(0x515c00)
    b1 = overrides(exe, p, r, cases)
    b2, live = fades(exe, p, r, cases)
    b3, kinds = resolve(exe, p, r, cases)
    print(f'overrides: {cases}, {b1} differ; LoD fades: {cases}, {b2} differ ({live} not all zero); library names: '
          f'{cases}, {b3} differ {kinds}')
    if b1 or b2 or b3 or live == 0 or min(kinds.values()) == 0:
        raise SystemExit(1)
    print('ok: the override setters, the LoD fades and the library names as PM\'s exe')


if __name__ == '__main__':
    main()
