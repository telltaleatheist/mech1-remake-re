"""Pirate's Moon's network sends, the mech's vt+0x168 0x471210 (MW3's 0x46f160), against the port's net_send_event
through probe_mech_net_send, every kind but the clock (0x11, test_pm_net_tick) and the timer (test_pm_net_tick):
the state (0xf: the place, the velocity, the heading, the torso's two angles, the controls, the aim point, the
flags word, the target's player, the shots and the damaged parts), the ping (0x10, to the next player round the
list 0x472cb0), made again (0x12), killed (0x13), a part's damage (0x15), a shot (0x16), an action (0x17) and a
network mech's part shot off (0x21), on PM's mech fields (docs/pm-map.md; the offsets in Exe.run). Compared: every
send (to, guaranteed, size, every word), the kill heard here, the answer, the damaged parts and the shots after,
and each remote's ping fields (+0x760 sent at, +0x764 its id).

The state's aim (+0x28..+0x30) is the weapons' aim point as it is in PM (MW3 sends a turn off ahead and a range).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_send.py [cases]

BREAK=1 hands the port the throttle for the turn; it must fail.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402
from original import FTOL  # noqa: E402
from test_pm_net_state import COUNT, K, NAMES, bits, case as state_case  # noqa: E402,F401

I32 = ctypes.c_int32
SENDER, SEND, LOCAL_ID, IS_HOST, POSITION, NOW_MS = 0x471210, 0x5a97d0, 0x5a8a30, 0x5a8a40, 0x5cb260, 0x5967b0
GAME, CRC, GAME_OVER, KILL_IN, PART_MECH, TARGET_VEHICLE = 0x47b710, 0x597bc0, 0x472940, 0x470b80, 0x461430, 0x4f8da0
PART_BY_ID, HEADLIGHT_ON, NEW, DELETE = 0x41e2d0, 0x41f920, 0x5ce8c6, 0x5ce8c0
IAT_FTOL, IAT_CEIL, IAT_ASIN, IAT_COPYSIGN = 0x5de644, 0x5de618, 0x5de600, 0x5de5bc  # PM's own slots, written directly
RUNNING, MECHS, MECHS_N, TURN, COUNTER, ACKED = 0x629a88, 0x629a5c, 0x629a60, 0x629a98, 0x607760, 0x629a94
# ceil(x) on the x87: the rounding up for one frndint.
# _CIasin and _copysign for the torso's angles (0x53f8d0 -> 0x53d590), as test_matrix's.
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')
COPYSIGN = bytes.fromhex('8b442410 2500000080 8b4c2408 81e1ffffff7f 0bc8 894c2408 dd442404 c3')
CEIL = bytes.fromhex('dd442404 83ec04 d93c24 668b0424 6625fff3 660d0008 6689442402 d96c2402 d9fc d92c24 83c404 c3')


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def fl(b):
    return struct.unpack('<f', struct.pack('<i', b))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, code in ((IAT_FTOL, FTOL), (IAT_CEIL, CEIL), (IAT_ASIN, ASIN), (IAT_COPYSIGN, COPYSIGN)):
            self.w(slot, z.cpu.code(code))
        z.cpu.stub_import(test_pm_zar.IAT['malloc'], lambda c: z.alloc(c.arg(0)))  # MW3's slot names: mapped to PM's
        z.cpu.stub_import(test_pm_zar.IAT['free'], lambda c: 0)
        z.native_stub(NEW, lambda c: z.alloc(c.arg(0)))
        z.native_stub(DELETE, lambda c: 0)
        z.native_stub(SEND, self.send)
        z.native_stub(LOCAL_ID, lambda c: 9)
        z.native_stub(IS_HOST, lambda c: self.v[K['host']])
        z.native_stub(POSITION, lambda c: self.pos_at)
        z.native_stub(NOW_MS, lambda c: self.v[K['now']])
        z.native_stub(GAME, lambda c: self.game, 4)
        z.native_stub(CRC, lambda c: self.r(self.game + 0x9c))  # the rules check, left out of the port: passes
        z.native_stub(GAME_OVER, self.log_(4))
        z.native_stub(KILL_IN, self.log_(6))  # cdecl
        z.native_stub(PART_MECH, lambda c: self.r(c.ecx + 4))  # a part's mech (+0x04 here)
        z.native_stub(TARGET_VEHICLE, lambda c: self.vehicle)
        z.native_stub(PART_BY_ID, lambda c: self.parts.get(c.arg(0), 0), 4)
        z.native_stub(HEADLIGHT_ON, lambda c: self.light)
        # small functions as code: a value read from memory, or a constant
        self.fld = lambda at, pop=0: z.cpu.code(b'\xd9\x05' + struct.pack('<I', at) + (b'\xc2' + struct.pack('<H', pop) if pop else b'\xc3'))
        self.self_fn = z.cpu.code(b'\x89\xc8\xc3')        # mov eax, ecx; ret
        self.zero_fn = z.cpu.code(b'\x31\xc0\xc3')        # xor eax, eax; ret
        self.nine_fn = z.cpu.code(b'\xb8\x09\x00\x00\x00\xc3')  # mov eax, 9; ret
        # a part's vt+0x48 / vt+0x44 (section on the stack): its internal / armour of that section
        self.part_vt = z.alloc(0x100)
        self.w(self.part_vt + 0x48, z.cpu.code(b'\x8b\x44\x24\x04\xd9\x84\x81\x00\x01\x00\x00\xc2\x04\x00'))
        self.w(self.part_vt + 0x44, z.cpu.code(b'\x8b\x44\x24\x04\xd9\x84\x81\x40\x01\x00\x00\xc2\x04\x00'))
        self.power = z.alloc(4)
        self.power_fn = z.cpu.code(b'\xa1' + struct.pack('<I', self.power) + b'\xc3')
        self.vals = z.alloc(16)  # the controls' throttle, turn and jump, read by their getters (code made once)
        self.getters = [self.fld(self.vals + 4 * i) for i in range(3)]
        self.mark = z.cpu.heap_next  # each case's memory from here (the heap is a 16 MB bump allocator)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def wf(self, a, f):
        self.uc.mem_write(a, struct.pack('<f', f))

    def w8(self, a, v):
        self.uc.mem_write(a, bytes([v & 0xff]))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def log_(self, code):
        def run(c):
            self.log.append(code)
            return 0
        return run

    def send(self, c):
        msg, size, guaranteed, to = c.arg(0), c.arg(1) & 0xffff, c.arg(2), c.arg(3)
        data = bytes(self.uc.mem_read(msg, size)) + bytes(4)
        self.log += [1, s32(to), 1 if guaranteed & 0xff else 0, size]
        self.log += [struct.unpack('<i', data[k:k + 4])[0] for k in range(0, size, 4)]
        self.send_id += 1
        return self.send_id

    def push(self, head, value):
        z = self.z
        last = self.r(head + 4)
        node = z.alloc(12)
        self.w(node, head), self.w(node + 4, last), self.w(node + 8, value)
        self.w(last, node), self.w(head + 4, node)

    def part(self, internal, armor):
        p = self.z.alloc(0x180)
        self.w(p, self.part_vt)
        for s in range(6):
            self.wf(p + 0x100 + 4 * s, internal[s])
            self.wf(p + 0x140 + 4 * s, armor[s])
        return p

    def run(self, v, light):
        z, w, w8 = self.z, self.w, self.w8
        self.v, self.log, self.send_id, self.light = v, [], 100, light
        z.cpu.heap_next = self.mark
        m, vt = z.alloc(0x900), z.alloc(0x200)
        w(m, vt)
        w(vt + 0xac, self.power_fn)
        w(vt + 0x174, self.nine_fn)  # the player's id
        w(self.power, 1 if v[K['power']] else 0)
        w8(m + 0x8, v[K['active']])
        for i in range(3):
            w(m + 0x30 + 4 * i, v[K['vel'] + i])
        w(m + 0x48, v[K['qw']]), w(m + 0x50, v[K['qy']])
        self.vehicle = 0
        if v[K['target']]:
            w(m + 0x6c, z.alloc(16))
            self.vehicle = z.alloc(0x800)
            w(self.vehicle + 0x76c, v[K['target']])
        w8(m + 0xbc, v[K['xbc']])
        wm = z.alloc(0x40)
        w(wm + 4, v[K['state']])
        for i in range(3):
            w(wm + 0xc + 4 * i, v[K['aim'] + i])
        w(m + 0xd4, wm)
        w(m + 0x10c, v[K['clear']])
        # the controls: their virtual base at +4 offset 0, the table its own vtable (as test_pm_net_state's)
        ctl, tbl, vals = z.alloc(0x20), z.alloc(0x100), self.vals
        w(ctl + 4, tbl), w(tbl + 4, 0)
        w(vals, v[K['throttle']]), w(vals + 4, v[K['turn']]), w(vals + 8, v[K['jump']])
        w(tbl + 0x2c, self.getters[0]), w(tbl + 0x24, self.getters[1]), w(tbl + 0x38, self.getters[2])
        w(m + 0x38c, ctl)
        torso = z.alloc(0x200)
        for i in range(4):
            w(torso + 0x120 + 4 * i, v[K['turn_q'] + i])
        w(m + 0x3ac, torso)
        # the parts by id: the torso (1, and its sides 7 and 8 as sections 1 and 2), then 2..6 as the port's Mech
        ti, ta = fl(v[K['torso_internal']]), fl(v[K['torso_armor']])
        side = [fl(v[K['side_internal']]), fl(v[K['side_internal'] + 1])]
        sec = [ta] + [fl(v[K['sec_armor'] + i]) for i in range(5)]
        tp = self.part([ti, side[0], side[1], ti, side[0], side[1]], sec)
        self.parts = {1: tp, 7: tp, 8: tp}
        for k, pid in enumerate((2, 3, 4, 5, 6)):
            i, a = fl(v[K['parts'] + 2 * k]), fl(v[K['parts'] + 2 * k + 1])
            self.parts[pid] = self.part([i] * 6, [a] * 6)
        if v[K['ams']]:
            a = z.alloc(0x20)
            w8(a, 1 if v[K['ams']] == 2 else 0)
            w(m + 0x400, a)
        head = z.alloc(12)
        if v[K['action']] >= 0:
            node = z.alloc(12)
            w(head, node), w(head + 4, node), w(node, head), w(node + 4, head), w(node + 8, v[K['action']])
            w(m + 0x494, 1)
        else:
            w(head, head), w(head + 4, head)
        w(m + 0x490, head)
        w(m + 0x594, v[K['narc']]), w(m + 0x598, v[K['masc']]), w(m + 0x59c, v[K['anim_state']])
        w8(m + 0x5ab, v[K['crouched']])
        w(m + 0x5d0, v[K['limp']])
        shots = z.alloc(12)
        w(shots, shots), w(shots + 4, shots)
        for k in range(v[K['shots']]):
            rec = z.alloc(16)
            w(rec, v[K['shot'] + 3 * k]), w(rec + 8, v[K['shot'] + 3 * k + 1]), w(rec + 12, v[K['shot'] + 3 * k + 2])
            self.push(shots, rec)
        w(m + 0x69c, shots), w(m + 0x6a0, v[K['shots']]), w(m + 0x6a4, v[K['damaged']])
        self.pos_at = z.alloc(12)
        self.uc.mem_write(self.pos_at, struct.pack('<3i', *v[K['pos']:K['pos'] + 3]))
        lst = z.alloc(12)
        w(lst, lst), w(lst + 4, lst)
        w(MECHS, lst), w(MECHS_N, v[K['remotes']])
        nvt, pvt = z.alloc(0x200), z.alloc(0x200)  # a network mech's vt+0x60 (itself); a plain mech's (0)
        w(nvt + 0x60, self.self_fn), w(nvt + 0x174, self.nine_fn), w(pvt + 0x60, self.zero_fn), w(pvt + 0x174, self.nine_fn)
        remotes = []
        for k in range(v[K['remotes']]):
            r = z.alloc(0x800)
            w(r, nvt)
            w(r + 0x76c, v[K['owners'] + k]), w(r + 0x750, v[K['echo'] + k]), w(r + 0x748, v[K['heard'] + k])
            self.push(lst, r)
            remotes.append(r)
        plain = z.alloc(0x800)
        w(plain, pvt)
        wt = v[K['weapon_target']]
        tobj = 0 if wt == 0 else remotes[wt - 1] if wt <= len(remotes) else plain
        weapon = z.alloc(0x80)
        w(weapon + 0x20, v[K['weapon_slot']])
        w8(weapon + 0x60, v[K['weapon_locked']])
        w(weapon + 0x14, tobj)
        w8(RUNNING, 1)
        w(TURN, v[K['ping_turn']]), w(COUNTER, v[K['ping_counter']]), w(ACKED, v[K['acked']])
        g = self.game = z.alloc(0x100)
        w(g + 0x9c, 0x1234)
        part = z.alloc(0x80)
        w(part + 4, tobj)
        if v[K['arg']] >= 0:
            node = z.alloc(0x40)
            w(node + 0x28, v[K['arg']])
            w(part + 8, node)
        arg = {0x16: weapon, 0x13: tobj, 0x21: part}.get(v[K['kind']], v[K['arg']])
        ret = s32(z.call(SENDER, ecx=m, args=(v[K['kind']], arg)))
        self.log += [90, ret, s32(self.r(m + 0x6a4)), self.r(m + 0x6a0)]
        n = self.r(shots)
        while n != shots:
            rec = self.r(n + 8)
            self.log += [s32(self.r(rec + 4 * i)) for i in range(4)]
            n = self.r(n)
        for r in remotes:
            self.log += [92, s32(self.r(r + 0x760)), s32(self.r(r + 0x764))]
        return self.log


KINDS = [0xf, 0xf, 0xf, 0xf, 0x10, 0x10, 0x12, 0x13, 0x15, 0x16, 0x17, 0x21]


def case(rng):
    v = state_case(rng)
    v[K['kind']] = rng.choice(KINDS)
    if v[K['kind']] == 0x15:
        v[K['arg']] = rng.randrange(0, 12)
    if v[K['kind']] == 0x21:  # a network mech's part: the exe reads its mech's +0x76c unchecked
        v[K['remotes']] = max(v[K['remotes']], 1)
        v[K['weapon_target']] = rng.randrange(1, v[K['remotes']] + 1)
    return v


def port_log(probe, v, light):
    probe.probe_mech_net_light(light)
    out = (I32 * 8192)()
    n = probe.probe_mech_net_send((I32 * COUNT)(*v), out, 8192)
    log = list(out[:n])
    i = 0  # walked by its records: each send (1, to, guaranteed, size, words), 2 / 5 with one word, else alone
    while log[i] != 90:
        i += 4 + (log[i + 3] + 3) // 4 if log[i] == 1 else 2 if log[i] in (2, 5) else 1
    end = i + 4 + 4 * log[i + 3]  # 90, the answer, the damaged parts, the shots' count, then each shot's 4 words
    return log[:end] + log[end + 12:]  # the timer's 91 block (12 words) dropped; the remotes' 92 blocks kept


FLOAT_WORDS = (6, 7, 8)  # a state's +0x18 heading, +0x1c / +0x20 the torso's angles (atan2 and the quat's on the x87)


def same(mine, theirs):
    """Equal but the state messages' computed floats, within float rounding."""
    if len(mine) != len(theirs):
        return False
    loose = set()
    i = 0
    while i < len(theirs):
        if theirs[i] == 1 and i + 4 < len(theirs) and theirs[i + 4] & 0xffff == 0xf:
            base = i + 4
            loose.update(base + w for w in FLOAT_WORDS)
            i = base + (theirs[i + 3] + 3) // 4
        else:
            i += 1
    for k, (a, b) in enumerate(zip(mine, theirs)):
        if a == b:
            continue
        if k not in loose:
            return False
        x, y = fl(a), fl(b)
        if not (math.isfinite(x) and math.isfinite(y) and abs(x - y) <= 2e-6 * max(1.0, abs(y))):
            return False
    return True


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_send.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    probe.probe_mech_net_send.restype = I32
    probe.probe_mech_net_light.argtypes = [I32]
    assert COUNT == probe.probe_mech_net_in_count(), (COUNT, probe.probe_mech_net_in_count())
    brk = int(os.environ.get('BREAK', '0'))
    n_cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(0x471210)
    fails = 0
    kinds, parts = {}, 0
    for n in range(n_cases):
        v = case(rng)
        light = rng.randrange(2)
        theirs = exe.run(v, light)
        mine_v = list(v)
        if brk == 1:
            mine_v[K['turn']] = mine_v[K['throttle']]
        mine = port_log(probe, mine_v, light)
        kinds[v[K['kind']]] = kinds.get(v[K['kind']], 0) + 1
        parts += v[K['kind']] == 0xf and v[K['damaged']] & 0x1fe != 0
        if not same(mine, theirs):
            fails += 1
            if fails <= 2:
                i = next((j for j, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
                print(f'FAIL case {n} kind {v[K["kind"]]:#x} at {i}:\n  port     {mine[max(0, i - 8):i + 8]}\n'
                      f'  original {theirs[max(0, i - 8):i + 8]}')
    if fails:
        print(f'{fails} of {n_cases} differ')
        sys.stdout.flush(), os._exit(1)
    print(f'ok: {n_cases} sends identical to PM\'s 0x471210 (by kind '
          f'{ {hex(k): c for k, c in sorted(kinds.items())} }; {parts} states with damaged parts)')
    sys.stdout.flush(), os._exit(0)


if __name__ == '__main__':
    main()
