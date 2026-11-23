#!/usr/bin/env python3
"""1.1.0 (FIXES 118, 122): the spending soak's report: how hard the upgrades' prices make them to get.

usage: spend_soak_report.py <dir with soak_*.txt>

Each file is one playthrough run's output (mw1tests, MW1_PLAYTHROUGH=1 MW1_SOAK_SPEND=<mode> MW1_PLAYTHROUGH_SEED=n);
only its SOAK lines are read. Prints:
- one table per run: when each milestone was bought (contract, fight, game day, date), the funds and timing at each
  story rung won against the baseline's (MW1_SOAK_SPEND=none, the same seed), and what it owned at the ending and at
  twice the ending's game days;
- one line per mode: the median contract of each milestone across its seeds;
- the rungs' timing against the baseline (the plan's 10% check).
Numbers only: MW1's text is never in the input.
"""
import glob
import os
import statistics
import sys

MILESTONES = {"sinks": (1, 5, 10, 20), "armour": (10, 25, 50, 100), "steps": (1, 3, 5, 10)}
MODES = ("none", "sinks", "armour", "steps", "mixed", "trader")
LADDERS = {"none": (), "sinks": ("sinks",), "armour": ("armour",), "steps": ("steps",),
           "mixed": ("sinks", "armour", "steps"), "trader": ("sinks", "armour", "steps")}


def fields(words, start):
    """key value pairs from words[start:]"""
    return {words[i]: words[i + 1] for i in range(start, len(words) - 1, 2)}


def read(path):
    run = {"mode": None, "seed": None, "reserve": None, "bought": {}, "rungs": {}, "at": {}, "blocked": None}
    for line in open(path, errors="replace"):
        if line.startswith("    BLOCK: ") and run["blocked"] is None:  # a run from before the SOAK blocked line
            run["blocked"] = (-1, -1, line.split("BLOCK: ", 1)[1].strip())
        if not line.startswith("SOAK: "):
            continue
        w = line.split()[1:]
        if w[0] == "mode":
            run["mode"], run["seed"], run["reserve"] = w[1], int(w[3]), int(w[5])
        elif w[0] == "bought":
            f = fields(w, 3)
            run["bought"][(w[1], int(w[2]))] = f
        elif w[0] == "rung":
            run["rungs"][int(w[1])] = fields(w, 2)
        elif w[0] == "at":
            run["at"][w[1]] = fields(w, 2)
        elif w[0] == "blocked":
            run["blocked"] = (int(w[2]), int(w[4]), " ".join(w[6:]))
    return run


def money(v):
    return f"{int(v):,}"


def pct(a, b):
    return f"{(a - b) * 100.0 / b:+.0f}%" if b else "n/a"


def phase(run, f):
    """when a purchase came: before rung 1, in the story (rung 1 to the ending), in the tail (the ending to twice it)"""
    if not f:
        return "not by 2x"
    fight = int(f["fights"])
    r1 = run["rungs"].get(1)
    end = run["at"].get("end")
    if not r1 or fight < int(r1["fights"]):
        return "before rung 1"
    if not end or fight <= int(end["fights"]):
        return "story"
    return "2x tail"


def table(run, base):
    print(f"\n## mode {run['mode']}, seed {run['seed']}, reserve {money(run['reserve'])}")
    end, twice = run["at"].get("end"), run["at"].get("twice")
    if not end:
        print("  NOT FINISHED: no ending reached" + (f" ({run['blocked'][2]})" if run["blocked"] else ""))
    ladders = LADDERS.get(run["mode"], ())
    if ladders:
        print(f"  {'milestone':<12} {'contract':>8} {'fight':>6} {'day':>6}  {'date':<10} {'funds after the look':>21}  when")
        for ladder in ladders:
            for n in MILESTONES[ladder]:
                f = run["bought"].get((ladder, n))
                name = f"{ladder} {n}"
                if f:
                    print(f"  {name:<12} {f['contracts']:>8} {f['fights']:>6} {f['day']:>6}  {f['date']:<10} {money(f['funds']):>21}  {phase(run, f)}")
                else:
                    print(f"  {name:<12} {'not by 2x':>8}")
    print(f"  {'rung':<6} {'fight':>6} {'contract':>8} {'day':>6} {'funds':>14}   against the baseline (fight, day)")
    for r in sorted(run["rungs"]):
        f = run["rungs"][r]
        vs = ""
        if base and r in base["rungs"] and run is not base:
            b = base["rungs"][r]
            vs = f"{pct(int(f['fights']), int(b['fights'])):>6} {pct(int(f['day']), int(b['day'])):>6}"
        print(f"  {r:<6} {f['fights']:>6} {f['contracts']:>8} {f['day']:>6} {money(f['funds']):>14}   {vs}")
    for name, a in (("ending", end), ("twice", twice)):
        if a:
            trade = ""
            if run["mode"] == "trader":
                trade = (f", traded {money(a['trade_spent'])} in, {money(a['trade_sold'])} out, profit {money(a.get('trade_profit', 0))}, "
                         f"{a.get('sell_visits', '?')} sell visits")
            print(f"  at {name:<7} day {a['day']:>5}, contract {a['contracts']:>4}, fight {a['fights']:>4}: funds {money(a['funds'])}, "
                  f"{a['mechs']} 'Mechs, {a['lancemates']} lancemates, sinks {a['sinks']}, armour {a['armour']}, steps {a['steps']}{trade}")


