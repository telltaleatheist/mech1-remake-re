"""The key switch's debug commands on Pirate's Moon's exe (app_key, 0x40ba30; MW3's in test_debug_keys.py): random
runs of PM's commands 0x69..0xaf (0xa2, the editor's, apart: ours), each target stood in for on both sides and
logged, the switch's state compared after every command (zTimer's own 0x5964e0 / 0x596510 run); PM's 0xaf (the
2D-through-Direct3D flag, MW3's 0xa4 the mech lab) among them. Then the next-mech pick 0x4a6810 (vt+0x2c, vt+0x48,
vt+0x114) and the stealth overlay's readout 0x4a7ef0 (controls at +0xe8, the lines 0x4b8 apart).

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_debug_keys.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from twod_world import IAT, c_format  # noqa: E402  (PM's slots)
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_pm_dev_text  # noqa: E402
import test_pm_zar  # noqa: E402

SWITCH = 0x40ba30
PILOT, VIEW, ZONE = 0x62c40c, 0x61874c, 0x60d5e8
MAX_RATE, CLAMP, CURSOR, DEV_TEXT, READOUT, TWOD = 0x602c90, 0x602c94, 0x602c98, 0x61881c, 0x618820, 0x618824
TIMER_MAX_DT, TIMER_CLAMP = 0x6141b0, 0x6141b4
LINES, ROWS, LINE = 0x6203e0, 0x6203f0, 0x4b8
CMDS = [c for c in range(0x69, 0xb0) if c != 0xa2]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.log = []
        simple = {0x41d8f0: 'bit', 0x4a6810: 'next', 0x4a6940: 'vehdlg', 0x408ac0: 'cycle', 0x4a65a0: 'marker',
                  0x4a8450: 'teleport', 0x4bec10: 'editveh', 0x4832a0: 'console', 0x4af360: 'vmode',
                  0x40f940: 'msgtoggle', 0x410470: None}
        for addr, name in simple.items():
            cpu.stub_function(addr, lambda c, n=name: (self.log.append(n) if n else None) or 0)
        cpu.stub_function(0x409830, lambda c: self.log.append('camdlg') or 0)
        cpu.stub_function(0x433770, lambda c: self.log.append('list') or self.list_answer)
        cpu.stub_function(0x47a6e0, lambda c: self.log.append(f'remake {s32(c.arg(0))}') or 0, pop_bytes=4)
        cpu.stub_function(0x408a70, lambda c: self.log.append(f'cam {s32(c.arg(0))}') or 0, pop_bytes=4)
        cpu.stub_function(0x5c0340, lambda c: self.log.append(f'twod {s32(c.arg(0))}') or 0)
        cpu.stub_import(IAT['ShowCursor'], self.show_cursor, pop_bytes=4)
        cpu.stub_function(0x579ef0, self.mip)
        cpu.stub_function(0x54fe70, lambda c: self.log.append(f'bias {s32(c.arg(0))}') or 0)
        self.bias_cell = cpu.alloc(4)
        cpu.write(0x54fe90, bytes.fromhex('d905') + struct.pack('<I', self.bias_cell) + b'\xc3')  # fld [cell]; ret
        cpu.stub_function(0x410780, lambda c: self.log.append('msg:' + self.cstr(c.arg(0))) or 0)
        cpu.stub_function(0x5c0170, lambda c: setattr(self, 'fps_hidden', 0) or 0, pop_bytes=4)
        cpu.stub_function(0x5c01c0, lambda c: setattr(self, 'fps_hidden', 1) or 0)
        lines_vt, active = cpu.alloc(0x10), cpu.alloc(0x10)
        cpu.stub_function(active, lambda c: setattr(self, 'lines_active', c.arg(0)) or 0, pop_bytes=4)
        cpu.u32(lines_vt + 8, active)  # PM's set_active is vt+8
        cpu.u32(LINES, lines_vt)
        pilot, mech, x6a8 = cpu.alloc(0x80), cpu.alloc(0x700), cpu.alloc(0x20)
        cpu.u32(PILOT, pilot)
        cpu.u32(pilot + 0x14, mech)
        cpu.u32(mech + 0x6a8, x6a8)
        cpu.u32(VIEW, cpu.alloc(0x80))

    def cstr(self, a):
        return self.cpu.read(a, 64).split(b'\0')[0].decode()

    def show_cursor(self, c):
        self.log.append(f'cursor {s32(c.arg(0))}')
        v = self.cursor[self.cursor_at & 63]
        self.cursor_at += 1
        return v & 0xffffffff

    def mip(self, c):
        self.log.append('mip')
        v = self.mips[self.mip_at & 63]
        self.mip_at += 1
        return v & 0xffffffff

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, cmds, list_answer, cursor, mips, bias, zone):
        cpu = self.cpu
        self.list_answer, self.cursor, self.mips = list_answer & 0xffffffff, cursor, mips
        self.cursor_at = self.mip_at = 0
        cpu.write(self.bias_cell, struct.pack('<f', bias))
        cpu.write(MAX_RATE, struct.pack('<f', 8.0))
        for a, v in ((CLAMP, 1), (CURSOR, 1), (DEV_TEXT, 0), (READOUT, 0), (TWOD, 0), (ZONE, zone), (TIMER_CLAMP, 1)):
            cpu.u32(a, v)
        cpu.write(TIMER_MAX_DT, struct.pack('<f', 0.125))
        self.fps_hidden, self.lines_active = 1, 0
        out = []
        for cmd in cmds:
            self.log = []
            cpu.call(SWITCH, stack_args=(cmd,))
            r = self.r32
            state = (f'| {r(DEV_TEXT)} {r(READOUT)} {r(CLAMP)} {r(CURSOR)} {r(MAX_RATE):08x} {s32(r(ZONE))} '
                     f'{r(TIMER_MAX_DT):08x} {r(TIMER_CLAMP)} {self.fps_hidden} {self.lines_active}')
            out.append(''.join(x + ';' for x in self.log) + state)
        return out


class NextMech:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        me, none, ret = (cpu.code(bytes.fromhex(h)) for h in ('89c8c3', '31c0c3', 'c3'))
        self.picked = None
        view = cpu.alloc(0x20)
        cpu.stub_function(view, lambda c: setattr(self, 'picked', c.ecx) or 0)
        self.vts = {}
        for kind, (x2c, x48) in {1: (none, none), 2: (me, none), 3: (me, me)}.items():
            vt = cpu.alloc(0x120)
            cpu.write(vt, bytes(0x120))
            cpu.u32(vt + 0x2c, x2c)
            cpu.u32(vt + 0x48, x48)
            cpu.u32(vt + 0x114, view)
            self.vts[kind] = vt
        rows_vt = cpu.alloc(0x80)
        cpu.u32(rows_vt + 0x7c, ret)
        for k in range(25):
            cpu.u32(ROWS + k * LINE, rows_vt)
        self.outside = cpu.alloc(0x10)
        cpu.u32(self.outside, self.vts[3])
        self.view = cpu.alloc(0x80)
        cpu.u32(VIEW, self.view)

    def run(self, kinds, target):
        cpu = self.cpu
        objs = []
        for k in kinds:
            o = 0
            if k:
                o = cpu.alloc(0x10)
                cpu.u32(o, self.vts[k])
            objs.append(o)
        lst = cpu.alloc(4 * max(1, len(objs)))
        for i, o in enumerate(objs):
            cpu.u32(lst + 4 * i, o)
        cpu.u32(0x8d3ac4, lst if objs else 0)
        cpu.u32(0x8d3ac8, lst + 4 * len(objs) if objs else 0)
        cpu.u32(self.view + 0x64, objs[target] if target >= 0 else self.outside if target == -2 else 0)
        self.picked = None
        cpu.call(0x4a6810)
        if self.picked is None:
            return -1
        return -2 if self.picked == self.outside else objs.index(self.picked)


class Readout(test_pm_dev_text.Exe):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.pos = cpu.alloc(12)
        cpu.stub_function(0x5cb260, lambda c: self.pos)
        cpu.stub_function(0x5bfd20, lambda c: 0, pop_bytes=4)
        cpu.stub_import(IAT['sprintf'], self.sprintf)
        cpu.import_code(IAT_FTOL, FTOL)

    def sprintf(self, c):
        s = c_format(self, self.cstr(c.arg(1)), c.uc.reg_read(UC_X86_REG_ESP) + 12).encode('latin-1')
        self.cpu.write(c.arg(0), s + b'\0')
        return len(s)

    def run(self, mode, yaw, pos, name, active):
        cpu = self.cpu
        for k in range(25):
            self.new_line(ROWS + k * LINE)
        cpu.write(self.pos, struct.pack('<3f', *pos))
        node = cpu.alloc(64)
        cpu.write(node, name.encode() + b'\0')
        owner, ctl = cpu.alloc(0x120), cpu.alloc(0xa40)
        cpu.write(owner, bytes(0x120))
        cpu.write(ctl, bytes(0xa40))
        cpu.u32(owner + 4, node)
        cpu.u32(ctl + 0x764, mode & 0xffffffff)
        cpu.write(ctl + 0x330, struct.pack('<f', yaw))
        cpu.u32(owner + 0xe8, ctl)
        layer = cpu.alloc(0x120)
        cpu.write(layer, bytes(0x120))
        cpu.u32(layer + 4, active)
        cpu.u32(layer + 0x10, owner)
        cpu.call(0x4a7ef0, ecx=layer, stack_args=(0,))
        rows = []
        for k in range(3):
            at = ROWS + k * LINE
            rows.append(('1' if self.r32(at + 0xc) & 0x10 else '0') + self.cstr(at + 0x34))
        return rows


def main():
    p = test_pm_zar.load_pm_probe()
    i32p = ctypes.POINTER(ctypes.c_int32)
    p.probe_debug_keys.argtypes = [i32p, ctypes.c_int, ctypes.c_int32, i32p, i32p, ctypes.c_float, ctypes.c_int32,
                                   ctypes.c_char_p, ctypes.c_int]
    p.probe_next_view_mech.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    p.probe_stealth_readout.argtypes = [ctypes.c_int32, ctypes.c_int, ctypes.c_float, ctypes.POINTER(ctypes.c_float),
                                        ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x40ba30)
    buf = ctypes.create_string_buffer(1 << 18)
    bad = steps = 0
    seen = set()
    for trial in range(300):
        cmds = [r.choice(CMDS) if r.random() < 0.6 else r.choice((0x8a, 0x8b, 0x8e, 0x8f, 0x90, 0x9b, 0x9f, 0xa0,
                                                                     0xa6, 0xa7, 0x8c, 0xaf))
                for _ in range(r.randrange(1, 40))]
        list_answer = r.choice((-1, 0, 3, 17))
        cursor = [r.choice((-1, 0, 1, 2)) for _ in range(64)]
        mips = [r.choice((0, 1, 7)) for _ in range(64)]
        bias = r.choice((0.0, 0.1, 0.2, 0.3, 1.0, r.uniform(-1, 3)))
        zone = r.choice((0, 1, 2, 5, -4))
        n = p.probe_debug_keys((ctypes.c_int32 * len(cmds))(*cmds), len(cmds), list_answer,
                               (ctypes.c_int32 * 64)(*cursor), (ctypes.c_int32 * 64)(*mips), bias, zone, buf, len(buf))
        assert n >= 0
        got, want = buf.value.decode().splitlines(), exe.run(cmds, list_answer, cursor, mips, bias, zone)
        steps += len(cmds)
        seen.update(cmds)
        for k, (w, g) in enumerate(zip(want, got)):
            if w != g:
                bad += 1
                if bad <= 6:
                    print(f'trial {trial} step {k} cmd {cmds[k]:#x}:\n  exe  {w}\n  port {g}')
                break
        bad += len(want) != len(got)
    nm = NextMech()
    picks = {}
    for trial in range(2000):
        kinds = [r.choice((0, 1, 2, 3, 3)) for _ in range(r.randrange(0, 9))]
        target = r.choice([-1, -2] + list(range(len(kinds))))
        want, got = nm.run(kinds, target), p.probe_next_view_mech(bytes(kinds), len(kinds), target)
        picks[want] = picks.get(want, 0) + 1
        if want != got:
            bad += 1
            if bad <= 6:
                print(f'next mech {kinds} target {target}: exe {want} port {got}')
    ro = Readout()
    shown = 0
    for trial in range(300):
        mode, active = r.choice((0, 1, 2, 3, 4, 5, 6, -1, 99)), r.randrange(4) != 0
        yaw = r.choice((0.0, 1.5707964, -3.1, r.uniform(-7, 7)))
        pos = [r.choice((0.0, -0.5, 1234.9, r.uniform(-5000, 5000))) for _ in range(3)]
        name = r.choice(('Stealth', 'stealth01', 'x'))
        want = ro.run(mode, yaw, pos, name, int(active))
        p.probe_stealth_readout(mode, 1, yaw, (ctypes.c_float * 3)(*pos), name.encode(), int(active), buf, len(buf))
        got = buf.value.decode().splitlines()
        shown += want[2].startswith('0POS')
        if want != got:
            bad += 1
            if bad <= 6:
                print(f'readout mode {mode} yaw {yaw} pos {pos} active {active}:\n  exe  {want}\n  port {got}')
    print(f'300 runs, {steps} commands ({len(seen)} kinds); next mech 2000 lists, picks {dict(sorted(picks.items()))}; '
          f'stealth readout {shown} written: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: PM's debug commands as its exe")


if __name__ == '__main__':
    main()
