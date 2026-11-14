"""The debug menu's Game half (mech3/debug_dialogs, DebugStartHooks; docs/editor.md, The debug menu) against
Mech3.exe 1.2: Start 0x483100, the 21 campaign jumps 0x483720..0x483e20 (their mechs through 0x4836e0, the exe's
own), training 0x483e80..0x483f70, the lab levels 0x483fc0 / 0x484040 / 0x4840c0, the network and Instant Action
starts 0x484190 / 0x4841b0, each with the install check passing and failing: every call out in order (the pilot's,
the store's, the app's; stood in), then the pilot set ([0x60861c]), the training mission (+0x40) and the slot's mech
(0x5a2ba0, the exe's own). Then the install check 0x480420 with and without a card on every InstallOptions answer.

    cmake --build build --target mw3probe && python3 tests/differential/test_debug_start.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import iat_slots  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

CAMPAIGN = {40085: 0x483720, 40086: 0x483770, 40087: 0x4837c0, 40088: 0x483810, 40089: 0x483860, 40090: 0x4838b0,
            40091: 0x483910, 40092: 0x483960, 40093: 0x4839b0, 40094: 0x483a10, 40095: 0x483a70, 40096: 0x483ac0,
            40097: 0x483b20, 40098: 0x483b90, 40099: 0x483bf0, 40100: 0x483c50, 40101: 0x483cb0, 40102: 0x483d10,
            40103: 0x483d60, 40104: 0x483dc0, 40114: 0x483e20}
TRAINING = {1: 0x483e80, 2: 0x483ed0, 3: 0x483f20, 4: 0x483f70}
LAB = {7: 0x483fc0, 8: 0x484040, 6: 0x4840c0}
PILOT_VAR = 0x60861c


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        self.strs, self.log = {}, []
        self.ok, self.mech = 1, 0
        put = lambda s: self.log.append(s) or 0
        cpu.stub_function(0x480420, lambda c: put('check') or self.ok)
        cpu.stub_function(0x478a40, lambda c: put(f'campaign {c.ecx} {c.edx}') or self.pilot)
        cpu.stub_function(0x4788b0, lambda c: put(f'create {self.cstr(c.ecx)}') or self.pilot)
        cpu.stub_function(0x478250, lambda c: put(f'select {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x576040, lambda c: self.cs(f'm{c.ecx}'))
        cpu.stub_function(0x479430, lambda c: self.store)
        cpu.stub_function(0x4733b0, lambda c: put(f'add {c.arg(0)} {self.cstr(c.arg(1))}') or 1, pop_bytes=8)
        cpu.stub_function(0x4794a0, lambda c: self.training)
        cpu.stub_function(0x4845f0, lambda c: self.mech)
        cpu.stub_function(0x47fed0, lambda c: put(f'post {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x58fc70, lambda c: put('start'))
        cpu.stub_function(0x47ff60, lambda c: put('lab'))
        cpu.stub_function(0x47fc40, lambda c: put(f'net {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x47fc20, lambda c: put(f'ia {c.arg(0)} {c.arg(1)}'), pop_bytes=8)
        # the store (vt+0x10 add_weapon) and the lab game (vt+0x50 the level, vt+0x34 its slot)
        self.store = cpu.alloc(0x40)
        svt = cpu.alloc(0x40)
        weapon = cpu.alloc(0x10)
        cpu.stub_function(weapon, lambda c: put(f'weapon {c.arg(0)} {c.arg(1)}'), pop_bytes=8)
        cpu.u32(svt + 0x10, weapon)
        cpu.u32(self.store, svt)
        self.game = cpu.alloc(0x100)
        gvt = cpu.alloc(0x60)
        level = cpu.alloc(0x10)
        cpu.stub_function(level, lambda c: put(f'level {c.arg(0)}'), pop_bytes=4)
        slot = cpu.alloc(0x10)
        cpu.stub_function(slot, lambda c: self.game + 0x20 if c.arg(0) == 0 else 0, pop_bytes=4)
        cpu.u32(gvt + 0x50, level)
        cpu.u32(gvt + 0x34, slot)
        cpu.u32(self.game, gvt)
        self.pilot = cpu.alloc(0x100)
        cpu.u32(self.pilot + 0x6c, self.game)
        self.training = cpu.alloc(0x60)

    def cs(self, s):
        if s not in self.strs:
            a = self.cpu.alloc(len(s) + 1)
            self.cpu.write(a, s.encode() + b'\0')
            self.strs[s] = a
        return self.strs[s]

    def cstr(self, a):
        return self.cpu.read(a, 64).split(b'\0')[0].decode('latin-1')

    def r32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def run(self, addr, ok, mech):
        cpu = self.cpu
        self.ok, self.mech, self.log = ok, mech, []
        cpu.u32(PILOT_VAR, 0)
        cpu.u32(self.training + 0x40, 0xffffffff)
        cpu.u32(self.game + 0x24, 0xffffffff)
        cpu.call(addr, ecx=cpu.alloc(0x10))
        return ''.join(x + ';' for x in self.log) + f'|pilot {int(self.r32(PILOT_VAR) == self.pilot)}' + \
            f'|training {self.r32(self.training + 0x40)}|slot {self.r32(self.game + 0x24)}'

    def install(self, hw, opts):
        cpu = Original()
        iat = iat_slots()
        log = []
        strs = {}

        def cs(s):
            if s not in strs:
                a = cpu.alloc(len(s) + 1)
                cpu.write(a, s.encode() + b'\0')
                strs[s] = a
            return strs[s]
        cstr = lambda a: cpu.read(a, 64).split(b'\0')[0].decode()
        cpu.stub_function(0x40d4d0, lambda c: hw)
        cpu.stub_function(0x597d80, lambda c: log.append(f'opts {c.ecx}') or (opts & c.ecx))
        cpu.stub_function(0x576040, lambda c: cs(f'm{c.ecx}'))
        cpu.stub_import(iat['GetFocus'], lambda c: log.append('focus') or 0x77)
        cpu.stub_import(iat['MessageBoxA'], lambda c: log.append(
            f'box {c.arg(0)} {cstr(c.arg(1))} {cstr(c.arg(2))} {c.arg(3)}') or 1, pop_bytes=16)
        cpu.call(0x480420, ecx=0x608c88)
        return ''.join(x + ';' for x in log) + '|' + str(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff)


def main():
    p = load_probe()
    p.probe_dd_start.argtypes = [ctypes.c_int32] * 4 + [ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_install.argtypes = [ctypes.c_int32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int32]
    exe = Exe()
    buf = ctypes.create_string_buffer(1 << 14)
    cases = [(0, 0, 0x483100)] + [(1, k, a) for k, a in CAMPAIGN.items()] + [(2, k, a) for k, a in TRAINING.items()] + \
        [(3, k, a) for k, a in LAB.items()] + [(4, 0, 0x484190), (5, 0, 0x4841b0)]
    bad = n = 0
    for kind, arg, addr in cases:
        for ok in (1, 0):
            for mech in (0, 9, 18):
                want = exe.run(addr, ok, mech)
                k = p.probe_dd_start(kind, arg, ok, mech, buf, len(buf))
                got = buf.raw[:k].decode()
                n += 1
                if want != got:
                    bad += 1
                    if bad <= 6:
                        print(f'{addr:#x} ok {ok} mech {mech}:\n  exe  {want}\n  port {got}')
    for hw in (0, 1, 2):
        for opts in (0, 1, 0x80, 0x100, 0x8000, 0xffff, 0x10000):
            want = exe.install(hw, opts)
            k = p.probe_dd_install(hw, opts, buf, len(buf))
            got = buf.raw[:k].decode()
            n += 1
            if want != got:
                bad += 1
                if bad <= 10:
                    print(f'install hw {hw} opts {opts:#x}:\n  exe  {want}\n  port {got}')
    print(f'{n} cases: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: the debug menu's starts and the install check as the original")


if __name__ == '__main__':
    main()
