"""PM's in/out record against its exe (docs/ai.md section 11): the constructor 0x4c7f30 and reset 0x4c8010
(mech3::ai::InOutRecordPm, 0x54 bytes: MW3's 0x3c and five fields more), which PM's pull 0x4ccaf0 fills.

    cmake --build build --target pmprobe && .venv/bin/python tests/differential/test_pm_inout_record.py [runs]

Each run: 0x54 random bytes and a frame counter; the constructor or the reset on them, the bytes after
compared (the constructor's +0x24..+0x2f left out: the exe leaves them, the port zeroes them).
"""
import ctypes
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

CTOR, RESET, FRAME, ZERO, SIZE = 0x4c7f30, 0x4c8010, 0x8c4b60, 0x66f5e0, 0x54


def main():
    probe = load_probe()
    probe.probe_pm_inout_record.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p]
    cpu = Original(exe='pm')
    cpu.tr = lambda a, size=1: a
    rec = cpu.alloc(0x60)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    r = random.Random(0x4c7f30)
    seen = [0, 0]
    for i in range(runs):
        reset = r.randrange(2)
        seen[reset] += 1
        data = bytes(r.randrange(256) for _ in range(SIZE))
        frame = r.randrange(1 << 32)
        cpu.write(rec, data)
        cpu.u32(FRAME, frame)
        cpu.write(ZERO, bytes(12))
        cpu.call(RESET if reset else CTOR, ecx=rec)
        want = bytearray(cpu.read(rec, SIZE))
        out = ctypes.create_string_buffer(SIZE)
        probe.probe_pm_inout_record(reset, data, frame, out)
        got = bytearray(out.raw)
        if not reset:
            want[0x24:0x30] = got[0x24:0x30] = bytes(12)
        if want != got:
            k = next(j for j in range(SIZE) if want[j] != got[j])
            raise SystemExit(f'run {i} ({"reset" if reset else "constructor"}): differs at +{k:#x}: {want.hex()} {got.hex()}')
    if min(seen) == 0:
        raise SystemExit(f'not every call reached: {seen}')
    print(f'PM inout record: {runs} runs identical ({seen[0]} constructors, {seen[1]} resets)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
