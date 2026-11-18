"""
Reads the trace of a filmed fight (Saved/GutTrace.csv, from Tools/film_gut_fight.sh with GUT_ARGS=-GutTrace) and says how much
the feet of a fighter slide while it stands its ground: no clip playing on it, free or with its guard up, and not walking for the last half second.
A person standing has at least one foot still at any moment, so what is measured is the slower of the two feet, over the floor.
It is given twice: for the moments the fighter is being turned to face its opponent, and for the moments it is not.

  python Tools/measure_gut_feet.py [trace.csv ...]       (a Python with numpy)
"""

import csv
import sys

import numpy as np


def measure(path):
    rows = {}

    for row in csv.DictReader(open(path)):
        rows.setdefault(int(row["who"]), []).append(row)

    for who, track in sorted(rows.items()):
        yaw = np.unwrap(np.radians([float(row["yaw"]) for row in track]))
        turning = np.degrees(np.abs(np.diff(yaw))) * 60.0
        going = np.array([float(row["move"]) for row in track])[1:]
        standing = np.array([row["state"] in ("0", "4") and row["clip"] == "-" for row in track])
        standing = standing[1:] & standing[:-1] & (going < 5.0)

        # and has been for half a second: a fighter that has only just stopped is still bringing its feet together
        settled = np.convolve(standing.astype(float), np.ones(30), mode="full")[: standing.size] >= 29.5
        standing = standing & settled

        feet = [np.array([[float(row["s_" + foot + "_" + axis]) for axis in "xy"] for row in track]) for foot in ("foot_l", "foot_r")]
        slower = np.minimum(*[np.linalg.norm(np.diff(foot, axis=0), axis=1) * 60.0 for foot in feet])

        for label, chosen in (("being turned", standing & (turning > 8.0)), ("not being turned", standing & (turning <= 8.0))):
            if chosen.sum() < 10:
                print("{} fighter {} standing, {}: too few frames to say ({})".format(path, who, label, int(chosen.sum())))
                continue

            print("{} fighter {} standing, {}: {:.1f} s; the slower foot moves {:.1f} cm/s on average, {:.1f} cm/s half the time or less, over 10 cm/s for {:.0f}% of it{}".format(
                path, who, label, chosen.sum() / 60.0, slower[chosen].mean(), np.median(slower[chosen]), 100.0 * (slower[chosen] > 10.0).mean(),
                "; turned at {:.0f} degrees a second".format(turning[chosen].mean()) if label == "being turned" else ""))


for name in sys.argv[1:] or ["Saved/GutTrace.csv"]:
    measure(name)