def median_contract(runs, ladder, n):
    """the median contract across seeds; a seed that never got there counts as later than any"""
    got = sorted(int(r["bought"][(ladder, n)]["contracts"]) if (ladder, n) in r["bought"] else float("inf") for r in runs)
    m = got[(len(got) - 1) // 2] if len(got) % 2 else (got[len(got) // 2 - 1] + got[len(got) // 2]) / 2
    reached = sum(1 for g in got if g != float("inf"))
    return ("not by 2x" if m == float("inf") else f"{m:g}") + ("" if reached == len(got) else f" ({reached}/{len(got)})")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    runs = [read(f) for f in sorted(glob.glob(os.path.join(sys.argv[1], "soak_*.txt")))]
    runs = [r for r in runs if r["mode"]]
    by = {}
    for r in runs:
        by.setdefault(r["mode"], []).append(r)
    base = {r["seed"]: r for r in by.get("none", [])}
    print("# The spending soak: how hard the upgrades are to get")
    for mode in MODES:
        for r in sorted(by.get(mode, []), key=lambda r: r["seed"]):
            table(r, base.get(r["seed"]))

    strands = [read(f) for f in sorted(glob.glob(os.path.join(sys.argv[1], "strand_*.txt")))]
    strands += [r for r in runs if r["blocked"]]
    if strands:
        print("\n# WARNING: a player spending to the reserve before the Dark Wing can strand")
        for r in strands:
            got = sorted(r["bought"], key=lambda k: int(r["bought"][k]["fights"]))
            last = r["bought"][got[-1]] if got else None
            reached = "; ".join(f"{ladder} {n} at contract {r['bought'][(ladder, n)]['contracts']}"
                                for ladder in MILESTONES for n in MILESTONES[ladder] if (ladder, n) in r["bought"])
            why = r["blocked"][2] if r["blocked"] else "not finished"
            after = f" after contract {last['contracts']}" if last else ""
            print(f"  {r['mode']}, seed {r['seed']}: {why}{after}, the Dark Wing never won; "
                  f"rungs won {sorted(r['rungs'])}; milestones by then: {reached or 'none'}")

    print("\n# Median contract of each milestone, by mode (seeds that never got there in brackets: reached/seeds)")
    for mode in MODES:
        rs = by.get(mode, [])
        if not rs or not LADDERS[mode]:
            continue
        parts = []
        for ladder in LADDERS[mode]:
            parts.append(ladder + " " + ", ".join(f"{n}: {median_contract(rs, ladder, n)}" for n in MILESTONES[ladder]))
        print(f"  {mode:<7} ({len(rs)} seeds)  " + " | ".join(parts))

    print("\n# When each milestone came, by mode (per seed: before rung 1, story = rung 1 to the ending, 2x tail = the ending to twice its days)")
    for mode in MODES:
        rs = sorted(by.get(mode, []), key=lambda r: r["seed"])
        if not rs or not LADDERS[mode]:
            continue
        print(f"  {mode} (seeds {', '.join(str(r['seed']) for r in rs)}):")
        for ladder in LADDERS[mode]:
            print("    " + "; ".join(f"{ladder} {n}: " + "/".join(phase(r, r["bought"].get((ladder, n))) for r in rs) for n in MILESTONES[ladder]))

    if by.get("trader"):
        print("\n# The trader's trade (no detours: it sells only where the story and the contracts take it)")
        for r in sorted(by["trader"], key=lambda r: r["seed"]):
            a = r["at"].get("twice") or r["at"].get("end") or {}
            visits = int(a.get("sell_visits", 0))
            print(f"  seed {r['seed']}: profit {money(a.get('trade_profit', 0))} from {money(a.get('trade_sold', 0))} sold, "
                  f"{money(a.get('trade_spent', 0))} bought, {visits} sell visits to tier 2-3 worlds")
            if visits < 5:
                print("    UNDERSTATES THE LOOPHOLE: it barely reached a world to sell at; a player who detours to sell would make far more")

    print("\n# The rungs against the baseline, seed for seed (the plan's check: within about 10%)")
    for mode in MODES[1:]:
        worst_f, worst_d, rows = [], [], 0
        for r in by.get(mode, []):
            b = base.get(r["seed"])
            if not b:
                continue
            for rung, f in r["rungs"].items():
                if rung in b["rungs"]:
                    bf, bd = int(b["rungs"][rung]["fights"]), int(b["rungs"][rung]["day"])
                    if bf:
                        worst_f.append((int(f["fights"]) - bf) * 100.0 / bf)
                    if bd:
                        worst_d.append((int(f["day"]) - bd) * 100.0 / bd)
                    rows += 1
        if not rows:
            continue
        late = max(worst_d) if worst_d else 0
        print(f"  {mode:<7} {rows} rungs: days median {statistics.median(worst_d):+.0f}%, latest {late:+.0f}%; "
              f"fights median {statistics.median(worst_f):+.0f}%, latest {max(worst_f):+.0f}%  "
              f"{'WITHIN 10%' if late <= 10 else 'PAST 10%'}")


if __name__ == "__main__":
    main()
