"""
Reads the trace a filmed fight leaves behind (Tools/film_gut_fight.sh with GUT_ARGS=-GutTrace, which writes Saved/GutTrace.csv)
and lists the frames at which a fighter's body jerks: where a bone is suddenly somewhere it was not heading.

  python Tools/find_gut_jerks.py [trace.csv] [how many to list]

A jerk is measured as the distance between where a bone is and where it would be had it carried on as it was going for
one more frame, less what is ordinary for that bone around then. Each is listed with what the fighter was doing, what
clip was playing and whether either had just changed, and a tally by cause is printed at the end.
Needs numpy: run it with a Python that has it.
"""

import csv
import sys
from collections import Counter, defaultdict

import numpy as np

STATES = ["Idle", "Windup", "Active", "Recovery", "Blocking", "Dodging", "Stunned", "Staggered", "Finishing", "Dead"]
BONES = ["pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "Saved/GutTrace.csv"
    show = int(sys.argv[2]) if len(sys.argv) > 2 else 40

    rows = defaultdict(list)

    for row in csv.DictReader(open(path)):
        rows[int(row["who"])].append(row)

    for who, track in rows.items():
        frames = len(track)
        state = [int(row["state"]) for row in track]
        clip = [row["clip"] for row in track]
        weight = np.array([float(row["clipweight"]) for row in track])
        cliptime = np.array([float(row["cliptime"]) for row in track])
        place = np.array([[float(row[k]) for k in ("x", "y", "z")] for row in track])
        yaw = np.unwrap(np.radians([float(row["yaw"]) for row in track]))

        print("\n=== fighter {} ({} frames, {:.1f} s) ===".format(who, frames, frames / 60.0))

        if "neckgap" in track[0]:
            gap = np.array([float(row["neckgap"]) for row in track])
            held = gap >= 0.0

            if held.any():
                # a neck is about 6 cm from its middle to its skin, a skull about 9
                print("weapon in the fist for {} frames: nearest to the middle of the neck or skull {:.1f} cm, within 8 cm for {} frames ({:.1f}%), of them {} while standing or walking".format(
                    int(held.sum()), gap[held].min(), int((gap[held] < 8.0).sum()), 100.0 * (gap[held] < 8.0).mean(),
                    int(sum(1 for f in range(frames) if held[f] and gap[f] < 8.0 and state[f] in (0, 4)))))

        found = []

        for prefix, label in (("s_", "seen"), ("", "skeleton")):
            for bone in BONES:
                spot = np.array([[float(row[prefix + bone + "_" + axis]) for axis in "xyz"] for row in track])

                if not spot.any():
                    continue

                # in the fighter's own space, so that being carried by a lunge or a shove is not a jerk of the body
                cos, sin = np.cos(-yaw), np.sin(-yaw)
                local = spot - place
                own = np.stack([local[:, 0] * cos - local[:, 1] * sin, local[:, 0] * sin + local[:, 1] * cos, local[:, 2]], axis=1)

                for space, points in (("world", spot), ("own", own)):
                    carried_on = 2.0 * points[1:-1] - points[:-2]
                    off = np.linalg.norm(points[2:] - carried_on, axis=1)

                    # what is ordinary: the middle value over the half second around
                    for f in range(off.shape[0]):
                        usual = np.median(off[max(0, f - 15): f + 16])
                        over = off[f] - 3.0 * usual

                        if off[f] > 2.5 and over > 1.5:
                            found.append((off[f], f + 2, label, bone, space))

        actor = np.linalg.norm(place[2:] - (2.0 * place[1:-1] - place[:-2]), axis=1)
        turn = np.degrees(np.abs(yaw[2:] - (2.0 * yaw[1:-1] - yaw[:-2])))

        # one line a frame: the worst bone
        worst = {}

        for size, frame, label, bone, space in found:
            if label == "seen" and (frame not in worst or size > worst[frame][0]):
                worst[frame] = (size, bone, space)

        causes = Counter()
        sizes = defaultdict(list)
        listed = sorted(worst.items(), key=lambda item: -item[1][0])

        for frame, (size, bone, space) in sorted(worst.items()):
            before, now = state[frame - 1], state[frame]
            notes = []

            for back in range(0, 3):
                f = frame - back

                if f > 0 and state[f] != state[f - 1]:
                    notes.append("{}->{} {}f ago".format(STATES[state[f - 1]], STATES[state[f]], back))

                if f > 0 and clip[f] != clip[f - 1]:
                    notes.append("clip {}->{} {}f ago".format(clip[f - 1], clip[f], back))

                if f > 0 and clip[f] == clip[f - 1] and clip[f] != "-" and abs(cliptime[f] - cliptime[f - 1]) > 0.12:
                    notes.append("clip time jumps {:.2f}->{:.2f} {}f ago".format(cliptime[f - 1], cliptime[f], back))

            if actor[frame - 2] > 2.0:
                notes.append("actor jerks {:.1f} cm".format(actor[frame - 2]))

            if turn[frame - 2] > 3.0:
                notes.append("actor turn jerks {:.1f} deg".format(turn[frame - 2]))

            cause = "; ".join(notes) if notes else "nothing changed ({} in {}, weight {:.2f})".format(STATES[now], clip[frame], weight[frame])
            key = "; ".join(note.rsplit(" ", 2)[0] if note.endswith("ago") else note.split(" jerks")[0] for note in notes) if notes else "nothing changed, {}{}".format(
                STATES[now], "" if clip[frame] == "-" else " in a clip")

            causes[key] += 1
            sizes[key].append(size)
            worst[frame] = (size, bone, space, cause)

        print("{} frames with a jerk of the seen body".format(len(worst)))

        for frame, _ in listed[:show]:
            size, bone, space, cause = worst[frame]

            print("  frame {:5d} ({:6.2f} s) {:7s} {:5.1f} cm ({}): {}".format(frame, frame / 60.0, bone, size, space, cause))

        print("by cause:")

        for key, count in causes.most_common(25):
            print("  {:4d}  typically {:4.1f} cm, at most {:4.1f} cm  {}".format(count, float(np.median(sizes[key])), max(sizes[key]), key))


main()
