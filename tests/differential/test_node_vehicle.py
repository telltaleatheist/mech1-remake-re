"""The release's Add veh to node (mech3/node_vehicle_dlg; docs/editor.md, Add veh to node) against Mech3.exe 1.2:
template 141's team and side (0x49d810 over random packed teams and friend/foe masks; the Friend, Foe and Other
buttons 0x49d8d0 / 0x49d920 / 0x49d970; the team typed, 0x49d9e0), the close's yaw in radians (0x49d7b0), the
record 0x4df6a0 builds (every byte of its 0xcc, the defaults the exe's own), and the row's step 0x403820 over every
branch (no editor, no pick, the dialog's result, no type, the net found, rebuilt or not, 0x4671d0's answer; the
dialog, the net and 0x4671d0 stood in, 0x4e0540 and 0x4df6a0 the exe's). Then template 152's Conform (0x404090 ->
0x403ec0, mech3/editor_conform) over random elements and ground hits (0x4edab0 stood in), at the x87's 24 bits.

    cmake --build build --target mw3probe && python3 tests/differential/test_node_vehicle.py
"""
import ctypes
import itertools
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import os  # noqa: E402
os.environ.setdefault('MW3_X87', '24')  # the editor runs with the game up (CLAUDE.md)
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import iat_slots  # noqa: E402

