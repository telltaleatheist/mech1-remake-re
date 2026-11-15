"""zImage's bitmap fonts (gamez/zimage/zimg_fonts.h) against the original x86: a column clear of the key 0x51a080,
the glyphs cut from an image 0x519fa0, a text's size 0x51a0d0 with the font lookup 0x519df0, and the loader
0x519e50 with the free 0x519f70 over a FONTS list (zReader's read and find, the image fetch, calloc and free,
the image path and the WINDOWS_FONTS call stubbed alike on both sides).

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_zimg_fonts.py [runs]
    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_zimg_fonts.py

Images are rows of glyphs on the key colour (gaps of 1 to 6 columns, ink touching the gap's edges or not,
stray key pixels inside a glyph, a glyph of one column), always 95 glyphs and a clear column at the right
edge: the exe's scan never ends otherwise (FIX (A) in the port, checked on the port alone). Compared: every
answer, the glyphs, which block each slot points at, the calls out and the reports in order.
"""
import ctypes
import json
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import ROOT, Original  # noqa: E402

SLOTS, KEY = 0x638484, 0x6384e0
COLUMN, SCAN, SIZE, FONT, LOAD, FREE = 0x51a080, 0x519fa0, 0x51a0d0, 0x519df0, 0x519e50, 0x519f70
IAT_CALLOC, IAT_FREE = 0x5bb60c, 0x5bb65c
# Pirate's Moon (MW3_EXE=pm): its copies (0x533660..0x533960) take MW3's ECX and EDX on the stack, as every call
# out (zReader's find 0x578950, the fetch 0x5327d0 with a 0 after the name); the addresses from the map.
PM = os.environ.get('MW3_EXE') == 'pm'
if PM:
    _map = json.load(open(ROOT / 'analysis-output' / 'pm-diff' / 'addrmap.json'))
    _MAP = {int(k, 16): int(v, 16) for part in ('data', 'imports', 'functions') for k, v in _map[part].items()}
    _MAP[0x5569e0] = 0x578950
    A = lambda a: _MAP[a]  # noqa: E731
else:
    A = lambda a: a  # noqa: E731
I32 = ctypes.c_int32


def make_font(rng, key, h=None):
    """An image of 95 glyphs on the key: (width, height, pixels)."""
    h = h or rng.randint(1, 12)
    cols = [[key] * h for _ in range(rng.randint(1, 4))]  # leading clear columns
    for _ in range(95):
        for _ in range(rng.randint(1, 6)):
            col = [rng.choice((key ^ 0x1f, key ^ 0x7c0, rng.getrandbits(16) | 1)) for _ in range(h)]
            if h > 1 and rng.random() < 0.3:  # a key pixel inside the ink: the column still counts as ink
                col[rng.randrange(h)] = key
            if all(p == key for p in col):
                col[0] = key ^ 1
            cols.append(col)
        cols += [[key] * h for _ in range(rng.randint(1, 6))]
    w = len(cols)
    return w, h, [cols[x][y] for y in range(h) for x in range(w)]


