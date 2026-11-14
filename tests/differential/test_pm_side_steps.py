"""Pirate's Moon's guard state's side state's steps (0x4dbe00) against the original x86 (docs/ai.md, PM's guard
state): a jump step (0x4dadc0) of the value given, linked on to a node step (0x4dd800) at the place given,
linked on to the suspended manoeuvre's step (0x4d4d60), through the steps' own link slots (vt+0x30: 0x4d4650,
0x4d4080). Never made in play; ported for preservation.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_side_steps.py [runs]

Each run: a value and a place (random words, specials among them). Compared: the jump's value and its next,
the node's place (bits), its links (one, the third step), each step's kind (by its vtable) and flags.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

MAKE, NULL_STEP_INIT = 0x4dbe00, 0x410ab0
NEW, DELETE = 0x5ce8c6, 0x5ce8c0
VT_JUMP, VT_NODE, VT_AFTER = 0x5eed00, 0x5eef64, 0x5ee070


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        cpu.stub_function(NEW, lambda c: c.alloc(max(1, c.arg(0))))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.call(NULL_STEP_INIT)  # the null step's start-up constructor (the links' "none")
        self.place = cpu.alloc(16)

    def run(self, value, place):
        cpu = self.cpu
        obj = cpu.alloc(0x54)
        cpu.write(self.place, struct.pack('<3I', *place))
        cpu.call(MAKE, ecx=obj, stack_args=(value, self.place))
        w = struct.unpack('<21I', cpu.read(obj, 0x54))
        jump, node, after = obj + 4, obj + 0x18, obj + 0x44
        b, e = w[0x38 // 4], w[0x3c // 4]
        links = list(struct.unpack(f'<{(e - b) // 4}I', cpu.read(b, e - b))) if b else []
        return [w[0x14 // 4], int(w[0x10 // 4] == node), *w[0x28 // 4:0x34 // 4], len(links),
                int(links[:1] == [after]),
                int(w[1] == VT_JUMP and w[0x18 // 4] == VT_NODE and w[0x44 // 4] == VT_AFTER),
                (w[2] | w[0x1c // 4] | w[0x48 // 4]) & 0xff]


def main():
    probe = load_probe()
    probe.probe_pm_side_steps.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32),
                                          ctypes.POINTER(ctypes.c_uint32)]
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    r = random.Random(MAKE)
    words = [0, 0x3f800000, 0x80000000, 0x7f800000, 0x7fc00000, 0xffffffff]
    for i in range(runs):
        value = r.choice(words + [r.getrandbits(32)] * 3)
        place = [r.choice(words + [r.getrandbits(32)] * 3) for _ in range(3)]
        want = emu.run(value, place)
        out = (ctypes.c_uint32 * 9)()
        probe.probe_pm_side_steps(value, (ctypes.c_uint32 * 3)(*place), out)
        got = list(out)
        if want != got:
            print(f'run {i}: value {value:#x} place {[hex(p) for p in place]}\n  exe  {want}\n  port {got}')
            raise SystemExit(1)
    print(f'pm side steps: {runs} runs identical (jump -> node -> the suspended manoeuvre\'s step)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