PILOT, EDITOR = 0x60861c, 0x5f4c6c
f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]
bits = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        iat = iat_slots()
        self.strs = {}
        pilot, self.mech = cpu.alloc(0x40), cpu.alloc(0x100)
        cpu.u32(PILOT, pilot)
        cpu.u32(pilot + 0x14, self.mech)
        self.update = None
        cpu.stub_function(0x5abd8c, lambda c: (self.update(c.arg(0)) if self.update else 0) or 0, pop_bytes=4)
        cpu.stub_function(0x5abd9e, lambda c: 0)  # CWnd::OnDestroy
        cpu.stub_import(iat['SendMessageA'], lambda c: 0 if c.arg(1) == 0x147 else 0x4321, pop_bytes=16)
        cpu.stub_import(iat['ShowCursor'], lambda c: 0, pop_bytes=4)
        # the flow's
        self.log = []
        put = lambda s: self.log.append(s) or 0
        self.editor = cpu.alloc(0x40)
        holder, hvt = cpu.alloc(0x10), cpu.alloc(0x40)
        ready = cpu.alloc(0x10)
        cpu.stub_function(ready, lambda c: put('ready') or (self.net if self.s[4] else 0))
        cpu.u32(hvt + 0x34, ready)
        cpu.u32(holder, hvt)
        cpu.u32(self.editor + 0x14, holder)
        self.net, nvt = cpu.alloc(0x10), cpu.alloc(0x40)
        getid = cpu.alloc(0x10)
        cpu.stub_function(getid, lambda c: self.s[6])
        cpu.u32(nvt + 0x18, getid)
        cpu.u32(self.net, nvt)
        self.mark, node, nodevt, getpos = cpu.alloc(0x20), cpu.alloc(0x10), cpu.alloc(0x40), cpu.alloc(0x10)
        self.posbuf = cpu.alloc(0x10)
        cpu.stub_function(getpos, lambda c: self.posbuf)
        cpu.u32(nodevt + 0x1c, getpos)
        cpu.u32(node, nodevt)
        cpu.u32(self.mark + 0xc, node)
        cpu.stub_function(0x403770, lambda c: self.mark if self.s[1] else 0)
        cpu.stub_function(0x402f10, lambda c: (None if self.made else put('box ' + self.cstr(c.ecx))) or 0, pop_bytes=4)
        cpu.stub_function(0x49d350, lambda c: c.ecx, pop_bytes=4)
        cpu.stub_function(0x49d8b0, self.modal)
        cpu.stub_function(0x4e0490, lambda c: c.ecx)
        cpu.stub_function(0x4e04f0, lambda c: 0)
        cpu.stub_function(0x5abd2c, lambda c: 0)
        cpu.stub_function(0x5abd20, lambda c: 0)
        cpu.stub_function(0x4bd340, lambda c: 0)
        cpu.stub_function(0x4be670, lambda c: put('rebuild') or (self.net if self.s[5] else 0))
        cpu.stub_function(0x4671d0, self.entry, pop_bytes=0x10)
        cpu.stub_import(iat['sprintf'], lambda c: 0)
        for a, pop in ((0x556740, 4), (0x4e0680, 4), (0x5569e0, 0), (0x556820, 0)):
            cpu.stub_function(a, lambda c: 0, pop_bytes=pop)
        cpu.stub_function(0x49fce0, lambda c: put(f'made ttruck_01 {self.s[6]}'), pop_bytes=0xc)
        self.ttruck = self.cs('ttruck')

    def cs(self, s):
        if s not in self.strs:
            a = self.cpu.alloc(len(s) + 1)
            self.cpu.write(a, s.encode() + b'\0')
            self.strs[s] = a
        return self.strs[s]

    def cstr(self, a):
        return self.cpu.read(a, 64).split(b'\0')[0].decode('latin-1')

    def masks(self, friends, foes):
        self.cpu.u32(self.mech + 0xc4, friends)
        self.cpu.u32(self.mech + 0xc8, foes)

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def team(self, op, arg, friends, foes, before):
        cpu = self.cpu
        self.masks(friends, foes)
        d = cpu.alloc(0x100)
        cpu.write(d, bytes(0x100))
        cpu.u32(d + 0xb4, before[0] & 0xffffffff)
        cpu.u32(d + 0xb8, before[1] & 0xffffffff)
        if op == 0:
            self.update = None
            cpu.call(0x49d810, ecx=d, stack_args=(arg,))
        elif op == 1:
            self.update = lambda save: cpu.u32(d + 0xb8, arg) if save else None
            cpu.call((0x49d8d0, 0x49d920, 0x49d970)[arg], ecx=d)
        else:
            self.update = None
            cpu.call(0x49d9e0, ecx=d)
        self.update = None
        return [self.i32(d + 0xb4), self.i32(d + 0xb8)]

    def close(self, yaw):
        d = self.cpu.alloc(0x100)
        self.cpu.write(d, bytes(0x100))
        self.cpu.write(d + 0xac, struct.pack('<f', yaw))
        self.cpu.call(0x49d7b0, ecx=d)
        return bits(struct.unpack('<f', self.cpu.read(d + 0xac, 4))[0])

    def record(self, net, f, team):
        r = self.cpu.alloc(0x100)
        self.cpu.write(r, bytes(0x100))
        self.cpu.call(0x4df6a0, ecx=r, stack_args=(net & 0xffffffff, bits(f[0]), bits(f[1]), bits(f[2]), bits(f[3]),
                                                   team & 0xffffffff))
        return self.cpu.read(r + 4, 0xcc)

    def modal(self, c):
        cpu, d = self.cpu, c.ecx
        rd = lambda o: struct.unpack('<I', cpu.read(d + o, 4))[0]
        self.log.append(f'modal {rd(0xa4):08x} {rd(0xa8):08x} {rd(0xb0):08x} {rd(0xac):08x}')
        cpu.u32(d + 0x60, self.ttruck if self.s[3] else 0)
        cpu.write(d + 0xac, struct.pack('<f', self.pos[3]))
        cpu.u32(d + 0xb4, int(self.pos[4]) & 0xffffffff)
        return self.s[2] & 0xffffffff

    def entry(self, c):
        r = c.arg(3)
        v = struct.unpack('<iIIIIi', self.cpu.read(r + 4, 24))
        t = self.cstr(c.arg(2)) if c.arg(2) else '-'
        self.log.append(f'entry {t} rec {v[0]} {v[1]:08x} {v[2]:08x} {v[3]:08x} {v[4]:08x} {v[5]}')
        self.made = bool(self.s[7])
        return self.s[7]

    def add(self, s, pos):
        cpu = self.cpu
        self.s, self.pos, self.log, self.made = s, pos, [], False
        cpu.u32(EDITOR, self.editor if s[0] else 0)
        cpu.write(self.posbuf, struct.pack('<3f', *pos[:3]))
        cpu.call(0x403820, stack_args=(0,))
        return ''.join(x + ';' for x in self.log)


