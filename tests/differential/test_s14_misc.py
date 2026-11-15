"""S14's small preservation ports against the original x86: the wind's step 0x548840 (rand the MSVC sequence on
both sides); zClass's setters 0x4f3540 ([0x5ea53c]), 0x4f8f20 (the frame counter and the stamps' freeze),
0x4f8f60 ([0x5ea82c]); zReader's define list start 0x557490; zInput's saved maps freed 0x51af90; and the
mechlib's materials writer: one material 0x4fc4d0, the list 0x4fcca0 and its archive entry 0x4fc990 (WriteFile
and zar's add stubbed: the bytes compared).

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_s14_misc.py [runs]

A resolved material's texture word is the texture entry's address in the exe and 1 in the port (FIX (A): the
reader only tests it for nonzero); compared as nonzero.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

IAT_RAND, IAT_WRITEFILE = 0x5bb668, 0x5bb17c
NEW, DELETE = 0x5abd26, 0x5abd1a
# the wind: current (3), base (3), angle, speed, max speed, angular velocity, accel
WIND = [0x72be28, 0x72be2c, 0x72be30, 0x72be38, 0x72be3c, 0x72be40, 0x72be44, 0x72be48, 0x72be4c, 0x72be50, 0x72be54]
F = ctypes.c_float


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


def eax(cpu):
    from unicorn.x86_const import UC_X86_REG_EAX
    return cpu.uc.reg_read(UC_X86_REG_EAX)


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision, as the puffers' test
        self.rand = MsvcRand(1)
        self.deleted = 0
        self.files = {}
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        cpu.stub_import(IAT_WRITEFILE, self.write, 20)
        cpu.stub_function(NEW, lambda c: cpu.alloc(max(c.arg(0), 4)))
        cpu.stub_function(DELETE, self.delete)
        cpu.stub_function(0x576890, self.add, 20)  # thiscall, five words

    def delete(self, c):
        self.deleted += 1
        return 0

    def write(self, c):
        handle, buf, n, done = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        self.files[handle] = self.files.get(handle, b'') + self.cpu.read(buf, n)
        self.cpu.u32(done, n)
        return 1

    def add(self, c):
        # 0x576890(name, write, user, 0, 0): the entry's bytes from write(file, user), as the exe's ECX/EDX thunk
        name = b''
        while True:
            ch = self.cpu.read(c.arg(0) + len(name), 1)
            if ch == b'\0':
                break
            name += ch
        self.added = name.decode()
        self.callback = (c.arg(1), c.arg(2))  # run after the call (no emulation inside a hook)
        return 0


def main():
    probe = load_probe()
    probe.probe_wind_step.argtypes = [ctypes.POINTER(F), F, ctypes.c_uint32, ctypes.c_int]
    I = ctypes.POINTER(ctypes.c_int32)
    probe.probe_node_setters.argtypes = [ctypes.c_int, ctypes.c_int32, I, I]
    probe.probe_write_materials.argtypes = [ctypes.c_int, I, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                            ctypes.c_char_p, ctypes.c_int]
    emu = Emulated()
    cpu = emu.cpu
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    rng = random.Random(0x548840)
    stats = {'wind steps': 0, 'setters': 0, 'defines': 0, 'saved maps': 0, 'materials': 0, 'entries': 0}
    for run in range(runs):
        # the wind
        w = [f32(rng.uniform(-3, 3)) for _ in range(6)] + [f32(rng.uniform(-7, 7)), f32(rng.uniform(-2, 6)),
                                                           f32(rng.uniform(0, 8)), f32(rng.uniform(0, 4)),
                                                           f32(rng.uniform(0, 3))]
        dt, seed, steps = f32(rng.uniform(0, 0.2)), rng.getrandbits(31), rng.randint(1, 8)
        for a, v in zip(WIND, w):
            cpu.write(a, struct.pack('<f', v))
        emu.rand = MsvcRand(seed)
        for _ in range(steps):
            cpu.call(0x548840, stack_args=[struct.unpack('<I', struct.pack('<f', dt))[0]])
        want = [struct.unpack('<f', cpu.read(a, 4))[0] for a in WIND]
        got = (F * 11)(*w)
        probe.probe_wind_step(got, dt, seed, steps)
        if [struct.pack('<f', v) for v in want] != [struct.pack('<f', v) for v in got]:
            raise SystemExit(f'run {run}: wind {w} dt {dt} x{steps}\n  exe  {want}\n  port {list(got)}')
        stats['wind steps'] += steps
        # zClass's setters
        for op in range(3):
            st = [rng.getrandbits(31), rng.getrandbits(31), rng.getrandbits(31), rng.getrandbits(31)]
            v = rng.getrandbits(31)
            for a, x in zip((0x5ea53c, 0x5e9e2c, 0x60b94c, 0x5ea82c), st):
                cpu.u32(a, x)
            cpu.call((0x4f3540, 0x4f8f20, 0x4f8f60)[op], ecx=v)
            want = [struct.unpack('<i', cpu.read(a, 4))[0] for a in (0x5ea53c, 0x5e9e2c, 0x60b94c, 0x5ea82c)]
            want.append(eax(cpu) if op == 1 else 0)
            out = (ctypes.c_int32 * 5)()
            probe.probe_node_setters(op, v, (ctypes.c_int32 * 4)(*st), out)
            if want != list(out):
                raise SystemExit(f'run {run}: setter {op} ({v}): exe {want}, port {list(out)}')
            stats['setters'] += 1
        # the define list's start: empty after, whatever it held
        n = rng.randint(0, 5)
        cpu.u32(0x72bf98, n)
        cpu.call(0x557490)
        head = struct.unpack('<I', cpu.read(0x72bf94, 4))[0]
        links = struct.unpack('<II', cpu.read(head, 8))
        want = struct.unpack('<I', cpu.read(0x72bf98, 4))[0]
        if links != (head, head) or want != probe.probe_defines_reset(n):
            raise SystemExit(f'run {run}: defines: exe count {want} links {links}, port {probe.probe_defines_reset(n)}')
        stats['defines'] += 1
        # zInput's saved maps: two lists of nodes on the state, all let go, the counts zero
        state = cpu.alloc(0x4860)
        total = 0
        for off in (0x4848, 0x484c):
            k = rng.randint(0, 4)
            total += k
            prev = 0
            nodes = [cpu.alloc(12) for _ in range(k)]
            for j, nd in enumerate(nodes):
                cpu.write(nd, struct.pack('<II', nodes[j + 1] if j + 1 < k else 0, prev))
                prev = nd
            cpu.u32(state + off, nodes[0] if nodes else 0)
        for off in (0x4850, 0x4854, 0x4858):
            cpu.u32(state + off, rng.getrandbits(16))
        emu.deleted = 0
        cpu.call(0x51af90, ecx=state)
        fields = struct.unpack('<5I', cpu.read(state + 0x4848, 20))
        left = probe.probe_saved_maps_free(total)
        if emu.deleted != total or any(fields) or left != 0:
            raise SystemExit(f'run {run}: saved maps: exe freed {emu.deleted} of {total}, fields {fields}; port left {left}')
        stats['saved maps'] += 1
        # the materials writer
        n = rng.randint(0, 6)
        kinds = [rng.choice((0, 1, 2)) for _ in range(n)]
        recs, names, mats = [], [], []
        for i in range(n):
            r = bytearray(rng.getrandbits(8) for _ in range(0x28))
            struct.pack_into('<I', r, 0x10, 0)
            if kinds[i]:
                r[1] &= ~4  # not cycled: the exe's +0x24 is then the file's word, as the port's
            nm = ''.join(rng.choice('abcxyz019') for _ in range(rng.randint(1, 18)))
            names.append(nm)
            recs.append(bytes(r))
            mat = cpu.alloc(12)
            if kinds[i] == 0:
                raw = cpu.alloc(0x28)
                cpu.write(raw, bytes(r))
                cpu.write(mat, struct.pack('<IiI', raw, 0, 0))
            else:
                world = cpu.alloc(0x2c)
                cpu.write(world, bytes(r) + bytes(4))
                if kinds[i] == 1:
                    entry = cpu.alloc(0x28)
                    cpu.write(entry, bytes(8) + nm.encode().ljust(20, b'\0') + bytes(12))
                    cpu.u32(world + 0x10, entry)
                cpu.write(mat, struct.pack('<IiI', world, 1, 0))
            mats.append(mat)
        lib = cpu.alloc(0x40)
        arr = cpu.alloc(max(4 * n, 4))
        cpu.write(arr, struct.pack(f'<{n}I', *mats) if n else b'')
        cpu.u32(lib + 0x20, arr if n else 0), cpu.u32(lib + 0x24, arr + 4 * n if n else 0)
        for entry_path in (0, 1):
            emu.files, emu.added = {}, None
            if entry_path:
                emu.callback = None
                cpu.call(0x4fc990, ecx=lib)
                ok = eax(cpu) & 0xff
                if emu.callback:
                    cpu.call(emu.callback[0], ecx=0x1234, edx=emu.callback[1])
                want = emu.files.get(0x1234, b'') if ok else None
                if ok and emu.added != 'materials':
                    raise SystemExit(f'run {run}: the entry is named {emu.added!r}')
            else:
                cpu.call(0x4fcca0, ecx=lib, stack_args=[0x1234])
                want = emu.files.get(0x1234, b'')
                if eax(cpu) != len(want):
                    raise SystemExit(f'run {run}: materials: the exe counts {eax(cpu)}, wrote {len(want)}')
            buf = ctypes.create_string_buffer(1 << 14)
            ln = probe.probe_write_materials(n, (ctypes.c_int32 * max(n, 1))(*kinds), b''.join(recs) or b'\0',
                                             b''.join(nm.encode() + b'\0' for nm in names) or b'\0', entry_path,
                                             buf, len(buf))
            got = buf.raw[:ln] if ln >= 0 else None
            if want is not None:  # the texture words of resolved materials: nonzero on both sides
                w2, at = bytearray(want), 4
                for i in range(n):
                    if kinds[i] == 1 and struct.unpack_from('<I', w2, at + 0x10)[0]:
                        struct.pack_into('<I', w2, at + 0x10, 1)
                    at += 0x28 + (4 + len(names[i]) if kinds[i] == 1 else 0)
                want = bytes(w2)
            if want != got:
                raise SystemExit(f'run {run}: materials {kinds} (entry {entry_path}):\n  exe  {want!r}\n  port {got!r}')
            stats['entries' if entry_path else 'materials'] += 1
    print(f'S14 misc: {runs} runs identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
