"""Pirate's Moon's AI tester's handlers (docs/editor.md, PM's AI tester; mech3/ai_tester_pm) against Mech3.exe PM 1.0:
WM_ACTIVATE 0x4c3290, Kill 0x4c32f0, 1251 0x4c3320, Power 0x4c3380, 1217 0x4c33e0, the strategy list's 0x4c3440 /
0x4c34d0 / 0x4c34e0 and 1300 0x4c34f0, each run on a made-up dialog and vehicle (its controls, their brain with its
virtual base and settings; a mech or a basic vehicle) with MFC's and the game's calls stood in and logged: every call
out, the settings' four words and the list's flag after. No vehicle is the port's FIX (A) where the exe calls through
it (1251, 1217, the strategy, 1300): not run on the exe there.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_ai_tester.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_PM_MOVED', '1')
from original import Original  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

HANDLERS = [0x4c3290, 0x4c32f0, 0x4c3320, 0x4c3380, 0x4c33e0, 0x4c3440, 0x4c34d0, 0x4c34e0, 0x4c34f0]
UPDATE_DATA, DEFAULT, APP_NOTE = 0x5cea58, 0x5cea04, 0x5b19c0
APP_OFF, APP_ON, KEYS_OFF, KEYS_ON, VIDEO, POWER = 0x5b11e0, 0x5b1200, 0x534860, 0x534890, 0x59b1c0, 0x41dab0
SEND_MESSAGE = 0x5de73c


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.log = ''
        put = lambda s: setattr(self, 'log', self.log + s) or 0
        cpu.stub_function(UPDATE_DATA, lambda c: put('U;') or 1, pop_bytes=4)
        cpu.stub_function(DEFAULT, lambda c: put('D;'))
        cpu.stub_function(APP_NOTE, lambda c: self.note if self.s[14] else 0)
        cpu.stub_function(APP_OFF, lambda c: put('A0;'))
        cpu.stub_function(APP_ON, lambda c: put('A1;'))
        cpu.stub_function(KEYS_OFF, lambda c: put('K0;'))
        cpu.stub_function(KEYS_ON, lambda c: put('K1;'))
        cpu.stub_function(VIDEO, lambda c: put('V;'))
        cpu.stub_function(POWER, lambda c: put(f'P{ctypes.c_int32(c.arg(0)).value};'), pop_bytes=4)
        cpu.stub_import(SEND_MESSAGE, self.send, pop_bytes=16)

        def fn(f, pop=0):
            a = cpu.code(b'\xcc')  # a stub's own address
            cpu.stub_function(a, f, pop_bytes=pop)
            return a

        def obj(slots, size=0x200):
            o, v = cpu.alloc(size), cpu.alloc(0x200)
            for off, f in slots.items():
                cpu.u32(v + off, f)
            cpu.u32(o, v)
            return o
        self.note = obj({4: fn(lambda c: put(f'N{ctypes.c_int32(c.arg(0)).value};'), 4)})
        self.params = cpu.alloc(0x100)
        vbase = obj({0x88: fn(lambda c: self.params if self.s[11] else 0)})
        self.brain = cpu.alloc(0x200)
        vbt = cpu.alloc(8)
        cpu.u32(vbt + 4, vbase - (self.brain + 0x100))
        cpu.u32(self.brain + 0x100, vbt)
        ctl = obj({0x58: fn(lambda c: self.brain if self.s[10] else 0)})
        basic = obj({0x7c: fn(lambda c: put(f'C{c.arg(1)};'), 8)})
        self.mech = cpu.alloc(0x10)
        self.vehicle = obj({0x14c: fn(lambda c: ctl if self.s[9] else 0), 0x78: fn(lambda c: put('X;')),
                            0x58: fn(lambda c: self.mech if self.s[12] else 0),
                            0x50: fn(lambda c: basic if self.s[13] else 0)})
        self.dlg = cpu.alloc(0x200)

    def send(self, c):
        h, msg = c.arg(0), c.arg(1)
        assert h == 0x1233
        if msg == 0x147:
            self.log += 'S;'
            return self.s[16] & 0xffffffff
        self.log += f'G{ctypes.c_int32(c.arg(2)).value};'
        return self.s[17] & 0xffffffff

    def run(self, s):
        cpu, d = self.cpu, self.dlg
        self.s, self.log = s, ''
        cpu.write(d, b'\0' * 0x200)
        cpu.u32(d + 0x88, 0x1233)
        cpu.u32(d + 0xac, s[1]), cpu.u32(d + 0xbc, s[2]), cpu.u32(d + 0xc0, s[3]), cpu.u32(d + 0xc8, s[4])
        cpu.u32(d + 0xe0, s[5])
        cpu.write(d + 0x129, bytes([s[6]]))
        cpu.u32(d + 0xe4, self.vehicle if s[7] else 0)
        cpu.write(self.vehicle + 8, bytes([s[8]]))
        cpu.write(self.params + 0x3c, struct.pack('<i', s[18]))
        cpu.write(self.params + 0x40, struct.pack('<i', s[19]))
        cpu.write(self.params + 0x54, struct.pack('<i', s[20]))
        cpu.write(self.params + 0x80, struct.pack('<i', s[21]))
        args = (s[15], 0, 0) if s[0] == 0 else ()
        cpu.call(HANDLERS[s[0]], ecx=d, stack_args=args)
        words = [struct.unpack('<i', cpu.read(self.params + o, 4))[0] for o in (0x3c, 0x40, 0x54, 0x80)]
        return words + [cpu.read(d + 0x129, 1)[0]], self.log


def main():
    lib = load_pm_probe()
    exe = Exe()
    rng = random.Random(9)
    bad = runs = 0
    seen = set()
    for _ in range(4000):
        h = rng.randrange(9)
        w = lambda: rng.choice([0, 1, 2, 5, -1, rng.randrange(-1000, 1000)])
        s = [h, rng.choice([0, 0, 1]), w(), rng.choice([0, 1, 7]), w(), w(), rng.choice([0, 1])]
        s += [int(rng.random() < 0.9)] + [int(rng.random() < 0.8) for _ in range(7)]
        s += [rng.choice([0, 1, 2]), rng.choice([-1, 0, 1, 3]), w()] + [w() for _ in range(4)]
        if h in (2, 4, 5, 8) and not s[7]:
            continue  # FIX (A): the exe calls through no vehicle
        if rng.random() < 0.3:  # the settings already as the dialog has them
            s[18], s[20], s[21] = s[4], s[2], s[5]
        want = exe.run(s)
        out = (ctypes.c_int32 * 5)()
        buf = ctypes.create_string_buffer(512)
        n = lib.probe_pm_ai_tester((ctypes.c_int32 * 22)(*s), out, buf, len(buf))
        got = (list(out), buf.raw[:n].decode())
        runs += 1
        seen.add((h, want[1]))
        if got != want:
            bad += 1
            if bad <= 3:
                print(f's {s}\n exe:  {want}\n port: {got}')
    print(f'{runs} handler runs ({len(seen)} distinct handler and call paths), {bad} differ')
    print('OK' if not bad else 'FAIL')
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