class Conform:
    """0x404090 over fake elements on an edit object (+0x14 the list, +0x138 modified)."""

    def __init__(self):
        self.cpu = cpu = Original()
        self.edit, lst, lvt = cpu.alloc(0x200), cpu.alloc(0x10), cpu.alloc(0x40)
        cnt, at = cpu.alloc(0x10), cpu.alloc(0x10)
        cpu.stub_function(cnt, lambda c: len(self.elems))
        cpu.stub_function(at, lambda c: self.addrs[c.arg(0)], pop_bytes=4)
        cpu.u32(lvt + 0x10, cnt)
        cpu.u32(lvt + 0x0c, at)
        cpu.u32(lst, lvt)
        cpu.u32(self.edit + 0x14, lst)
        self.evt = cpu.alloc(0x40)
        pos, setp, moved = cpu.alloc(0x10), cpu.alloc(0x10), cpu.alloc(0x10)
        cpu.stub_function(pos, self.position)
        cpu.stub_function(setp, self.set, pop_bytes=4)
        cpu.stub_function(moved, lambda c: self.log.append(f'moved {self.idx[c.ecx]}') or 0)
        cpu.u32(self.evt + 0x18, pos)
        cpu.u32(self.evt + 0x2c, setp)
        cpu.u32(self.evt + 0x38, moved)
        world = cpu.alloc(0x10)
        cpu.u32(0x608c84, world)
        cpu.stub_function(0x4edab0, self.ground, pop_bytes=12)
        self.current = None

    def position(self, c):
        self.current = self.idx[c.ecx]
        return self.pos_of[c.ecx]

    def set(self, c):
        y = struct.unpack('<I', self.cpu.read(c.arg(0) + 4, 4))[0]
        i = self.idx[c.ecx]
        self.log.append(f'set {i} {y:08x}')
        return self.elems[i]['set']

    def ground(self, c):
        e = self.elems[self.current]
        self.cpu.write(c.edx, bytes([len(e['hits'])]))
        for k, h in enumerate(e['hits']):
            self.cpu.write(c.edx + 4 + 0x28 * k + 0x10, struct.pack('<I', h))
        return 0 if e['found'] else 1

    def run(self, elems):
        cpu = self.cpu
        self.elems, self.log, self.addrs, self.pos_of, self.idx = elems, [], [], {}, {}
        for i, e in enumerate(elems):
            if not e['present']:
                self.addrs.append(0)
                continue
            a, p = cpu.alloc(0x10), cpu.alloc(0x10)
            cpu.u32(a, self.evt)
            cpu.write(p, struct.pack('<3I', *e['pos']))
            self.addrs.append(a)
            self.pos_of[a], self.idx[a] = p, i
        cpu.u32(self.edit + 0x138, 0)
        cpu.call(0x404090, ecx=self.edit)
        return ''.join(x + ';' for x in self.log) + f"|modified {struct.unpack('<I', cpu.read(self.edit + 0x138, 4))[0]}"


