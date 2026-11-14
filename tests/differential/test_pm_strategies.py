"""Pirate's Moon's attack strategy table and its index (docs/editor.md, The vehicle editor; game_editor_vehicles
pm_strategy_table / pm_strategy_index) against Mech3.exe PM 1.0: 0x4c31c0 run as at start-up, then 0x4c3280 ->
0x4c2ee0 (std::sort's pieces 0x4c3550 / 0x4c3680 / 0x4c36c0 / 0x4c3750 / 0x4c37c0 and the tests 0x4c2ea0 / 0x4c2ec0,
the exe's own): the eight entries, and the index's two orders, by number and by the name's address.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_strategies.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_PM_MOVED', '1')
from original import Original  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

TABLE, INDEX = 0x62f920, 0x62f960


def main():
    cpu = Original(exe='pm')
    cpu.call(0x4c31c0)
    cpu.call(0x4c3280)
    u32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]
    ids = [u32(TABLE + 8 * k) for k in range(8)]
    names = [cpu.read(u32(TABLE + 8 * k + 4), 16).split(b'\0')[0].decode() for k in range(8)]
    entry = lambda p: (p - TABLE) // 8
    exe = (ids, [entry(u32(INDEX + 4 + 4 * k)) for k in range(8)], [entry(u32(INDEX + 0x24 + 4 * k)) for k in range(8)],
           names, u32(INDEX) == TABLE)
    lib = load_pm_probe()
    out = (ctypes.c_int32 * 25)()
    buf = ctypes.create_string_buffer(256)
    lib.probe_pm_strategies(out, buf, len(buf))
    port = (list(out[0:8]), list(out[8:16]), list(out[16:24]), buf.value.decode().split('|')[:8], bool(out[24]))
    print('exe: ', exe)
    print('port:', port)
    ok = exe == port
    print('OK' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
