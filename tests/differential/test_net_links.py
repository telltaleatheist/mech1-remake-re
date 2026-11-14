"""The AI path editor's pieces no release path reaches, against Mech3.exe 1.2 (docs/ai.md, The developer tools;
docs/editor.md, The node dialog):

- the steps' link edits (zai_steps: a node's 0x4c9140..0x4c9480, a step's 0x4c9d20..0x4c9dd0, the base's
  0x58ae90 / 0x410560 / 0x410590 and the answers 0x4104f0 / 0x410520 / 0x410540), run through the exe's own
  vtables on worlds of nodes, speed steps, hold behaviours (the base's slots) and region edges, with a foreign element (no behaviour), the null step
  and none as targets: each answer and every link after each edit;
- the node dialog's Update (template 154, 0x4d9f30 -> 0x4d9ed0 -> 0x4d9d80 / 0x4d9dc0) over the same worlds as a
  znet object, by its id and an element number (UpdateData stood in, the list boxes' messages read back);
- the net file's folder (0x4d9fc0): its own, else aiv.zrd's, found once (0x4debd0 stood in).

    cmake --build build --target mw3probe && python3 tests/differential/test_net_links.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

NULL_STEP, NODE_VT, SPEED_VT, EDGE_VT, BASE_VT, HOLD_VT = 0x603fa0, 0x5cc6f0, 0x5cb488, 0x5cad70, 0x5cb180, 0x5bca40
NULL_STEP_VT, REGISTRY, OBJECT_VT, RET0 = 0x5bc8d0, 0x603fd8, 0x5cefd0, 0x4d72a0
SEND_MESSAGE, SPRINTF, STRDUP = 0x5bb734, 0x5bb658, 0x5bb670
OP_NEW, OP_DELETE, UPDATE_DATA = 0x5abd26, 0x5abd1a, 0x5abd8c
UPDATE, FOLDER, RESOLVE_DIR, AIV_MAKE, AIV_DROP = 0x4d9f30, 0x4d9fc0, 0x4debd0, 0x4e0490, 0x4e04f0
AIV, SHARED = 0x60b5e0, 0x60b5a0
EXE = None  # Original's exe (test_pm_net_links sets 'pm' and the addresses above)
bits = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]
SLOT = {0: 0xc, 1: 0x24, 2: 0x2c, 3: 0x30, 4: 0x34, 5: 0x38, 6: 0x3c, 7: 0x44, 8: 0x4c, 9: 0x54, 10: 0x8, 11: 0x4}
ARGS = {0: 't', 1: '', 2: 'p', 3: 't', 4: 't', 5: '', 6: 't', 7: 'ti', 8: 'ti', 9: 'ti', 10: 'i', 11: ''}


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe=EXE) if EXE else Original()
        cpu.stub_function(OP_NEW, lambda c: c.alloc(max(1, c.arg(0))))
        cpu.stub_function(OP_DELETE, lambda c: 0)
        self.lists = {}
        cpu.stub_import(SEND_MESSAGE, self.send, pop_bytes=16)
        cpu.stub_import(SPRINTF, self.sprintf)
        cpu.stub_import(STRDUP, self.strdup)
        self.answers, self.log = [], []
        cpu.stub_function(UPDATE_DATA, self.update_data, pop_bytes=4)
        self.foreign_vt = cpu.alloc(0xb0)
        cpu.write(self.foreign_vt, cpu.read(BASE_VT, 0xb0))
        cpu.u32(self.foreign_vt + 0x60, RET0)  # no behaviour
        self.name = cpu.alloc(16)
        cpu.write(self.name, b'node_name\0')

    @property
    def eax(self):
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def u32r(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def send(self, c):
        h, msg, lp = c.arg(0), c.arg(1), c.arg(3)
        if msg == 0x184:
            self.lists[h] = []
        elif msg == 0x180:
            self.lists.setdefault(h, []).append(self.cpu.read(lp, 16).split(b'\0')[0].decode())
        return 0

    def sprintf(self, c):
        assert self.cpu.read(c.arg(1), 3) == b'%d\0'
        self.cpu.write(c.arg(0), str(ctypes.c_int32(c.arg(2)).value).encode() + b'\0')
        return 0

    def strdup(self, c):
        s = self.cpu.read(c.arg(0), 64).split(b'\0')[0]
        a = self.cpu.alloc(len(s) + 1)
        self.cpu.write(a, s + b'\0')
        return a

    def update_data(self, c):
        self.log.append('U1;' if c.arg(0) else 'U0;')
        return self.answers.pop(0)

    # --- the world -----------------------------------------------------------------------------------------
    def build(self, world):
        cpu = self.cpu
        cpu.u32(NULL_STEP, NULL_STEP_VT)
        cpu.write(NULL_STEP + 4, b'\0' * 8)
        self.objs, self.kinds = [], []
        for e in world:
            if e[0] == 0:
                o = cpu.alloc(0x2c)
                cpu.write(o, b'\0' * 0x2c)
                cpu.u32(o, NODE_VT)
                cpu.u32(o + 0xc, self.name if e[2] else 0)
                cpu.write(o + 0x10, struct.pack('<3I', *e[1]))
            elif e[0] == 1:
                o = cpu.alloc(0x14)
                cpu.write(o, b'\0' * 0x14)
                cpu.u32(o, SPEED_VT)
            elif e[0] == 2:
                o = cpu.alloc(0x1c)
                cpu.write(o, b'\0' * 0x1c)
                cpu.u32(o, EDGE_VT)
                cpu.write(o + 4, struct.pack('<3I', *e[1]))
            else:
                o = cpu.alloc(0xc)
                cpu.write(o, b'\0' * 0xc)
                cpu.u32(o, HOLD_VT)
            self.objs.append(o)
            self.kinds.append(e[0])
        self.foreign = cpu.alloc(0x10)
        cpu.u32(self.foreign, self.foreign_vt)
        for o, e in zip(self.objs, world):
            if e[0] == 0:
                links, extra = e[3], e[4]
                if links or extra:
                    b = cpu.alloc(4 * (len(links) + extra))
                    for k, r in enumerate(links):
                        cpu.u32(b + 4 * k, self.ref(r))
                    cpu.u32(o + 0x20, b)
                    cpu.u32(o + 0x24, b + 4 * len(links))
                    cpu.u32(o + 0x28, b + 4 * (len(links) + extra))
            elif e[0] == 1:
                cpu.u32(o + 0xc, self.ref(e[1]))

    def ref(self, r):
        return NULL_STEP if r == -1 else 0 if r < 0 else self.foreign if r == len(self.objs) else self.objs[r]

    def back(self, p):
        if p == NULL_STEP:
            return -1
        if p == 0:
            return -2
        if p in self.objs:
            return self.objs.index(p)
        return len(self.objs) if p == self.foreign else -3

    def state(self):
        s = ''
        for o, k in zip(self.objs, self.kinds):
            if k == 0:
                b, e = self.u32r(o + 0x20), self.u32r(o + 0x24)
                s += 'N%08x,%08x,%08x[' % struct.unpack('<3I', self.cpu.read(o + 0x10, 12))
                s += ''.join(f'{self.back(self.u32r(a))},' for a in range(b, e, 4)) + ']'
            elif k == 1:
                s += f'S{self.back(self.u32r(o + 0xc))}'
            else:
                s += 'E' if k == 2 else 'B'
            s += ';'
        return s

    def links(self, i):
        o = self.objs[i]
        if self.kinds[i] == 0:
            b, e = self.u32r(o + 0x20), self.u32r(o + 0x24)
            return [self.back(self.u32r(a)) for a in range(b, e, 4)]
        return [self.back(self.u32r(o + 0xc))]

    def op(self, op, actor, target, i, pos):
        o = self.objs[actor]
        fn = self.u32r(self.u32r(o) + SLOT[op])
        args = []
        for a in ARGS[op]:
            if a == 't':
                args.append(self.ref(target))
            elif a == 'i':
                args.append(i & 0xffffffff)
            else:
                p = self.cpu.alloc(12)
                self.cpu.write(p, struct.pack('<3I', *pos))
                args.append(p)
        self.cpu.call(fn, ecx=o, stack_args=args)
        r = self.eax
        if op == 5:
            r = 0
        elif op == 1:
            r = int(r != 0)
        elif op == 10:
            r = self.back(r)
        else:
            r = ctypes.c_int32(r).value
        return r

    def run_ops(self, world, ops):
        self.build(world)
        return ''.join(f'{self.op(*o)} {self.state()}\n' for o in ops)

    # --- the dialog ----------------------------------------------------------------------------------------
    def dialog(self, world, net, node, answers):
        cpu = self.cpu
        self.build(world)
        objs = []
        for oid, elems in ((4321, self.objs), (4322, [])):
            o = cpu.alloc(0x18)
            cpu.write(o, b'\0' * 0x18)
            cpu.u32(o, OBJECT_VT)
            cpu.u32(o + 4, oid)
            if elems:
                b = cpu.alloc(4 * len(elems))
                for k, e in enumerate(elems):
                    cpu.u32(b + 4 * k, e)
                cpu.u32(o + 0xc, b)
                cpu.u32(o + 0x10, b + 4 * len(elems))
                cpu.u32(o + 0x14, b + 4 * len(elems))
            objs.append(o)
        head = cpu.alloc(12)
        nodes = [cpu.alloc(12) for _ in objs]
        ring = [head] + nodes
        for k, n in enumerate(ring):
            cpu.u32(n, ring[(k + 1) % len(ring)])
            cpu.u32(n + 4, ring[k - 1])
        for n, o in zip(nodes, objs):
            cpu.u32(n + 8, o)
        cpu.u32(REGISTRY + 4, head)
        d = cpu.alloc(0x100)
        cpu.write(d, b'\0' * 0x100)
        cpu.u32(d + 0x84, 0x1110)
        cpu.u32(d + 0xc4, 0x1108)
        self.lists = {0x1110: ['stale'], 0x1108: ['stale']}
        cpu.write(d + 0xe4, struct.pack('<fff', 7, 0, 0))
        cpu.u32(d + 0xf0, net)
        cpu.u32(d + 0xf4, -5)
        cpu.u32(d + 0xf8, node & 0xffffffff)
        self.answers, self.log = list(answers), []
        cpu.call(UPDATE, ecx=d)
        p = struct.unpack('<3I', cpu.read(d + 0xe4, 12))
        cnt = ctypes.c_int32(self.u32r(d + 0xf4)).value
        out = ''.join(self.log) + 'pos %08x %08x %08x count %d to' % (*p, cnt)
        out += ''.join(' ' + t for t in self.lists[0x1108]) + ' from' + ''.join(' ' + f for f in self.lists[0x1110])
        return out

    # --- the folder ----------------------------------------------------------------------------------------
    def folder(self, steps):
        cpu = self.cpu
        dirs = {}

        def s(i):
            if i not in dirs:
                a = cpu.alloc(16)
                cpu.write(a, f'dir{i}\\'.encode() + b'\0')
                dirs[i] = a
            return dirs[i]
        cpu.stub_function(RESOLVE_DIR, lambda c: self.u32r(c.ecx + 8))
        cpu.stub_function(AIV_MAKE, lambda c: 0)
        cpu.stub_function(AIV_DROP, lambda c: 0)
        cpu.u32(SHARED, 0)
        out = ''
        for aiv, own in steps:
            if aiv >= -1:
                cpu.u32(AIV + 8, s(aiv) if aiv >= 0 else 0)
            f = cpu.alloc(0x118)
            cpu.write(f, b'\0' * 0x118)
            mine = s(own) if own >= 0 else 0
            cpu.u32(f + 8, mine)
            cpu.call(FOLDER, ecx=f)
            r = self.eax
            if not r:
                out += 'none;'
            else:
                out += cpu.read(r, 16).split(b'\0')[0].decode() + (' own;' if r == mine else ' shared;')
        return out


def gen_world(rng, n, to_nodes=False):
    """to_nodes: a step leads only to a node or nowhere (a step's way point is its next's, so a ring of steps
    recurses in the exe and the port alike)."""
    world = []
    kinds = [0] + [rng.choice((0, 0, 1, 1, 2, 3)) for _ in range(n - 1)]
    beh = [k for k in range(n) if kinds[k] != 2]
    nodes = [k for k in range(n) if kinds[k] == 0]
    for k in kinds:
        pos = tuple(bits(rng.uniform(-500, 500)) for _ in range(3))
        if k == 0:
            nl = rng.choice((0, 0, 1, 2, 3, 4))
            links = [rng.choice(beh + [-1, -1]) for _ in range(nl)]
            world.append((0, pos, rng.random() < 0.5, links, rng.choice((0, 0, 1, 2))))
        elif k == 1:
            world.append((1, rng.choice((nodes if to_nodes else beh) + [-1, -1, -1])))
        elif k == 2:
            world.append((2, pos))
        else:
            world.append((3,))
    return world


def flat(world):
    s = [len(world)]
    for e in world:
        s.append(e[0])
        if e[0] == 0:
            s += list(e[1]) + [int(e[2]), len(e[3])] + list(e[3])
        elif e[0] == 1:
            s.append(e[1])
        elif e[0] == 2:
            s += list(e[1])
    return s


def gen_ops(rng, world, count):
    """Ops on behaviours; set/clear on a node keep i in its links; none never asked of a node whose links hold
    none (the exe calls through it: FIX (A))."""
    n = len(world)
    actors = [k for k in range(n) if world[k][0] != 2]
    targets = list(range(n + 1)) + [-1, -2]
    ops = []
    for _ in range(count):
        a = rng.choice(actors)
        op = rng.choice((0, 1, 2, 3, 3, 3, 4, 4, 5, 6, 6, 7, 8, 9, 10, 11))
        t = rng.choice(targets)
        i = rng.randrange(-1, 5)
        pos = tuple(bits(rng.uniform(-100, 100)) for _ in range(3))
        ops.append((op, a, t, i, pos))
    return ops


def safe(world_kinds, links, op, a, t, i):
    if world_kinds[a] == 0:
        if op in (7, 8) and not 0 <= i < len(links):
            return False
        if op in (4, 6) and t == -2 and -2 in links:
            return False
    return True


def main(lib=None):
    lib = lib or load_probe()
    lib.probe_links.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.c_char_p, ctypes.c_int32]
    lib.probe_node_dlg.argtypes = lib.probe_links.argtypes
    lib.probe_net_folder.argtypes = lib.probe_links.argtypes
    buf = ctypes.create_string_buffer(1 << 20)

    def port(fn, s):
        arr = (ctypes.c_int32 * len(s))(*[ctypes.c_int32(v & 0xffffffff).value for v in s])
        n = fn(arr, buf, len(buf))
        return buf.raw[:n].decode()

    rng = random.Random(11)
    bad = cases = 0
    # Links: runs of edits, each checked after it.
    exe = Exe()
    for run in range(1500):
        world = gen_world(rng, rng.randrange(1, 7))
        exe.build(world)
        ops, out_exe = [], ''
        for o in gen_ops(rng, world, 12):
            if not safe(exe.kinds, exe.links(o[1]), *o[:4]):
                continue
            ops.append(o)
            out_exe += f'{exe.op(*o)} {exe.state()}\n'
        s = flat(world)
        for op, a, t, i, pos in ops:
            s += [op, a, t, i] + list(pos)
        s.append(-1)
        out_port = port(lib.probe_links, s)
        cases += len(ops)
        if out_port != out_exe:
            bad += 1
            if bad <= 3:
                print(f'links run {run}: world {world}\n ops {ops}\n exe:\n{out_exe} port:\n{out_port}')
    print(f'links: {cases} edits in 1500 runs, {bad} runs differ')
    # The node dialog.
    dbad = dcases = 0
    for run in range(2000):
        world = gen_world(rng, rng.randrange(1, 8), to_nodes=True)
        net = rng.choice((4321, 4321, 4321, 4321, 999))
        node = rng.choice(list(range(len(world))) * 3 + [len(world), 50, -1])
        answers = [rng.random() < 0.85, rng.random() < 0.8]
        out_exe = exe.dialog(world, net, node, [int(x) for x in answers])
        out_port = port(lib.probe_node_dlg, flat(world) + [net, node] + [int(x) for x in answers])
        dcases += 1
        if out_exe != out_port:
            dbad += 1
            if dbad <= 3:
                print(f'dialog run {run}: world {world} net {net} node {node} {answers}\n exe:  {out_exe}\n port: {out_port}')
    print(f'node dialog: {dcases} updates, {dbad} differ')
    # The folder, once per process (it is found once).
    steps = [(-1, -1), (-2, 3), (5, -1), (6, -1), (-2, 2), (-1, -1), (-1, 4)]
    out_exe = Exe().folder(steps)
    s = [v for st in steps for v in st] + [-3]
    out_port = port(lib.probe_net_folder, s)
    fbad = int(out_exe != out_port)
    if fbad:
        print(f'folder:\n exe:  {out_exe}\n port: {out_port}')
    print(f'folder: {len(steps)} files, {fbad} differ')
    print('OK' if not (bad or dbad or fbad) else 'FAIL')
    return 0 if not (bad or dbad or fbad) else 1


if __name__ == '__main__':
    sys.exit(main())
