"""Pirate's Moon's view pick 0x41dd40 (MW3's 0x41cee0 in PM's layout; Engine::view_pick, docs/editor.md) against PM's
exe: the mech the view ([0x61874c] +0x64) is on, or another; the pilot HUD 0x430e30 (on the mech's +0x6a8) answered
and logged on both sides, or none; out given or not.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_view_pick.py [runs]
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
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

PICK, PILOT_HUD, VIEW = 0x41dd40, 0x430e30, 0x61874c


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        cpu.stub_function(PILOT_HUD, self.pilot_hud)
        self.base = cpu.alloc(0x60000)
        self.view = cpu.alloc(0x80)

    def pilot_hud(self, c):
        self.asked = c.ecx
        return self.hud

    def pick(self, mech, viewed, data, hud, words, dst):
        cpu = self.cpu
        cpu.u32(VIEW, self.view), cpu.u32(self.view + 0x64, viewed)
        cpu.u32(mech + 0x6a8, data)
        for k, w in enumerate(words if hud else ()):
            cpu.u32(hud + 0x1d68 + 4 * k, w)
        if dst:
            cpu.write(dst, bytes(12))
        self.hud, self.asked = hud, 0xffffffff
        cpu.call(PICK, ecx=mech, stack_args=[dst])
        got = cpu._uc.reg_read(UC_X86_REG_EAX)
        return [got, self.asked] + (list(struct.unpack('<3I', cpu.read(dst, 12))) if dst else [0, 0, 0])


def main():
    p = test_pm_zar.load_pm_probe()
    U = ctypes.POINTER(ctypes.c_uint32)
    p.probe_pm_view_pick.argtypes = [ctypes.c_uint32] * 4 + [U, ctypes.c_uint32, U]
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    rng = random.Random(0x41dd40)
    out = (ctypes.c_uint32 * 8)()
    stats = dict.fromkeys(('picks', 'viewed', 'copied', 'addresses'), 0)
    for run in range(runs):
        b = emu.base
        mech = b + 0x1000 * rng.randrange(4)
        viewed = rng.choice((mech, mech, b, 0))
        data, hud = b + 0x10000 + 0x100 * rng.randrange(8), rng.choice((b + 0x20000, b + 0x30000, 0))
        words = [rng.getrandbits(32) for _ in range(3)]
        dst = rng.choice((0, b + 0x50000))
        want = emu.pick(mech, viewed, data, hud, words, dst)
        k = p.probe_pm_view_pick(mech, viewed, data, hud, (ctypes.c_uint32 * 3)(*words), dst, out)
        got = list(out[:k])
        if want != got:
            print(f'pick {run} mech {mech:#x} viewed {viewed:#x} hud {hud:#x} dst {dst:#x}:\n  exe  {want}\n  port {got}')
            raise SystemExit(1)
        stats['picks'] += 1
        stats['viewed'] += viewed == mech
        stats['copied'] += bool(dst and want[0])
        stats['addresses'] += bool(not dst and want[0])
    print(f'pm view pick: {runs} runs identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
