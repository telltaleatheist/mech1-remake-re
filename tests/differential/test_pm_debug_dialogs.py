"""Pirate's Moon's debug menu and dialogs against its exe (MW3's in test_debug_dialogs.py; docs/editor.md, The debug
menu): the frame's handlers 0x485b10..0x485ea0 (PM has no Select Mech item, 40037, and no Reset Options, 40051), its
app's flags 4 bytes below MW3's (+0x1a8..+0x1b4, full screen +0x1c5), its callees cdecl where MW3's are fastcall;
Select Mech (0x433840 its 25 rows, 0x4fc4d0 the OK, 0x433770 through 0x4338f0); Vehicle Select (0x4b2ec0 the rows,
vt+0x48 its mech test, 0x4b2e70 the close, 0x4a6940 the pick through vt+0x114); Camera Properties (0x409830, the mode
on the stack). The port's side is MW3's, by its handler.

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_debug_dialogs.py
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
os.environ.setdefault('MW3_X87', '24')
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402
from test_debug_dialogs import DEVICES, MECHS, OK_FIELDS, Exe, bits  # noqa: E402

APP = 0x62ca98
# PM handler -> the port's (MW3's) handler
UI = {0x485b20: 0x483170, 0x485bd0: 0x483230, 0x485c60: 0x483330, 0x485d10: 0x4833f0, 0x485d30: 0x483410,
      0x485d60: 0x4834b0, 0x485dc0: 0x483510, 0x485e10: 0x483570, 0x485e40: 0x4835c0, 0x485e60: 0x483600,
      0x485ea0: 0x483640}
CMD = {0x485b10: 0x483150, 0x485ba0: 0x483200, 0x485c50: 0x483310, 0x485cb0: 0x483390, 0x485cc0: 0x4833a0,
       0x485cd0: 0x4833b0, 0x485ce0: 0x4833c0, 0x485cf0: 0x4833d0, 0x485d00: 0x4833e0, 0x485d20: 0x483400,
       0x485d50: 0x4834a0, 0x485db0: 0x483500, 0x485e00: 0x483550, 0x485e30: 0x4835a0, 0x485e80: 0x483620}
SEND, SHOW, ASIN = 0x5de73c, 0x5de738, 0x5de600  # PM's import slots: SendMessageA, ShowCursor, _CIasin


class Menu:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.strs = {}
        st = self.state = {}
        cpu.stub_function(0x40b360, lambda c: st['hw'])
        cpu.stub_function(0x40a950, lambda c: st['dev'] & 0xffffffff)
        cpu.stub_function(0x598b30, lambda c: self.cs(st['names'][c.arg(0)]) if c.arg(0) < len(st['names']) else 0)
        cpu.stub_function(0x598b60, lambda c: st['count'])
        cpu.stub_function(0x40b8e0, lambda c: st['vmode'])
        cpu.stub_function(0x4822c0, lambda c: self.put(f'vmode {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x40afa0, lambda c: st['joy'])
        cpu.stub_function(0x40adf0, self.set_joy)
        cpu.stub_function(0x596e50, lambda c: self.cs(f'm{c.arg(0)}'))
        cpu.stub_function(0x596de0, self.fmt)
        cpu.stub_function(0x4824d0, lambda c: self.put(f'restore {c.arg(0)} {c.arg(1)}'), pop_bytes=8)
        self.ui = cpu.alloc(0x10)
        vt = cpu.alloc(0x20)
        for off, tag in ((0, 'E'), (4, 'C'), (0xc, 'T')):
            f = cpu.alloc(0x10)
            if tag == 'T':
                cpu.stub_function(f, lambda c: self.put('T' + self.cstr(c.arg(0))), pop_bytes=4)
            else:
                cpu.stub_function(f, lambda c, t=tag: self.put(f'{t}{c.arg(0)}'), pop_bytes=4)
            cpu.u32(vt + off, f)
        cpu.u32(self.ui, vt)

    cs, cstr, fmt = Exe.cs, Exe.cstr, Exe.fmt

    def put(self, s):
        self.log.append(s)
        return 0

    def set_joy(self, c):
        self.put(f'joy {c.arg(0)}')
        self.state['joy'] = c.arg(0) & 1
        return 0

    def run(self, addr, s, names):
        cpu = self.cpu
        self.state.update(hw=s[0], dev=s[1], count=s[2], vmode=s[3], joy=s[4], names=names)
        cpu.write(APP + 0x1a8, bytes(s[6:19]))
        cpu.write(APP + 0x1c5, bytes([s[19]]))
        self.log = []
        frame = cpu.alloc(0x10)
        if addr in UI:
            cpu.call(addr, ecx=frame, stack_args=(self.ui,))
        else:
            cpu.call(addr, ecx=frame)
        # in the port's shape: its +0x1ac..+0x1c7, PM's +0x1a8.. moved up by 4, full screen (+0x1c5) last
        after = cpu.read(APP + 0x1a8, 13) + bytes(14) + cpu.read(APP + 0x1c5, 1)
        return ''.join(x + ';' for x in self.log) + '|' + after.hex() + f"|joy {self.state['joy']}|"


class Lists:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        self.strs, self.log, self.rows, self.cur = {}, [], [], [-1]
        self.modal, self.chosen, self.ok_val, self.kind = 0, 0, 0, ''
        cpu.stub_import(SEND, self.send, pop_bytes=16)
        cpu.stub_import(SHOW, lambda c: 0, pop_bytes=4)
        cpu.stub_function(0x5ce9ec, lambda c: 1)  # CDialog::OnInitDialog
        cpu.stub_function(0x5ce9f8, lambda c: 0)  # CDialog::OnOK
        cpu.stub_function(0x5cea6a, lambda c: 0)  # CWnd::OnDestroy
        cpu.stub_function(0x5cea0a, self.domodal)  # CDialog::DoModal
        cpu.stub_function(0x433650, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x4b2dc0, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x598fb0, lambda c: self.log.append('gdi') or 0)
        cpu.stub_function(0x533b90, lambda c: self.log.append('keys') or 0)
        for dtor in (0x5ceb00, 0x5ce8d2, 0x5ce8cc):
            cpu.stub_function(dtor, lambda c: 0)
        me, none = cpu.code(bytes.fromhex('89c8c3')), cpu.code(bytes.fromhex('31c0c3'))
        one = cpu.code(bytes.fromhex('b801000000c3'))
        self.vts = {}
        for kind, (x2c, x48) in {1: (me, none), 2: (me, one), 0: (none, none)}.items():
            vt = cpu.alloc(0x120)
            cpu.write(vt, bytes(0x120))
            cpu.u32(vt + 0x2c, x2c)
            cpu.u32(vt + 0x48, x48)
            take = cpu.alloc(0x10)
            cpu.stub_function(take, lambda c: self.log.append(f'take {self.addr_ix.get(c.ecx, -1)}') or 0)
            cpu.u32(vt + 0x114, take)
            self.vts[kind] = vt
        self.addr_ix = {}

    cs, cstr = Exe.cs, Exe.cstr

    def send(self, c):
        m, w, l = c.arg(1), c.arg(2), c.arg(3)
        rows, cur = self.rows, self.cur
        if m in (0x180, 0x143):
            rows.append([self.cstr(l), None])
            return len(rows) - 1
        if m in (0x19a, 0x151):
            rows[w][1] = l
        if m in (0x186, 0x14e):
            cur[0] = w if w < len(rows) else -1
            return cur[0] & 0xffffffff
        if m in (0x188, 0x147):
            return cur[0] & 0xffffffff
        if m in (0x199, 0x150):
            return rows[w][1] if 0 <= w < len(rows) else 0xffffffff
        return 0

    def domodal(self, c):
        self.cpu.u32(c.ecx + (0xa0 if self.kind == 'mech' else 0x60), (self.ok_val if self.kind == 'mech' else self.chosen) & 0xffffffff)
        return self.modal & 0xffffffff

    def dialog(self):
        d = self.cpu.alloc(0x100)
        self.cpu.write(d, bytes(0x100))
        self.cpu.u32(d + 0x80, 0x5150)
        self.cpu.u32(d + 0x84, 0x5150)
        return d

    def mech_list(self, modal, sel):
        cpu = self.cpu
        self.kind, self.rows, self.cur, self.log = 'mech', [], [-1], []
        dlg = self.dialog()
        cpu.call(0x433840, ecx=dlg)
        out = ''.join(f'{n}={d};' for n, d in self.rows) + f'sel {self.cur[0]};'
        if sel != -2:
            self.cur[0] = sel
        cpu.call(0x4fc4d0, ecx=dlg)
        self.ok_val = struct.unpack('<i', cpu.read(dlg + 0xa0, 4))[0]
        self.modal, self.log = modal, []
        cpu.call(0x433770)
        r = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        return out + '|' + ''.join(x + ';' for x in self.log) + str(r)

    def vehicle(self, ents, names, modal, sel):
        cpu = self.cpu
        arr = cpu.alloc(4 * max(1, len(ents)))
        self.addr_ix = {}
        for i, (kind, ni) in enumerate(ents):
            if kind < 0:
                cpu.u32(arr + 4 * i, 0)
                continue
            e = cpu.alloc(0x10)
            cpu.u32(e, self.vts[kind])
            cpu.u32(e + 4, self.cs(names[ni]))
            cpu.u32(arr + 4 * i, e)
            self.addr_ix[e] = i
        cpu.u32(0x8d3ac4, arr)
        cpu.u32(0x8d3ac8, arr + 4 * len(ents))
        self.kind, self.rows, self.cur, self.log = 'vehicle', [], [-1], []
        dlg = self.dialog()
        cpu.u32(dlg + 0x60, 0)
        cpu.call(0x4b2ec0, ecx=dlg)
        out = ''.join(f'{n}={self.addr_ix[d]};' for n, d in self.rows) + f'sel {self.cur[0]};'
        if sel != -2:
            self.cur[0] = sel
        cpu.call(0x4b2e70, ecx=dlg)
        self.chosen = struct.unpack('<I', cpu.read(dlg + 0x60, 4))[0]
        self.modal, self.log = modal, []
        if modal != 0 and self.chosen == 0:
            return 'gdi;' + out + '|-1'  # through the unset +0x60 (the port's FIX (A))
        cpu.call(0x4a6940)
        t = [x for x in self.log if x.startswith('take')]
        return ('gdi;' if 'gdi' in self.log else '') + out + '|' + (t[0].split()[1] if t else '-1')


class Camera:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.import_code(ASIN, bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3'))
        cpu.stub_function(0x409a10, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x409bb0, self.domodal)
        cpu.stub_function(0x5ce8cc, lambda c: 0)

    def domodal(self, c):
        cpu, d = self.cpu, c.ecx
        self.shown = [struct.unpack('<f', cpu.read(d + o, 4))[0] for o in OK_FIELDS]
        if self.ok:
            for o, v in zip(OK_FIELDS, self.edit):
                cpu.write(d + o, struct.pack('<f', v))
            cpu.write(d + 0x88, bytes([1 if self.norm else 0]))
        return self.ok

    def run(self, m, ok, edit, norm):
        cpu = self.cpu
        self.ok, self.edit, self.norm = ok, edit, norm
        mode = cpu.alloc(0x80)
        cpu.write(mode, bytes(0x80))
        cpu.write(mode + 0x08, struct.pack('<4f', *m[:4]))
        cpu.write(mode + 0x18, struct.pack('<3f', *m[4:7]))
        cpu.write(mode + 0x28, struct.pack('<4f', m[7], m[8], m[9], m[10]))
        cpu.call(0x409830, stack_args=(mode,))
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        after = struct.unpack('<4f', cpu.read(mode + 8, 16)) + struct.unpack('<3f', cpu.read(mode + 0x18, 12)) + \
            struct.unpack('<4f', cpu.read(mode + 0x28, 16))
        return r, self.shown, list(after)


def main():
    p = test_pm_zar.load_pm_probe()
    i32p, fp = ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)
    p.probe_dd_menu.argtypes = [ctypes.c_uint32, i32p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_mech_list.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_vehicle.argtypes = [i32p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32,
                                   ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_camera.argtypes = [fp, ctypes.c_int32, fp, ctypes.c_int32, fp]
    menu, lists, cam = Menu(), Lists(), Camera()
    r = random.Random(0x485b20)
    buf = ctypes.create_string_buffer(1 << 16)
    bad = n = 0

    def report(what, want, got):
        nonlocal bad
        if want != got:
            bad += 1
            if bad <= 8:
                print(f'{what}:\n  exe  {want}\n  port {got}')

    for trial in range(400):
        names = r.sample(DEVICES, r.randrange(1, len(DEVICES) + 1))
        s = [r.choice((0, 1, 1)), r.randrange(len(names)), r.choice((0, 1, 3)), r.choice((0, 3, 5, 7)), r.randrange(2),
             -1] + [r.choice((0, 1, 1, 0x80)) for _ in range(13)] + [r.randrange(2)]
        mech = r.choice(MECHS)
        for addr in list(UI) + list(CMD):
            want = menu.run(addr, s, names)
            k = p.probe_dd_menu(UI.get(addr) or CMD[addr], (ctypes.c_int32 * 20)(*s), '\n'.join(names).encode(),
                                mech.encode(), buf, len(buf))
            got = buf.raw[:k].decode('latin-1')
            report(f'menu {addr:#x} {s} {names}', want, got[:got.rindex('|') + 1])  # PM has no default mech here
            n += 1
    for modal, sel in ((1, -2), (2, -2), (1, 0), (1, 5), (1, 24), (1, 25), (2, 7), (1, -1), (0, 3)):
        want = lists.mech_list(modal, sel)
        k = p.probe_dd_mech_list(modal, sel, buf, len(buf))
        report(f'mech list {modal} {sel}', want, buf.raw[:k].decode('latin-1'))
        n += 1
    vnames = ['hero', 'lance1', 'tank_a', 'stealth', 'enemy7', 'cop']
    for trial in range(120):
        ents = [(r.choice((-1, 0, 1, 2, 2)), r.randrange(len(vnames))) for _ in range(r.randrange(0, 7))]
        mechs = sum(1 for k, _ in ents if k == 2)
        modal, sel = r.choice((0, 1, 2, -1)), r.choice([-2, -2] + list(range(mechs)))
        want = lists.vehicle(ents, vnames, modal, sel)
        flat = [x for e in ents for x in e] or [0]
        k = p.probe_dd_vehicle((ctypes.c_int32 * len(flat))(*flat), len(ents), '\n'.join(vnames).encode(), modal, sel,
                               buf, len(buf))
        report(f'vehicle {ents} {modal} {sel}', want, buf.raw[:k].decode('latin-1'))
        n += 1
    for trial in range(300):
        a = [r.uniform(-math.pi, math.pi) for _ in range(3)]
        q = [math.cos(a[0] / 2), math.sin(a[0] / 2) * math.cos(a[1]), math.sin(a[0] / 2) * math.sin(a[1]) * math.cos(a[2]),
             math.sin(a[0] / 2) * math.sin(a[1]) * math.sin(a[2])]
        m = [struct.unpack('<f', struct.pack('<f', v))[0] for v in
             q + [r.uniform(-30, 30) for _ in range(3)] + [r.uniform(0, 80), r.uniform(0, 2), r.uniform(0, 10), r.uniform(0, 5)]]
        ok = r.choice((0, 1, 1))
        edit = [struct.unpack('<f', struct.pack('<f', v))[0] for v in
                [r.uniform(0, 80)] + [r.choice((0.0, r.uniform(-30, 30))) for _ in range(3)] +
                [r.uniform(-180, 180) for _ in range(3)] + [r.uniform(0, 5), r.uniform(0, 10), r.uniform(0, 2)]]
        norm = r.randrange(2)
        res, shown, after = cam.run(m, ok, edit, norm)
        o = (ctypes.c_float * 21)()
        got_r = p.probe_dd_camera((ctypes.c_float * 11)(*m), ok, (ctypes.c_float * 10)(*edit), norm, o)
        near = all(abs(x - y) <= 1e-4 + 1e-6 * abs(x) for x, y in zip(shown[4:7], o[4:7]))
        want = (res, [bits(v) for k, v in enumerate(shown) if k not in (4, 5, 6)], [bits(v) for v in after])
        got = (got_r, [bits(v) for k, v in enumerate(o[:10]) if k not in (4, 5, 6)], [bits(v) for v in o[10:]])
        report(f'camera {m} ok {ok} norm {norm}', want, got)
        if not near:
            report(f'camera angles {m}', shown[4:7], list(o[4:7]))
        n += 1
    print(f'{n} cases on PM: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: PM's debug menu and dialogs as its exe")


if __name__ == '__main__':
    main()
