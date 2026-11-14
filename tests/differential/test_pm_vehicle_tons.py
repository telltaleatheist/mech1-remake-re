"""Pirate's Moon's vehicle tons (vt+0x8c of the field base, stealth object and basic vehicle, 0x459cd0) against
the original x86 (docs/vehicles.md, The motion block): the controls' (+0xe8) motion mass at +0x9c4.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_vehicle_tons.py [runs]

Each run: a mass (ordinary, tiny, huge, zero, negative, infinite, NaN), its float's bits compared. Without controls
the exe reads through null ([0x260]) and faults; the port answers 0 (FIX (A), docs/fixes.md).
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

TONS = 0x459cd0


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        self.veh, self.ctl, self.out = cpu.alloc(0x200), cpu.alloc(0xa94), cpu.alloc(8)
        # call 0x459cd0 with the vehicle in ecx, keep st0 as a float
        self.thunk = cpu.code(b'\xb8' + struct.pack('<I', TONS) + b'\xff\xd0\xd9\x1d' + struct.pack('<I', self.out) +
                              b'\xc3')

    def run(self, has, mass_bits):
        cpu = self.cpu
        cpu.u32(self.veh + 0xe8, self.ctl if has else 0)
        cpu.u32(self.ctl + 0x9c4, mass_bits)
        try:
            cpu.call(self.thunk, ecx=self.veh)
        except Exception as e:  # the null read
            return None, str(e)
        return struct.unpack('<I', cpu.read(self.out, 4))[0], None


def main():
    probe = load_probe()
    probe.probe_pm_vehicle_tons.argtypes = [ctypes.c_int, ctypes.c_float]
    probe.probe_pm_vehicle_tons.restype = ctypes.c_float
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    r = random.Random(TONS)
    special = [0.0, -0.0, 1.0, 65.0, 100.0, -3.0, 1e-30, 3e38, float('inf'), float('-inf')]
    for i in range(runs):
        m = special[i] if i < len(special) else r.choice([r.uniform(0, 200), r.uniform(-1e6, 1e6),
                                                          struct.unpack('<f', struct.pack('<I', r.getrandbits(32)))[0]])
        mb = bits(m)
        if m != m:  # NaN: ctypes passes a float, keep one quiet NaN on both sides
            mb = 0x7fc00000
            m = struct.unpack('<f', struct.pack('<I', mb))[0]
        want, _ = emu.run(1, mb)
        got = bits(probe.probe_pm_vehicle_tons(1, m))
        if want != got:
            print(f'run {i}: mass {mb:#010x}: exe {want:#010x} port {got:#010x}')
            raise SystemExit(1)
    want, err = emu.run(0, 0)
    got = probe.probe_pm_vehicle_tons(0, 5.0)
    if want is not None or got != 0.0:
        print(f'no controls: exe {want} ({err}), port {got}: expected the exe to fault and the port to answer 0')
        raise SystemExit(1)
    print(f'pm vehicle tons: {runs} masses identical; no controls: the exe faults ({err.split(":")[0]}), '
          f'the port answers 0 (FIX (A))')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
