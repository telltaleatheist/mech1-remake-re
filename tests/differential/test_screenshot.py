"""zImage's screenshot (gamez/zimage/zimg_shot; docs/screenshots.md) and the debug command 0x7e (mech3/debug_keys
debug_screenshot) against Mech3.exe 1.2: random screens in 565, 555 and odd formats grabbed (0x519990: the grab
0x579500 stood in for, the format's bits and masks from zVideo's globals, 24-bit by 0x519a40, rows flipped by
0x519be0/0x519b30) and written (0x519c20: CreateFileA/WriteFile caught), the file's bytes compared; then the command
0x4abbc0 in and out of a lobby session, the Save dialog OK or cancelled, _mktemp's names taken or free.

    cmake --build build --target mw3probe && python3 tests/differential/test_screenshot.py
"""
import ctypes
import random
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import iat_slots  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

OBJ_SIZE = 0x6c
BITS, MASKS = 0x802720, 0x80272c
GRAB, FREE_TEX, NEW, DELETE, LOBBY, GDI, KEYS, SAVE_DLG = (0x579500, 0x517cc0, 0x5abd26, 0x5abd1a, 0x586ce0, 0x577340,
                                                           0x51a300, 0x5ac5dc)
FORMATS = [((5, 6, 5), (0xf800, 0x7e0, 0x1f)), ((5, 5, 5), (0x7c00, 0x3e0, 0x1f)), ((4, 4, 4), (0xf00, 0xf0, 0xf)),
           ((6, 5, 5), (0xfc00, 0x3e0, 0x1f))]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        iat = iat_slots()
        self.files = {}
        self.events = []
        cpu.stub_function(GRAB, self.grab)
        cpu.stub_function(FREE_TEX, lambda c: 0)
        cpu.stub_function(NEW, lambda c: self.alloc(c.arg(0)))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_import(iat['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(iat['free'], lambda c: 0)
        cpu.stub_import(iat['CreateFileA'], self.create, pop_bytes=28)
        cpu.stub_import(iat['WriteFile'], self.write, pop_bytes=20)
        cpu.stub_import(iat['CloseHandle'], lambda c: 1, pop_bytes=4)
        cpu.stub_import(iat['ShowCursor'], lambda c: self.events.append(f'cursor {c.arg(0)}') or 0, pop_bytes=4)
        cpu.stub_import(iat['_mktemp'], self.mktemp)
        cpu.stub_function(LOBBY, lambda c: int(self.lobby))
        cpu.stub_function(GDI, lambda c: self.events.append('gdi') or 0)
        cpu.stub_function(KEYS, lambda c: self.events.append('keys') or 0)
        cpu.stub_function(SAVE_DLG, self.save_dialog, pop_bytes=4)
        cpu.u32(0x72f348, 1)  # the Direct3D path

    def alloc(self, n):
        a = self.cpu.alloc(max(n, 4))
        return a

    def cstr(self, a):
        return self.cpu.read(a, 260).split(b'\0')[0].decode('latin-1')

    def grab(self, c):
        self.events.append('grab')
        if c.ecx != 2 or self.w < 0:
            return 0
        t = self.alloc(0x38)
        self.cpu.write(t, bytes(0x38))
        self.cpu.write(t + 4, struct.pack('<hh', self.w, self.h))
        px = self.alloc(2 * self.w * self.h + 2)
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

    def mktemp(self, c):
        """MSVCRT's _mktemp (the multithreaded CRT's id, here `pid`)."""
        a = c.arg(0)
        s = bytearray(self.cpu.read(a, 64).split(b'\0')[0])
        i, n, number = len(s) - 1, 0, self.pid
        while i >= 0 and s[i] == ord('X') and n < 5:
            s[i] = ord('0') + number % 10
            number //= 10
            n += 1
            i -= 1
        if i < 0 or s[i] != ord('X') or n < 5:
            return 0
        s[i] = ord('a')
        while s.decode() in self.taken:
            if s[i] == ord('z'):
                return 0
            s[i] += 1
        self.cpu.write(a, bytes(s) + b'\0')
        return a

    def save_dialog(self, c):
        """GetSaveFileNameA: the path into the struct's buffer ([0x638544])."""
        self.events.append('dialog')
        if self.dialog is None:
            return 0
        buf = struct.unpack('<I', self.cpu.read(0x638544, 4))[0]
        self.cpu.write(buf, self.dialog.encode() + b'\0')
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
        o = self.alloc(OBJ_SIZE)
        cpu.call(0x5199e0, ecx=o)
        cpu.call(0x519990, ecx=o)
        ok = cpu.uc.reg_read(UC_X86_REG_EAX)
        out = bytes([1 if ok else 0])
        if ok:
            path = self.alloc(32)
            cpu.write(path, b'shot.bmp\0')
            cpu.call(0x519c20, ecx=o, stack_args=(path,))
            out += self.files['shot.bmp']
        return out

    def command(self, w, h, px, lobby, dialog, taken, pid):
        self.screen(w, h, px, FORMATS[0])
        self.lobby, self.dialog, self.taken, self.pid = lobby, dialog, set(taken), pid
        self.files, self.events = {}, []
        self.cpu.call(0x4abbc0)
        name = next(iter(self.files), '')
        ev = ''.join(e + ';' for e in self.events)
        return (ev + '|' + name + '|').encode('latin-1') + (self.files[name] if name else b'')


def main():
    p = load_probe()
    p.probe_shot.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_uint16),
                             ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p,
                             ctypes.c_char_p, ctypes.c_int]
    p.probe_debug_screenshot.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_uint16), ctypes.c_int,
                                         ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p,
                                         ctypes.c_char_p, ctypes.c_int]
    p.probe_mktemp.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x519990)
    buf = ctypes.create_string_buffer(1 << 22)
    tmp = Path(tempfile.mkdtemp(prefix='mw3-shot-'))
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
            got = buf.raw[:n]
            if want != got:
                bad += 1
                if bad <= 4:
                    print(f'shot {w}x{h} {fmt}: exe {len(want)} bytes {want[:60].hex()}\n  port {len(got)} {got[:60].hex()}')
        for trial in range(80):
            w, h = r.choice((-1, 2, 4, 9)), r.choice((1, 3, 6))
            px = [r.getrandbits(16) for _ in range(max(w, 0) * h)]
            lobby = r.randrange(2)
            dialog = r.choice((None, 'picked.bmp', 'sub_shot.bmp'))
            pid = r.choice((0, 7, 12345, 99999, 4000000000 + r.randrange(1000)))
            mk = lambda letter: 'mw3_' + letter + ''.join(str(pid // 10 ** i % 10) for i in range(4, -1, -1))
            taken = r.choice(([], [mk('a')], [mk(ch) for ch in 'abc'], [mk(chr(ch)) for ch in range(97, 123)]))
            want = exe.command(w, h, px, lobby, dialog, taken, pid)
            n = p.probe_debug_screenshot(w, h, (ctypes.c_uint16 * max(1, len(px)))(*px or [0]), lobby,
                                         dialog.encode() if dialog else None, '\n'.join(taken).encode(), pid,
                                         str(tmp).encode(), buf, len(buf))
            got = buf.raw[:n]
            if want != got:
                bad += 1
                if bad <= 8:
                    print(f'command lobby {lobby} dialog {dialog} taken {len(taken)} pid {pid}:\n'
                          f'  exe  {want[:90]}\n  port {got[:90]}')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'120 screens and 80 commands: {bad} differ')
    if bad:
        raise SystemExit(1)
    print('ok: the screenshot as the original')


if __name__ == '__main__':
    main()
