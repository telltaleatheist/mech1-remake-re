"""test_clipfrustum.py on Pirate's Moon 1.0 (docs/pm-tests.md, S16-S18): PM's twins of the unused frustum clippers,
0x54eb60 (positions; hidden after 0x54d1c0's end like MW3's), 0x54aea0, 0x54be20 and 0x54d1c0, cdecl with every
argument on the stack (PM's build), its clip struct at 0x8d4b08 and its cotangents at 0x8d43a4 (inverses 0x66f4b8),
against the pm build's port.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_clipfrustum.py [runs]
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_PM_MOVED', '1')
import test_clipfrustum as t  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

t.EXE, t.CDECL = 'pm', True
t.CLIP, t.COT, t.INV = 0x8d4b08, 0x8d43a4, 0x66f4b8
t.FUNCS = [(0x54eb60, ''), (0x54aea0, 'tu'), (0x54be20, 'tu'), (0x54d1c0, 'tuc')]

if __name__ == '__main__':
    sys.exit(t.main(load_pm_probe()))
