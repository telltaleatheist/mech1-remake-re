"""Rewrites a function's [esp + N] operands as offsets in its frame, for reading code the decompiler garbles
(thiscall arguments lost, locals aliased).

    python3 tools/analysis/esp_frames.py <start> [end] --asm=<objdump file, e.g. analysis-output/pm.asm>

The frame base F is esp after the prologue's `sub esp` and its register pushes; [esp + N] at any later point is
shown as [F+M], M = N - (bytes pushed since), with that depth as dN. Pushes count 4; `add esp, N` after a call takes
its arguments back; a call's own pop is the callee's `ret N` (cdecl 0), read from the same file. A jump carries its
depth to its target, learnt over repeated passes so backward jumps count; code no known jump reaches yet shows d?.
Arguments past the return address show as argK (the first after `this` is arg0). Written for PM's triangle
stripper (docs/pm-tests.md), whose thiscall helpers the decompiler shows without their `this`.
"""
import re
import sys


def load(path):
    lines = []
    for raw in open(path):
        m = re.match(r'\s*([0-9a-f]+):\s*(?:[0-9a-f]{2} )*\s*(.*)', raw)
        if m and m.group(2).strip():
            lines.append((int(m.group(1), 16), re.sub(r'\s*<[^>]*>', '', m.group(2)).strip()))
    return lines


def ret_pops(lines):
    """Each function's pop, by the address after the previous ret (good enough for MSVC's packed code)."""
    pops, start = {}, None
    for a, ins in lines:
        if start is None:
            start = a
        m = re.match(r'ret\s*(0x[0-9a-f]+|\d+)?$', ins)
        if m:
            pops.setdefault(start, int(m.group(1), 0) if m.group(1) else 0)
            start = None
        elif ins.startswith(('nop', 'int3')):
            start = None
    return pops


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    asm = next(a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('--asm='))
    start = int(args[0], 16)
    end = int(args[1], 16) if len(args) > 1 else None
    lines = load(asm)
    pops = ret_pops(lines)
    body = [(a, i) for a, i in lines if a >= start and (end is None or a < end)]
    label_depth = {}
    for _ in range(8):  # learn every label's depth, backward jumps included, until nothing changes
        known = dict(label_depth)
        walk(body, pops, label_depth, False)
        if known == label_depth:
            break
    walk(body, pops, label_depth, True)


def walk(body, pops, label_depth, final):
    depth, frame, base_set, unknown = 0, 0, False, False
    prologue_pushes = 0
    for a, ins in body:
        if depth is None or unknown:
            if a in label_depth:
                depth, unknown = label_depth[a], False
            else:  # not reached by a jump known so far: shown from 0, passed on to nothing
                depth, unknown = 0 if depth is None else depth, True
        shown = ins
        def rew(m):
            n = int(m.group(1), 16) if m.group(1) else 0
            off = n - depth
            if off >= frame + 4 + prologue_pushes * 4:
                k = (off - frame - prologue_pushes * 4 - 4) // 4
                return f'[arg{k}]' if (off - frame - prologue_pushes * 4 - 4) % 4 == 0 else f'[F+{off:#x}]'
            return f'[F+{off:#x}]'
        shown = re.sub(r'\[esp(?: \+ (0x[0-9a-f]+))?\]', rew, ins)
        if final:
            print(f'{a:06x} d{depth if not unknown else "?":<3} {shown}')
        # the depth after this instruction
        if not base_set:
            m = re.match(r'sub\s+esp, (0x[0-9a-f]+|\d+)$', ins)
            if m:
                frame = int(m.group(1), 0)
                continue
            if ins.startswith('push'):
                prologue_pushes += 1
                continue
            if not frame and not ins.startswith('mov'):
                base_set = True  # no frame: the base is where the pushes end
            elif frame:
                base_set = True
            else:
                continue
        if ins.startswith('push'):
            depth += 4
        elif ins.startswith('pop'):
            depth -= 4
        elif re.match(r'add\s+esp, ', ins):
            depth -= int(ins.split(',')[1], 0)
        elif re.match(r'sub\s+esp, ', ins):
            depth += int(ins.split(',')[1], 0)
        elif ins.startswith('call'):
            m = re.match(r'call\s+(0x[0-9a-f]+)$', ins)
            depth -= pops.get(int(m.group(1), 16), 0) if m else 0
        m = re.match(r'j\w+\s+(0x[0-9a-f]+)$', ins)
        if m and not unknown:
            label_depth.setdefault(int(m.group(1), 16), depth)
            if ins.startswith('jmp'):
                depth, unknown = None, False
        if ins.startswith('ret'):
            depth, unknown = None, False


if __name__ == '__main__':
    main()
