"""Pirate's Moon's vehicle editor Apply, 0x4c1cb0 (docs/editor.md, The vehicle editor; game_editor_vehicles store)
against Mech3.exe PM 1.0: the record built from the settings shown (0x4e9e10 the exe's, its vlist pass 0x4e95f0 stood
in: no valves), compared with the cache's (the names 0x4a06c0, the sections 0x4c21d0..0x4c25a0 and the flag bits, all
the exe's; the vlists 0x4e9580 stood in as equal) and put there when they differ (0x4f0810 into the editor's made
record, which the test reads, then 0x4f1a40 logged); the five
"as the brain has it" words; then the same compare with the brain's, which takes the record (0x4e9c80 stood in) and
gets the team's masks. Random records, the cache's and the brain's often the built one with a word, a bit or a name
changed (NaNs and signed zeros among them), at the x87's 24 bits.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_apply.py [runs]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_X87', '24')  # the editor runs with the game up (CLAUDE.md)
os.environ.setdefault('MW3_PM_MOVED', '1')
from original import Original  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

APPLY, SEND_MESSAGE = 0x4c1cb0, 0x5de73c
COPY, VLIST_FINISH, FINISH, READ_NAMED, VLIST_SAME = 0x4e9be0, 0x4e95f0, 0x4e9e10, 0x4f19e0, 0x4e9580
MADE_SET, EDIT, ASSIGN, DTOR = 0x4f0810, 0x4f1a40, 0x4e9c80, 0x4e9cf0
RECVT = 0x5efe3c
NAMES = [None, 'Dom', 'Alan']
WORDS = range(4, 0xf0, 4)


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.log = []
        cpu.stub_import(SEND_MESSAGE, lambda c: self.item if c.arg(1) == 0x150 else 0, pop_bytes=16)
        cpu.stub_function(COPY, self.copy, pop_bytes=4)
        cpu.stub_function(VLIST_FINISH, lambda c: 0)
        cpu.stub_function(READ_NAMED, lambda c: self.kept, pop_bytes=4)
        cpu.stub_function(VLIST_SAME, lambda c: 1, pop_bytes=4)
        cpu.stub_function(MADE_SET, self.made_set, pop_bytes=4)
        cpu.stub_function(EDIT, lambda c: self.log.append('edit') or 1, pop_bytes=8)
        cpu.stub_function(ASSIGN, self.assign, pop_bytes=4)
        cpu.stub_function(DTOR, lambda c: 0)
        self.strs = {}
        for n in NAMES[1:] + ['apply_unit']:
            a = cpu.alloc(16)
            cpu.write(a, n.encode() + b'\0')
            self.strs[n] = a
        self.dlg, self.kept, self.brain, self.scratch = cpu.alloc(0xa00), cpu.alloc(0x104), cpu.alloc(0x104), cpu.alloc(0x104)
        self.item, obj = cpu.alloc(0x100), cpu.alloc(0x10)
        ivt, ovt = cpu.alloc(0x150), cpu.alloc(0x90)
        cpu.u32(ivt + 0x14c, cpu.code(b'\xb8' + struct.pack('<I', obj) + b'\xc3'))
        cpu.u32(ovt + 0x88, cpu.code(b'\xb8' + struct.pack('<I', self.brain) + b'\xc3'))
        cpu.u32(self.item, ivt)
        cpu.u32(obj, ovt)

    def u32r(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def copy_rec(self, dst, src):
        self.cpu.write(dst + 4, self.cpu.read(src + 4, 0xf8))  # the words, the vlist and the two names (shallow)

    def copy(self, c):
        self.copy_rec(c.ecx, c.arg(0))
        self.cpu.u32(c.ecx, RECVT)
        return c.ecx

    def made_set(self, c):
        self.log.append('made')
        self.copy_rec(c.ecx, c.arg(0))
        return c.ecx

    def assign(self, c):
        self.log.append('brain')
        self.copy_rec(c.ecx, c.arg(0))
        return c.ecx

    def put_rec(self, at, raw, pilot, sound):
        self.cpu.write(at, b'\0' * 0x104)
        self.cpu.u32(at, RECVT)
        self.cpu.write(at + 4, raw[4:0xf0])
        self.cpu.u32(at + 0xf4, self.strs[NAMES[pilot]] if pilot else 0)
        self.cpu.u32(at + 0xf8, self.strs[NAMES[sound]] if sound else 0)

    def finished(self, raw):
        """The record built from `raw` (0x4e9e10 on a copy)."""
        self.put_rec(self.scratch, raw, 0, 0)
        self.cpu.call(FINISH, ecx=self.scratch)
        return bytes(4) + self.cpu.read(self.scratch + 4, 0xec)

    def code(self, p):
        if not p:
            return 0
        s = self.cpu.read(p, 8).split(b'\0')[0].decode()
        return {'Dom': 1, 'Alan': 2}.get(s, 3)

    def run(self, recs, names, keep):
        cpu = self.cpu
        self.put_rec(self.dlg + 0x64, recs[0], names[0], names[1])
        self.put_rec(self.kept, recs[1], names[2], names[3])
        self.put_rec(self.brain, recs[2], names[4], names[5])
        cpu.write(self.dlg + 0x9b4, struct.pack('<5i', *keep))
        cpu.u32(self.dlg + 0x180, 0x777)
        cpu.write(self.item + 4, struct.pack('<I', self.strs['apply_unit']) + b'\0' * 0xc4)
        self.log = []
        cpu.call(APPLY, ecx=self.dlg, stack_args=(0,))
        kept = bytes(4) + cpu.read(self.kept + 4, 0xec)
        brain = bytes(4) + cpu.read(self.brain + 4, 0xec)
        masks = list(struct.unpack('<3I', cpu.read(self.item + 0xc0, 12)))
        tail = masks + [int('brain' in self.log)] + list(struct.unpack('<5I', cpu.read(self.dlg + 0x9b4, 20)))
        tail += [self.code(self.u32r(self.kept + 0xf4)) | self.code(self.u32r(self.kept + 0xf8)) << 4,
                 self.code(self.u32r(self.brain + 0xf4)) | self.code(self.u32r(self.brain + 0xf8)) << 4]
        return kept, brain, tail


def rand_word(rng):
    k = rng.random()
    if k < 0.08:
        return struct.pack('<I', rng.choice([0, 1, 2, 0xffffffff, 0x80000000, 0x7fc00000]))
    if k < 0.2:
        return struct.pack('<i', rng.randrange(-5, 40))
    return struct.pack('<f', rng.uniform(-300, 300))


def rand_rec(rng):
    raw = bytearray(4)
    for _ in WORDS:
        raw += rand_word(rng)
    raw[0x18:0x1c] = struct.pack('<i', rng.choice([0, 1, 2, 5, 37]))  # the team, for the masks
    return raw


def changed(rng, base):
    """The built record with a word, a bit or nothing changed, or a word the x87 calls equal and the bits don't."""
    r = bytearray(base)
    k = rng.random()
    if k < 0.25:
        return r
    if k < 0.4:  # equal for the x87, not as bits: a NaN, or 0 for -0
        off = rng.choice(WORDS)
        r[off:off + 4] = struct.pack('<I', 0x7fc00000) if rng.random() < 0.6 else struct.pack('<I', 0x80000000)
        return r
    if k < 0.65:
        off = rng.choice(WORDS)
        r[off:off + 4] = rand_word(rng) if rng.random() < 0.7 else struct.pack('<f', -struct.unpack('<f', r[off:off + 4])[0])
    else:
        off = rng.choice([0xec, 0xed, 0xee, 0xef])
        r[off] ^= 1 << rng.randrange(8)
    return r


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    lib = load_pm_probe()
    exe = Exe()
    rng = random.Random(31)
    bad = edits = brains = 0
    for run in range(runs):
        shown = rand_rec(rng)
        built = exe.finished(shown)
        built = bytearray(built)
        built[0x18:0x1c] = shown[0x18:0x1c]
        kept = changed(rng, built) if rng.random() < 0.85 else rand_rec(rng)
        brain = changed(rng, built) if rng.random() < 0.85 else rand_rec(rng)
        n0 = rng.choice([0, 1, 1, 2])
        n1 = rng.choice([0, 1, 1, 2])
        names = [n0, n1, n0 if rng.random() < 0.8 else rng.randrange(3), n1 if rng.random() < 0.8 else rng.randrange(3),
                 n0 if rng.random() < 0.8 else rng.randrange(3), n1 if rng.random() < 0.8 else rng.randrange(3)]
        keep = [2] * 5 if rng.random() < 0.5 else [rng.choice([0, 1, 2, 2, 3]) for _ in range(5)]
        want = exe.run((shown, kept, brain), names, keep)
        edits += 'made' in exe.log
        brains += 'brain' in exe.log
        recs = (ctypes.c_uint8 * 0x2d0)(*(bytes(shown[:0xf0]) + bytes(kept[:0xf0]) + bytes(brain[:0xf0])))
        out = (ctypes.c_uint8 * 0x1e0)()
        tail = (ctypes.c_uint32 * 11)()
        lib.probe_pm_apply(recs, (ctypes.c_int32 * 6)(*names), (ctypes.c_int32 * 5)(*keep), out, tail)
        got = (bytes(out[0:0xf0]), bytes(out[0xf0:0x1e0]), list(tail))
        if got != want:
            bad += 1
            if bad <= 3:
                print(f'run {run}: names {names} keep {keep} log {exe.log}')
                for what, a, b in (('cache', want[0], got[0]), ('brain', want[1], got[1])):
                    for off in range(4, 0xf0, 4):
                        if a[off:off + 4] != b[off:off + 4]:
                            print(f'  {what} +{off:#x}: exe {a[off:off + 4].hex()} port {b[off:off + 4].hex()}')
                if want[2] != got[2]:
                    print(f'  tail exe {want[2]}\n       port {got[2]}')
    print(f'{runs} applies ({edits} put in the cache, {brains} on the brain), {bad} differ')
    print('OK' if not bad else 'FAIL')
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
