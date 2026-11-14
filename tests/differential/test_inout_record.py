"""The vehicle's in/out record against the original x86 (docs/ai.md section 11): its constructor 0x4c0ed0 and
reset 0x4c0f70 (mech3::ai::InOutRecord), which the pull 0x4c3000 fills and nothing reads.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_inout_record.py [runs]

Each run: 0x3c random bytes and a frame counter; the constructor or the reset on them, the 0x3c bytes after
compared (the constructor's +0x24..+0x2f left out: the exe leaves them, the port zeroes them).
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

FRAME, ZERO = 0x72f338, 0x63d870


def main():
    probe = load_probe()
    probe.probe_inout_record.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p]
    cpu = Original()
    rec = cpu.alloc(0x40)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    r = random.Random(0x4c0ed0)
    for i in range(runs):
        reset = r.randrange(2)
        data = bytes(r.randrange(256) for _ in range(0x3c))
        frame = r.randrange(1 << 32)
        cpu.write(rec, data)
        cpu.u32(FRAME, frame)
        cpu.write(ZERO, bytes(12))
        cpu.call(0x4c0f70 if reset else 0x4c0ed0, ecx=rec)
        want = bytearray(cpu.read(rec, 0x3c))
        out = ctypes.create_string_buffer(0x3c)
        probe.probe_inout_record(reset, data, frame, out)
        got = bytearray(out.raw)
        if not reset:
            want[0x24:0x30] = got[0x24:0x30] = bytes(12)
        if want != got:
            k = next(j for j in range(0x3c) if want[j] != got[j])
            raise SystemExit(f'run {i} ({"reset" if reset else "constructor"}): differs at +{k:#x}: {want.hex()} {got.hex()}')
    print(f'inout record: {runs} runs identical')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
