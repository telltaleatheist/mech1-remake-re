"""The motion script's text side (gamez/zsiscript/si_script; docs/motion.md, The text side) against Mech3.exe 1.2:
random loose .zan texts read by 0x5a9e10 (the C runtime's fscanf, rewind, calloc, strncmp and _strdup stood in over
the text; the parts made by 0x5aa500 / 0x5aa5a0 and the angles turned by 0x5aa210, the exe's own), the frame and part
counts, each part's name and every key compared bit for bit; then each read script written back by 0x5a9bc0 (its
parts by 0x5aa620, the names by 0x5763f0; WriteFile caught) against the port's write. At the x87's 24 bits
(MW3_X87=53: the rotations' last bit apart, from the angle sums the exe keeps in its registers).

    cmake --build build --target mw3probe && python3 tests/differential/test_motion_text.py
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_X87', '24')  # a mission's load runs at Direct3D's 24 bits (CLAUDE.md); at 53 the quaternions are an ulp apart
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import iat_slots  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402

READ_TEXT, WRITE, EULER, NEW = 0x5a9e10, 0x5a9bc0, 0x5aa210, 0x5abd26
FLOAT = re.compile(r'[+-]?(\d+\.?\d*([eE][+-]?\d+)?|\.\d+([eE][+-]?\d+)?)')
INT = re.compile(r'[+-]?\d+')


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        iat = iat_slots()
        self.file = 0x1234
        cpu.stub_import(iat['fscanf'], self.fscanf)
        cpu.stub_import(iat['rewind'], lambda c: setattr(self, 'pos', 0) or 0)
        cpu.stub_import(iat['calloc'], lambda c: self.zalloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(iat['malloc'], lambda c: self.zalloc(c.arg(0)))
        cpu.stub_import(iat['strncmp'], self.strncmp)
        cpu.stub_import(iat['_strdup'], self.strdup)
        cpu.stub_import(iat['WriteFile'], self.write, pop_bytes=20)
        cpu.stub_function(NEW, lambda c: self.zalloc(c.arg(0)))

    def zalloc(self, n):
        a = self.cpu.alloc(max(n, 4))
        self.cpu.write(a, bytes(max(n, 4)))
        return a

    def raw(self):
        """The emulator without the PM address map (main's Original has none)."""
        return getattr(self.cpu, '_uc', None) or self.cpu.uc

    def cstr(self, a, n=4096):
        # untranslated: on PM's exe the pointers it passes are its own (with MW3_EXE=pm the test runs there too)
        return bytes(self.raw().mem_read(a, n)).split(b'\0')[0].decode('latin-1')

    def strncmp(self, c):
        a, b = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)]
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def strdup(self, c):
        s = self.cstr(c.arg(0)).encode('latin-1')
        a = self.zalloc(len(s) + 1)
        self.cpu.write(a, s + b'\0')
        return a

    def skip(self):
        while self.pos < len(self.text) and self.text[self.pos].isspace():
            self.pos += 1

    def fscanf(self, c):
        """%s, %d and "%f %f %f", as MSVCRT reads them (a float only stored when its digits parse)."""
        fmt = self.cstr(c.arg(1))
        esp = c.uc.reg_read(UC_X86_REG_ESP)
        args = [struct.unpack('<I', self.cpu.read(esp + 12 + 4 * i, 4))[0] for i in range(3)]
        convs = re.findall(r'%[sdf]', fmt)
        n = 0
        for k, conv in enumerate(convs):
            self.skip()
            if self.pos >= len(self.text):
                return n if n else 0xffffffff
            rest = self.text[self.pos:]
            if conv == '%s':
                m = re.match(r'\S+', rest)
                self.cpu.write(args[k], m.group(0).encode('latin-1') + b'\0')
            else:
                m = (INT if conv == '%d' else FLOAT).match(rest)
                if not m:
                    return n
                val = int(m.group(0)) if conv == '%d' else float(m.group(0))
                self.cpu.write(args[k], struct.pack('<i', val) if conv == '%d' else struct.pack('<f', val))
            self.pos += m.end()
            n += 1
        return n

    def write(self, c):
        self.out += bytes(self.raw().mem_read(c.arg(1), c.arg(2)))  # untranslated, as cstr
        self.cpu.u32(c.arg(3), c.arg(2))
        return 1

    def run(self, text):
        cpu = self.cpu
        self.text, self.pos = text, 0
        s = self.zalloc(0x88)
        cpu.call(0x5a9920, ecx=s)  # the script's constructor (rate, mult, stride, wraps)
        cpu.call(READ_TEXT, ecx=s, stack_args=(self.file,))
        ok = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        r32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]
        frames, parts = r32(s + 0x64), r32(s + 0x68)
        o = bytes([1 if ok else 0]) + struct.pack('<II', frames, parts)
        if ok:
            table = r32(s + 0x6c)
            for i in range(parts):
                p = r32(table + 4 * i)
                name = r32(p + 0x44)
                o += (self.cstr(name).encode('latin-1') if name else b'') + b'\0'
                o += cpu.read(r32(p), (frames + 1) * 12) + cpu.read(r32(p + 8), (frames + 1) * 16)
            self.out = b''
            cpu.call(WRITE, ecx=s, stack_args=(0x100,))
            o += struct.pack('<I', len(self.out)) + self.out
        return o


def make_text(r):
    frames = r.randrange(1, 5)
    names = ['hip', 'torso', 'l_leg', 'r_arm', 'cockpit_00'][:r.randrange(1, 5)]
    lines = []
    num = lambda: r.choice(('%.6f' % r.uniform(-4, 4), '%g' % r.uniform(-200, 200), '0', '-1', '3.14159265'))
    bad = r.random() < 0.08  # a later frame with an object more than the first
    for f in range(frames):
        lines.append('Frame %d' % (f * r.choice((1, 2, 5))))
        objs = names + (['extra'] if bad and f == frames - 1 and f > 0 else [])
        for nm in objs:
            lines.append('  Object %s' % nm)
            entries = ['Scaling %s %s %s' % (num(), num(), num()), 'Rotation %s %s %s' % (num(), num(), num()),
                       'Translation %s %s %s' % (num(), num(), num())]
            if r.random() < 0.3:
                r.shuffle(entries)
            if r.random() < 0.15:
                entries.insert(r.randrange(len(entries) + 1), 'Comment x')
            lines += ['    ' + e for e in entries]
    return '\n'.join(lines) + '\n'


def main():
    p = load_probe()
    p.probe_motion_text.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_euler_quat.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float)]
    exe = Exe()
    r = random.Random(0x5a9e10)
    buf = ctypes.create_string_buffer(1 << 20)
    bad = oks = 0
    for trial in range(300):
        text = make_text(r)
        want = exe.run(text)
        n = p.probe_motion_text(text.encode(), buf, len(buf))
        got = buf.raw[:n]
        oks += want[0]
        if want != got:
            bad += 1
            if bad <= 4:
                k = next(i for i in range(min(len(want), len(got))) if want[i] != got[i]) if want[:len(got)] != got[:len(want)] else min(len(want), len(got))
                print(f'trial {trial}: first difference at byte {k} of {len(want)}/{len(got)}\n{text[:300]}')
    print(f'300 texts ({oks} read whole): {bad} differ')
    if bad:
        raise SystemExit(1)
    print('ok: the motion text side as the original')


if __name__ == '__main__':
    main()
