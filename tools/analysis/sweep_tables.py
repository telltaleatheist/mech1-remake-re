"""The S20 sweep (docs/port-audit.md, its head): what the audit's function list (func2file plus
port_audit_extra.tsv) does not hold that the exe points at.

    python3 -I tools/analysis/sweep_tables.py [--game mw3|pm]

Prints, below the C runtime (the audit's own boundary):
- created: functions Ghidra was made to decompile (analysis-output/decomp, CreateAndDecompile) that func2file lacks;
- msgmap: every AFX message map (a GetBaseMessageMap pointer followed by its entries, 24 bytes each: message, code,
  id, last id, signature, handler) and each handler the list lacks;
- behind: a function a listed thunk without a decomp jumps to (often a virtual-base destructor) that is not listed;
- data: every dword in a non-code section holding a code address that is no known function's start: a switch's
  table (only where code jumps through it, jmp [4*reg + base]; this exe keeps them in .text), an unknown start (after
  a ret, jmp, int3 or nop: often a vtable slot into a function Ghidra made too long), or inside an instruction run.
No dependencies.
"""
import argparse
import bisect
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
A = ROOT / 'analysis-output'
GAMES = {
    'mw3': dict(f2f=A / 'func2file.json', decomp=A / 'decomp', exe=ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe',
                asm=A / 'mech3.asm'),
    'pm': dict(f2f=A / 'pm-func2file.json', decomp=A / 'pm-decomp', exe=ROOT / 'game' / 'pm' / 'Mech3.exe',
               asm=A / 'pm.asm'),
}


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    out = []
    for i in range(n):
        o = pe + 24 + opt + 40 * i
        vs, va, rs, ro = struct.unpack_from('<IIII', data, o + 8)
        flags = struct.unpack_from('<I', data, o + 36)[0]
        name = data[o:o + 8].rstrip(b'\0').decode('latin-1')
        out.append(dict(name=name, va=base + va, size=max(vs, rs), raw=ro, rsize=rs, code=bool(flags & 0x20)))
    return out


