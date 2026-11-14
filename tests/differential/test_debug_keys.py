"""The key switch's debug commands (mech3/app_keys app_key, 0x40dba0; keybindings.md) against Mech3.exe 1.2: random
runs of the commands 0x61..0xa4 (0x9a, the editor's, apart: ours). Each target is stood in for on both sides and
logged with its argument; the switch's own state is compared after every command: the frame rate's and the developer
lines' toggle, the readout's flag, the timer's clamp and lowest rate (zTimer's own 0x575ab0 / 0x575af0 run), the
cursor's flag, zModel's zone mode, and the message the panel got.

    cmake --build build --target mw3probe && python3 tests/differential/test_debug_keys.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from twod_world import c_format, iat_slots  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_dev_text  # noqa: E402

SWITCH = 0x40dba0
PILOT, VIEW, ZONE = 0x60861c, 0x5f4c8c, 0x5ea1d0
MAX_RATE, CLAMP, CURSOR, DEV_TEXT, READOUT = 0x5e0c98, 0x5e0c9c, 0x5e0ca0, 0x5f4d6c, 0x5f4d70
TIMER_MAX_DT, TIMER_CLAMP = 0x5f0960, 0x5f0964
FPS, LINES = 0x5fc378, 0x5fc838
CMDS = [c for c in range(0x61, 0xa5) if c != 0x9a]


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        self.log = []
        iat = iat_slots()
        simple = {0x41ca90: 'bit', 0x4a33f0: 'next', 0x4a3550: 'vehdlg', 0x40aed0: 'cycle', 0x40bb00: 'camdlg',
                  0x4a3220: 'marker', 0x4a37a0: 'teleport', 0x4b86e0: 'editveh', 0x480b20: 'console',
                  0x4ab910: 'vmode', 0x40f070: 'msgtoggle', 0x40fb50: None}
        for addr, name in simple.items():
            cpu.stub_function(addr, lambda c, n=name: (self.log.append(n) if n else None) or 0)
        cpu.stub_function(0x448830, lambda c: self.log.append('lab') or 0, pop_bytes=4)
        cpu.stub_function(0x432c60, lambda c: self.log.append('list') or self.list_answer)
        cpu.stub_function(0x4783c0, lambda c: self.log.append(f'remake {self.s32(c.arg(0))}') or 0, pop_bytes=4)
        cpu.stub_function(0x40ae70, lambda c: self.log.append(f'cam {self.s32(c.arg(0))}') or 0, pop_bytes=4)
        cpu.stub_import(iat['ShowCursor'], self.show_cursor, pop_bytes=4)
        cpu.stub_function(0x557f70, self.mip)
        cpu.stub_function(0x535ed0, lambda c: self.log.append(f'bias {self.s32(c.arg(0))}') or 0, pop_bytes=4)
        self.bias_cell = cpu.alloc(4)
        cpu.write(0x535ef0, bytes.fromhex('d905') + struct.pack('<I', self.bias_cell) + b'\xc3')  # fld [cell]; ret
        cpu.stub_function(0x40fe50, lambda c: self.log.append('msg:' + self.cstr(c.arg(0))) or 0)
        # The frame rate's show and hide, the developer lines' set_active (their vt+4).
        cpu.stub_function(0x59ff10, lambda c: setattr(self, 'fps_hidden', 0) or 0, pop_bytes=4)
        cpu.stub_function(0x59ff50, lambda c: setattr(self, 'fps_hidden', 1) or 0)
        lines_vt = cpu.alloc(0x10)
        active = cpu.alloc(0x10)
        cpu.stub_function(active, lambda c: setattr(self, 'lines_active', c.arg(0)) or 0, pop_bytes=4)
        cpu.u32(lines_vt + 4, active)
        cpu.u32(LINES, lines_vt)
        # The pilot: its mech (+0x14, whose +0x684's +0xc the list dialog takes) and +0x6c (the mech lab's).
        pilot, mech, x684 = cpu.alloc(0x80), cpu.alloc(0x700), cpu.alloc(0x20)
        cpu.u32(PILOT, pilot)
        cpu.u32(pilot + 0x14, mech)
        cpu.u32(mech + 0x684, x684)
        cpu.u32(pilot + 0x6c, cpu.alloc(0x20))
        view = cpu.alloc(0x80)
        cpu.u32(VIEW, view)

    def s32(self, v):
        return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]

    def cstr(self, a):
        return self.cpu.read(a, 64).split(b'\0')[0].decode()

    def show_cursor(self, c):
        self.log.append(f'cursor {self.s32(c.arg(0))}')
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
        for a, v in ((CLAMP, 1), (CURSOR, 1), (DEV_TEXT, 0), (READOUT, 0), (ZONE, zone), (TIMER_CLAMP, 1)):
            cpu.u32(a, v)
        cpu.write(TIMER_MAX_DT, struct.pack('<f', 0.125))
        self.fps_hidden, self.lines_active = 1, 0
        out = []
        for cmd in cmds:
            self.log = []
            cpu.call(SWITCH, ecx=cmd)
            r = self.r32
            state = (f'| {r(DEV_TEXT)} {r(READOUT)} {r(CLAMP)} {r(CURSOR)} {r(MAX_RATE):08x} {self.s32(r(ZONE))} '
                     f'{r(TIMER_MAX_DT):08x} {r(TIMER_CLAMP)} {self.fps_hidden} {self.lines_active}')
            out.append(''.join(x + ';' for x in self.log) + state)
        return out


class NextMech:
    """0x4a33f0 on a list of fake entities: vt+0x2c (a vehicle: itself), vt+0x40 (a mech: itself), vt+0x108 logged."""

    def __init__(self, cpu):
        self.cpu = cpu
        me, none, ret = (cpu.code(bytes.fromhex(h)) for h in ('89c8c3', '31c0c3', 'c3'))
        self.picked = None
        view = cpu.alloc(0x20)
        cpu.stub_function(view, lambda c: setattr(self, 'picked', c.ecx) or 0)
        self.vts = {}
        for kind, (x2c, x40) in {1: (none, none), 2: (me, none), 3: (me, me)}.items():
            vt = cpu.alloc(0x110)
            cpu.write(vt, bytes(0x110))
            cpu.u32(vt + 0x2c, x2c)
            cpu.u32(vt + 0x40, x40)
            cpu.u32(vt + 0x108, view)
            self.vts[kind] = vt
        rows_vt = cpu.alloc(0x80)
        cpu.u32(rows_vt + 0x7c, ret)
        for k in range(25):
            cpu.u32(0x5fc848 + k * 0x4b0, rows_vt)
        self.outside = cpu.alloc(0x10)
        cpu.u32(self.outside, self.vts[3])

    def run(self, kinds, target):
        cpu = self.cpu
        objs = []
        for k in kinds:
            if k:
                o = cpu.alloc(0x10)
                cpu.u32(o, self.vts[k])
                objs.append(o)
            else:
                objs.append(0)
        lst = cpu.alloc(4 * max(1, len(objs)))
        for i, o in enumerate(objs):
            cpu.u32(lst + 4 * i, o)
        cpu.u32(0x73655c, lst if objs else 0)
        cpu.u32(0x736560, lst + 4 * len(objs) if objs else 0)
        t = objs[target] if target >= 0 else self.outside if target == -2 else 0
        cpu.u32(cpu_view(cpu) + 0x64, t)
        self.picked = None
        cpu.call(0x4a33f0)
        if self.picked is None:
            return -1
        return -2 if self.picked == self.outside else objs.index(self.picked)


class Readout(test_dev_text.Exe):
    """0x4a56b0 on a fake overlay and stealth, the developer lines with the exe's own show, hide and set_text."""

    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.pos = cpu.alloc(12)
        cpu.stub_function(0x5a85e0, lambda c: self.pos)
        cpu.stub_function(0x59fd20, lambda c: 0, pop_bytes=4)
        cpu.stub_import(iat_slots()['sprintf'], self.sprintf)
        cpu.import_code(IAT_FTOL, FTOL)

    def sprintf(self, c):
        s = c_format(self, self.cstr(c.arg(1)), c.uc.reg_read(UC_X86_REG_ESP) + 12).encode('latin-1')
        self.cpu.write(c.arg(0), s + b'\0')
        return len(s)

    def run(self, mode, has_controls, yaw, pos, name, active):
        cpu = self.cpu
        for k in range(25):
            self.new_line(0x5fc848 + k * 0x4b0)
        cpu.write(self.pos, struct.pack('<3f', *pos))
        node = cpu.alloc(64)
        cpu.write(node, name.encode() + b'\0')
        owner = cpu.alloc(0x110)
        cpu.write(owner, bytes(0x110))
        cpu.u32(owner + 4, node)
        if has_controls:
            ctl = cpu.alloc(0xa20)
            cpu.write(ctl, bytes(0xa20))
            cpu.u32(ctl + 0x764, mode & 0xffffffff)
            cpu.write(ctl + 0x330, struct.pack('<f', yaw))
            cpu.u32(owner + 0xe4, ctl)
        layer = cpu.alloc(0x110)
        cpu.write(layer, bytes(0x110))
        cpu.u32(layer + 4, active)
        cpu.u32(layer + 0x10, owner)
        cpu.call(0x4a56b0, ecx=layer, stack_args=(0,))
        rows = []
        for k in range(3):
            at = 0x5fc848 + k * 0x4b0
            rows.append(('1' if self.r32(at + 0xc) & 0x10 else '0') + self.cstr(at + 0x34))
        return rows


