"""A hit on an animation on Pirate's Moon's exe: the hit handlers 0x51b2c0 (a collision) and 0x51b380 (a weapon) run
0x51b340 after the damage, which starts the def's second sequence (+0xd4) from its start (0x51fc00) and runs it
(0x520800) on the frame's time. No shipped def has one (docs/pm-tests.md); the sequence here holds PM's colour
events (45 and 44, as test_pm_color_events.py) on the def's own node, so the run's effect shows in the colours, half of
them under an IF on the health (0x800), which shows the pass comes after the damage.
Compared: the result, the health, the run time, the time left, the sequence's state and place, the colours. The
health stays above 0 (the start of the animation is not this test's).

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_hit_sequence.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
os.environ['MW3_X87'] = '24'
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import test_pm_zar  # noqa: E402
from twod_world import IAT  # noqa: E402

COLLIDE, WEAPON = 0x51b2c0, 0x51b380
DT, TIME_LEFT, HEALTH_AFTER = 0x8c1b4c, 0x8d3f68, 0x5a5150


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def event(r, kind):
    ev = bytearray(0x78)
    ev[0] = kind
    struct.pack_into('<i', ev, 4, 0x78)
    struct.pack_into('<f', ev, 8, 0.0)
    struct.pack_into('<i', ev, 0x2c, -100)
    for off in range(0x34, 0x70, 4):
        struct.pack_into('<f', ev, off, r.uniform(-2, 2))
    struct.pack_into('<hh', ev, 0x70, -1, -1)
    struct.pack_into('<f', ev, 0x74, r.choice((0.0, 0.01, 0.5, 2.0)))
    return bytes(ev)


def case(r):
    weapon = r.random() < 0.5
    kind = r.choice((0, 2)) if weapon else r.choice((1, 2, 0, 3))
    health = r.uniform(50, 500)
    f = [kind, health, r.choice((0.0, 5.0, 40.0)), r.uniform(500, 900), r.uniform(0, 30),
         r.uniform(0, 45), r.choice((0.033, 0.1, 0.5)), r.uniform(0, 1)]
    f = [f32(x) for x in f]
    events = None
    if r.random() < 0.8:
        events = b''.join(event(r, r.choice((44, 45))) for _ in range(r.randint(0 if r.random() < 0.1 else 1, 3)))
        if events and r.random() < 0.5:  # IF the health (0x800) is at most a value near it after the hit: the pass
            cond = bytearray(0x1c)        # sees the health after the damage
            cond[0] = 31
            struct.pack_into('<if', cond, 4, 0x1c, 0.0)
            struct.pack_into('<Iiff', cond, 0xc, 0x800, -1, f32(health - r.uniform(0, 45)), 0.0)
            end = bytearray(0xc)
            end[0] = 34
            struct.pack_into('<i', end, 4, 0xc)
            events = bytes(cond) + events + bytes(end)
    polys = r.randint(1, 4)
    return dict(weapon=weapon, f=f, events=events, reset=r.choice((0, 1)), polys=polys,
                verts=[r.randint(1, 6) for _ in range(polys)])


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(max(c.arg(0), 4)))
        cpu.stub_function(HEALTH_AFTER, lambda c: 0)
        cpu.call(0x521c70)  # the event handler table, as the start-up fills it

    def alloc(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, c):
        cpu, f = self.cpu, c['f']
        polys = self.alloc(0x28 * c['polys'])
        for k, v in enumerate(c['verts']):
            cpu.u32(polys + 0x28 * k, v)
        m = self.alloc(0x68)
        cpu.u32(m + 0x10, c['polys'])
        cpu.u32(m + 0x34, polys)
        n = self.alloc(0xd4)
        cpu.u32(n + 0x3c, m)
        d = self.alloc(0x110)
        cpu.u32(d + 0x48, n)
        cpu.write(d + 0xa1, bytes([int(f[0])]))
        cpu.write(d + 0xb8, struct.pack('<f', f[1]))
        cpu.write(d + 0xbc, struct.pack('<f', f[2]))
        cpu.write(d + 0xb4, struct.pack('<f', f[3]))
        cpu.write(d + 0xb0, struct.pack('<f', f[4]))
        s = ev = 0
        if c['events'] is not None:
            ev = self.alloc(len(c['events']) + 0x10)
            cpu.write(ev, c['events'])
            s = self.alloc(0x40)
            cpu.u32(s + 0x38, ev)
            cpu.u32(s + 0x3c, len(c['events']))
            cpu.write(s + 0x21, bytes([c['reset']]))
            cpu.u32(d + 0xd4, s)
        cpu.write(DT, struct.pack('<f', f[6]))
        cpu.write(TIME_LEFT, struct.pack('<f', f[7]))
        if c['weapon']:
            w = self.alloc(0x1f4)
            cpu.call(WEAPON, stack_args=(d, w, 0, struct.unpack('<I', struct.pack('<f', f[5]))[0]))
            out = struct.pack('<I', cpu._uc.reg_read(UC_X86_REG_EAX))
        else:
            cpu.call(COLLIDE, stack_args=(d, 0, struct.unpack('<I', struct.pack('<f', f[5]))[0]))
            out = cpu.read(d + 0xb8, 4)  # its result is the health (+0xb8), left on the x87 stack
        out += cpu.read(d + 0xb8, 4) + cpu.read(d + 0xb0, 4) + cpu.read(TIME_LEFT, 4)
        state = cpu.read(s + 0x20, 1)[0] if s else 0
        cur = self.r32(s + 0x34) - ev if s and self.r32(s + 0x34) else 0xffffffff
        out += struct.pack('<iI', state, cur & 0xffffffff)
        for k, v in enumerate(c['verts']):
            col = self.r32(polys + 0x28 * k + 0x1c)
            out += cpu.read(col, 12 * v) if col else bytes(12 * v)
        return out


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 800
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_hit_sequence.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_float), ctypes.c_char_p, ctypes.c_int,
                                        ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int32), ctypes.c_char_p,
                                        ctypes.c_int]
    exe = Exe()
    r = random.Random(0x51b340)
    buf = ctypes.create_string_buffer(1 << 16)
    bad = 0
    seen = {'collide': 0, 'weapon': 0, 'ran': 0, 'no sequence': 0, 'no events': 0}
    for k in range(cases):
        c = case(r)
        want = exe.run(c)
        evb = ctypes.create_string_buffer(c['events'], len(c['events']) + 0x10) if c['events'] is not None else None
        n = p.probe_pm_hit_sequence(int(c['weapon']), (ctypes.c_float * 8)(*c['f']), evb,
                                    len(c['events'] or b''), c['reset'], c['polys'],
                                    (ctypes.c_int32 * c['polys'])(*c['verts']), buf, len(buf))
        got = buf.raw[:n]
        seen['weapon' if c['weapon'] else 'collide'] += 1
        hit = int(c['f'][0]) in ((0, 2) if c['weapon'] else (1, 2))
        if c['events'] is None:
            seen['no sequence'] += 1
        elif not c['events']:
            seen['no events'] += 1
        elif hit:
            seen['ran'] += 1
        if got != want:
            bad += 1
            if bad <= 4:
                print(f'case {k}: exe {want[:24].hex()} port {got[:24].hex()} ({len(want)}/{len(got)})')
    print(f'{cases} cases, {bad} differ; {seen}')
    if bad or min(seen.values()) == 0:
        raise SystemExit(1)
    print(f'ok: PM\'s hit pass as PM\'s exe ({cases} hits)')


if __name__ == '__main__':
    main()
