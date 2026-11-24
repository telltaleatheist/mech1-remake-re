#!/usr/bin/env python3
"""FIXES 111: sums the balance soak's campaign files (mw1tests, MW1_BALANCE=n) into a report.

usage: balance_report.py <dir with campaign_*.txt>

Per tier and record band: contracts, win rate, 'Mechs lost per 10 contracts, pilots killed and missing, win nets as %
of pay, how often the House's floor paid; first-minute lancemate losses on MW3's worlds and PM's; the money curves;
the enemies met (chassis, loadouts); PM's items that reached the stores and those that never did. Numbers only: MW1's
text is never printed.
"""
import glob
import os
import statistics
import sys
from collections import Counter, defaultdict

FIELDS = ("seed n tier band objective mw3 story payment outcome timed_out mechs_lost killed missing downed "
          "downed_first_minute lance_out commander_down advance fee salvage repairs covered lost_value pool wrecks "
          "clan_wrecks enemies clan_enemies seconds").split()
OBJ = {0: "destroy", 1: "reach", 2: "attack", 3: "defend"}


def read(d):
    contracts, curves, chassis, loadouts, stored, offered, runs, stranded = [], {}, Counter(), Counter(), Counter(), set(), [], []
    no_way_out = []
    files = sorted(glob.glob(os.path.join(d, "campaign_*.txt")))
    for f in files:
        for line in open(f, errors="replace"):
            w = line.split()
            if not w:
                continue
            tag = w[0]
            if tag == "contract":
                vals = w[1:1 + len(FIELDS)]
                c = {k: (float(v) if k == "seconds" else int(v)) for k, v in zip(FIELDS, vals)}
                c["world"] = w[1 + len(FIELDS)] if len(w) > 1 + len(FIELDS) else ""
                contracts.append(c)
            elif tag == "curve":
                curves[int(w[1])] = [int(x) for x in w[2:]]
            elif tag == "chassis":
                chassis[" ".join(w[2:])] += int(w[1])
            elif tag == "loadout":
                loadouts[" ".join(w[2:])] += int(w[1])
            elif tag == "stored":
                stored[" ".join(w[1:])] += 1
            elif tag == "offered":
                offered.add(" ".join(w[1:]))
            elif tag == "run":
                runs.append(line.strip())
            elif tag == "stranded":  # FIXES 144: the way out, "sold" or "passage"
                stranded.append((os.path.basename(f), w[1], w[3] if len(w) > 3 else "?"))
            elif tag == "no-way-out":
                no_way_out.append((os.path.basename(f), w[1]))
    return files, contracts, curves, chassis, loadouts, stored, offered, runs, stranded, no_way_out


def net(c):
    return c["advance"] + c["fee"] + c["salvage"] - c["repairs"] + c["covered"]


def pct(a, b):
    return f"{100.0 * a / b:5.1f}%" if b else "   - "


