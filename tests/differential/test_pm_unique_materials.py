"""Pirate's Moon's script command CreateUniqueMaterials (0x50cc10, with its model's 0x55bcf0) against PM's exe:
a node tree's models' polygons given copies of their materials, one per material met (or one per slot with
per-polygon), the children's when recursive, then each model's polygons sorted by material (0x55c470).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_unique_materials.py

The material copy 0x557290 (some sources answered null), the sort 0x55c470, the exe's realloc 0x596520 and free
stand in on both sides (the port's through pm::zmod::unique_materials_hooks). Compared: what each slot holds
after, and the copies and sorts in order.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import Original  # noqa: E402

MAKE, COPY, SORT, REALLOC, IAT_FREE = 0x50cc10, 0x557290, 0x55c470, 0x596520, 0x5de65c


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.tr = lambda a, size=1: a
        cpu.stub_function(COPY, self.copy, 0)
        cpu.stub_function(SORT, self.sort, 0)
        cpu.stub_function(REALLOC, self.realloc, 0)
        cpu.stub_import(IAT_FREE, lambda c: 0)

    def copy(self, c):
        mid = c.arg(0) >> 4
        self.events.append(mid)
        if mid in self.nulls:
            return 0
        self.next += 1
        return 0x10000000 + self.next

    def sort(self, c):
        self.events.append(-1 - self.models.index(c.arg(0)))
        return 0

    def realloc(self, c):
        old, size = c.arg(0), c.arg(1)
        new = self.cpu.alloc(max(size, 4))
        if old:
            self.cpu.write(new, self.cpu.read(old, min(self.sizes.get(old, 0), size)))
        self.sizes[new] = size
        return new

    def run(self, tree, recursive, per_polygon, nulls):
        cpu = self.cpu
        self.events, self.next, self.nulls, self.sizes = [], 0, set(nulls), {}
        nodes = [cpu.alloc(0xd0) for _ in tree]
        self.models = []
        slots = []  # (node index, the materials array, count) in order
        for i, (parent, polys) in enumerate(tree):
            cpu.write(nodes[i], bytes(0xd0))
            if polys is None:
                self.models.append(None)
                continue
            model = cpu.alloc(0x68)
            cpu.write(model, bytes(0x68))
            cpu.u32(model + 0x10, len(polys))
            parr = cpu.alloc(max(0x28 * len(polys), 4))
            cpu.write(parr, bytes(max(0x28 * len(polys), 4)))
            for k, ids in enumerate(polys):
                marr = cpu.alloc(max(4 * len(ids), 4))
                cpu.write(marr, struct.pack(f'<{len(ids)}I', *[m << 4 for m in ids]) if ids else b'')
                cpu.u32(parr + 0x28 * k + 0x10, len(ids)), cpu.u32(parr + 0x28 * k + 0x14, marr)
                slots.append((marr, len(ids)))
            cpu.u32(model + 0x34, parr)
            cpu.u32(nodes[i] + 0x3c, model)
            self.models.append(model)
        for i in range(len(tree)):
            kids = [nodes[j] for j, (parent, _) in enumerate(tree) if parent == i]
            arr = cpu.alloc(max(4 * len(kids), 4))
            cpu.write(arr, struct.pack(f'<{len(kids)}I', *kids) if kids else b'')
            cpu.write(nodes[i] + 0x56, struct.pack('<h', len(kids)))
            cpu.u32(nodes[i] + 0x5c, arr)
        cpu.call(MAKE, stack_args=[nodes[0], recursive, per_polygon])
        from unicorn.x86_const import UC_X86_REG_EAX
        out = [cpu._uc.reg_read(UC_X86_REG_EAX)]
        for marr, n in slots:
            out += list(struct.unpack(f'<{n}I', cpu.read(marr, 4 * n))) if n else []
        return [v if v < 0x80000000 else v - (1 << 32) for v in out] + self.events


def main():
    p = test_pm_zar.load_pm_probe()
    I = ctypes.POINTER(ctypes.c_int32)
    p.probe_pm_unique_materials.argtypes = [I, ctypes.c_int, ctypes.c_int, ctypes.c_int, I, ctypes.c_int, I]
    emu = Emulated()
    rng = random.Random(0x50cc10)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    stats = {'copies': 0, 'sorts': 0, 'nulls': 0}
    for run in range(runs):
        n = rng.randint(1, 7)
        tree = []
        for i in range(n):
            parent = -1 if i == 0 else rng.randrange(i)
            if rng.random() < 0.2:
                polys = None
            else:
                polys = [[rng.randint(1, 6) for _ in range(rng.randint(0, 4))] for _ in range(rng.randint(0, 5))]
            tree.append((parent, polys))
        recursive, per_polygon = rng.choice((0, 1)), rng.choice((0, 1))
        nulls = [m for m in range(1, 7) if rng.random() < 0.15]
        want = emu.run(tree, recursive, per_polygon, nulls)
        spec = []
        for parent, polys in tree:
            spec.append(parent)
            if polys is None:
                spec.append(-1)
                continue
            spec.append(len(polys))
            for ids in polys:
                spec += [len(ids)] + ids
        out = (ctypes.c_int32 * 4096)()
        k = p.probe_pm_unique_materials((ctypes.c_int32 * len(spec))(*spec), n, recursive, per_polygon,
                                         (ctypes.c_int32 * max(len(nulls), 1))(*nulls), len(nulls), out)
        got = list(out[:k])
        if want != got:
            print(f'run {run}: tree {tree} recursive {recursive} per polygon {per_polygon} nulls {nulls}\n'
                  f'  exe  {want}\n  port {got}')
            raise SystemExit(1)
        stats['copies'] += sum(1 for e in emu.events if e > 0)
        stats['sorts'] += sum(1 for e in emu.events if e < 0)
        stats['nulls'] += sum(1 for e in emu.events if e in emu.nulls)
    print(f'pm unique materials: {runs} runs identical: {stats}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
