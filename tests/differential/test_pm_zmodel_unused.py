"""test_zmodel_unused.py on Pirate's Moon 1.0 (docs/pm-tests.md, S19): PM's twins of the unused zModel helpers, all
cdecl with MW3's register arguments first on the stack: 0x55a2f0 (planar), 0x559db0 (concave), 0x55ad60 (a light
added; its realloc PM's own 0x596520), 0x540a90 / 0x540ca0 (to the screen), 0x541830 / 0x5418e0 (the moves), on
PM's globals, against the pm build's port.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_zmodel_unused.py [runs]
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_PM_MOVED', '1')
import test_zmodel_unused as t  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

t.EXE, t.CDECL = 'pm', True
t.FN = {'planar': 0x55a2f0, 'concave': 0x559db0, 'light': 0x55ad60, 'point': 0x540a90, 'points': 0x540ca0,
        'below': 0x541830, 'two': 0x5418e0}
t.VIEW, t.FX, t.FY, t.CX, t.CY = 0x8d43c0, 0x8d439c, 0x8d43a0, 0x8d4388, 0x8d438c
t.NEAR, t.CLIP_RIGHT, t.POINT_RECT = 0x8d4b14, 0x8d4b24, 0x8d4b2c
t.SLOTS, t.TOP = 0x66f4d0, 0x611220
t.PRINTF, t.MALLOC, t.REALLOC_FN, t.FTOL_SLOT = 0x5de5f4, 0x5de664, 0x596520, 0x5de644

if __name__ == '__main__':
    t.main(load_pm_probe())
