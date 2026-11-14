"""Pirate's Moon's Game menu against its exe (MW3's in test_debug_start.py; pm-tests.md S9b): Start 0x485ac0, the
campaign by mission number (0x485f40.. and 0x486300 / 0x486330: 0x47ad20(n), no mechs added, only the first checked),
the lab levels 0x486130 / 0x486190 / 0x4861f0 (mech 5, "DefaultPlayer"), the network and Instant Action starts
0x486290 / 0x4862b0, the install check passing and failing; then the install check 0x482b40 itself.

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_debug_start.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402

CAMPAIGN = {1: 0x485f40, 2: 0x485f80, 3: 0x485fb0, 4: 0x485fe0, 5: 0x486010, 6: 0x486040, 7: 0x486070, 8: 0x4860a0,
            9: 0x4860d0, 10: 0x486100, 11: 0x486300, 12: 0x486330}
LAB = {7: 0x486130, 8: 0x486190, 6: 0x4861f0}
PILOT_VAR, GETFOCUS, MESSAGEBOX = 0x62c40c, 0x5de724, 0x5de730


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.log, self.ok = [], 1
        put = lambda s: self.log.append(s) or 0
        cpu.stub_function(0x482b40, lambda c: put('check') or self.ok)
        cpu.stub_function(0x47ad20, lambda c: put(f'campaign 0 {c.arg(0)}') or self.pilot)
        cpu.stub_function(0x47aba0, lambda c: put(f'create {self.cstr(c.arg(0))}') or self.pilot)
        cpu.stub_function(0x47a560, lambda c: put(f'select {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x4825a0, lambda c: put(f'post {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x5b13e0, lambda c: put('start'))
        cpu.stub_function(0x482620, lambda c: put('lab'))
        cpu.stub_function(0x4822f0, lambda c: put(f'net {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x4822d0, lambda c: put(f'ia {c.arg(0)} {c.arg(1)}'), pop_bytes=8)
        self.game = cpu.alloc(0x100)
        gvt = cpu.alloc(0x60)
        level = cpu.alloc(0x10)
        cpu.stub_function(level, lambda c: put(f'level {c.arg(0)}'), pop_bytes=4)
        slot = cpu.alloc(0x10)
        cpu.stub_function(slot, lambda c: self.game + 0x20 if c.arg(0) == 0 else 0, pop_bytes=4)
        cpu.u32(gvt + 0x4c, level)
        cpu.u32(gvt + 0x30, slot)
        cpu.u32(self.game, gvt)
        self.pilot = cpu.alloc(0x100)
        cpu.u32(self.pilot + 0x6c, self.game)

    def cstr(self, a):
        return self.cpu.read(a, 64).split(b'\0')[0].decode('latin-1')

    def r32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def run(self, addr, ok):
        cpu = self.cpu
        self.ok, self.log = ok, []
        cpu.u32(PILOT_VAR, 0)
        cpu.u32(self.game + 0x24, 0xffffffff)
        cpu.call(addr, ecx=cpu.alloc(0x10))
        # PM has no training: the port's line stays -1
        return ''.join(x + ';' for x in self.log) + f'|pilot {int(self.r32(PILOT_VAR) == self.pilot)}' + \
            f'|training -1|slot {self.r32(self.game + 0x24)}'


def install(hw, opts):
    cpu = Original(exe='pm')
    log, strs = [], {}

    def cs(s):
        if s not in strs:
            a = cpu.alloc(len(s) + 1)
            cpu.write(a, s.encode() + b'\0')
            strs[s] = a
        return strs[s]
    cstr = lambda a: cpu.read(a, 64).split(b'\0')[0].decode()
    cpu.stub_function(0x40b360, lambda c: hw)
    cpu.stub_function(0x5b90d0, lambda c: log.append(f'opts {c.arg(0)}') or (opts & c.arg(0)))
    cpu.stub_function(0x596e50, lambda c: cs(f'm{c.arg(0)}'))
    cpu.stub_import(GETFOCUS, lambda c: log.append('focus') or 0x77)
    cpu.stub_import(MESSAGEBOX, lambda c: log.append(
        f'box {c.arg(0)} {cstr(c.arg(1))} {cstr(c.arg(2))} {c.arg(3)}') or 1, pop_bytes=16)
    cpu.call(0x482b40, ecx=0x62ca98)
    return ''.join(x + ';' for x in log) + '|' + str(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff)


def main():
    p = test_pm_zar.load_pm_probe()
    p.probe_dd_start.argtypes = [ctypes.c_int32] * 4 + [ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_install.argtypes = [ctypes.c_int32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int32]
    exe = Exe()
    buf = ctypes.create_string_buffer(1 << 14)
    cases = [(0, 0, 0x485ac0), (4, 0, 0x486290), (5, 0, 0x4862b0)] + [(6, k, a) for k, a in CAMPAIGN.items()] + \
        [(7, k, a) for k, a in LAB.items()]
    bad = n = 0
    for kind, arg, addr in cases:
        for ok in (1, 0):
            want = exe.run(addr, ok)
            k = p.probe_dd_start(kind, arg, ok, 9, buf, len(buf))
            got = buf.raw[:k].decode()
            n += 1
            if want != got:
                bad += 1
                if bad <= 6:
                    print(f'{addr:#x} ok {ok}:\n  exe  {want}\n  port {got}')
    for hw in (0, 1):
        for opts in (0, 1, 0x100, 0xffff):
            want = install(hw, opts)
            k = p.probe_dd_install(hw, opts, buf, len(buf))
            n += 1
            if want != buf.raw[:k].decode():
                bad += 1
                print(f'install hw {hw} opts {opts:#x}:\n  exe  {want}\n  port {buf.raw[:k].decode()}')
    print(f'{n} cases on PM: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: PM's debug menu starts and install check as its exe")


if __name__ == '__main__':
    main()