def cpu_view(cpu):
    return struct.unpack('<I', cpu.read(VIEW, 4))[0]


def main():
    p = load_probe()
    i32p = ctypes.POINTER(ctypes.c_int32)
    p.probe_debug_keys.argtypes = [i32p, ctypes.c_int, ctypes.c_int32, i32p, i32p, ctypes.c_float, ctypes.c_int32,
                                   ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x40dba0)
    buf = ctypes.create_string_buffer(1 << 18)
    bad = runs = steps = 0
    seen = set()
    for trial in range(300):
        cmds = [r.choice(CMDS) if r.random() < 0.6 else r.choice((0x82, 0x83, 0x86, 0x87, 0x88, 0x93, 0x97, 0x98,
                                                                     0x9e, 0x9f, 0x84))
                for _ in range(r.randrange(1, 40))]
        list_answer = r.choice((-1, 0, 3, 17))
        cursor = [r.choice((-1, 0, 1, 2)) for _ in range(64)]
        mips = [r.choice((0, 1, 7)) for _ in range(64)]
        bias = r.choice((0.0, 0.1, 0.2, 0.3, 1.0, r.uniform(-1, 3)))
        zone = r.choice((0, 1, 2, 5, -4))
        n = p.probe_debug_keys((ctypes.c_int32 * len(cmds))(*cmds), len(cmds), list_answer,
                               (ctypes.c_int32 * 64)(*cursor), (ctypes.c_int32 * 64)(*mips), bias, zone, buf, len(buf))
        assert n >= 0
        got = buf.value.decode().splitlines()
        want = exe.run(cmds, list_answer, cursor, mips, bias, zone)
        runs += 1
        steps += len(cmds)
        seen.update(cmds)
        for k, (w, g) in enumerate(zip(want, got)):
            if w != g:
                bad += 1
                if bad <= 6:
                    print(f'trial {trial} step {k} cmd {cmds[k]:#x}:\n  exe  {w}\n  port {g}')
                break
        if len(want) != len(got):
            bad += 1
    cpu = Original()
    cpu.u32(VIEW, cpu.alloc(0x80))
    nm = NextMech(cpu)  # its own: the switch's run stands 0x4a33f0 in
    p.probe_next_view_mech.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
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
    print(f'next mech: 2000 lists, picks {dict(sorted(picks.items()))}')
    ro = Readout()
    p.probe_stealth_readout.argtypes = [ctypes.c_int32, ctypes.c_int, ctypes.c_float, ctypes.POINTER(ctypes.c_float),
                                        ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    shown = 0
    for trial in range(300):
        mode, has, active = r.choice((0, 1, 2, 3, 4, 5, 6, -1, 99)), r.randrange(4) != 0, r.randrange(4) != 0
        if not has:
            continue  # the exe reads through null without controls (FIX (A)): the port's alone
        yaw = r.choice((0.0, 1.5707964, -3.1, r.uniform(-7, 7)))
        pos = [r.choice((0.0, -0.5, 1234.9, r.uniform(-5000, 5000))) for _ in range(3)]
        name = r.choice(('Stealth', 'stealth01', 'x'))
        want = ro.run(mode, int(has), yaw, pos, name, int(active))
        n = p.probe_stealth_readout(mode, int(has), yaw, (ctypes.c_float * 3)(*pos), name.encode(), int(active), buf,
                                    len(buf))
        got = buf.value.decode().splitlines()
        shown += want[2].startswith('0POS')
        if want != got:
            bad += 1
            if bad <= 6:
                print(f'readout mode {mode} yaw {yaw} pos {pos} active {active}:\n  exe  {want}\n  port {got}')
    print(f'stealth readout: {shown} written, e.g. {want}')
    print(f'{runs} runs, {steps} commands ({len(seen)} kinds): {bad} differ')
    if bad:
        raise SystemExit(1)
    print('ok: the debug commands as the original')


if __name__ == '__main__':
    main()