def eax(cpu):
    from unicorn.x86_const import UC_X86_REG_EAX
    return (getattr(cpu, 'uc', None) or cpu._uc).reg_read(UC_X86_REG_EAX)


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        if PM:
            cpu.tr = lambda a, size=1: a  # addresses given as PM's (A)
        self.log = []
        self.nodes = {}
        r0 = (lambda c: c.arg(0)) if PM else (lambda c: c.ecx)  # MW3's ECX: PM's first word
        r1 = (lambda c: c.arg(1)) if PM else (lambda c: c.edx)
        cpu.stub_import(A(IAT_CALLOC), lambda c: self.calloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(A(IAT_FREE), lambda c: self.say('free') or 0)
        cpu.stub_function(A(0x556740), lambda c: self.say('read ' + self.cstr(r0(c))) or self.root, 0 if PM else 4)
        cpu.stub_function(A(0x5569e0), lambda c: self.nodes.get(self.cstr(r1(c)), 0), 0)
        cpu.stub_function(A(0x556820), lambda c: self.say('free tree') or 0, 0)
        cpu.stub_function(A(0x516f50), lambda c: self.say('image path ' + self.cstr(r0(c))) or 0, 0)
        cpu.stub_function(A(0x518ee0), lambda c: self.fetch(r0(c)), 0)
        cpu.stub_function(A(0x5175e0), lambda c: self.say('windows fonts list' if r0(c) else 'windows fonts none') or 1, 0)
        cpu.stub_function(A(0x517650), lambda c: 0, 0)
        cpu.stub_function(A(0x49bfd0), lambda c: self.say(f'report {c.arg(0)} {c.arg(2)}') or 0, 0)
        self.out, self.text = cpu.alloc(0x40), cpu.alloc(0x400)

    def say(self, s):
        self.log.append(s)

    def calloc(self, n):
        a = self.cpu.alloc(max(n, 4))
        self.cpu.write(a, bytes(n))
        return a

    def cstr(self, a):
        b = b''
        while True:
            c = self.cpu.read(a + len(b), 1)
            if c == b'\0':
                return b.decode('latin-1')
            b += c

    def string(self, s):
        a = self.cpu.alloc(len(s) + 1)
        self.cpu.write(a, s.encode('latin-1') + b'\0')
        return a

    def image(self, w, h, px):
        cpu = self.cpu
        t, p = cpu.alloc(0x38), cpu.alloc(max(2 * w * h, 2))
        cpu.write(t, bytes(0x38))
        cpu.write(t + 4, struct.pack('<hh', w, h))
        cpu.u32(t + 0x10, p)
        cpu.write(p, struct.pack(f'<{w * h}H', *px))
        return t

    def go(self, fn, ecx=None, edx=None, stack=()):
        """MW3's fn with its registers and stack; on PM its copy with them all on the stack."""
        if PM:
            self.cpu.call(A(fn), stack_args=[x for x in (ecx, edx) if x is not None] + list(stack))
        else:
            self.cpu.call(fn, ecx=ecx or 0, edx=edx or 0, stack_args=list(stack))

    def fetch(self, at):
        name = self.cstr(at)
        self.say('fetch ' + name)
        return self.images.get(name, 0)


def main():
    probe = load_probe()
    P16 = ctypes.POINTER(ctypes.c_uint16)
    probe.probe_font_column.argtypes = [ctypes.c_int, ctypes.c_int, P16, ctypes.c_uint16, ctypes.c_int]
    probe.probe_font_scan.argtypes = [ctypes.c_int, ctypes.c_int, P16, ctypes.c_uint16, ctypes.POINTER(I32)]
    probe.probe_text_size.argtypes = [ctypes.POINTER(I32), ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(I32)]
    probe.probe_fonts_load.argtypes = [ctypes.c_char_p, ctypes.POINTER(I32), P16, ctypes.c_uint16, ctypes.c_int,
                                       ctypes.POINTER(I32), ctypes.c_char_p, ctypes.c_int]
    emu = Emulated()
    cpu = emu.cpu
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    rng = random.Random(0x519e50)
    stats = {'columns': 0, 'scans': 0, 'sizes': 0, 'loads': 0, 'missing images': 0, 'reports': 0}
    for run in range(runs):
        key = rng.getrandbits(16)
        cpu.write(A(KEY), struct.pack('<H', key))
        # the column test and the scan
        w, h, px = make_font(rng, key)
        img = emu.image(w, h, px)
        arr = (ctypes.c_uint16 * len(px))(*px)
        for _ in range(40):
            col = rng.randrange(-2, w + 3)
            emu.go(COLUMN, img, col & 0xffffffff)
            want = eax(cpu)
            if col < 0:
                continue  # the exe reads before the pixels; the port too: not compared
            got = probe.probe_font_column(w, h, arr, key, col)
            if want != got:
                raise SystemExit(f'run {run}: column {col} of {w}x{h}: exe {want}, port {got}')
            stats['columns'] += 1
        font = emu.calloc(0x5f8)
        cpu.u32(font, img)
        emu.go(SCAN, font)
        found = eax(cpu)
        want = [found] + list(struct.unpack('<381i', cpu.read(font + 4, 4 * 381)))
        out = (I32 * 381)()
        got = [probe.probe_font_scan(w, h, arr, key, out)] + list(out)
        if want != got:
            k = next(i for i in range(len(want)) if want[i] != got[i])
            raise SystemExit(f'run {run}: scan of {w}x{h}: word {k} exe {want[k]}, port {got[k]}')
        stats['scans'] += 1
        # text sizes over a table of fonts (some slots empty)
        metrics = []
        table = []
        for s in range(20):
            present = s == 0 or rng.random() < 0.5
            fh = rng.randint(1, 20)
            g = [rng.randint(0, 300) for _ in range(380)]
            space = rng.randint(-1, 9)
            metrics += [int(present), fh, space] + g
            if present:
                t = emu.image(1, fh, [0] * fh)
                f = emu.calloc(0x5f8)
                cpu.u32(f, t), cpu.u32(f + 4, space & 0xffffffff)
                cpu.write(f + 8, struct.pack('<380i', *g))
                table.append(f)
            else:
                table.append(0)
        cpu.write(A(SLOTS), struct.pack('<20I', *table))
        mi = (I32 * len(metrics))(*metrics)
        for _ in range(10):
            text = ''.join(rng.choice('ab Z~!\n\r\x7f\x80\xff \t09') for _ in range(rng.randint(0, 30)))
            fid = rng.choice((rng.randrange(20), rng.randrange(-3, 24)))
            cpu.write(emu.text, text.encode('latin-1') + b'\0')
            cpu.write(emu.out, struct.pack('<ii', -7, -7))
            emu.log = []
            emu.go(SIZE, emu.text, fid & 0xffffffff, [emu.out, emu.out + 4])
            want = list(struct.unpack('<ii', cpu.read(emu.out, 8))) + [len(emu.log)]
            o = (I32 * 3)(-7, -7, 0)
            probe.probe_text_size(mi, text.encode('latin-1'), fid, o)
            got = [o[0], o[1], 1 if o[2] else 0]
            want[2] = 1 if want[2] else 0
            if want != got:
                raise SystemExit(f'run {run}: size of {text!r} in font {fid}: exe {want}, port {got}')
            stats['sizes'] += 1
        # the loader and the free
        n = rng.randint(1, 20)
        names, specs, pix = [f'font{k}' for k in range(n)], [], []
        emu.images = {}
        for k in range(n):
            if rng.random() < 0.2:
                specs += [0, 0]
                stats['missing images'] += 1
                continue
            fw, fh, fpx = make_font(rng, key, rng.randint(1, 6))
            if rng.random() < 0.1:  # no columns: "Only found 1 characters" (the one way short of 95 that ends)
                fw, fpx = 0, []
            emu.images[names[k]] = emu.image(fw, fh, fpx)
            specs += [fw, fh]
            pix += fpx
        windows = rng.random() < 0.7
        # the exe's tree: the FONTS list and maybe WINDOWS_FONTS, found through the stubbed find
        fl = cpu.alloc(8 * (n + 2))
        cpu.write(fl, struct.pack('<ii', 1, n + 1) + struct.pack('<iI', 3, emu.string('fonts')))
        for k, nm in enumerate(names):
            cpu.write(fl + 8 * (k + 2), struct.pack('<iI', 3, emu.string(nm)))
        node = cpu.alloc(8)
        cpu.write(node, struct.pack('<iI', 4, fl))
        emu.nodes = {'FONTS': node}
        if windows:
            wl = cpu.alloc(8)
            cpu.write(wl, struct.pack('<iI', 4, cpu.alloc(16)))
            emu.nodes['WINDOWS_FONTS'] = wl
        emu.root = cpu.alloc(8)
        cpu.write(A(SLOTS), bytes(80))
        emu.log = []
        file = emu.string('fonts.zrd')
        emu.go(LOAD, file)
        ret = eax(cpu) & 0xff
        slots = list(struct.unpack('<20I', cpu.read(A(SLOTS), 80)))
        block = slots[0]
        want = [ret] + [(s - block) // 0x5f8 if s else -1 for s in slots]
        for s in slots:
            if not s:
                break
            image = struct.unpack('<I', cpu.read(s, 4))[0]
            idx = next((k for k, nm in enumerate(names) if emu.images.get(nm) == image), -1)
            want += [idx] + list(struct.unpack('<381i', cpu.read(s + 4, 4 * 381)))
        emu.go(FREE)
        want_log = [x.replace('report 512 101', 'report 512 101') for x in emu.log] + ['freed 1']
        out = (I32 * (21 + 20 * 382))()
        logbuf = ctypes.create_string_buffer(1 << 14)
        probe.probe_fonts_load('\n'.join(names).encode(), (I32 * len(specs))(*specs), (ctypes.c_uint16 * max(len(pix), 1))(*pix),
                               key, int(windows), out, logbuf, len(logbuf))
        got = list(out[:len(want)])
        got_log = logbuf.value.decode().splitlines()
        want_log = [x for x in want_log if x != 'free']
        if want != got or want_log != got_log:
            print(f'run {run}: load of {n} fonts ({names}), images {[nm in emu.images for nm in names]}')
            if want != got:
                k = next(i for i in range(min(len(want), len(got))) if want[i] != got[i])
                print(f'  word {k}: exe {want[k]}, port {got[k]}  (slots exe {want[1:21]}, port {got[1:21]})')
            if want_log != got_log:
                print(f'  calls exe  {want_log}\n        port {got_log}')
            raise SystemExit(1)
        stats['loads'] += 1
        stats['reports'] += sum(x.startswith('report') for x in want_log)
    print(f'zimg fonts: {runs} runs identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