def table(title, groups):
    print(f"\n{title}")
    print(f"  {'group':<16}{'n':>5}{'won':>8}{'lost':>8}{'withdrew':>9}{'mechs/10':>9}{'killed':>7}{'missing':>8}"
          f"{'win net/pay':>12}{'floor paid':>11}{'1st-min down':>13}")
    for key in sorted(groups):
        cs = groups[key]
        n = len(cs)
        won = [c for c in cs if c["outcome"] == 0]
        lost = sum(c["outcome"] == 1 for c in cs)
        wd = sum(c["outcome"] == 2 for c in cs)
        mechs = sum(c["mechs_lost"] for c in cs)
        ratios = [net(c) / c["payment"] for c in won if c["payment"] > 0]
        win_net = f"{100 * statistics.median(ratios):6.0f}%" if ratios else "     -"
        floor = sum(c["covered"] > 0 for c in cs)
        print(f"  {key:<16}{n:>5}{pct(len(won), n):>8}{pct(lost, n):>8}{pct(wd, n):>9}{10.0 * mechs / n:>9.1f}"
              f"{sum(c['killed'] for c in cs):>7}{sum(c['missing'] for c in cs):>8}{win_net:>12}{pct(floor, n):>11}"
              f"{sum(c['downed_first_minute'] for c in cs):>6} of {sum(c['downed'] for c in cs):<4}")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    files, cs, curves, chassis, loadouts, stored, offered, runs, stranded, no_way_out = read(sys.argv[1])
    print(f"{len(files)} campaigns, {len(cs)} contracts")
    for r in runs:
        print("  " + r)
    if not cs:
        return
    by_tier, by_band, by_tb, by_obj, by_world = (defaultdict(list) for _ in range(5))
    for c in cs:
        by_tier[f"tier {c['tier']}"].append(c)
        by_band[f"band {c['band']}"].append(c)
        by_tb[f"t{c['tier']} b{c['band']}"].append(c)
        by_obj[OBJ.get(c["objective"], "?") + (" story" if c["story"] else "")].append(c)
        by_world["MW3 worlds" if c["mw3"] else "PM worlds"].append(c)
    table("By tier", by_tier)
    table("By record band", by_band)
    table("By tier and band", by_tb)
    table("By objective", by_obj)
    table("MW3's worlds and PM's (106 c: lancemates downed in the first minute)", by_world)

    to = sum(c["timed_out"] for c in cs)
    cd = sum(c["commander_down"] for c in cs)
    print(f"\nTimed out (withdrawn at 480 s): {to} of {len(cs)}; the commander's 'Mech destroyed: {cd}")
    print(f"Lance fielded (pilots out): {Counter(c['lance_out'] for c in cs)}")

    print("\nMoney, per contract (C-bills, medians): pay, advance, fee, salvage, repairs, the floor, net; the wrecks' worth")
    for key in sorted(by_tier):
        g = by_tier[key]
        med = lambda f: int(statistics.median(f(c) for c in g))
        print(f"  {key}: pay {med(lambda c: c['payment']):,} adv {med(lambda c: c['advance']):,} fee {med(lambda c: c['fee']):,} "
              f"salvage {med(lambda c: c['salvage']):,} repairs {med(lambda c: c['repairs']):,} floor {med(lambda c: c['covered']):,} "
              f"net {med(net):,}; wrecks' worth {med(lambda c: c['pool']):,}")
    clan = [c for c in cs if c["clan_wrecks"]]
    if clan:
        per_is = [c["pool"] / c["wrecks"] for c in cs if c["wrecks"] and not c["clan_wrecks"]]
        per_clan = [c["pool"] / c["wrecks"] for c in clan]
        print(f"  contracts with Clan wrecks: {len(clan)}; worth a wreck (median): Clan-bearing {int(statistics.median(per_clan)):,}"
              f", IS only {int(statistics.median(per_is)) if per_is else 0:,}")

    print("\nThe money curves (funds after each contract)")
    for seed, cur in sorted(curves.items()):
        pts = cur[:: max(1, len(cur) // 10)]
        print(f"  campaign {seed}: " + " ".join(f"{v / 1e6:.1f}M" for v in pts) + (f" ... {cur[-1] / 1e6:.1f}M" if cur else ""))
    if stranded:
        print(f"  stranded, out by a sale or the working passage: {len(stranded)}: " +
              ", ".join(f"{f} after {n} ({way})" for f, n, way in stranded))
    if no_way_out:
        print(f"  STRANDED WITH NO WAY OUT: {len(no_way_out)}: " + ", ".join(f"{f} after {n}" for f, n in no_way_out))

    print(f"\nEnemies met: {sum(chassis.values())} over {len(chassis)} chassis")
    for name, n in chassis.most_common():
        ls = sorted((k.split('/')[-1], v) for k, v in loadouts.items() if k.rsplit('/', 1)[0] == name)
        print(f"  {name:<14}{n:>5}  loadouts " + " ".join(f"{l}:{v}" for l, v in ls))

    print(f"\nPM's items: {len(offered)} the lab offers; {len(set(stored) & offered)} reached a store")
    never = sorted(offered - set(stored))
    print(f"  never reached a store ({len(never)}):")
    for s in never:
        print(f"    {s}")
    print("  reached (campaigns that saw it):")
    for s, n in sorted(stored.items(), key=lambda x: (-x[1], x[0])):
        print(f"    {n:>3}  {s}")


if __name__ == "__main__":
    main()
