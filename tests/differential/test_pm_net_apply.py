"""Pirate's Moon's state applied, 0x4705e0 (MW3's 0x46e4d0), on active states (test_pm_net_state's part 2 has the
inactive ones), against the port's net_state_apply through probe_mech_net_apply: the velocity (x 1/127), the
throttle and turn through the controls (vt+0x20, vt+0x28), the torso's turn (0x53ebc0 on the two angles, then
0x465f90), the aim at the point sent (0x4b4a20; MW3 rebuilt it from a turn and a distance), the heading, the passive
radar's bit (vt+0xc4 when it changes), the target (the pilot's mech for the local player, else the list's, 0x4ab090),
the shots replayed (0x4b4750), the damaged parts (vt+0x3c, vt+0x40 by section), then the flags' half (power, the AMS,
MASC, NARC, crouch, the headlight, the jump, the motion state's turns). Messages of random words, shot and part
counts (the size covering them; a cut-short one is FIX (A) in the port). Compared: every call in order with its arguments, the velocity, the
heading, the torso's quaternion, the controls' values, the passive bit and the AMS after.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_apply.py [cases]
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
from test_pm_net_state import Apply, APPLY, f2u, u2f  # noqa: E402

I32 = ctypes.c_int32
TORSO_SET, AIM, PICK, FIRE, PART_BY_ID, LOCAL_ID, FIND = 0x465f90, 0x4b4a20, 0x4ab090, 0x4b4750, 0x41e2d0, 0x5a8a30, 0x4730b0
MECHS, PILOT_GAME, IAT_FTOL = 0x629a5c, 0x62c40c, 0x5de644
# probe_mech_net_apply's input (tests/probe_mech_net.cpp, Rx)
R = dict(mode=0, len=1, msg=2, flags=66, pos=67, vel=70, heading=73, aim_q=77, heard=81, latency=82, orient=83,
         world=87, torso_at=90, xbc=93, power=94, masc=95, narc=96, crouched=97, anim=98, ams=99, local=100, now=101,
         dt=102, ping_id=103, ping_sent=104, acked=105, pilot_sent=106, remotes=107, owners=108, count=112)
SIZES = {10: 3, 11: 4, 12: 4, 13: 5, 14: 1, 15: 2, 16: 4, 17: 2, 20: 4}  # the port's log records by code


class Active(Apply):
    def __init__(self):
        super().__init__()
        z, w = self.z, self.w
        self.w(IAT_FTOL, z.cpu.code(FTOL))
        for off, code in ((0x20, 'throttle'), (0x28, 'turn')):
            fn = z.alloc(4)
            z.native_stub(fn, lambda u, code=code: self.vals.__setitem__(code, u.arg(0)), 4)
            w(self.tbl + off, fn)
        radar = z.alloc(4)
        z.native_stub(radar, lambda u: self.log.append((14,)))
        w(self.vt + 0xc4, radar)
        z.native_stub(TORSO_SET, lambda u: self.vals.__setitem__('torso', [self.r(u.arg(0) + 4 * i) for i in range(4)]), 4)
        z.native_stub(AIM, lambda u: self.log.append((11,) + tuple(self.r(u.arg(0) + 4 * i) for i in range(3))), 4)
        z.native_stub(PICK, lambda u: self.log.append((15, self.code_of(u.arg(0)))), 4)
        z.native_stub(FIRE, lambda u: self.log.append((12, u.arg(0), u.arg(1), u.arg(2))), 12)
        z.native_stub(LOCAL_ID, lambda u: self.local)
        z.native_stub(PART_BY_ID, lambda u: self.parts.get(u.arg(0), 0), 4)
        pvt = self.part_vt = z.alloc(0x100)
        for off, kind in ((0x3c, 0), (0x40, 1)):
            fn = z.alloc(4)
            z.native_stub(fn, lambda u, kind=kind: self.log.append((13, self.part_index[u.ecx], kind, u.arg(0), u.arg(1))), 8)
            w(pvt + off, fn)
        self.parts, self.part_index = {}, {}
        for pid in range(1, 9):  # the torso (1, its sides 7 and 8 the same object) and the parts 2..6 by index
            if pid in (7, 8):
                self.parts[pid] = self.parts[1]
                continue
            p = z.alloc(0x40)
            w(p, pvt)
            self.parts[pid] = p
            self.part_index[p] = pid
        self.mark = z.cpu.heap_next

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def code_of(self, a):
        if a == 0:
            return 0
        if a == self.pilot:
            return 1
        return 2 + self.remotes.index(a) if a in self.remotes else -1

    def go(self, msg, state, local, owners):
        z, w, m = self.z, self.w, self.mech
        z.cpu.heap_next = self.mark
        self.log, self.light, self.local, self.vals = [], state['light'], local, {}
        buf = z.alloc(len(msg) + 4)
        self.uc.mem_write(buf, bytes(msg))
        w(self.power, 1 if state['power'] else 0)
        w(m + 0x400, self.ams if state['ams'] else 0)
        self.uc.mem_write(self.ams, bytes([1 if state['ams'] == 2 else 0]))
        w(m + 0x598, state['masc']), w(m + 0x594, state['narc'])
        self.uc.mem_write(m + 0x5ab, bytes([1 if state['crouched'] else 0]))
        w(m + 0x59c, state['anim'])
        self.uc.mem_write(m + 0xbc, bytes([state['xbc']]))
        w(m + 0x3ac, z.alloc(0x200))  # the torso
        w(m + 0xd4, z.alloc(0x40))    # the weapons
        game = z.alloc(0x40)
        self.pilot = z.alloc(0x800)
        w(game + 0x14, self.pilot), w(PILOT_GAME, game)
        head = z.alloc(12)
        w(head, head), w(head + 4, head), w(MECHS, head)
        self.remotes = []
        for o in owners:
            r = z.alloc(0x800)
            w(r + 0x76c, o)
            node, last = z.alloc(12), self.r(head + 4)
            w(node, head), w(node + 4, last), w(node + 8, r), w(last, node), w(head + 4, node)
            self.remotes.append(r)
        self.z.call(APPLY, ecx=m, args=(buf,))
        vel = [self.r(m + 0x7d0 + 4 * i) for i in range(3)]
        heading = [self.r(m + 0x7c0 + 4 * i) for i in range(4)]
        ams = self.uc.mem_read(self.ams, 1)[0] if state['ams'] else -1
        return dict(calls=[e for e in self.log if e[0] != 'jump'], vel=vel, heading=heading,
                    torso=self.vals.get('torso'), throttle=self.vals.get('throttle'), turn=self.vals.get('turn'),
                    jump=next((e[1] for e in self.log if e[0] == 'jump'), None),
                    xbc=self.uc.mem_read(m + 0xbc, 1)[0] & 4, ams=ams)


def port(probe, msg, state, local, owners):
    probe.probe_mech_net_light(state['light'])
    v = [0] * R['count']
    v[R['mode']], v[R['len']] = 0, len(msg)
    padded = bytes(msg) + bytes(4 * 64 - len(msg))
    for k in range(64):
        v[R['msg'] + k] = struct.unpack_from('<i', padded, 4 * k)[0]
    for key in ('power', 'masc', 'narc', 'crouched', 'anim', 'ams', 'xbc'):
        v[R[key]] = state[key]
    v[R['local']] = local
    v[R['remotes']] = len(owners)
    for k, o in enumerate(owners):
        v[R['owners'] + k] = o
    v[R['heard']], v[R['now']] = 1000, 5000
    v[R['orient']] = f2u(1.0)
    out = (I32 * 4096)()
    n = probe.probe_mech_net_apply((I32 * R['count'])(*v), out, 4096)
    lg = [x & 0xffffffff for x in out[:n]]
    calls, i = [], 0
    while lg[i] != 90:
        k = SIZES[lg[i]]
        rec = tuple(lg[i:i + k])
        if rec[0] == 16:  # the respawn: the exe's stand-in (test_pm_net_state's) logs no place
            rec = (16,)
        calls.append(rec)
        i += k
    after = lg[i + 1:]
    vel, heading = after[15:18], after[18:22]
    torso = after[15 + 3 + 4 + 4 + 4 + 9:15 + 3 + 4 + 4 + 4 + 9 + 4]
    j = after.index(91)  # 91, heard, xbc, the AMS, throttle, turn, jump targets, jump held, latency
    b = after[j:]
    return dict(calls=calls, vel=vel, heading=heading, torso=torso, throttle=b[4], turn=b[5], jump=b[6],
                xbc=b[2] & 4, ams=b[3] if b[3] != 0xffffffff else -1)


def message(rng, owners, local):
    shots, parts = rng.choice([0, 0, rng.randrange(1, 8)]), rng.choice([0, 0, rng.randrange(1, 6)])
    msg = bytearray(0x3c + shots + 4 * parts + rng.choice([0, 0, 3]))
    struct.pack_into('<HH3f3h', msg, 0, 0xf, 0, *[rng.uniform(-900, 900) for _ in range(3)],
                     *[rng.randrange(-32768, 32768) for _ in range(3)])
    struct.pack_into('<3f2b', msg, 0x18, rng.uniform(-3.2, 3.2), rng.uniform(-1.5, 1.5), rng.uniform(-3.2, 3.2),
                     rng.randrange(-128, 128), rng.randrange(-128, 128))
    struct.pack_into('<3fI', msg, 0x28, *[rng.uniform(-900, 900) for _ in range(3)],
                     rng.choice([0, local, rng.choice(owners) if owners else 77, 77]))
    flags = rng.getrandbits(32) & ~2 & ~(0x7f << 11) & ~(0xf << 18)
    flags |= shots << 11 | parts << 18
    struct.pack_into('<I', msg, 0x38, flags)
    for k in range(shots):
        msg[0x3c + k] = rng.getrandbits(8)
    for k in range(parts):
        struct.pack_into('<I', msg, 0x3c + shots + 4 * k, rng.choice([0, 9, 12, rng.randrange(1, 9)]) |
                         rng.getrandbits(24) << 8)
    # The size covers what it holds: one cut short is read past by the exe and bounded by the port (FIX (A), a
    # peer's packet untrusted; fixes.md), so it is no case for comparing.
    struct.pack_into('<H', msg, 2, len(msg))
    return bytes(msg)


def close(a, b):
    if a == b:
        return True
    x, y = u2f(a), u2f(b)
    return math.isfinite(x) and math.isfinite(y) and abs(x - y) <= 2e-6 * max(1.0, abs(y))


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_apply.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    probe.probe_mech_net_apply.restype = I32
    probe.probe_mech_net_light.argtypes = [I32]
    assert R['count'] == probe.probe_mech_net_rx_count(), probe.probe_mech_net_rx_count()
    n_cases = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Active()
    rng = random.Random(0x4705e0)
    seen = {}
    for n in range(n_cases):
        owners = rng.sample(range(1, 9), rng.randrange(0, 4))
        local = rng.choice([o for o in range(1, 9) if o not in owners])
        msg = message(rng, owners, local)
        state = dict(power=rng.randrange(4), masc=f2u(rng.choice([0.0, 2.5])), narc=f2u(rng.choice([0.0, 3.0, -1.0])),
                     crouched=rng.randrange(2), anim=rng.choice([0, 0, 1, 5, 9, 10, 12, 13, rng.randrange(16)]),
                     ams=rng.randrange(3), light=rng.randrange(2), xbc=rng.getrandbits(8))
        theirs = exe.go(msg, state, local, owners)
        mine = port(probe, msg, state, local, owners)
        bad = [k for k in ('calls', 'xbc', 'ams', 'jump') if theirs[k] != mine[k]]
        for k in ('vel', 'heading', 'torso'):
            if theirs[k] is None or len(theirs[k]) != len(mine[k]) or not all(close(a, b) for a, b in zip(theirs[k], mine[k])):
                bad.append(k)
        for k in ('throttle', 'turn'):  # the exe's argument, the port's value once set (IControls clamps the throttle)
            t = theirs[k]
            if k == 'throttle' and t is not None:
                t = f2u(max(-1.0, min(1.0, u2f(t))))
            if t is None or not close(t, mine[k]):
                bad.append(k)
        if bad:
            print(f'case {n}: {bad}\n  exe  {[theirs[k] for k in bad]}\n  port {[mine[k] for k in bad]}')
            sys.stdout.flush()
            os._exit(1)
        for e in theirs['calls']:
            seen[e[0]] = seen.get(e[0], 0) + 1
    print(f'ok: {n_cases} active states applied as PM\'s 0x4705e0 does (calls by kind {dict(sorted(seen.items()))}: '
          '10 the mech\'s, 11 the aim, 12 a shot, 13 a part, 14 the radar, 15 the target, 16 respawn, 17 the light)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
