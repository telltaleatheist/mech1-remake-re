"""Pirate's Moon's zmath translate 0x539f80 against the port's zmth::translate (tests/probe_pm.cpp): the top's
translation moved by (x, y, z) through its rotation, or set on an identity top. PM sums the words in its own
order, not MW3's 0x520480's (t9: x, z, then y; t11: y, x, then z): at 24 bits about 99.5% of these cases tell the
two apart. At 24 and at 53 bits, bit for bit; inputs made to cancel (the x and z terms nearly opposite).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_translate.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import Original  # noqa: E402

TRANSLATE, TOP_P, FLAG_P = 0x539f80, 0x611220, 0x61121c


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def main():
    p = test_pm_zar.load_pm_probe()
    F = ctypes.c_float
    p.probe_pm_translate.argtypes = [ctypes.POINTER(F), ctypes.c_int, F, F, F, ctypes.c_int, ctypes.POINTER(F),
                                     ctypes.POINTER(ctypes.c_int)]
    cpu = Original(exe='pm')
    cpu.tr = lambda a, size=1: a
    top, slots, flags = cpu.alloc(48), cpu.alloc(8), cpu.alloc(8)
    cpu.u32(slots + 4, top)
    rng = random.Random(TRANSLATE)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    for fpu53 in (0, 1):
        cpu.fpu_control(0x027f if fpu53 else 0x007f)
        for i in range(runs):
            x, y, z = (f32(rng.uniform(-500, 500)) for _ in range(3))
            t = [f32(rng.uniform(-1, 1)) for _ in range(9)] + [f32(rng.uniform(-3000, 3000)) for _ in range(3)]
            if i % 2 and z:  # cancelling: x's and z's terms nearly opposite, y's small
                for c in range(3):
                    t[6 + c] = f32(-x * t[c] / z * (1 + rng.uniform(-1e-6, 1e-6)))
                    t[3 + c] = f32(t[3 + c] * 1e-3)
            identity = 1 if i % 50 == 0 else 0
            cpu.write(top, struct.pack('<12f', *t))
            cpu.u32(flags + 4, identity)
            cpu.u32(TOP_P, slots + 4), cpu.u32(FLAG_P, flags + 4)
            cpu.call(TRANSLATE, stack_args=[bits(x), bits(y), bits(z)])
            want = cpu.read(top, 48) + struct.pack('<i', struct.unpack('<i', cpu.read(flags + 4, 4))[0])
            out, flag = (F * 12)(), ctypes.c_int()
            p.probe_pm_translate((F * 12)(*t), identity, x, y, z, fpu53, out, ctypes.byref(flag))
            got = bytes(out) + struct.pack('<i', flag.value)
            if want != got:
                print(f'{"53" if fpu53 else "24"} bits, run {i}: ({x}, {y}, {z}) identity {identity}\n'
                      f'  exe  {struct.unpack("<12f", want[:48])[9:]} flag {want[48]}\n'
                      f'  port {list(out)[9:]} flag {flag.value}')
                raise SystemExit(1)
    print(f'pm translate: {runs} runs at 24 and {runs} at 53 bits identical')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
