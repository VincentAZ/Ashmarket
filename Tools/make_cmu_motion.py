"""
Cuts loops of a fighter standing on guard out of the CMU Graphics Lab Motion Capture Database, for the Gut.

The database (mocap.cs.cmu.edu) is of people recorded in a motion capture suit: "The motion capture data may be copied,
modified, or redistributed without permission", and it is free for commercial projects. The BVH conversion read here is
Bruce Hahne's (cgspeed.com), who places no restriction on it either. Credit is asked for where results are published:
"The data used in this project was obtained from mocap.cs.cmu.edu. The database was created with funding from NSF
EIA-0196217."

It holds a little boxing by people who are not all boxers. What is wanted from it is the standing between the punches:
somebody on their guard, weight moving from foot to foot, hands up. This finds stretches of that which end as they began,
turns each to face the one being fought, takes out what little it travelled, closes the loop by spreading the difference
between its two ends over its length, and writes it as BVH (30 frames a second, centimetres, standing on the floor) into
ArtSource/cmu/export, with the facts of each beside it.

    python -I -B Tools/make_cmu_motion.py --survey 13_17 14_01     what there is in a take: the best stretches, scored

As of 21 November 2026 it only surveys. The boxing takes (13_17, 13_18, 14_01 to 14_03, 15_13, 17_10) are shadow boxing
without a pause; nothing in them was worth cutting, and nothing from this database is in the game.

The takes are looked for in ~/.cache/ashmarket-sim/cmu/data (or CMU_DATA): <take>.bvh, as in the database's own numbering.
"""

import json
import os
import pathlib
import sys

import numpy as np
from scipy.spatial.transform import Rotation

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = pathlib.Path(os.environ.get("CMU_DATA", str(pathlib.Path.home() / ".cache/ashmarket-sim/cmu/data")))
OUT = ROOT / "ArtSource/cmu/export"

# the conversion keeps the database's own unit, which is an inch divided by 0.45
TO_CM = 2.54 / 0.45

# name: (take, first frame of the stretch at 120 a second, how many frames, mirrored). Found with --survey and looked at.
CLIPS = {
}


def read_bvh(path):
    """The skeleton as it is written, each joint's parent and offset, and the frames as numbers. The first frame is a T-pose that the conversion added."""
    lines = path.read_text().splitlines()
    motion = next(i for i, line in enumerate(lines) if line.strip() == "MOTION")

    names, parents, offsets, stack = [], [], [], []
    ended = False

    for line in lines[:motion]:
        words = line.split()

        if not words:
            continue

        if words[0] in ("ROOT", "JOINT"):
            names.append(words[1])
            parents.append(stack[-1] if stack else -1)
            ended = False
        elif words[0] == "End":
            ended = True
        elif words[0] == "{":
            stack.append(len(names) - 1 if not ended else -2)
        elif words[0] == "}":
            stack.pop()
            ended = False
        elif words[0] == "OFFSET" and not ended and len(offsets) < len(names):
            offsets.append([float(word) for word in words[1:4]])

    frames = np.array([[float(word) for word in line.split()] for line in lines[motion + 3:] if line.strip()])

    return lines[:motion], names, parents, np.array(offsets) * TO_CM, frames[1:], float(lines[motion + 2].split()[-1])


def turns(frames, count):
    """Each joint's turn in each frame: its channels are Z, Y, X, turned in that order."""
    return [Rotation.from_euler("ZYX", frames[:, 3 + (joint * 3):6 + (joint * 3)], degrees=True) for joint in range(count)]


def places(frames, parents, offsets, every):
    """Where every joint is in every frame, in centimetres, and how each is turned in the recording's own space."""
    count = len(parents)
    where = np.zeros((len(frames), count, 3))
    world = [None] * count

    for joint in range(count):
        if parents[joint] < 0:
            world[joint] = every[joint]
            where[:, joint] = frames[:, 0:3] * TO_CM
        else:
            world[joint] = world[parents[joint]] * every[joint]
            where[:, joint] = where[:, parents[joint]] + world[parents[joint]].apply(offsets[joint])

    return where, world


def smooth(values, width):
    return np.convolve(values, np.ones(width) / width, mode="same")


