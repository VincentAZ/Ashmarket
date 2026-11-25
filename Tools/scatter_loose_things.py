#!/usr/bin/env python3
"""
Sets small loose things down about the town's places: cans, water bottles, plastic jerrycans, crates, boxes, stools,
chairs, rubbish bags. Poly Haven scans (Tools/fetch_polyhaven.py, Tools/import_loose_props.sh), each one a thing that
can be kicked (LOOSE in Tools/build_town_level.py).

    python3 Tools/scatter_loose_things.py            writes them into Tools/town_*.json ("scattered": true), replacing what it wrote before
    python3 Tools/scatter_loose_things.py --none     takes them out again

Where: on ground that is walked on, a stride or two in from its edge, where somebody walking along a wall or cutting a
corner meets them, in ones, twos and threes; clear of everything the list already has standing there, of everybody who
stands or walks a round, of doors, of where the player comes in, of the lots' way to the Block and of where a crowd
stands. The same every time it is run (a fixed deal per place). Then Tools/build_town_level.sh for each place.
"""
import json
import math
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# place -> how many
PLACES = {"pool_street": 24, "yard": 9, "block_square": 22, "main_street": 26}

# what, how often, and how much room it takes (m)
KINDS = [("can_rusted", 5, 0.12), ("plastic_bottle_gallon", 5, 0.14), ("plastic_jerrycan", 3, 0.24), ("oil_tin", 3, 0.12), ("plastic_crate_02", 3, 0.36),
         ("cardboard_box_01", 2, 0.34), ("metal_stool_02", 2, 0.32), ("plastic_monobloc_chair_01", 2, 0.42), ("trashbag", 2, 0.36), ("cement_bag", 1, 0.42)]

CLEAR_OF_THINGS = 1.15
CLEAR_OF_WAYS = 1.0
FROM_EDGE = (0.45, 1.9)


def points_of(listed):
    """Everything in the list that is somewhere: (x, y) of every entry that has both as numbers, and every pair in a list of pairs."""
    found = []

    def walk(node, key):
        if isinstance(node, dict):
            if isinstance(node.get("x"), (int, float)) and isinstance(node.get("y"), (int, float)):
                found.append((float(node["x"]), float(node["y"])))
            for name, inner in node.items():
                walk(inner, name)
        elif isinstance(node, list):
            pairs = [one for one in node if isinstance(one, list) and len(one) == 2 and all(isinstance(n, (int, float)) for n in one)]
            if pairs and len(pairs) == len(node):
                # a way: every step along it
                for a, b in zip(pairs, pairs[1:] + pairs[-1:]):
                    steps = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.5))
                    for i in range(steps + 1):
                        found.append((a[0] + (b[0] - a[0]) * i / steps, a[1] + (b[1] - a[1]) * i / steps))
            else:
                stops = [one for one in node if isinstance(one, dict) and isinstance(one.get("x"), (int, float)) and isinstance(one.get("y"), (int, float))]
                # a round somebody walks: the going between its stops as well
                if key in ("beat", "round", "stops") and len(stops) > 1:
                    for a, b in zip(stops, stops[1:] + stops[:1]):
                        steps = max(1, int(math.hypot(b["x"] - a["x"], b["y"] - a["y"]) / 0.5))
                        for i in range(steps + 1):
                            found.append((a["x"] + (b["x"] - a["x"]) * i / steps, a["y"] + (b["y"] - a["y"]) * i / steps))
                for inner in node:
                    walk(inner, key)

    walk(listed, "")

    return found


