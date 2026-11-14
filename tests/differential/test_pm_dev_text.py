"""The developers' text on Pirate's Moon's exe (mech3/dev_text, gamez/ztwod/twod_lines; test_dev_text.py is MW3's):
the state names 0x43dcf0, the zones text 0x43dbf0, the line log's add and colours 0x5c7030 / 0x5c6fe0 (set_timer
0x5c02c0), the readout 0x410470 and the frame rate 0x40fae0. PM's lines are 0x4b8 bytes (MW3's 0x4b0) with the same
fields; its readout, names and zones take their argument on the stack, the log's set_active is vt+8, and the frame
rate's fields sit 8 bytes later. The exe's show, hide and set_text (0x5c0170, 0x5c01c0, 0x5c1e70) run on the lines.

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_dev_text.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import Original  # noqa: E402
from twod_world import IAT, c_format  # noqa: E402  (IAT mapped to PM's slots)
import test_dev_text as mw3  # noqa: E402
import test_pm_zar  # noqa: E402

STATE_NAME, ZONES_TEXT, LOG_ADD, LOG_COLORS, READOUT, FPS_UPDATE = (0x43dcf0, 0x43dbf0, 0x5c7030, 0x5c6fe0,
                                                                    0x410470, 0x40fae0)
SHOW_FOR, HIDE, SET_TEXT = 0x5c0170, 0x5c01c0, 0x5c1e70
CS_CTOR, CS_DTOR, CS_FORMAT, CS_ADD_CS, CS_ADD_SZ, ATEXIT = 0x5ce8e4, 0x5ce8d8, 0x5ce8de, 0x5ceb1e, 0x5ceb30, 0x5cee12
LINE = 0x4b8
ROWS, READOUT_ON, ZONE = 0x6203f0, 0x627bf0, 0x6302ac
COUNTS = (0x6393cc, 0x6393c4, 0x8d4af8, 0x8d4af0, 0x68a8a4, 0x68a89c)
POOL, VIDEO = 0x5a34d0, 0x8c4bb4
# The frame rate: the format's guard and CString, the unscaled step, the net's flag and numbers, the frame number,
# zEntity's list, Object2D's update.
FPS_GUARD, FPS_CS, RAW, NET_ON, NET_NUMS, FRAME, ENTS, OBJ_UPDATE = (0x61fed0, 0x6279ec, 0x8c1b54, 0x629a88,
                                                                     (0x629a8c, 0x629a90, 0x629a60), 0x62db48,
                                                                     0x8d3ac4, 0x5c0220)


MW3Exe = mw3.Exe  # main() points the MW3 test's Exe at this one


class Exe(MW3Exe):
    def __init__(self):  # mw3.Exe's set-up, at PM's addresses
        self.cpu = cpu = Original(exe='pm')
        self.r32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]
        self.strings = {}
        ret = cpu.code(bytes.fromhex('c3'))
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
        cpu.u32(self.log_vt + 8, set_active)  # PM's set_active is vt+8 (MW3's vt+4)
        self.texts = None
        self.vid = cpu.alloc(0x20)
        self.vid_out = (0, 0, 0)
        cpu.stub_function(self.vid, self.video_memory)  # cdecl (-1, &total, &free)
        cpu.u32(VIDEO, self.vid)
        self.pool = (0, 0, 0, 0)
        cpu.stub_function(POOL, lambda c: [cpu.u32(c.arg(0) + 4 * i, v) for i, v in enumerate(self.pool)] and 0)

    def video_memory(self, c):
        ok, total, free = self.vid_out
        self.cpu.u32(c.arg(1), total)
        self.cpu.u32(c.arg(2), free)
        return ok

    def state_name(self, state):
        self.cpu.call(STATE_NAME, stack_args=(state & 0xffffffff,))
        return self.cstr(self.eax())

    def zones(self, zs):
        a = self.cpu.alloc(4)
        self.cpu.write(a, bytes(zs))
        self.cpu.call(ZONES_TEXT, stack_args=(a,))
        return self.cstr(self.r32(self.eax()))

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
                cpu.call(LOG_ADD, ecx=log, stack_args=(t, mw3.fbits(op[1])))
            elif op[0] == 'k':
                at = log + 0x10 + op[1] * LINE
                cpu.u32(at + 0x24c, op[2])
                cpu.u32(at + 0x250, op[3])
            else:
                cpu.call(LOG_COLORS, ecx=log, stack_args=(op[1], op[2]))
            out.append([self.active] + [self.line(log + 0x10 + k * LINE) for k in range(15)])
        return out

    def new_line(self, at, top=0x996a00, bottom=0x95c7ff):
        self.cpu.write(at, bytes(LINE))
        MW3Exe.new_line(self, at, top, bottom)

    def readout(self, zone, counts, pool, vid, clear):
        cpu = self.cpu
        for k in range(25):
            self.new_line(ROWS + k * LINE)
        cpu.u32(ZONE, zone)
        for addr, v in zip(COUNTS, counts):
            cpu.u32(addr, v)
        self.pool, self.vid_out = pool, vid
        cpu.u32(READOUT_ON, 0)
        for mode in (1, 2) + ((0,) if clear else ()):
            cpu.call(READOUT, stack_args=(mode,))
        return [[self.r32(ROWS + k * LINE + 0xc) & 0x10, self.cstr(ROWS + k * LINE + 0x34)] for k in range(25)]

    def fps(self, steps, net, frame, entities):
        cpu = self.cpu
        cpu.write(FPS_GUARD, b'\1')
        self.cs_set(FPS_CS, '')
        if self.texts is None:
            cpu.stub_function(SET_TEXT, lambda c: self.texts.append(c_format(self, self.cstr(c.arg(1)), 0)) or 0)
            cpu.stub_function(OBJ_UPDATE, lambda c: 0, pop_bytes=4)
        self.texts = texts = []
        f = cpu.alloc(0x4d0)
        cpu.write(f, bytes(0x4d0))
        cpu.write(NET_ON, bytes([net[0] & 0xff]))
        for addr, v in zip(NET_NUMS, net[1:]):
            cpu.u32(addr, v)
        cpu.u32(FRAME, frame)
        begin = 0x10000000 if entities else 0
        cpu.u32(ENTS, begin)
        cpu.u32(ENTS + 4, begin + 4 * entities)
        out = []
        for s in steps:
            cpu.u32(RAW, mw3.fbits(s))
            cpu.call(FPS_UPDATE, ecx=f, stack_args=(0,))
            out.append([self.r32(f + 0x4b8), self.r32(f + 0x4bc), self.r32(f + 0x4c0), texts[-1]])
        return out


def main():
    mw3.load_probe = test_pm_zar.load_pm_probe
    mw3.Exe = Exe
    mw3.main()


if __name__ == '__main__':
    main()
