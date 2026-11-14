"""The developers' menu and dialogs (mech3/debug_dialogs; docs/editor.md, The debug menu) against Mech3.exe 1.2:
every handler of the frame's debug menu (0x483170..0x483640, the six byte clears 0x483390.., the 640 x 480 update
0x483510, the four toggles 0x483150, 0x483310, 0x483550, 0x4835a0 and Reset Options 0x4835f0 included) on random states (the 3D card, the device and its name, the device count, the video mode, the
joystick, the app's flags and default mech, the Select Mech answer), the item's Enable, SetCheck and SetText calls
(a fake CCmdUI), the engine calls and the app's flags after, compared in order; the Select Mech list (0x432d30 its rows,
0x432c60 and 0x432da0 its answer), Vehicle Select (0x4af000 its rows over fake entities, 0x4aefb0 the close,
0x4a3550 the pick, Cancel included) and Camera Properties (0x40bb00 over random modes: the ten fields shown, then
random edits written back, normalised or not; DoModal stood in), bit for bit at the x87's 24 bits (the dialogs run
with the game up; at 53 the rebuilt rotation is an ulp apart, as test_motion_text.py's), the shown angles within
1e-4 degrees (the C runtime's asin).

    cmake --build build --target mw3probe && python3 tests/differential/test_debug_dialogs.py
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_X87', '24')  # Direct3D's 24 bits in play (CLAUDE.md)
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import iat_slots  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

APP, MECH = 0x608c88, 0x608e20
UI_HANDLERS = (0x483170, 0x483230, 0x483330, 0x4833f0, 0x483410, 0x483460, 0x4834b0, 0x483510, 0x483570, 0x4835c0,
               0x483600, 0x483640)
COMMANDS = (0x483150, 0x483310, 0x483550, 0x4835f0, 0x483200, 0x483390, 0x4833a0, 0x4833b0, 0x4833c0, 0x4833d0, 0x4833e0, 0x483400, 0x483430, 0x4834a0,
            0x483500, 0x4835a0, 0x483620)
DEVICES = ('Primary Display Driver', 'Voodoo2 3D Accelerator', 'Primary Display', 'RIVA TNT')
MECHS = ('bushwacker', 'madcat', 'nosuch', 'thor', '')
OK_FIELDS = (0x60, 0x78, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x7c, 0x80, 0x84)  # the dialog's dist, pos, angles, rates


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        iat = iat_slots()
        self.iat = iat
        self.strs = {}
        st = self.state = {}
        cpu.stub_function(0x40d4d0, lambda c: st['hw'])
        cpu.stub_function(0x40cc10, lambda c: st['dev'] & 0xffffffff)
        cpu.stub_function(0x579a70, lambda c: self.cs(st['names'][c.ecx]) if c.ecx < len(st['names']) else 0)
        cpu.stub_function(0x579aa0, lambda c: st['count'])
        cpu.stub_function(0x40d9d0, lambda c: st['vmode'])
        cpu.stub_function(0x47fc10, lambda c: self.put(f'vmode {c.arg(0)}'), pop_bytes=4)
        cpu.stub_function(0x40d1b0, lambda c: st['joy'])
        cpu.stub_function(0x40d070, self.set_joy)
        cpu.stub_function(0x576040, lambda c: self.cs(f'm{c.ecx}'))
        cpu.stub_function(0x575fd0, self.fmt)
        cpu.stub_function(0x432c60, lambda c: st['pick'] & 0xffffffff)
        cpu.stub_function(0x5abdbc, self.assign, pop_bytes=4)
        cpu.stub_import(iat['_stricmp'], lambda c: self.stricmp(self.cstr(c.arg(0)), self.cstr(c.arg(1))))
        cpu.stub_function(0x47fe10, lambda c: self.put(f'restore {c.arg(0)} {c.arg(1)}'), pop_bytes=8)
        self.ui = cpu.alloc(0x10)
        vt = cpu.alloc(0x20)
        for k, (off, tag) in enumerate(((0, 'E'), (4, 'C'), (0xc, 'T'))):
            f = cpu.alloc(0x10)
            if tag == 'T':
                cpu.stub_function(f, lambda c: self.put('T' + self.cstr(c.arg(0))), pop_bytes=4)
            else:
                cpu.stub_function(f, lambda c, t=tag: self.put(f'{t}{c.arg(0)}'), pop_bytes=4)
            cpu.u32(vt + off, f)
        cpu.u32(self.ui, vt)

    def put(self, s):
        self.log.append(s)
        return 0

    def cs(self, s):
        if s not in self.strs:
            a = self.cpu.alloc(len(s) + 1)
            self.cpu.write(a, s.encode('latin-1') + b'\0')
            self.strs[s] = a
        return self.strs[s]

    def cstr(self, a):
        return self.cpu.read(a, 128).split(b'\0')[0].decode('latin-1')

    @staticmethod
    def stricmp(a, b):
        a, b = a.lower(), b.lower()
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def set_joy(self, c):
        self.put(f'joy {c.ecx}')
        self.state['joy'] = c.ecx & 1
        return 0

    def fmt(self, c):
        s = f'f{c.arg(2)}:{self.cstr(c.arg(3))}'
        self.cpu.write(c.arg(0), s.encode('latin-1')[:c.arg(1) - 1] + b'\0')
        return len(s)

    def assign(self, c):  # CString's operator=: the mech is read back after
        self.cpu.u32(MECH, self.cs(self.cstr(c.arg(0))))
        return c.ecx

    def menu(self, addr, s, names, mech):
        cpu = self.cpu
        self.state.update(hw=s[0], dev=s[1], count=s[2], vmode=s[3], joy=s[4], pick=s[5], names=names)
        cpu.write(APP + 0x1ac, bytes(s[6:19]) + bytes(0x1c - 13))
        cpu.write(APP + 0x1c7, bytes([s[19]]))
        cpu.u32(MECH, self.cs(mech))
        self.log = []
        frame = cpu.alloc(0x10)
        if addr in UI_HANDLERS:
            cpu.call(addr, ecx=frame, stack_args=(self.ui,))
        else:
            cpu.call(addr, ecx=frame)
        bytes_after = cpu.read(APP + 0x1ac, 0x1c).hex()  # +0x1ac..+0x1c7
        mech_after = self.cstr(struct.unpack('<I', cpu.read(MECH, 4))[0])
        return ''.join(x + ';' for x in self.log) + '|' + bytes_after + f"|joy {self.state['joy']}|" + mech_after



class Lists:
    """The Select Mech and Vehicle Select lists, on an exe of their own (their stubs registered once)."""

    def __init__(self):
        self.cpu = cpu = Original()
        iat = iat_slots()
        self.strs, self.log = {}, []
        self.rows, self.cur = [], [-1]
        self.modal, self.sel, self.chosen, self.ok_val = 0, -2, 0, 0
        cpu.stub_import(iat['SendMessageA'], self.send, pop_bytes=16)
        cpu.stub_import(iat['ShowCursor'], lambda c: 0, pop_bytes=4)
        cpu.stub_import(iat['_stricmp'], lambda c: Exe.stricmp(self.cstr(c.arg(0)), self.cstr(c.arg(1))))
        cpu.stub_function(0x5abc18, lambda c: 1)  # CDialog::OnInitDialog
        cpu.stub_function(0x5abc0c, lambda c: 0)  # CDialog::OnOK
        cpu.stub_function(0x5abd9e, lambda c: 0)  # CWnd::OnDestroy
        cpu.stub_function(0x5abc1e, self.domodal)  # CDialog::DoModal
        cpu.stub_function(0x432b60, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x4aef00, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x577340, lambda c: self.log.append('gdi') or 0)
        cpu.stub_function(0x51a300, lambda c: self.log.append('keys') or 0)
        for dtor in (0x5abe46, 0x5abd2c, 0x5abd20):
            cpu.stub_function(dtor, lambda c: 0)
        me, none = cpu.code(bytes.fromhex('89c8c3')), cpu.code(bytes.fromhex('31c0c3'))
        one = cpu.code(bytes.fromhex('b801000000c3'))
        self.vts = {}
        for kind, (x2c, x40) in {1: (me, none), 2: (me, one), 0: (none, none)}.items():
            vt = cpu.alloc(0x110)
            cpu.write(vt, bytes(0x110))
            cpu.u32(vt + 0x2c, x2c)
            cpu.u32(vt + 0x40, x40)
            vt108 = cpu.alloc(0x10)
            cpu.stub_function(vt108, lambda c: self.log.append(f'take {self.addr_ix.get(c.ecx, -1)}') or 0)
            cpu.u32(vt + 0x108, vt108)
            self.vts[kind] = vt
        self.addr_ix = {}

    cs = Exe.cs
    cstr = Exe.cstr

    def send(self, c):
        m, w, l = c.arg(1), c.arg(2), c.arg(3)
        rows, cur = self.rows, self.cur
        if m in (0x180, 0x143):  # LB_ / CB_ADDSTRING
            rows.append([self.cstr(l), None])
            return len(rows) - 1
        if m in (0x19a, 0x151):  # SETITEMDATA
            rows[w][1] = l
        if m in (0x186, 0x14e):  # SETCURSEL
            cur[0] = w if w < len(rows) else -1
            return cur[0] & 0xffffffff
        if m in (0x188, 0x147):  # GETCURSEL
            return cur[0] & 0xffffffff
        if m in (0x199, 0x150):  # GETITEMDATA
            return rows[w][1] if 0 <= w < len(rows) else 0xffffffff
        return 0

    def domodal(self, c):
        self.log.append('modal')
        if self.kind == 'mech':
            self.cpu.u32(c.ecx + 0xa0, self.ok_val & 0xffffffff)
        else:
            self.cpu.u32(c.ecx + 0x60, self.chosen)
        return self.modal & 0xffffffff

    def dialog(self):
        d = self.cpu.alloc(0x100)
        self.cpu.write(d, bytes(0x100))
        self.cpu.u32(d + 0x80, 0x5150)
        self.cpu.u32(d + 0x84, 0x5150)
        return d

    # 0x432d30's rows, 0x432da0's answer on the row chosen, then 0x432c60 (0x432df0 run, DoModal standing in).
    def mech_list(self, modal, sel):
        cpu = self.cpu
        self.kind, self.rows, self.cur, self.log = 'mech', [], [-1], []
        dlg = self.dialog()
        cpu.call(0x432d30, ecx=dlg)
        out = ''.join(f'{n}={d};' for n, d in self.rows) + f'sel {self.cur[0]};'
        if sel != -2:
            self.cur[0] = sel
        cpu.call(0x432da0, ecx=dlg)
        self.ok_val = struct.unpack('<i', cpu.read(dlg + 0xa0, 4))[0]
        self.modal, self.log = modal, []
        cpu.call(0x432c60)
        r = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        ev = ''.join(x + ';' for x in self.log if x != 'modal')
        return out + '|' + ev + str(r)

    # zEntity's list (0x73655c..0x736560) of fake entities, 0x4af000's rows, 0x4aefb0's close on the chosen row, then
    # 0x4a3550 with DoModal standing in (its +0x60 the close's).
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
        cpu.u32(0x73655c, arr)
        cpu.u32(0x736560, arr + 4 * len(ents))
        self.kind, self.rows, self.cur, self.log = 'vehicle', [], [-1], []
        dlg = self.dialog()
        cpu.u32(dlg + 0x60, 0)  # the exe leaves it unset
        cpu.call(0x4af000, ecx=dlg)
        out = ''.join(f'{n}={self.addr_ix[d]};' for n, d in self.rows) + f'sel {self.cur[0]};'
        if sel != -2:
            self.cur[0] = sel
        cpu.call(0x4aefb0, ecx=dlg)
        self.chosen = struct.unpack('<I', cpu.read(dlg + 0x60, 4))[0]
        self.modal, self.log = modal, []
        if modal != 0 and self.chosen == 0:
            return 'gdi;' + out + '|-1'  # the exe would call through its unset +0x60 (the port's FIX (A))
        cpu.call(0x4a3550)
        t = [x for x in self.log if x.startswith('take')]
        return ('gdi;' if 'gdi' in self.log else '') + out + '|' + (t[0].split()[1] if t else '-1')


class Camera:
    """Camera Properties (0x40bb00) with its window (0x40bd00) and DoModal (0x40bea0) standing in."""

    def __init__(self):
        self.cpu = cpu = Original()
        cpu.import_code(0x5bb5f4, bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3'))  # _CIasin, as test_matrix.py
        cpu.stub_function(0x40bd00, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x40bea0, self.domodal)
        cpu.stub_function(0x5abd20, lambda c: 0)

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
        cpu.call(0x40bb00, ecx=mode)
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        after = struct.unpack('<4f', cpu.read(mode + 8, 16)) + struct.unpack('<3f', cpu.read(mode + 0x18, 12)) + \
            struct.unpack('<4f', cpu.read(mode + 0x28, 16))
        return r, self.shown, list(after)


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def main():
    p = load_probe()
    i32p = ctypes.POINTER(ctypes.c_int32)
    p.probe_dd_menu.argtypes = [ctypes.c_uint32, i32p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_mech_list.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32]
    p.probe_dd_vehicle.argtypes = [i32p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32,
                                   ctypes.c_char_p, ctypes.c_int32]
    fp = ctypes.POINTER(ctypes.c_float)
    p.probe_dd_camera.argtypes = [fp, ctypes.c_int32, fp, ctypes.c_int32, fp]
    exe = Exe()
    r = random.Random(0x483170)
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
             r.choice((-1, 0, 4, 18))] + [r.choice((0, 1, 1, 0x80)) for _ in range(13)] + [r.randrange(2)]
        mech = r.choice(MECHS)
        for addr in UI_HANDLERS + COMMANDS:
            want = exe.menu(addr, s, names, mech)
            k = p.probe_dd_menu(addr, (ctypes.c_int32 * 20)(*s), '\n'.join(names).encode(), mech.encode(), buf, len(buf))
            report(f'menu {addr:#x} {s} {names} {mech!r}', want, buf.raw[:k].decode('latin-1'))
            n += 1
    lists, cam = Lists(), Camera()
    for modal, sel in ((1, -2), (2, -2), (1, 0), (1, 5), (1, 18), (2, 7), (1, -1), (0, 3)):
        want = lists.mech_list(modal, sel)
        k = p.probe_dd_mech_list(modal, sel, buf, len(buf))
        report(f'mech list {modal} {sel}', want, buf.raw[:k].decode('latin-1'))
        n += 1
    vnames = ['hero', 'lance1', 'tank_a', 'stealth', 'enemy7', 'cop']
    for trial in range(120):
        ents = [(r.choice((-1, 0, 1, 2, 2)), r.randrange(len(vnames))) for _ in range(r.randrange(0, 7))]
        mechs = sum(1 for k, _ in ents if k == 2)
        modal, sel = r.choice((0, 1, 2, -1)), r.choice([-2, -2] + list(range(mechs)))  # a row the list has
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
        if r.random() < 0.1:
            q = [1, 0, 0, 0]
        m = [struct.unpack('<f', struct.pack('<f', v))[0] for v in
             q + [r.uniform(-30, 30) for _ in range(3)] + [r.uniform(0, 80), r.uniform(0, 2), r.uniform(0, 10), r.uniform(0, 5)]]
        ok = r.choice((0, 1, 1))
        edit = [r.uniform(0, 80)] + [r.choice((0.0, r.uniform(-30, 30))) for _ in range(3)] + \
            [r.uniform(-180, 180) for _ in range(3)] + [r.uniform(0, 5), r.uniform(0, 10), r.uniform(0, 2)]
        edit = [struct.unpack('<f', struct.pack('<f', v))[0] for v in edit]
        norm = r.randrange(2)
        res, shown, after = cam.run(m, ok, edit, norm)
        o = (ctypes.c_float * 21)()
        got_r = p.probe_dd_camera((ctypes.c_float * 11)(*m), ok, (ctypes.c_float * 10)(*edit), norm, o)
        # the angles within 1e-4 degrees (asin is the C runtime's, as test_matrix.py allows); the rest bit for bit
        near = all(abs(a - b) <= 1e-4 + 1e-6 * abs(a) for a, b in zip(shown[4:7], o[4:7]))
        want = (res, [bits(v) for k, v in enumerate(shown) if k not in (4, 5, 6)], [bits(v) for v in after])
        got = (got_r, [bits(v) for k, v in enumerate(o[:10]) if k not in (4, 5, 6)], [bits(v) for v in o[10:]])
        report(f'camera {m} ok {ok} norm {norm}', want, got)
        if not near:
            report(f'camera angles {m}', shown[4:7], list(o[4:7]))
        n += 1
    print(f'{n} cases: {bad} differ')
    if bad:
        raise SystemExit(1)
    print('ok: the debug menu and dialogs as the original')


if __name__ == '__main__':
    main()