def scatter(place, count):
    path = os.path.join(HERE, "town_{}.json".format(place))

    with open(path) as source:
        text = source.read()

    listed = json.loads(text)

    kept = [thing for thing in listed["things"] if not thing.get("scattered")]

    listed_without = dict(listed)
    listed_without["things"] = kept

    taken = points_of({key: value for key, value in listed_without.items() if key not in ("ground", "walk", "surfaces", "walls", "rows", "runs", "bystanders", "pictures", "pasted", "bulbs", "sky")})
    walls = points_of({key: listed_without.get(key) for key in ("rows", "runs", "walls", "bulbs")})
    crowds = [(crowd["x"], crowd["y"]) for crowd in listed.get("bystanders", []) if isinstance(crowd, dict) and isinstance(crowd.get("x"), list)]
    walked = [(patch["x"], patch["y"]) for patch in listed.get("walk", []) if isinstance(patch, dict)]

    deal = random.Random("ashmarket loose " + place)
    bag = [kind for kind in KINDS for _ in range(kind[1])]
    made = []

    def free(x, y, room):
        if not any(xs[0] + room <= x <= xs[1] - room and ys[0] + room <= y <= ys[1] - room for xs, ys in walked):
            return False
        if any(xs[0] - 0.6 <= x <= xs[1] + 0.6 and ys[0] - 0.6 <= y <= ys[1] + 0.6 for xs, ys in crowds):
            return False
        if any(math.hypot(x - px, y - py) < CLEAR_OF_THINGS + room for px, py in taken):
            return False
        if any(math.hypot(x - px, y - py) < 0.5 + room for px, py in walls):
            return False
        return all(math.hypot(x - other["x"], y - other["y"]) > room + other["_room"] + 0.08 for other in made)

    tries = 0

    while len(made) < count and tries < 20000:
        tries += 1

        xs, ys = deal.choices(walked, weights=[(a[1] - a[0]) * (b[1] - b[0]) for a, b in walked])[0]

        # a stride or two in from one of the patch's long edges
        along_x = (xs[1] - xs[0]) >= (ys[1] - ys[0])
        inset = deal.uniform(*FROM_EDGE)

        if along_x:
            x = deal.uniform(xs[0] + 1.0, xs[1] - 1.0)
            y = ys[0] + inset if deal.random() < 0.5 else ys[1] - inset
        else:
            y = deal.uniform(ys[0] + 1.0, ys[1] - 1.0)
            x = xs[0] + inset if deal.random() < 0.5 else xs[1] - inset

        # in ones, twos and threes
        for nth in range(deal.choice((1, 1, 2, 2, 3))):
            if len(made) >= count:
                break

            name, _, room = deal.choice(bag)
            px = x if nth == 0 else x + deal.uniform(-0.75, 0.75)
            py = y if nth == 0 else y + deal.uniform(-0.75, 0.75)

            if nth == 0 and not free(px, py, room):
                break

            if free(px, py, room):
                made.append({"model": name, "x": round(px, 2), "y": round(py, 2), "yaw": deal.randrange(0, 360, 5), "scattered": True, "_room": room})

    return path, text, kept, made


def main():
    none = "--none" in sys.argv

    for place, count in PLACES.items():
        path, text, kept, made = scatter(place, 0 if none else count)

        # written into the list's own text, after the last thing it has, so that the rest of the file stays as it is laid out
        start = text.index('"things"')
        opened = text.index("[", start)
        depth = 0

        for at in range(opened, len(text)):
            depth += {"[": 1, "]": -1}.get(text[at], 0)
            if depth == 0:
                closed = at
                break

        inside = text[opened + 1:closed]

        # what was scattered before goes
        inside = re.sub(r',?\s*\{[^{}]*"scattered": true[^{}]*\}', "", inside).rstrip()

        lines = ",\n".join("  " + json.dumps({key: value for key, value in thing.items() if not key.startswith("_")}) for thing in made)

        text = text[:opened + 1] + inside + ((",\n" + lines) if made else "") + "\n " + text[closed:]

        json.loads(text)

        with open(path, "w") as out:
            out.write(text)

        counts = {}

        for thing in made:
            counts[thing["model"]] = counts.get(thing["model"], 0) + 1

        print("{}: {} set down ({})".format(place, len(made), ", ".join("{} {}".format(n, k) for k, n in sorted(counts.items()))))


main()