def crt_start(f2f):
    """The audit's boundary: the first function after the last game source file (si_script.cpp)."""
    last = max(int(a, 16) for a, f in f2f.items() if 'si_script.cpp' in f and not f.startswith('(after'))
    after = sorted(int(a, 16) for a in f2f if int(a, 16) > last and 'si_script.cpp)' in f2f[a])
    return after[0] if after else last + 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', default='mw3', choices=GAMES)
    o = ap.parse_args()
    g = GAMES[o.game]
    f2f = json.load(open(g['f2f']))
    known = {int(a, 16) for a in f2f}
    for line in open(ROOT / 'tools' / 'analysis' / 'port_audit_extra.tsv'):
        p = line.rstrip('\n').split('\t')
        if len(p) > 1 and p[0] == o.game:
            known.add(int(p[1], 16))
    crt = crt_start(f2f)
    starts = sorted(known)
    data = g['exe'].read_bytes()
    secs = sections(data)
    code = [s for s in secs if s['code']]
    in_code = lambda v: any(s['va'] <= v < s['va'] + s['size'] for s in code)

    def word(va):
        for s in secs:
            if s['va'] <= va < s['va'] + s['rsize']:
                return struct.unpack_from('<I', data, s['raw'] + va - s['va'])[0]
        return None

    def owner(v):
        i = bisect.bisect_right(starts, v) - 1
        return starts[i] if i >= 0 else None

    ins = re.compile(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*([a-z]\S*)\s*([^<]*)')
    prev_mn, last, text, switch_bases = {}, None, {}, set()
    for line in open(g['asm'], errors='replace'):
        m = ins.match(line)
        if m:
            a = int(m.group(1), 16)
            prev_mn[a] = last
            last = m.group(2)
            text[a] = f'{m.group(2)} {m.group(3).strip()}'
            t = re.search(r'\[4\*[a-z]+ \+ 0x([0-9a-f]+)\]', m.group(3)) if m.group(2) == 'jmp' else None
            if t:
                switch_bases.add(int(t.group(1), 16))
    start_like = lambda v: v in prev_mn and prev_mn[v] in (None, 'ret', 'jmp', 'int3', 'nop')
    print(f'{o.game}: {len(known)} known functions, the C runtime from 0x{crt:08x}')
    created = sorted(int(x[:-2], 16) for x in os.listdir(g['decomp']) if x.startswith('0x') and x.endswith('.c'))
    missing = [a for a in created if a not in known and a < crt]
    print(f'created, not listed ({len(missing)}): {" ".join(f"0x{a:08x}" for a in missing)}')
    # message maps
    maps, handlers = [], []
    for s in secs:
        if s['code']:
            continue
        for off in range(0, s['rsize'] - 32, 4):
            va = s['va'] + off
            base_fn, entries = struct.unpack_from('<II', data, s['raw'] + off)
            if entries != va + 8 or not in_code(base_fn):
                continue
            k, ents = 0, []
            while True:
                e = struct.unpack_from('<6I', data, s['raw'] + off + 8 + 24 * k)
                if e == (0, 0, 0, 0, 0, 0) or not in_code(e[5]) or e[4] > 0x80 or e[0] > 0xffff:
                    break
                ents.append(e)
                k += 1
            if ents:
                maps.append((va, ents))
                handlers += [(va, e) for e in ents]
    unknown_h = [(m, e) for m, e in handlers if e[5] not in known and e[5] < crt]
    print(f'msgmap: {len(maps)} maps, {len(handlers)} entries, {len(unknown_h)} handlers not listed')
    for m, e in unknown_h:
        print(f'  map 0x{m:08x} msg 0x{e[0]:x} code 0x{e[1]:x} id {e[2]}..{e[3]} sig 0x{e[4]:x} -> 0x{e[5]:08x}'
              f' (inside 0x{owner(e[5]) or 0:08x})')
    # every code address in data
    switch_slots = set()
    for b in switch_bases:
        k = b
        while k < b + 4 * 512 and (x := word(k)) is not None and in_code(x) and owner(x) == owner(word(b)):
            switch_slots.add(k)
            k += 4
    inside, unknown, mid = {}, {}, {}
    for s in secs:
        if s['name'] not in ('.rdata', '.data'):
            continue
        for off in range(0, s['rsize'] - 3, 4):
            v = struct.unpack_from('<I', data, s['raw'] + off)[0]
            if v in known or not in_code(v) or v >= crt:
                continue
            f = owner(v)
            va = s['va'] + off
            # a switch's table only where code jumps through it (jmp [4*reg + base]); a run of pointers into one
            # function is not enough (a vtable's slots into a function Ghidra made too long: hidden functions)
            if va in switch_slots:
                inside.setdefault(f, []).append(va)
            elif start_like(v):
                unknown.setdefault(v, []).append(va)
            else:
                mid.setdefault(v, []).append(va)
    # a body behind a listed thunk (no decomp of its own): reached only through the thunk
    behind = {}
    for a in sorted(known):
        if a >= crt or (g['decomp'] / f'0x{a:x}.c').exists():
            continue
        t = thunk_target(text, a)
        if t is not None and t not in known and t < crt and in_code(t):
            behind[t] = a
    print(f'behind a thunk, not listed ({len(behind)}): '
          + ' '.join(f'0x{t:08x} (from 0x{a:08x})' for t, a in sorted(behind.items())))
    print(f'data: {sum(len(x) for x in inside.values())} jump-table entries in {len(inside)} functions; '
          f'{len(mid)} addresses inside an instruction run (no function start: not code pointers, or labels); '
          f'{len(unknown)} function starts not listed:')
    for v in sorted(unknown):
        print(f'  0x{v:08x} (after 0x{owner(v) or 0:08x}) from {" ".join(f"0x{x:08x}" for x in unknown[v][:4])}')


def thunk_target(text, a):
    """Where a thunk (jmp X, or a virtual-base adjustor: sub ecx, [ecx - 4] (sub ecx, n) jmp X) at `a` goes."""
    t = sorted(x for x in text if a <= x < a + 16)
    for x in t[:3]:
        m = re.match(r'^jmp 0x([0-9a-f]+)$', text[x])
        if m:
            return int(m.group(1), 16)
        if not re.match(r'^sub ecx, (dword ptr \[ecx - 0x4\]|0x[0-9a-f]+)$', text[x]):
            return None
    return None


if __name__ == '__main__':
    main()
