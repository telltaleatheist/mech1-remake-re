"""The vehicle editor's Valve page lists (docs/editor.md, The vehicle editor's Valve page) against the original: the
valve list 0x4bb300 with its step 0x4bb2e0 (each valve of [0x60b504] by name) and the region list 0x4bb620 with
0x4bb5f0 (each region of [0x60b3f4] by its vt+0x1c name), into sorted combo boxes (CBS_SORT modelled as the port's
DlgCombo sorts), against game_editor_vehicles' valves_fill / regions_fill. With MW3_EXE=pm, Pirate's Moon's
0x4c17f0 / 0x4c17d0 and 0x4c1af0 / 0x4c1ac0. A valve without a name is the port's FIX (A) (the exe hands the combo
null): not run on the exe.

    cmake --build build --target mw3probe && python3 tests/differential/test_vehicle_lists.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
if os.environ.get('MW3_EXE') == 'pm':
    os.environ.setdefault('MW3_PM_MOVED', '1')  # PM's own addresses below
from original import Original  # noqa: E402

PM = os.environ.get('MW3_EXE') == 'pm'
if PM:
    VALVES_FILL, REGIONS_FILL, VALVES, REGIONS, REGION_NAME = 0x4c17f0, 0x4c1af0, 0x62fbe4, 0x62faac, 0x4d0f50
    VALVE_HWND, REGION_HWND, SEND_MESSAGE = 0x3d8, 0x418, 0x5de73c
else:
    VALVES_FILL, REGIONS_FILL, VALVES, REGIONS, REGION_NAME = 0x4bb300, 0x4bb620, 0x60b504, 0x60b3f4, 0x47a920
    VALVE_HWND, REGION_HWND, SEND_MESSAGE = 0x398, 0x3d8, 0x5bb734
WORDS = ['gate', 'Gate', 'bridge', 'door_a', 'door_b', 'Alpha', 'alpha', 'zz', 'valve_17', 'a', 'B', 'mid', '']


def sorted_add(items, s):
    at = 0
    while at < len(items) and items[at].lower() <= s.lower():
        at += 1
    items.insert(at, s)


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm') if PM else Original()
        self.lists = {}
        cpu.stub_import(SEND_MESSAGE, self.send, pop_bytes=16)
        self.rvt = cpu.alloc(0x40)
        cpu.u32(self.rvt + 0x1c, REGION_NAME)

    def send(self, c):
        h, msg = c.arg(0), c.arg(1)
        if msg == 0x14b:
            self.lists[h] = []
        elif msg == 0x143:
            sorted_add(self.lists.setdefault(h, []), self.cpu.read(c.arg(3), 64).split(b'\0')[0].decode())
        return 0

    def cs(self, s):
        a = self.cpu.alloc(len(s) + 1)
        self.cpu.write(a, s.encode() + b'\0')
        return a

    def std_list(self, at, values):
        cpu = self.cpu
        head = cpu.alloc(12)
        nodes = [cpu.alloc(12) for _ in values]
        ring = [head] + nodes
        for k, n in enumerate(ring):
            cpu.u32(n, ring[(k + 1) % len(ring)])
            cpu.u32(n + 4, ring[k - 1])
        for n, v in zip(nodes, values):
            cpu.u32(n + 8, v)
        cpu.u32(at, head)

    def run(self, valves, regions):
        cpu = self.cpu
        vs = []
        for n in valves:
            v = cpu.alloc(0x10)
            cpu.u32(v, self.cs(n))
            vs.append(v)
        rs = []
        for n in regions:
            r = cpu.alloc(0x40)
            cpu.u32(r, self.rvt)
            cpu.u32(r + 0x1c, self.cs(n))
            rs.append(r)
        self.std_list(VALVES, vs)
        self.std_list(REGIONS, rs)
        e = cpu.alloc(0x500)
        cpu.u32(e + VALVE_HWND, 0x1176)
        cpu.u32(e + REGION_HWND, 0x1177)
        self.lists = {}
        cpu.call(VALVES_FILL, ecx=e)
        cpu.call(REGIONS_FILL, ecx=e)
        return ''.join(t + '|' for t in self.lists[0x1176]) + ';' + ''.join(t + '|' for t in self.lists[0x1177])


def main():
    if PM:
        from test_pm_zar import load_pm_probe
        lib = load_pm_probe()
    else:
        from calltrace import load_probe
        lib = load_probe()
    lib.probe_vehicle_lists.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32]
    buf = ctypes.create_string_buffer(1 << 14)
    exe = Exe()
    rng = random.Random(5)
    bad = done = 0
    runs = 500
    for run in range(runs):
        valves = [rng.choice(WORDS) for _ in range(rng.randrange(0, 9))]
        regions = [rng.choice(WORDS) for _ in range(rng.randrange(0, 9))]
        want = exe.run(valves, regions)
        if valves == [''] or regions == ['']:
            continue  # one empty name joins to the empty string, which the probe reads as none
        n = lib.probe_vehicle_lists('|'.join(valves).encode(), '|'.join(regions).encode(), buf, len(buf))
        got = buf.raw[:n].decode()
        done += 1
        if got != want:
            bad += 1
            if bad <= 3:
                print(f'run {run}: valves {valves} regions {regions}\n exe:  {want}\n port: {got}')
    print(f'{done} fills, {bad} differ')
    print('OK' if not bad else 'FAIL')
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
