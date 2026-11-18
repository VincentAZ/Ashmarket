"""
Reads the logs of Tools/run_gut_weapons.sh and prints three tables:
how each pair of weapons came out, how each pair was fought, and how each weapon did overall.
"""

import pathlib
import re
import sys

SUMMARY = re.compile(
    r"BALANCE (\w+) v (\w+) SUMMARY: (\d+) fights, player won (\d+) .*?median (\d+) s, "
    r"middle 80% of fights (\d+) to (\d+) s, .*?(\d+) abandoned(?:, (\d+) ended by the cage)?")

STYLE = re.compile(r"BALANCE (\w+) v (\w+) STYLE: fought (\d+) cm apart on average; player (.*?); opponent (.*)")

SIDE = re.compile(
    r"(\d+) swings a minute, (\d+)% hit, (\d+)% blocked, ([\d.]+) guard breaks a fight, "
    r"lost ([\d.]+) limbs a fight, disarmed in (\d+)% of fights(?:, hurt by the cage ([\d.]+) times a fight)?")


def read(log_dir):
    pairs = []

    for log in sorted(pathlib.Path(log_dir).glob("*.log")):
        text = log.read_text(errors="replace")
        summary = SUMMARY.search(text)
        style = STYLE.search(text)

        if not summary or not style:
            print("No result in {}".format(log), file=sys.stderr)
            continue

        first, second, fights, wins, median, short, long_, abandoned, by_cage = summary.groups()

        pairs.append({
            "first": first, "second": second, "fights": int(fights), "wins": int(wins), "median": int(median),
            "short": int(short), "long": int(long_), "abandoned": int(abandoned), "by_cage": int(by_cage or 0), "gap": int(style.group(3)),
            "sides": [SIDE.search(style.group(4)).groups(), SIDE.search(style.group(5)).groups()],
        })

    return pairs


def table(title, header, rows):
    widths = [max(len(str(row[i])) for row in [header] + rows) for i in range(len(header))]

    print()
    print(title)
    print("  ".join(str(cell).ljust(width) for cell, width in zip(header, widths)))
    print("  ".join("-" * width for width in widths))

    for row in rows:
        print("  ".join(str(cell).ljust(width) for cell, width in zip(row, widths)))


def main():
    pairs = read(sys.argv[1])

    if not pairs:
        print("No results to show.", file=sys.stderr)
        sys.exit(1)

    # weapons in the order they first appear, which is the order they were given
    weapons = []

    for pair in pairs:
        for name in (pair["first"], pair["second"]):
            if name not in weapons:
                weapons.append(name)

    table("HOW EACH PAIR CAME OUT ({} fights each)".format(pairs[0]["fights"]),
          ["Pair", "First wins", "Middle fight", "Middle 80% of fights", "Ended by the cage", "Abandoned"],
          [["{} v {}".format(p["first"], p["second"]), "{:.0f}%".format(100.0 * p["wins"] / p["fights"]),
            "{} s".format(p["median"]), "{} to {} s".format(p["short"], p["long"]),
            "{:.0f}%".format(100.0 * p["by_cage"] / p["fights"]), p["abandoned"]] for p in pairs])

    rows = []

    for p in pairs:
        for name, side in zip((p["first"], p["second"]), p["sides"]):
            other = p["second"] if name == p["first"] else p["first"]
            swings, hit, blocked, breaks, limbs, disarmed, cage_hurts = side

            rows.append(["{} (v {})".format(name, other), "{} cm".format(p["gap"]), swings, hit + "%", blocked + "%", breaks, limbs, disarmed + "%",
                         cage_hurts or "-"])

            # a weapon against itself is the same on both sides
            if p["first"] == p["second"]:
                break

    table("HOW EACH PAIR WAS FOUGHT",
          ["Weapon", "Apart", "Swings/min", "Hit", "Blocked", "Guard breaks/fight", "Limbs lost/fight", "Disarmed", "Hurt by the cage/fight"], rows)

    rows = []

    for weapon in weapons:
        wins = 0
        fights = 0
        results = []

        for p in pairs:
            if p["first"] == p["second"] or weapon not in (p["first"], p["second"]):
                continue

            won = p["wins"] if weapon == p["first"] else p["fights"] - p["wins"]

            wins += won
            fights += p["fights"]
            results.append("{} {:.0f}%".format(p["second"] if weapon == p["first"] else p["first"], 100.0 * won / p["fights"]))

        mirror = next((p for p in pairs if p["first"] == weapon and p["second"] == weapon), None)

        rows.append([weapon, "{:.0f}%".format(100.0 * wins / max(1, fights)), ", ".join(results),
                     "{} s".format(mirror["median"]) if mirror else "-"])

    table("HOW EACH WEAPON DID", ["Weapon", "Wins against the others", "Against each", "Middle fight against itself"], rows)


main()
