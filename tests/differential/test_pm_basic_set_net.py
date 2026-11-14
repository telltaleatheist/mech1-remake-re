"""The basic vehicle's controls' set-net on Pirate's Moon's exe (IControls vt+0x04: the thunk 0x4b2300 into
0x4b1ec0; MW3's 0x4aea00 into 0x4ae5e0), ported for preservation (BasicControls::set_net, not wired): the controls'
+0x38 any of 0..5, an owner none, without or with its +4, a net none, a version-105 one (its vt+0x38 says so, its
vt+0x18 its number) or another; compared, the result, the controls' net number (PM's +0xa1c) after and whether the
base's init (0x4b0230, MW3's 0x4ac8a0) ran. With no net where the exe would ask it (FIX (A)), the port's 0 alone.

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_basic_set_net.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402

THUNK, BASE_INIT = 0x4b2300, 0x4b0230
ICTL = 0xa74  # the IControls virtual base in PM's basic controls (MW3: +0xa5c)


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.inits = 0
        cpu.stub_function(BASE_INIT, self.init)
        # A net's vt+0x38 (the version-105 cast: itself, else 0) and vt+0x18 (its +0x18), as code.
        self.yes = cpu.code(bytes.fromhex('89c8c3'))           # mov eax, ecx; ret
        self.no = cpu.code(bytes.fromhex('31c0c3'))            # xor eax, eax; ret
        self.x18 = cpu.code(bytes.fromhex('8b4118c3'))         # mov eax, [ecx+0x18]; ret

    def init(self, c):
        self.inits += 1
        return 0

    def alloc(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def run(self, state, owner, kind, x18, net0):
        cpu = self.cpu
        ctl = self.alloc(ICTL + 0x40)
        this = ctl + ICTL
        vbt = self.alloc(8)
        cpu.u32(vbt + 4, ICTL - 4)       # the virtual base from the vbptr at +4
        cpu.u32(ctl + 4, vbt)
        cpu.u32(ctl + 0x38, state)
        cpu.u32(ctl + 0xa1c, net0)
        cpu.u32(this - 4, 0)             # the vtordisp the thunk subtracts
        if owner:
            o = self.alloc(0x20)
            cpu.u32(o + 4, 0x1234 if owner == 2 else 0)
            cpu.u32(this + 0x1c, o)  # the owner: the IControls base's +0x1c in PM (MW3: +0x10)
        net = 0
        if kind:
            vt = self.alloc(0x60)
            cpu.u32(vt + 0x38, self.yes if kind == 1 else self.no)
            cpu.u32(vt + 0x18, self.x18)
            net = self.alloc(0x40)
            cpu.u32(net, vt)
            cpu.u32(net + 0x18, x18)
        self.inits = 0
        cpu.call(THUNK, ecx=this, stack_args=(net,))
        res = cpu._uc.reg_read(UC_X86_REG_EAX)
        return [res, struct.unpack('<i', cpu.read(ctl + 0xa1c, 4))[0], self.inits]


def main():
    p = test_pm_zar.load_pm_probe()
    p.probe_basic_set_net.argtypes = [ctypes.c_int32] * 5 + [ctypes.POINTER(ctypes.c_int32)]
    exe = Exe()
    r = random.Random(0x4b1ec0)
    bad, taken, nulls = 0, 0, 0
    for k in range(600):
        state, owner, kind = r.randrange(6), r.randrange(3), r.randrange(3)
        x18, net0 = r.choice((0, 7, -1, 0x1234, r.getrandbits(31))), r.choice((0, 3, 99))
        if state == 4 and owner == 2 and kind == 0:  # the exe asks a null net its vt+0x38 (FIX (A)): the port's 0
            want = [0, net0, 0]
            nulls += 1
        else:
            want = exe.run(state, owner, kind, x18, net0)
        out = (ctypes.c_int32 * 3)()
        p.probe_basic_set_net(state, owner, kind, x18, net0, out)
        got = list(out)
        taken += want[0] == 1
        if got != want:
            bad += 1
            if bad <= 4:
                print(f'state {state} owner {owner} net {kind}: exe {want} port {got}')
    print(f'600 cases, {bad} differ ({taken} took the net; {nulls} with no net where the exe reads through null)')
    if bad or not taken:
        raise SystemExit(1)
    print('ok: the basic controls\' set-net as PM\'s exe')


if __name__ == '__main__':
    main()
