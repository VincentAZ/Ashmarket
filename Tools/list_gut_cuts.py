"""
Reads what Ashmarket.Fit.Cuts measured (Saved/cuts_all.txt, one line a finding) and says it by body:
for each part of each body, in how many of the ways it was stood something it wears was under its skin, how deep at
worst and over how many points. Only what is at least --deep centimetres and --points points is listed.

    python3 Tools/list_gut_cuts.py [--deep 2.0] [--points 40] [file]
"""

import collections
import sys

args = sys.argv[1:]
deep = 2.0
points = 40
path = "Saved/cuts_all.txt"

while args:
    arg = args.pop(0)

    if arg == "--deep":
        deep = float(args.pop(0))
    elif arg == "--points":
        points = int(args.pop(0))
    else:
        path = arg

found = collections.defaultdict(lambda: collections.defaultdict(list))
stood = collections.defaultdict(set)
wears = collections.defaultdict(list)

for line in open(path):
    if " wears: " in line:
        who, what = line.strip().split(" wears: ", 1)
        wears[who].append(what)
        continue

    parts = [part.strip() for part in line.split("|")]

    if len(parts) != 6:
        continue

    who, how, garment, where, count, depth = parts
    stood[who].add(how)

    if float(depth) >= deep and int(count) >= points:
        found[who][where].append((how, int(count), float(depth)))

# A finding that is the same in every way the body was stood is not something an arm or a stance did: it is how the
# garment was made against the body (the hem of a pair of shorts inside the line where the skin under it was cut away).
# Looked at on three bodies in shorts on 23 November, nothing of it shows. These are listed apart.
made_so = collections.defaultdict(dict)

for who in list(found):
    for where in list(found[who]):
        ways = found[who][where]
        depths = [way[2] for way in ways]

        if len(ways) >= max(8, len(stood[who]) - 3) and max(depths) - min(depths) < 0.6 and not any(part in where for part in ("arm", "hand", "clavicle")):
            made_so[who][where] = ways
            del found[who][where]

    if not found[who]:
        del found[who]

clean = sorted(who for who in wears if who not in found)

for who in sorted(found, key=lambda name: -sum(len(ways) for ways in found[name].values())):
    print(who)

    for where, ways in sorted(found[who].items(), key=lambda item: -len(item[1])):
        worst = max(ways, key=lambda way: way[1] * way[2])
        print("    {:<12} in {:>2} ways; worst {} ({} points, {:.1f} cm): {}".format(
            where, len(ways), worst[0], worst[1], worst[2], " ".join(sorted(way[0] for way in ways))))

print("\n{} bodies measured, {} with nothing over {} cm on {} points that a stance did: {}".format(len(wears), len(clean), deep, points, ", ".join(clean)))

if made_so:
    print("\nThe same however they stand (how the garment sits on the body, not a stance): " + "; ".join(
        "{} ({})".format(who, ", ".join(sorted(parts))) for who, parts in sorted(made_so.items())))

# and by stance: how many bodies have an arm or a hand in what they wear, in each way of standing
by_stance = collections.Counter()

for who in found:
    for where, ways in found[who].items():
        if any(part in where for part in ("arm", "hand")):
            for way in set(way[0] for way in ways):
                by_stance[(way, "hand" if "hand" in where else ("forearm" if "lowerarm" in where else "upper arm"))] += 1

if by_stance:
    print("\nBodies with an arm in what they wear, by the way they stand:")

    for way in sorted(set(key[0] for key in by_stance)):
        print("    {:<10} upper arm {:>2}   forearm {:>2}   hand {:>2}".format(way, by_stance[(way, "upper arm")], by_stance[(way, "forearm")], by_stance[(way, "hand")]))
