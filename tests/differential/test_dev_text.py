"""The developers' text (mech3/dev_text, gamez/ztwod/twod_lines; hud.md, The developer text) against Mech3.exe 1.2:
the motion state names (0x43cc00), the zones text (0x43cb10), the line log's add and colours (0x5a3e80, 0x5a3e30,
with set_timer 0x5a0050), the readout (0x40fb50) and the frame rate (0x40f1f0). The exe's own show, hide and
set_text (0x59ff10, 0x59ff50, 0x5a11b0) run on the lines and rows; CString and the CRT's text calls are stood in.

    cmake --build build --target mw3probe && python3 tests/differential/test_dev_text.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from twod_world import c_format  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402

STATE_NAME, ZONES_TEXT = 0x43cc00, 0x43cb10
LOG_ADD, LOG_COLORS = 0x5a3e80, 0x5a3e30
READOUT, FPS_UPDATE = 0x40fb50, 0x40f1f0
SHOW_FOR, HIDE, SET_TEXT, RENDERED = 0x59ff10, 0x59ff50, 0x5a11b0, 0x5a10b0
IAT = {'_vsnprintf': 0x5bb64c, 'strncmp': 0x5bb654, 'strncpy': 0x5bb648}
CS_CTOR, CS_DTOR, CS_FORMAT, CS_ADD_CS, CS_ADD_SZ, ATEXIT = 0x5abdb0, 0x5abda4, 0x5abdaa, 0x5abe4c, 0x5abe5e, 0x5ac140
LINE = 0x4b0
ROWS, READOUT_ON = 0x5fc848, 0x603f8c


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        self.r32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]
        self.strings = {}  # CString address -> its text
        ret = cpu.code(bytes.fromhex('c3'))
        # A FontText's vtable: the exe's show, hide and set_text, its render (+0xac) and +0x20 nothing.
        self.line_vt = vt = cpu.alloc(0xc0)
        cpu.write(vt, bytes(0xc0))
        for off, fn in ((0x20, ret), (0x60, SHOW_FOR), (0x64, HIDE), (0x7c, SET_TEXT), (0xac, ret)):
            cpu.u32(vt + off, fn)
        cpu.stub_import(IAT['_vsnprintf'], self.vsnprintf)
        cpu.stub_import(IAT['strncmp'], self.strncmp)
        cpu.stub_import(IAT['strncpy'], self.strncpy)
        cpu.stub_function(CS_CTOR, lambda c: self.cs_set(c.ecx, '') or c.ecx)
        cpu.stub_function(CS_DTOR, lambda c: 0)
        cpu.stub_function(ATEXIT, lambda c: 0)
        cpu.stub_function(CS_FORMAT, self.cs_format)
        cpu.stub_function(CS_ADD_CS, lambda c: self.cs_set(c.ecx, self.strings[c.ecx] + self.strings[c.arg(0)]) or
                          c.ecx, pop_bytes=4)
        cpu.stub_function(CS_ADD_SZ, lambda c: self.cs_set(c.ecx, self.strings[c.ecx] + self.cstr(c.arg(0))) or
                          c.ecx, pop_bytes=4)
        self.log_vt = cpu.alloc(0x10)
        self.active = 0
        set_active = cpu.alloc(0x10)
        cpu.stub_function(set_active, lambda c: setattr(self, 'active', c.arg(0)) or 0, pop_bytes=4)
        cpu.u32(self.log_vt + 4, set_active)
        self.texts = None
        self.vid = cpu.alloc(0x20)
        self.vid_out = (0, 0, 0)
        cpu.stub_function(self.vid, self.video_memory, pop_bytes=4)
        cpu.u32(0x72f38c, self.vid)
        self.pool = (0, 0, 0, 0)
        cpu.stub_function(0x581e50, lambda c: [cpu.u32(c.ecx + 4 * i, v) for i, v in enumerate(self.pool)] and 0)

    def cstr(self, a):
        out = bytearray()
        while True:
            chunk = self.cpu.read(a, 64)
            z = chunk.find(b'\0')
            if z >= 0:
                return (out + chunk[:z]).decode('latin-1')
            out += chunk
            a += 64

    def cs_set(self, cs, text):
        self.strings[cs] = text
        buf = self.cpu.alloc(len(text) + 1)
        self.cpu.write(buf, text.encode('latin-1') + b'\0')
        self.cpu.u32(cs, buf)

    def cs_format(self, c):
        self.cs_set(c.arg(0), c_format(self, self.cstr(c.arg(1)), c.uc.reg_read(UC_X86_REG_ESP) + 12))
        return 0

    def vsnprintf(self, c):
        buf, n, fmt, va = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        s = c_format(self, self.cstr(fmt), va).encode('latin-1')[:n - 1]
        self.cpu.write(buf, s + b'\0')
        return len(s)

    def strncmp(self, c):
        a, b = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)]
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def strncpy(self, c):
        s = self.cstr(c.arg(1)).encode('latin-1')[:c.arg(2) - 1]
        self.cpu.write(c.arg(0), s + b'\0')
        return c.arg(0)

    def video_memory(self, c):
        ok, total, free = self.vid_out
        self.cpu.u32(c.edx, total)
        self.cpu.u32(c.arg(0), free)
        return ok

    def eax(self):
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def state_name(self, state):
        self.cpu.call(STATE_NAME, ecx=state)
        return self.cstr(self.eax())

    def zones(self, zs):
        a = self.cpu.alloc(4)
        self.cpu.write(a, bytes(zs))
        self.cpu.call(ZONES_TEXT, ecx=a)
        return self.cstr(self.r32(self.eax()))

    def new_line(self, at, top=0x996a00, bottom=0x95c7ff):
        cpu = self.cpu
        cpu.write(at, bytes(LINE))
        cpu.u32(at, self.line_vt)
        cpu.u32(at + 0xc, 0x10)  # hidden
        cpu.u32(at + 0x24c, top)
        cpu.u32(at + 0x250, bottom)

    def line(self, at):
        r = self.r32
        return [r(at + 0xc) & 0x11, r(at + 0x10), r(at + 0x24c), r(at + 0x250), self.cstr(at + 0x34)]

    def linelog(self, ops):
        cpu = self.cpu
        log = cpu.alloc(0x10 + 15 * LINE)
        cpu.write(log, bytes(0x10))
        cpu.u32(log, self.log_vt)
        for k in range(15):
            self.new_line(log + 0x10 + k * LINE)
        self.active = 0
        out = []
        for op in ops:
            if op[0] == 'a':
                t = cpu.alloc(len(op[2]) + 1)
                cpu.write(t, op[2].encode() + b'\0')
                cpu.call(LOG_ADD, ecx=log, stack_args=(t, fbits(op[1])))
            elif op[0] == 'k':
                at = log + 0x10 + op[1] * LINE
                cpu.u32(at + 0x24c, op[2])
                cpu.u32(at + 0x250, op[3])
            else:
                cpu.call(LOG_COLORS, ecx=log, stack_args=(op[1], op[2]))
            out.append([self.active] + [self.line(log + 0x10 + k * LINE) for k in range(15)])
        return out

    def readout(self, zone, counts, pool, vid, clear):
        cpu = self.cpu
        for k in range(25):
            self.new_line(ROWS + k * LINE)
        cpu.u32(0x60bb98, zone)
        for addr, v in zip((0x60bf3c, 0x60bf34, 0x7402e8, 0x7402e0, 0x63fbcc, 0x63fbc4), counts):
            cpu.u32(addr, v)
        self.pool, self.vid_out = pool, vid
        cpu.u32(READOUT_ON, 0)
        for mode in (1, 2) + ((0,) if clear else ()):
            cpu.call(READOUT, ecx=mode)
        return [[self.r32(ROWS + k * LINE + 0xc) & 0x10, self.cstr(ROWS + k * LINE + 0x34)] for k in range(25)]

    def fps(self, steps, net, frame, entities):
        cpu = self.cpu
        cpu.write(0x603d78, b'\1')  # the format's CString made
        self.cs_set(0x603d80, '')
        if self.texts is None:  # once: a stub's hook stays (the readout ran the exe's set_text before)
            cpu.stub_function(SET_TEXT, lambda c: self.texts.append(c_format(self, self.cstr(c.arg(1)), 0)) or 0)
            cpu.stub_function(0x59ffb0, lambda c: 0, pop_bytes=4)
        self.texts = texts = []
        f = cpu.alloc(0x4c0)
        cpu.write(f, bytes(0x4c0))
        cpu.write(0x605fb4, bytes([net[0] & 0xff]))
        for addr, v in zip((0x605fbc, 0x605fc0, 0x605f88), net[1:]):
            cpu.u32(addr, v)
        cpu.u32(0x6099c0, frame)
        begin = 0x10000000 if entities else 0
        cpu.u32(0x73655c, begin)
        cpu.u32(0x736560, begin + 4 * entities)
        out = []
        for s in steps:
            cpu.u32(0x72c534, fbits(s))
            cpu.call(FPS_UPDATE, ecx=f, stack_args=(0,))
            out.append([self.r32(f + 0x4b0), self.r32(f + 0x4b8 - 4), self.r32(f + 0x4b8), texts[-1]])
        return out


def split(raw, n):
    """Records of `n` u32s then a string."""
    out, i = [], 0
    while i < len(raw):
        vals = list(struct.unpack_from(f'<{n}I', raw, i))
        i += 4 * n
        z = raw.index(b'\0', i)
        out.append(vals + [raw[i:z].decode('latin-1')])
        i = z + 1
    return out


def main():
    p = load_probe()
    buf = ctypes.create_string_buffer(1 << 16)
    p.probe_dev_state_name.argtypes = [ctypes.c_int32, ctypes.c_char_p, ctypes.c_int]
    p.probe_dev_zones.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_dev_linelog.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    i32p = ctypes.POINTER(ctypes.c_int32)
    p.probe_dev_readout.argtypes = [ctypes.c_uint32, i32p, i32p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int,
                                    ctypes.c_char_p, ctypes.c_int]
    p.probe_dev_fps.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_int, i32p, ctypes.c_int32, ctypes.c_int32,
                                ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x40fb50)
    bad = 0

    def check(what, want, got):
        nonlocal bad
        if want != got:
            bad += 1
            if bad <= 6:
                print(f'{what}:\n  exe  {want}\n  port {got}')

    for st in list(range(-2, 17)) + [0x7fffffff]:
        n = p.probe_dev_state_name(st, buf, len(buf))
        check(f'state {st}', exe.state_name(st), buf.raw[:n - 1].decode())

    for zs in ([0, 0, 0, 0], [1, 5, 0, 0], [2, 7, 255, 0], [3, 1, 2, 3], [4, 9, 8, 7], [255, 1, 1, 1]) + \
            tuple([r.randrange(6)] + [r.randrange(256) for _ in range(3)] for _ in range(20)):
        n = p.probe_dev_zones(bytes(zs), buf, len(buf))
        check(f'zones {zs}', exe.zones(zs), buf.raw[:n - 1].decode())

    words = ('Lance mate down', 'Objective complete', 'Nav Alpha', 'Mission failed', 'Jenner destroyed', '')
    logs = 0
    for trial in range(40):
        ops = []
        for _ in range(r.randrange(1, 24)):
            if r.random() < 0.1:
                ops.append(('c', r.getrandbits(24), r.getrandbits(24)))
            elif r.random() < 0.15:  # one line's colours set straight, so a scroll has colours to carry
                ops.append(('k', r.randrange(15), r.getrandbits(24), r.getrandbits(24)))
            else:
                text = r.choice(words) if r.random() < 0.6 else f'msg {r.randrange(1000)}'
                ops.append(('a', r.choice((-1.0, 0.0, 2.5, 4.0, r.uniform(-3, 10))), text))
        packed = b''.join(b'a' + struct.pack('<f', o[1]) + o[2].encode() + b'\0' if o[0] == 'a'
                          else b'k' + bytes([o[1]]) + struct.pack('<II', o[2], o[3]) if o[0] == 'k'
                          else b'c' + struct.pack('<II', o[1], o[2]) for o in ops)
        n = p.probe_dev_linelog(packed, len(packed), buf, len(buf))
        assert n > 0
        raw, got, i = buf.raw[:n], [], 0
        while i < n:
            active = struct.unpack_from('<I', raw, i)[0]
            i += 4
            lines = []
            for _ in range(15):
                vals = list(struct.unpack_from('<4I', raw, i))
                i += 16
                z = raw.index(b'\0', i)
                lines.append(vals + [raw[i:z].decode()])
                i = z + 1
            got.append([active] + lines)
        want = exe.linelog(ops)
        for k, (w, g) in enumerate(zip(want, got)):
            check(f'log {trial} op {k} {ops[k]}', w, g)
        logs += len(ops)

    reads = 0
    for trial in range(60):
        cnt = r.randrange(5)
        zone = cnt | (r.getrandbits(8) << 8) | (r.getrandbits(8) << 16) | (r.getrandbits(8) << 24)
        counts = [r.randrange(-5, 100000) for _ in range(6)]
        pool = (r.randrange(400), r.randrange(400), r.randrange(400), r.randrange(-3, 50))
        total = r.choice((0, 1024, 4 << 20, r.getrandbits(32)))
        vid = (r.randrange(2), total, r.randrange(total + 1) if total else r.getrandbits(16))
        clear = r.randrange(4) == 0
        n = p.probe_dev_readout(zone, (ctypes.c_int32 * 6)(*counts), (ctypes.c_int32 * 4)(*pool),
                                (ctypes.c_uint32 * 3)(*vid), clear, buf, len(buf))
        got = [[v[0], v[1]] for v in split(buf.raw[:n], 1)]
        check(f'readout zone {zone:#x} counts {counts} pool {pool} vid {vid} clear {clear}',
              exe.readout(zone, counts, pool, vid, clear), got)
        reads += 1

    frames = 0
    for trial in range(30):
        steps = [r.choice((0.0, 1 / 30, 1 / 60, 0.5, 1.0, r.uniform(0, 0.3))) for _ in range(r.randrange(1, 80))]
        net = (r.randrange(2), r.randrange(-5, 99), r.randrange(-5, 99), r.randrange(-5, 99))
        frame, ents = r.randrange(100000), r.randrange(30)
        n = p.probe_dev_fps((ctypes.c_float * len(steps))(*steps), len(steps), (ctypes.c_int32 * 4)(*net), frame,
                            ents, buf, len(buf))
        got = split(buf.raw[:n], 3)
        want = exe.fps(steps, net, frame, ents)
        for k, (w, g) in enumerate(zip(want, got)):
            check(f'fps {trial} frame {k} net {net}', w, g)
        frames += len(steps)

    print(f'22 states, 26 zone lists, {logs} line log steps, {reads} readouts, {frames} frames: {bad} differ')
    if bad:
        raise SystemExit(1)
    print('ok: the developers\' text as the original')


if __name__ == '__main__':
    main()
