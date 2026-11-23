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
    run = {"mode": None, "seed": None, "reserve": None, "bought": {}, "rungs": {}, "at": {}, "blocked": None, "ticks": []}
    for line in open(path, errors="replace"):
        if line.startswith("    BLOCK: ") and run["blocked"] is None:  # a run from before the SOAK blocked line
            run["blocked"] = (-1, -1, line.split("BLOCK: ", 1)[1].strip())
        if not line.startswith("SOAK: "):
            continue
        w = line.split()[1:]
        if w[0] == "mode":
            run["mode"], run["seed"], run["reserve"] = w[1], int(w[3]), int(w[5])
            run["rule"] = w[7] if len(w) > 7 else "earlier"
        elif w[0] == "bought":
            f = fields(w, 3)
            run["bought"][(w[1], int(w[2]))] = f
        elif w[0] == "rung":
            run["rungs"][int(w[1])] = fields(w, 2)
        elif w[0] == "at":
            run["at"][w[1]] = fields(w, 2)
        elif w[0] == "tick":
            run["ticks"].append({k: int(v) for k, v in fields(w, 1).items()})
        elif w[0] == "blocked":
            run["blocked"] = (int(w[2]), int(w[4]), " ".join(w[6:]))
    return run


# The price evaluator: a single-ladder player replayed on a measured run's own income (its SOAK tick lines) under other
# price curves, with the game's own ladder arithmetic (mw1_upgrades.cpp ladder_price). Curves as (base, ratio permille);
# the first of each is the current one. The baseline's income was tried first and breaks (its 'Mech buys for the early
# rungs unaffordable after any curve's first purchases), so only the runs' own income is used.
# The first of each is the game's (mw1_upgrades.h, settled 2026-11-22); the rest are the curves weighed against it,
# the earliest placeholders (0.5M x 1.25, 0.1M x 1.05, 1M x 1.5) among them.
CURVES = {"sinks": [(3000000, 1150), (500000, 1250), (2000000, 1150), (2000000, 1200), (3000000, 1200)],
          "armour": [(500000, 1050), (100000, 1050), (300000, 1050), (1000000, 1050), (500000, 1030)],
          "steps": [(5000000, 1400), (1000000, 1500), (3000000, 1500), (5000000, 1500), (8000000, 1500)]}
CURRENT = {k: v[0] for k, v in CURVES.items()}
EVAL_MILESTONES = {"sinks": (1, 3, 5, 10, 20, 30), "armour": (10, 25, 50, 100), "steps": (1, 3, 5, 10)}
PRICE_AT = (1, 10, 20, 30)  # the single prices shown: the 1st, 10th, 20th and 30th item's
PRICE_MOST = 1 << 40


def ladder_price(base, ratio, bought):
    p = base
    for _ in range(bought):
        if p >= PRICE_MOST:
            break
        p = p * ratio // 1000
    return PRICE_MOST if p >= PRICE_MOST else p // 1000 * 1000


def replay(ticks, base, ratio, reserve, most):
    """each item's tick for a player buying one ladder on this income, the first purchase look's count (the burst),
    and the first tick where the stream's own buys ('Mechs, repairs) would have taken its wallet below nothing"""
    own, n, got, broken, burst = 0, 0, {}, None, 0
    for t in ticks:
        wallet = t["funds"] + t["spent"] + t["traded"] - own  # the stream's money less this player's purchases
        if wallet < 0 and broken is None:
            broken = t
        if not t["may"]:
            continue
        before = n
        while n < most:
            price = ladder_price(base, ratio, n)
            if wallet - price < reserve:
                break
            wallet, own, n = wallet - price, own + price, n + 1
            got[n] = t
        if before == 0 and n > 0:
            burst = n
    return got, burst, broken


def curve_name(base, ratio):
    return f"{base / 1e6:g}M x {ratio / 1000:g}^n"


def short_money(v):
    return f"{v / 1e6:.2f}M" if v < 1e9 else f"{v / 1e9:.2f}B"


def evaluate(by):
    print("\n# Price curves on each measured run's own income (the evaluator)")
    print("  A single-ladder player with the run's reserve and saving rule, buying at each look the run may spend; its")
    print("  income and pace are the measured run's, so a curve's own effect on the pace is not in it. BROKEN at N: from")
    print("  contract N this curve's extra spending would have cost the run's own 'Mech buys, so later cells are approximate.")
    print("  Cells: contract (game day); - = not by twice the ending. Burst: how many the first spending look buys (rung 1).")
    for ladder, curves in CURVES.items():
        ms = EVAL_MILESTONES[ladder]
        for measured in sorted(by.get(ladder, []), key=lambda r: r["seed"]):
            seed = measured["seed"]
            if not measured["ticks"]:
                print(f"\n  {ladder}, seed {seed}: UNVALIDATED, no tick lines in the measured run (no evaluator)")
                continue
            print(f"\n  {ladder}, seed {seed} (one seed):")
            head = "  ".join(f"{n:>13}" for n in ms)
            prices = "  ".join(f"{'#' + str(n):>7}" for n in PRICE_AT)
            print(f"    {'curve':<22}{head}  burst  {prices}")
            for base, ratio in curves:
                got, burst, broken = replay(measured["ticks"], base, ratio, measured["reserve"], max(ms))
                cells = "  ".join(f"{(str(got[n]['contracts']) + ' (' + str(got[n]['day']) + ')') if n in got else '-':>13}" for n in ms)
                each = "  ".join(f"{short_money(ladder_price(base, ratio, n - 1)):>7}" for n in PRICE_AT)
                mark = " (current)" if (base, ratio) == CURRENT[ladder] else ""
                warn = f"  BROKEN at {broken['contracts']}" if broken else ""
                print(f"    {curve_name(base, ratio) + mark:<22}{cells}  {burst:>5}  {each}{warn}")
            # Validation: the current curve on the run's own income must give the run's own milestones.
            got, _, _ = replay(measured["ticks"], *CURRENT[ladder], measured["reserve"], max(ms))
            same = all((n in got) == ((ladder, n) in measured["bought"]) and
                       (n not in got or got[n]["contracts"] == int(measured["bought"][(ladder, n)]["contracts"])) for n in ms)
            print(f"    validation: the current curve's replay {'matches the measured run exactly' if same else 'DIFFERS from the measured run (a replay fault, or steps switched weapon type)'}")


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
    rule = ("saves while building the lance and through the Dark Wing" if run.get("rule") == "saves-through-dark-wing"
            else "earlier rule: saves while building, may spend once the Dark Wing's lance is ready")
    print(f"\n## mode {run['mode']}, seed {run['seed']}, reserve {money(run['reserve'])} ({rule})")
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

    evaluate(by)

    print("\n# Story delay for a player who spends to the reserve: the rungs against the baseline, seed for seed")
    print("  (the plan's 10% check; it measures a greedy player's choice, not a defect)")
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
