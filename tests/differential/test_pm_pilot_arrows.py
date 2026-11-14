"""Pirate's Moon's pilot screen games list arrows (vt+0x90 0x491340 up, vt+0x94 0x4913e0 down) against the
original x86 (docs/menus.md, The pilot screen): outside Training the list's row step (0x48be90, stubbed:
its argument); in Training the chosen pilot's training game (0x47b740 with 0: the pilot's +0x6c if its type,
vt+0x28, is 4) has its mission (vt+0x48, 0x437c50: +0x3c) stepped and held to 1..4, and the lists filled again
(0x4916c0, stubbed: called).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_pilot_arrows.py

Every case: modes 0..5, missions -3..8, both arrows, a pilot whose game is training, one on a campaign, none.
The port (tests/probe_pm_pilot_arrows.cpp) runs a real screen without a dialog: its list's top from row 1 of
3 (the step), the mission, and the training cycles listed (the refill). Without a pilot whose game is training,
Training's arrows read through null in the exe (it faults); the port does nothing (FIX (A), docs/fixes.md).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

UP, DOWN, STEP, FILL, MISSION = 0x491340, 0x4913e0, 0x48be90, 0x4916c0, 0x437c50
TRAINING = 4


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        self.list, self.screen, self.pilot = cpu.alloc(0x200), cpu.alloc(0x13000), cpu.alloc(0x100)
        self.game, vt = cpu.alloc(0x100), cpu.alloc(0x100)
        cpu.u32(self.list + 8, self.screen)
        cpu.u32(self.game, vt)
        a = cpu.code(b'\xc3')
        cpu.stub_function(a, lambda c: self.type, 0)
        cpu.u32(vt + 0x28, a)
        cpu.u32(vt + 0x48, MISSION)
        cpu.stub_function(STEP, self.step, 4)
        cpu.stub_function(FILL, self.fill, 0)

    def step(self, c):
        assert c.ecx == self.list
        self.seen.append(('step', struct.unpack('<i', struct.pack('<I', c.arg(0)))[0]))
        return 0

    def fill(self, c):
        assert c.ecx == self.screen
        self.seen.append(('fill',))
        return 0

    def run(self, mode, pilot, mission, down):
        cpu, self.seen = self.cpu, []
        self.type = TRAINING if pilot == 1 else 1
        cpu.u32(self.screen + 0x12934, mode)
        cpu.u32(self.screen + 0x12938, self.pilot if pilot else 0)
        cpu.u32(self.pilot + 0x6c, self.game)
        cpu.u32(self.game + 0x3c, mission & 0xffffffff)
        try:
            cpu.call(DOWN if down else UP, ecx=self.list)
        except Exception:  # noqa: BLE001 the null read
            return 'fault'
        m = struct.unpack('<i', cpu.read(self.game + 0x3c, 4))[0]
        return self.seen, m


def port(probe, mode, pilot, mission, down):
    out = (ctypes.c_int32 * 3)()
    probe.probe_pm_pilot_arrow(mode, pilot, mission, down, out)
    top, m, listed = out
    seen = []
    if top != 1:
        seen.append(('step', top - 1))
    if listed:
        seen.append(('fill',))
    return seen, m


def main():
    probe = load_probe()
    probe.probe_pm_pilot_arrow.argtypes = [ctypes.c_int32] * 4 + [ctypes.POINTER(ctypes.c_int32)]
    emu = Emulated()
    cases = faults = fills = 0
    for mode in range(6):
        for pilot in (0, 1, 2):
            for mission in range(-3, 9):
                for down in (0, 1):
                    want = emu.run(mode, pilot, mission, down)
                    got = port(probe, mode, pilot, mission, down)
                    if want == 'fault':
                        faults += 1
                        want = ([], mission)  # FIX (A): nothing
                    if want != got:
                        print(f'mode {mode} pilot {pilot} mission {mission} {"down" if down else "up"}:\n'
                              f'  exe  {want}\n  port {got}')
                        raise SystemExit(1)
                    cases += 1
                    fills += ('fill',) in got[0]
    print(f'pm pilot arrows: {cases} cases identical ({fills} training steps); {faults} where the exe reads '
          f'through null, the port doing nothing (FIX (A))')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
