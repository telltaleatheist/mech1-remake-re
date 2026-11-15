"""The maths module's routines nothing calls (gamez/zmath/zmth_unused.h) against the original x86: the cross
product made unit 0x51e160, the views from angles 0x520c60 / 0x521080 and the camera 0x5214f0, the rotation
of three angles 0x521a90, the screen's projection 0x521e10, the segment and line sides 0x522280 / 0x5223c0, a
triangle's normal and facing 0x5224b0, screen points back into a model 0x523650, the turns about x 0x523df0
and z 0x523ef0 (its typo kept), the quaternion add, negate, subtract, dot and squared length 0x524f80..0x5250f0,
and the xz hull 0x525fa0.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_zmath_unused.py [runs]

Each run sets the matrix stack (a below and a top, either an identity), the view, the camera and the
projection, at random; runs one routine at 24 and at 53 bits on both sides, and compares every output, the
stack's matrices and flags, the view, the camera and the projection, bit for bit.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402

SLOTS, FLAGS, PSLOT, PFLAG = 0x63d760, 0x63d7e8, 0x5eda90, 0x5eda8c
VIEW, CAMERA = 0x73fbc0, 0x73fc00
# The probe's projection twelve: origin x, y, half w, h, inv cot h, v, cx, cy, inv fx, fy, fx, fy.
PROJ = [0x63d338, 0x63d33c, 0x63d748, 0x63d74c, 0x63d740, 0x63d744, 0x73fb88, 0x73fb8c, 0x63d758, 0x63d75c,
        0x73fb9c, 0x73fba0]
NAMES = ['cross_unit', 'view_from_angles', 'set_view_from_angles', 'camera_from_angles', 'rotation_matrix',
         'set_screen', 'segment_side', 'left_of', 'triangle_normal', 'unproject_to_model', 'rotate_x',
         'rotate_z_typo', 'quat_add', 'quat_negate', 'quat_sub', 'quat_dot', 'quat_norm_sq', 'hull_xz']
F32 = ctypes.c_float


def eax(cpu):
    from unicorn.x86_const import UC_X86_REG_EAX
    return (getattr(cpu, 'uc', None) or cpu._uc).reg_read(UC_X86_REG_EAX)


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def fb(*v):
    return struct.pack(f'<{len(v)}f', *v)


def u(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)  # the hull's _ftol
        self.below, self.top = cpu.alloc(48), cpu.alloc(48)
        self.a, self.b, self.c, self.out = cpu.alloc(0x800), cpu.alloc(0x800), cpu.alloc(0x40), cpu.alloc(0x800)
        self.args, self.fret = cpu.alloc(0x20), cpu.alloc(8)
        self.thunks = {}

    def thunk(self, fn, n):
        """Pushes n words from self.args, calls fn (ecx, edx kept) and stores its st0 as a float."""
        if (fn, n) in self.thunks:
            return self.thunks[fn, n]
        code = b''.join(b'\xff\x35' + struct.pack('<I', self.args + 4 * k) for k in reversed(range(n)))
        self.thunks[fn, n] = self.cpu.code(code + b'\xb8' + struct.pack('<I', fn) + b'\xff\xd0\xd9\x1d' +
                                           struct.pack('<I', self.fret) + b'\xc3')
        return self.thunks[fn, n]

    def set_state(self, st):
        cpu = self.cpu
        cpu.write(self.below, fb(*st[0:12])), cpu.write(self.top, fb(*st[13:25]))
        cpu.u32(SLOTS, self.below), cpu.u32(SLOTS + 4, self.top)
        cpu.u32(FLAGS, u(st[12])), cpu.u32(FLAGS + 4, u(st[25]))
        cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)
        cpu.write(VIEW, fb(*st[26:38])), cpu.write(CAMERA, fb(*st[38:50]))
        for k, a in enumerate(PROJ):
            cpu.write(a, fb(st[50 + k]))

    def get_state(self):
        cpu = self.cpu
        r = lambda a, n: list(struct.unpack(f'<{n}f', cpu.read(a, 4 * n)))
        st = r(self.below, 12) + r(FLAGS, 1) + r(self.top, 12) + r(FLAGS + 4, 1) + r(VIEW, 12) + r(CAMERA, 12)
        st += [r(a, 1)[0] for a in PROJ]
        depth = (struct.unpack('<I', cpu.read(PSLOT, 4))[0] - SLOTS) // 4
        return st, depth


def main():
    probe = load_probe()
    probe.probe_zmu.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(F32), ctypes.POINTER(F32),
                                ctypes.POINTER(F32), ctypes.POINTER(ctypes.c_int32)]
    probe.probe_zmu.restype = ctypes.c_int32
    emu = Emulated()
    cpu = emu.cpu
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rng = random.Random(0x51e160)
    counts = {}
    for fpu53 in (0, 1):
        cpu.fpu_control(0x027f if fpu53 else 0x007f)
        for run in range(runs):
            for op in range(len(NAMES)):
                st = [f32(rng.uniform(-1, 1)) for _ in range(9)] + [f32(rng.uniform(-500, 500)) for _ in range(3)]
                st += [struct.unpack('<f', struct.pack('<I', rng.random() < 0.2))[0]]
                st += [f32(rng.uniform(-1, 1)) for _ in range(9)] + [f32(rng.uniform(-500, 500)) for _ in range(3)]
                st += [struct.unpack('<f', struct.pack('<I', rng.random() < 0.3))[0]]
                st += [f32(rng.uniform(-1, 1)) for _ in range(9)] + [f32(rng.uniform(-500, 500)) for _ in range(3)]
                st += [f32(rng.uniform(-1, 1)) for _ in range(9)] + [f32(rng.uniform(-500, 500)) for _ in range(3)]
                st += [f32(rng.uniform(0, 100)), f32(rng.uniform(0, 100)), f32(rng.uniform(100, 400)),
                       f32(rng.uniform(100, 300)), f32(rng.uniform(0.3, 2)), f32(rng.uniform(0.3, 2)),
                       f32(rng.uniform(100, 500)), f32(rng.uniform(100, 400)), f32(rng.uniform(1e-3, 1e-2)),
                       f32(rng.uniform(1e-3, 1e-2)), f32(rng.uniform(100, 600)), f32(rng.uniform(100, 600))]
                emu.set_state(st)
                ins, iout, n_out, want_ret, count = [], [0, 0, 0], 0, None, 0
                ang = lambda: f32(rng.uniform(-7, 7))
                pt = lambda s=500: f32(rng.uniform(-s, s))
                if op == 0:
                    ins = [pt(2) for _ in range(6)]
                    cpu.write(emu.a, fb(*ins[:3])), cpu.write(emu.b, fb(*ins[3:]))
                    cpu.call(0x51e160, ecx=emu.a, edx=emu.b, stack_args=[emu.out])
                    n_out = 3
                elif op in (1, 2, 3):
                    ins = [ang(), ang(), ang(), pt(), pt(), pt()]
                    cpu.write(emu.a, fb(*ins[:3])), cpu.write(emu.b, fb(*ins[3:]))
                    cpu.call({1: 0x520c60, 2: 0x521080, 3: 0x5214f0}[op], ecx=emu.a, edx=emu.b)
                elif op == 4:
                    ins = [ang(), ang(), ang()]
                    cpu.call(0x521a90, ecx=emu.out, stack_args=[u(v) for v in ins])
                    n_out = 12
                elif op == 5:
                    ins = [f32(rng.uniform(100, 400)), f32(rng.uniform(100, 300)), f32(rng.uniform(0.3, 2.5)),
                           f32(rng.uniform(0.3, 2.5))]
                    cpu.write(emu.a, fb(*ins[:2])), cpu.write(emu.b, fb(*ins[2:]))
                    cpu.call(0x521e10, ecx=emu.a, edx=emu.b)
                elif op == 6:
                    ins = [pt() for _ in range(6)]
                    cpu.write(emu.a, fb(*ins[:2])), cpu.write(emu.b, fb(*ins[2:4])), cpu.write(emu.c, fb(*ins[4:]))
                    cpu.write(emu.args, struct.pack('<III', emu.c, emu.out + 0x100, emu.out + 0x104))
                    cpu.call(emu.thunk(0x522280, 3), ecx=emu.a, edx=emu.b)
                    want_ret = list(struct.unpack('<f', cpu.read(emu.fret, 4))) + list(struct.unpack('<2i', cpu.read(emu.out + 0x100, 8)))
                elif op == 7:
                    ins = [pt() for _ in range(6)]
                    if rng.random() < 0.25:  # on the line: exactly 0, which counts as left
                        ins[4:6] = ins[2:4] if rng.random() < 0.5 else ins[0:2]
                    cpu.write(emu.a, fb(*ins[:2])), cpu.write(emu.b, fb(*ins[2:4])), cpu.write(emu.c, fb(*ins[4:]))
                    cpu.call(0x5223c0, ecx=emu.a, edx=emu.b, stack_args=[emu.c])
                    want_ret = [eax(cpu)]
                elif op == 8:
                    ins = [pt(50) for _ in range(9)]
                    two = rng.random() < 0.5
                    iout = [int(two), 7, 0]
                    cpu.write(emu.a, fb(*ins)), cpu.u32(emu.b, 7)
                    cpu.call(0x5224b0, ecx=emu.a, edx=emu.b, stack_args=[int(two), emu.out])
                    want_ret = [eax(cpu),
                                struct.unpack('<i', cpu.read(emu.b, 4))[0]]
                    n_out = 3
                elif op == 9:
                    count = rng.randint(0, 6)
                    ins = []
                    for _ in range(count):
                        ins += [f32(rng.uniform(0, 640)), f32(rng.uniform(0, 480)), f32(rng.uniform(0.01, 1))]
                    iout = [count, 0, 0]
                    cpu.write(emu.a, fb(*ins) if ins else b'')
                    cpu.write(emu.out, bytes(0x100))
                    cpu.call(0x523650, ecx=emu.a, edx=emu.out, stack_args=[count])
                    n_out = 3 * count
                elif op in (10, 11):
                    ins = [pt(), pt(), pt(), ang()]
                    cpu.write(emu.a, fb(*ins[:3]))
                    cpu.call(0x523df0 if op == 10 else 0x523ef0, ecx=emu.out, edx=emu.a, stack_args=[u(ins[3])])
                    n_out = 3
                elif op in (12, 13, 14):
                    ins = [pt(2) for _ in range(8)]
                    cpu.write(emu.a, fb(*ins[:4])), cpu.write(emu.b, fb(*ins[4:]))
                    if op == 13:
                        cpu.call(0x524fd0, ecx=emu.a, edx=emu.out)
                    else:
                        cpu.call(0x524f80 if op == 12 else 0x525020, ecx=emu.a, edx=emu.b, stack_args=[emu.out])
                    n_out = 4
                elif op in (15, 16):
                    ins = [pt(2) for _ in range(8)]
                    cpu.write(emu.a, fb(*ins[:4])), cpu.write(emu.b, fb(*ins[4:]))
                    cpu.call(emu.thunk(0x5250a0 if op == 15 else 0x5250f0, 0), ecx=emu.a, edx=emu.b)
                    want_ret = list(struct.unpack('<f', cpu.read(emu.fret, 4)))
                elif op == 17:
                    count = rng.randint(1, 24)
                    s = rng.choice((5, 40, 300))
                    ins = []
                    for _ in range(count):
                        ins += [f32(rng.uniform(-s, s)), f32(rng.uniform(-5, 5)), f32(rng.uniform(-s, s))]
                    if rng.random() < 0.3:  # ties: points on a grid, repeats
                        ins = [f32(round(v / 7) * 7) for v in ins]
                    iout = [count, 0, 0]
                    cpu.write(emu.out, fb(*ins) + bytes(12))
                    cpu.call(0x525fa0, ecx=emu.out, edx=count)
                    want_ret = [eax(cpu)]
                    n_out = 3 * (count + 1)
                want_out = list(struct.unpack(f'<{n_out}f', cpu.read(emu.out, 4 * n_out))) if n_out else []
                want_st, want_depth = emu.get_state()
                # the port
                state = (F32 * 62)(*st)
                inb = (F32 * max(len(ins), 1))(*ins) if ins else (F32 * 1)()
                outb = (F32 * 512)()
                if op == 17:
                    for k, v in enumerate(ins):
                        outb[k] = v
                io = (ctypes.c_int32 * 3)(*iout)
                ret = probe.probe_zmu(op, fpu53, state, inb, outb, io)
                got_out = list(outb[:n_out])
                got_ret = None
                if op == 6:
                    got_ret = [outb[0], io[0], io[1]]
                    got_out, want_out = [], []
                elif op == 7 or op == 17:
                    got_ret = [ret]
                elif op == 8:
                    got_ret = [ret, io[1]]
                elif op in (15, 16):
                    got_ret = [outb[0]]
                    got_out, want_out = [], []
                bits = lambda xs: [struct.pack('<f', x) if isinstance(x, float) else x for x in xs]
                if (bits(want_out) != bits(got_out) or bits(want_st) != bits(list(state)) or
                        want_depth != io[2] or (want_ret is not None and bits(want_ret) != bits(got_ret))):
                    print(f'{NAMES[op]} at {"53" if fpu53 else "24"} bits, run {run}: ins {ins[:12]}')
                    if bits(want_out) != bits(got_out):
                        print(f'  out  exe {want_out[:12]}\n       port {got_out[:12]}')
                    if want_ret is not None and bits(want_ret) != bits(got_ret):
                        print(f'  ret  exe {want_ret}\n       port {got_ret}')
                    for k in range(62):
                        if struct.pack('<f', want_st[k]) != struct.pack('<f', state[k]):
                            print(f'  state[{k}] exe {want_st[k]} port {state[k]}')
                    if want_depth != io[2]:
                        print(f'  depth exe {want_depth} port {io[2]}')
                    raise SystemExit(1)
                counts[NAMES[op]] = counts.get(NAMES[op], 0) + 1
    print(f'zmath unused: {runs} runs of {len(NAMES)} routines at 24 and 53 bits identical')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
