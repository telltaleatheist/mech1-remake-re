"""Pirate's Moon's add-child 0x4fd5e0 against the port's (pm::zcls::node_add_child through the game table,
tests/probe_pm_add_child.cpp): MW3's 0x4eb9b0 with PM's files and report lines, and its own object3d
(0x50f9c0), animate (0x515380) and switch (0x514c50) adds; the LoD's 0x515bb0 calls the link.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_add_child.py

Each case: a parent of a class (every one but the camera's and the world's, which have their own tests), its
class data set or not, a null parent or child, or the parent its own child. On the exe the link 0x4fd6f0, the
report 0x406cb0 and the fatal report 0x527250 are stubs: compared are the result, whether the child was
linked under the parent, and each report's level, file, line and text (the fatal one's text from the sprintf
into 0x8d3f80).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT  # noqa: E402

ADD, LINK, REPORT, FATAL, BUFFER = 0x4fd5e0, 0x4fd6f0, 0x406cb0, 0x527250, 0x8d3f80
FATAL_FILE = b'C:\\zipper\\branchep1\\gamez\\zError\\zerr_old.c'

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_add_child.argtypes = [ctypes.c_int] * 5 + [ctypes.c_char_p, ctypes.c_int]
probe.probe_pm_add_child.restype = ctypes.c_int


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.said, self.linked = [], None
        self.native_stub(LINK, self.link)
        self.native_stub(REPORT, self.report)
        self.native_stub(FATAL, self.fatal)
        self.cpu.stub_import(IAT['sprintf'], self.sprintf, 0)

    def link(self, c):
        self.linked = (c.arg(0), c.arg(1))  # cdecl
        return 0

    def report(self, c):
        self.said.append(b'%d|%s|%d|%s' % (c.arg(0), self.cstr(c.arg(1)), c.arg(2), self.cstr(c.arg(3))))
        return 0

    def fatal(self, c):
        self.said.append(b'1|%s|35|%s' % (FATAL_FILE, self.cstr(BUFFER)))
        return 0

    def sprintf(self, c):
        fmt = self.cstr(c.arg(1)).decode('latin1')
        text = fmt.replace('%s', '{}', 1).replace('%d', '{}', 1).replace('%s', '{}', 1).format(
            self.cstr(c.arg(2)).decode('latin1'), c.arg(3), self.cstr(c.arg(4)).decode('latin1'))
        b = text.encode('latin1') + b'\0'
        self.cpu._uc.mem_write(c.arg(0), b)
        return len(b) - 1

    def run(self, cls, data, null_parent, null_child, same):
        parent, child = self.alloc(0xd4), self.alloc(0xd4)
        self.cpu._uc.mem_write(parent, b'parent_node\0')
        self.cpu._uc.mem_write(parent + 0x34, struct.pack('<i', cls))
        self.cpu._uc.mem_write(parent + 0x38, struct.pack('<I', self.alloc(0x100) if data else 0))
        if same:
            child = parent
        self.said, self.linked = [], None
        r = self.call(ADD, args=(0 if null_parent else parent, 0 if null_child else child)) & 0xffffffff
        linked = self.linked == (parent, child) and not same
        return r * 2 + int(linked), b''.join(s + b'\n' for s in self.said)


def main():
    emu = Emulated()
    cases = [(c, d, 0, 0, 0) for c in (0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 99, -1) for d in (0, 1)]
    cases += [(5, 1, 1, 0, 0), (5, 1, 0, 1, 0), (11, 1, 1, 1, 0), (8, 0, 0, 0, 1), (3, 1, 0, 0, 1)]
    bad = 0
    seen = {'linked': 0, 'refused': 0, 'fatal': 0}
    for case in cases:
        want = emu.run(*case)
        buf = ctypes.create_string_buffer(4096)
        r = probe.probe_pm_add_child(*case, buf, 4096)
        got = (r, buf.value)
        if want != got:
            bad += 1
            print(f'case {case}:\n  exe  {want}\n  port {got}')
        seen['linked'] += want[0] & 1
        seen['refused'] += want[0] >> 1 == 5
        seen['fatal'] += b'zerr_old' in want[1]
    print(f'{len(cases)} cases, {bad} differ; {seen}')
    sys.stdout.flush()
    os._exit(1 if bad or min(seen.values()) == 0 else 0)


if __name__ == '__main__':
    main()