def read_take(take):
    header, names, parents, offsets, frames, frame_time = read_bvh(DATA / (take + ".bvh"))
    every = turns(frames, len(names))
    where, world = places(frames, parents, offsets, every)
    rate = round(1.0 / frame_time)
    at = {name: index for index, name in enumerate(names)}

    # the floor is where the lower toes spend their time
    floor = float(np.percentile(np.minimum(where[:, at["LeftToeBase"], 1], where[:, at["RightToeBase"], 1]), 5))

    # which way the head looks over the ground: at the one being fought
    look = world[at["Head"]].apply([0.0, 0.0, 1.0])
    facing = np.unwrap(np.arctan2(smooth(look[:, 0], rate // 2), smooth(look[:, 2], rate // 2)))

    return {"header": header, "names": names, "parents": parents, "offsets": offsets, "frames": frames, "every": every, "where": where,
            "rate": rate, "at": at, "floor": floor, "facing": facing}


def on_guard(take):
    """For each frame: how fast the hands are going, how high they are held, and how far the fists are out from the shoulders."""
    where, at, rate = take["where"], take["at"], take["rate"]

    out, high, fast = [], [], []

    for side in ("Left", "Right"):
        hand = where[:, at[side + "Hand"]]
        shoulder = where[:, at[side + "Arm"]]

        out.append(np.linalg.norm(hand - shoulder, axis=1))
        high.append(hand[:, 1] - shoulder[:, 1])
        fast.append(smooth(np.linalg.norm(np.gradient(hand, axis=0), axis=1) * rate, rate // 10))

    return np.maximum(*out), np.minimum(*high), np.maximum(*fast)


def shape(take):
    """The body in each frame as its joints about its own hips, turned to face the way it looks: what two frames are compared by."""
    where, facing = take["where"], take["facing"]
    about = where - where[:, :1]
    cos, sin = np.cos(-facing), np.sin(-facing)

    x = (about[:, :, 0] * cos[:, None]) + (about[:, :, 2] * sin[:, None])
    z = (-about[:, :, 0] * sin[:, None]) + (about[:, :, 2] * cos[:, None])

    return np.stack([x, about[:, :, 1] + where[:, :1, 1], z], axis=2).reshape(len(where), -1)


def survey(name):
    take = read_take(name)
    where, at, rate, facing = take["where"], take["at"], take["rate"], take["facing"]
    out, high, fast = on_guard(take)
    body = shape(take)
    hips = where[:, 0]
    arm = float(np.linalg.norm(take["offsets"][at["LeftForeArm"]]) + np.linalg.norm(take["offsets"][at["LeftHand"]]))

    print("{}: {:.1f} s at {} a second; hips stand {:.0f} cm above the floor; arm {:.0f} cm; hands above the shoulders by a median {:.0f} cm; hands going a median {:.0f} cm/s".format(
        name, len(where) / rate, rate, float(np.median(hips[:, 1])) - take["floor"], arm, float(np.median(high)), float(np.median(fast))))

    found = []

    for length in range(int(rate * 1.6), int(rate * 5.0), int(rate * 0.2)):
        starts = np.arange(0, len(where) - length - 1, 6)

        if len(starts) == 0:
            continue

        unlike = np.linalg.norm(body[starts + length] - body[starts], axis=1) / np.sqrt(body.shape[1] / 3)
        turned = np.degrees(np.abs(facing[starts + length] - facing[starts]))
        swung = np.degrees(np.array([np.ptp(facing[start:start + length]) for start in starts]))
        roamed = np.array([np.hypot(np.ptp(hips[start:start + length, 0]), np.ptp(hips[start:start + length, 2])) for start in starts])
        punched = np.array([out[start:start + length].max() for start in starts])
        dropped = np.array([high[start:start + length].min() for start in starts])
        quick = np.array([fast[start:start + length].max() for start in starts])
        alive = np.array([np.ptp(hips[start:start + length, 1]) + roamed[i] for i, start in enumerate(starts)])

        # no punch thrown in it (an arm never nearly straight, a hand never fast), hands never down, the body not walked
        # off or turned away; and of those, the ones that end most as they began
        fine = (punched < arm * 0.86) & (quick < 260.0) & (dropped > -28.0) & (roamed < 45.0) & (swung < 30.0)
        cost = unlike + (turned * 0.3) + (roamed * 0.05)

        for i in np.argsort(cost):
            if fine[i]:
                found.append((float(cost[i]), int(starts[i]), length, float(unlike[i]), float(roamed[i]), float(punched[i] / arm), float(dropped[i]), float(quick[i]), float(alive[i])))
                break

    found.sort()
    seen = []

    for cost, start, length, unlike, roamed, punched, dropped, quick, alive in found:
        if any(abs(start - other) < rate for other in seen):
            continue

        seen.append(start)

        left = where[start:start + length, at["LeftFoot"]] - hips[start:start + length]
        right = where[start:start + length, at["RightFoot"]] - hips[start:start + length]
        mid = float(np.mean(facing[start:start + length]))
        ahead = np.array([np.sin(mid), 0.0, np.cos(mid)])

        print("  from {:5d} ({:5.1f} s) for {:.1f} s: ends {:.1f} cm unlike, roams {:.0f} cm, moves {:.0f}, fists out to {:.2f} of the arm, hands at lowest {:.0f} cm by the shoulder, fastest {:.0f} cm/s; left foot {:+.0f} cm ahead of the right".format(
            start, start / rate, length / rate, unlike, roamed, alive, punched, dropped, quick, float(np.mean((left - right) @ ahead))))

        if len(seen) >= 6:
            break


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--survey":
        for name in sys.argv[2:]:
            survey(name)

        return

    # Nothing is cut from it: the survey found no stretch of plain guard longer than 1.6 seconds (21 November 2026).
    sys.exit("Nothing to cut: run with --survey <take> ... to see what a take holds.")


if __name__ == "__main__":
    main()
