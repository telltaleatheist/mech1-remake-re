"""Pirate's Moon's screenshot against its exe (MW3's in test_screenshot.py): the screenshot object 0x533380..0x5335c0
(grab 0x533320 through 0x598600, the format from 0x5982a0 / 0x5982d0, 24-bit, flipped, written) on random screens
in four formats, then PM's own command 0x4af600 (CMD_SNAPSHOT, Ctrl+P by default): mw3%08x.bmp from the session's
count, stepped past the names that exist (0x596ea0), no dialog, the keys reset (0x533b90).

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_screenshot.py
"""
import ctypes
import os
import random
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import Original  # noqa: E402
from twod_world import IAT, c_format  # noqa: E402  (PM's slots)
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402
import test_pm_zar  # noqa: E402
from test_screenshot import FORMATS  # noqa: E402

BITS, MASKS = 0xb06184, 0xb06190
GRAB, FREE_TEX, NEW, DELETE, EXISTS, KEYS, COUNT = 0x598600, 0x531000, 0x5ce8c6, 0x5ce8c0, 0x596ea0, 0x533b90, 0x62f6d0


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.files, self.events = {}, []
        cpu.stub_function(GRAB, self.grab)
        cpu.stub_function(FREE_TEX, lambda c: 0)
        cpu.stub_function(NEW, lambda c: cpu.alloc(max(c.arg(0), 4)))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_function(EXISTS, lambda c: int(self.cstr(c.arg(0)) in self.taken))
        cpu.stub_function(KEYS, lambda c: self.events.append('keys') or 0)
        cpu.stub_import(IAT['malloc'], lambda c: cpu.alloc(max(c.arg(0), 4)))
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['CreateFileA'], self.create, pop_bytes=28)
        cpu.stub_import(IAT['WriteFile'], self.write, pop_bytes=20)
        cpu.stub_import(IAT['CloseHandle'], lambda c: 1, pop_bytes=4)
        cpu.stub_import(IAT['sprintf'], self.sprintf)
        self.taken = set()

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def cstr(self, a):
        return self.cpu.read(a, 260).split(b'\0')[0].decode('latin-1')

    def sprintf(self, c):
        s = c_format(self, self.cstr(c.arg(1)), c.uc.reg_read(UC_X86_REG_ESP) + 12).encode('latin-1')
        self.cpu.write(c.arg(0), s + b'\0')
        return len(s)

    def grab(self, c):
        self.events.append('grab')
        if c.arg(0) != 2 or self.w < 0:
            return 0
        t = self.cpu.alloc(0x38)
        self.cpu.write(t, bytes(0x38))
        self.cpu.write(t + 4, struct.pack('<hh', self.w, self.h))
        px = self.cpu.alloc(2 * self.w * self.h + 2)
        self.cpu.write(px, struct.pack(f'<{self.w * self.h}H', *self.px))
        self.cpu.u32(t + 0x10, px)
        return t

    def create(self, c):
        self.current = self.cstr(c.arg(0))
        self.files[self.current] = b''
        return 0x100

    def write(self, c):
        self.files[self.current] += self.cpu.read(c.arg(1), c.arg(2))
        self.cpu.u32(c.arg(3), c.arg(2))
        return 1

    def screen(self, w, h, px, fmt):
        self.w, self.h, self.px = w, h, px
        (rb, gb, bb), (rm, gm, bm) = fmt
        self.cpu.write(BITS, struct.pack('<3i', rb, gb, bb))
        self.cpu.write(MASKS, struct.pack('<3I', rm, gm, bm))

    def shot(self, w, h, px, fmt):
        cpu = self.cpu
        self.screen(w, h, px, fmt)
        self.files = {}
        o = cpu.alloc(0x6c)
        cpu.call(0x533380, ecx=o)
        cpu.call(0x533320, ecx=o)
        ok = cpu.uc.reg_read(UC_X86_REG_EAX)
        out = bytes([1 if ok else 0])
        if ok:
            path = cpu.alloc(32)
            cpu.write(path, b'shot.bmp\0')
            cpu.call(0x5335c0, ecx=o, stack_args=(path,))
            out += self.files['shot.bmp']
        return out

    def command(self, w, h, px, count, taken):
        self.screen(w, h, px, FORMATS[0])
        self.taken, self.files, self.events = set(taken), {}, []
        self.cpu.u32(COUNT, count)
        self.cpu.call(0x4af600)
        name = next(iter(self.files), '')
        ev = ''.join(e + ';' for e in self.events)
        return (ev + '|' + name + '|' + str(struct.unpack('<i', self.cpu.read(COUNT, 4))[0]) + '|').encode() + \
            (self.files[name] if name else b'')


def main():
    p = test_pm_zar.load_pm_probe()
    u16p = ctypes.POINTER(ctypes.c_uint16)
    p.probe_shot.argtypes = [ctypes.c_int32, ctypes.c_int32, u16p, ctypes.POINTER(ctypes.c_int32),
                             ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_screenshot.argtypes = [ctypes.c_int32, ctypes.c_int32, u16p, ctypes.c_int32, ctypes.c_char_p,
                                      ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x4af600)
    buf = ctypes.create_string_buffer(1 << 22)
    tmp = Path(tempfile.mkdtemp(prefix='mw3pm-shot-'))
    bad = 0
    try:
        for trial in range(120):
            fmt = r.choice(FORMATS)
            w, h = r.choice((-1, 1, 2, 3, 4, 7, 16, 33)), r.choice((1, 2, 3, 5, 8, 21))
            px = [r.getrandbits(16) for _ in range(max(w, 0) * h)]
            (rb, gb, bb), masks = fmt
            want = exe.shot(w, h, px, fmt)
            n = p.probe_shot(w, h, (ctypes.c_uint16 * max(1, len(px)))(*px or [0]), (ctypes.c_int32 * 3)(rb, gb, bb),
                             (ctypes.c_uint32 * 3)(*masks), str(tmp).encode(), buf, len(buf))
            if want != buf.raw[:n]:
                bad += 1
                if bad <= 4:
                    print(f'shot {w}x{h} {fmt}: exe {want[:40].hex()}\n  port {buf.raw[:min(n, 40)].hex()}')
        for trial in range(80):
            w, h = r.choice((-1, 2, 4, 9)), r.choice((1, 3, 6))
            px = [r.getrandbits(16) for _ in range(max(w, 0) * h)]
            count = r.choice((0, 5, 0xfff, 0x7ffffffe, -1))
            taken = [f'mw3{(count + k) & 0xffffffff:08x}.bmp' for k in range(r.choice((0, 1, 3)))]
            want = exe.command(w, h, px, count, taken)
            n = p.probe_pm_screenshot(w, h, (ctypes.c_uint16 * max(1, len(px)))(*px or [0]), count,
                                      '\n'.join(taken).encode(), str(tmp).encode(), buf, len(buf))
            got = buf.raw[:n]
            if want != got:
                bad += 1
                if bad <= 8:
                    print(f'command count {count} taken {taken}:\n  exe  {want[:90]}\n  port {got[:90]}')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'120 screens and 80 commands on PM: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: PM's screenshot as its exe")


if __name__ == '__main__':
    main()
