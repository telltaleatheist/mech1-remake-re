"""test_net_links.py on Pirate's Moon 1.0 (docs/pm-tests.md, S11): the steps' link edits (a node's 0x4d4000..0x4d4380,
a step's 0x4d4650..0x4d4720, the base's 0x5ac8f0 / 0x410b80 / 0x410bb0 and the answers 0x410b20 / 0x410b40 /
0x410b60), the node dialog's Update (0x4eae40 -> 0x4eade0 -> 0x4eac90 / 0x4eacd0) and the net file's folder
(0x4eb070), through PM's own vtables, against the pm build's port.

    cmake --build build --target pmprobe && MW3_PM_MOVED=1 python3 tests/differential/test_pm_net_links.py
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ.setdefault('MW3_PM_MOVED', '1')
import test_net_links as t  # noqa: E402
from test_pm_zar import load_pm_probe  # noqa: E402

t.EXE = 'pm'
t.NULL_STEP, t.NULL_STEP_VT = 0x627c08, 0x5df588
t.NODE_VT, t.SPEED_VT, t.EDGE_VT, t.BASE_VT, t.HOLD_VT = 0x5eef64, 0x5edcf0, 0x5ed6ec, 0x5eda08, 0x5df764
t.REGISTRY, t.OBJECT_VT, t.RET0 = 0x627c38, 0x5f1510, 0x51a4d0
t.SEND_MESSAGE, t.SPRINTF, t.STRDUP = 0x5de73c, 0x5de658, 0x5de678
t.UPDATE_DATA, t.OP_NEW, t.OP_DELETE = 0x5cea58, 0x5ce8c6, 0x5ce8c0
t.UPDATE, t.FOLDER, t.RESOLVE_DIR, t.AIV_MAKE, t.AIV_DROP = 0x4eae40, 0x4eb070, 0x4efcf0, 0x4f1810, 0x4f1880
t.AIV, t.SHARED = 0x62fc98, 0x62fc6c

if __name__ == '__main__':
    sys.exit(t.main(load_pm_probe()))
