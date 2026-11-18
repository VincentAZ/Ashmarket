#!/usr/bin/env python3
"""
Adds up whole-ladder balance runs (Tools/run_gut_balance.sh N Ladder, usually several at once) into one table:
for each fight on the ladder, how often it was fought, won and lost, and how long it took.

  python3 Tools/gut_ladder_table.py Saved/Logs/gut_balance_Ladder_*.log

"Fought" counts every attempt, so 24 wins out of 70 fought means the stand-in needed about three tries.
"""

import re
import statistics
import sys

LINE = re.compile(r"BALANCE Ladder arc (\d+): (\w+) fight (\d+) \((.*?)\) (won|lost, dead|lost, spared|lost, thrown)\S* in ([0-9.]+) s")

order = []
fights = {}
runs = 0

for path in sys.argv[1:]:
    arcs = set()

    for line in open(path, errors="replace"):
        found = LINE.search(line)

        if not found:
            continue

        arc, rank, number, name, how, seconds = found.groups()
        key = "{} fight {} ({})".format(rank, number, name)

        if key not in fights:
            fights[key] = {"won": 0, "dead": 0, "spared": 0, "seconds": []}
            order.append(key)

        fights[key]["won" if how == "won" else ("spared" if "spared" in how else "dead")] += 1
        fights[key]["seconds"].append(float(seconds))
        arcs.add(arc)

    runs += len(arcs)

print("RUNS {} (started; a run that was stopped early counts)".format(runs))

for key in order:
    one = fights[key]
    fought = one["won"] + one["dead"] + one["spared"]

    print("{:46s} fought {:4d}  won {:4d} ({:3.0f}%)  spared {:3d}  middle {:4.0f} s".format(
        key, fought, one["won"], 100.0 * one["won"] / fought, one["spared"], statistics.median(one["seconds"])))
