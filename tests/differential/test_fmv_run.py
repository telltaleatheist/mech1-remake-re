"""A movie action's run to the end against the original x86 (docs/fmv.md, the actions' slots): 0x493120, which
nothing calls, on a fake action (its step, start and end recorded stubs) and a scripted clock (0x575d30).

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_fmv_run.py [runs]

Each run: clock readings (a start, then rising, some past 2^24 ms where floats step coarsely, some wrapping
back), and the steps the action answers true before its false. Compared: the start's time and every step's
time (the doubles' bits), and the end called once. At 24 bits (play) and with MW3_PRECISION=53.
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

U, I = ctypes.c_uint32, ctypes.c_int32
END = struct.unpack('<2I', struct.pack('<d', -1234.5))


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision (MW3_PRECISION=53: the reference runtime's)
        self.obj, vt = cpu.alloc(8), cpu.alloc(0x10)
        cpu.u32(self.obj, vt)
        for off, fn, pop in ((4, self.step, 8), (8, self.start, 8), (0xc, self.end, 0)):
            a = cpu.code(b'\xc3')
            cpu.stub_function(a, fn, pop)
            cpu.u32(vt + off, a)
        cpu.stub_function(0x575d30, self.clock, 0)

    def clock(self, c):
        v = self.ms[min(self.at, len(self.ms) - 1)]
        self.at += 1
        return v

    def step(self, c):
        self.seen += [c.arg(0), c.arg(1)]
        self.steps -= 1
        return 1 if self.steps >= 0 else 0

    def start(self, c):
        self.seen += [c.arg(0), c.arg(1)]
        return 0

    def end(self, c):
        self.seen += list(END)
        return 0

    def run(self, ms, steps):
        self.ms, self.at, self.steps, self.seen = ms, 0, steps, []
        self.cpu.call(0x493120, ecx=self.obj)
        return [len(self.seen) // 2] + self.seen


def main():
    probe = load_probe()
    probe.probe_fmv_run.argtypes = [ctypes.POINTER(U), I, I, ctypes.POINTER(U)]
    probe.probe_fmv_run.restype = I
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    r = random.Random(0x493120)
    steps_seen = 0
    for i in range(runs):
        t = r.choice([r.randrange(0, 1 << 20), r.randrange(1 << 24, 1 << 26), r.randrange(0xfff00000, 1 << 32), 0])
        ms = [t]
        for _ in range(r.randint(1, 30)):
            t = (t + r.choice([0, 1, 16, 17, 33, r.randrange(0, 5000)])) & 0xffffffff
            ms.append(t)
        steps = r.randint(0, 25)
        want = emu.run(ms, steps)
        buf = (U * 256)()
        n = probe.probe_fmv_run((U * len(ms))(*ms), len(ms), steps, buf)
        got = list(buf[:n])
        if want != got:
            print(f'run {i}: clock {ms[:6]}.. steps {steps}\n  exe  {want[:12]}\n  port {got[:12]}')
            raise SystemExit(1)
        steps_seen += want[0] - 2
    print(f'fmv run to end: {runs} runs identical ({steps_seen} steps timed)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