def main():
    p = load_probe()
    i32p, fp = ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)
    p.probe_nv_team.argtypes = [ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, i32p]
    p.probe_nv_close.argtypes = [ctypes.c_float]
    p.probe_nv_close.restype = ctypes.c_float
    p.probe_nv_record.argtypes = [ctypes.c_int32, fp, ctypes.c_int32, ctypes.c_char_p]
    p.probe_nv_add.argtypes = [i32p, fp, ctypes.c_char_p, ctypes.c_int32]
    exe = Exe()
    r = random.Random(0x49d810)
    bad = n = 0

    def report(what, want, got):
        nonlocal bad, n
        n += 1
        if want != got:
            bad += 1
            if bad <= 8:
                print(f'{what}:\n  exe  {want}\n  port {got}')

    def mask():
        return r.choice((0, 1, 2, 6, 0x80000000, 0xffffffff, r.getrandbits(32), 1 << r.randrange(32)))

    for trial in range(1500):
        friends, foes = mask(), mask()
        op = r.randrange(3)
        before = [r.choice((0, 1, 5, 33, 0x421, r.getrandbits(31))), r.randrange(3)]
        arg = r.randrange(3) if op == 1 else r.choice((0, 1, 2, 31, 32, 33, 0x421, 0xffffffff, r.getrandbits(32)))
        want = exe.team(op, arg, friends, foes, before)
        out = (ctypes.c_int32 * 2)(*before)
        p.probe_nv_team(op, arg, friends, foes, out)
        report(f'team op {op} arg {arg:#x} friends {friends:#x} foes {foes:#x} before {before}', want, list(out))
    for trial in range(300):
        yaw = f32(r.choice((0.0, 90.0, -180.0, r.uniform(-720, 720))))
        report(f'close yaw {yaw}', exe.close(yaw), bits(p.probe_nv_close(yaw)))
    buf = ctypes.create_string_buffer(0x1000)
    for trial in range(200):
        f = [f32(r.uniform(-3000, 3000)) for _ in range(3)] + [f32(r.uniform(-7, 7))]
        net, team = r.randrange(-1, 40), r.choice((0, 1, 2, 0x421))
        p.probe_nv_record(net, (ctypes.c_float * 4)(*f), team, buf)
        report(f'record net {net} {f} {team}', exe.record(net, f, team), buf.raw[:0xcc])
    for s in itertools.product((0, 1), (0, 1), (0, 1, 2), (0, 1), (0, 1), (0, 1), (7,), (0, 1)):
        if not s[0] and any(s[1:6]):
            continue
        pos = [f32(r.uniform(-500, 500)) for _ in range(3)] + [f32(r.uniform(-180, 180)), r.choice((0, 2, 33))]
        want = exe.add(list(s), pos)
        k = p.probe_nv_add((ctypes.c_int32 * 8)(*s), (ctypes.c_float * 5)(*pos), buf, len(buf))
        report(f'add {s}', want, buf.raw[:k].decode())
    p.probe_conform.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p, ctypes.c_int32]
    conform = Conform()
    for trial in range(400):
        elems = []
        for _ in range(r.randrange(0, 5)):
            y = f32(r.uniform(-50, 150))
            hits = [bits(f32(r.choice((y, y - 0.05, y + 0.3, y - 0.7, r.uniform(-100, 200))))) for _ in range(r.randrange(0, 7))]
            elems.append(dict(present=r.random() > 0.1, pos=[bits(f32(r.uniform(-900, 900))), bits(y),
                              bits(f32(r.uniform(-900, 900)))], found=r.random() > 0.15, hits=hits, set=r.choice((0, 1, 1))))
        want = conform.run(elems)
        flat = [len(elems)]
        for e in elems:
            flat += [int(e['present'])] + e['pos'] + [int(e['found']), len(e['hits'])] + e['hits'] + [e['set']]
        k = p.probe_conform((ctypes.c_uint32 * len(flat))(*flat), buf, len(buf))
        report(f'conform {elems}', want, buf.raw[:k].decode())
    print(f'{n} cases: {bad} differ')
    if bad:
        raise SystemExit(1)
    print("ok: the release's Add veh to node as the original")


if __name__ == '__main__':
    main()
