"""
Cuts walking cycles and standing loops out of the 100STYLE motion capture dataset: a person recorded walking, running and
standing in a hundred styles (Mason, Starke, Komura 2022; https://zenodo.org/records/8127870; Creative Commons
Attribution 4.0, so it may be used in a game that is sold as long as it is credited: see for-review/sound-sources.md).

Nothing here is made up by the tool. It chooses which stretch of a recording to keep, takes the travel out of it (the game
moves the body), turns it to face straight ahead, and closes the loop by spreading the small difference between its two
ends over its length. The motion in between is the performer's.

Run it with a Python that has numpy and scipy (the one in the Kimodo install does):
  ~/Repos/DigitalHumanStudio/.runtime/kimodo/venv/bin/python -B Tools/make_style_motion.py [clip ...]
The dataset is looked for in ~/.cache/ashmarket-sim/100style/data/100STYLE (unzip 100STYLE.zip from Zenodo there).

What comes out, in ArtSource/style/export: <clip>.bvh at 30 frames a second on the dataset's own skeleton, and <clip>.json
with what the game needs to know (the speed a walk was walked at, how long the loop is, which frames it was cut from).
Tools/style_bvh_to_fbx.py and Tools/import_style_motion.sh take it from there.
"""

import csv
import json
import os
import pathlib
import sys

import numpy as np
from scipy.spatial.transform import Rotation, Slerp

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = pathlib.Path(os.environ.get("STYLE_DATA", str(pathlib.Path.home() / ".cache/ashmarket-sim/100style/data/100STYLE")))
OUT = ROOT / "ArtSource/style/export"

# how high the performer's hips are off the floor, standing: the toes are 96.5 cm under them in the skeleton as it rests
HIPS_STAND = 97.5

# clip -> (the style's folder, which of its recordings, what kind of loop). FW is walking forward, FR running forward,
# ID standing in the style.
CLIPS = {
    "style_walk_neutral": ("Neutral", "FW", "walk"),
    "style_walk_proud": ("Proud", "FW", "walk"),
    "style_walk_strut": ("Strutting", "FW", "walk"),
    "style_walk_depressed": ("Depressed", "FW", "walk"),
    "style_walk_old": ("Old", "FW", "walk"),
    "style_walk_pockets": ("HandsInPockets", "FW", "walk"),
    "style_run_neutral": ("Neutral", "FR", "walk"),
    "style_idle_neutral": ("Neutral", "ID", "idle"),
    "style_idle_proud": ("Proud", "ID", "idle"),
    "style_idle_strut": ("Strutting", "ID", "idle"),
    "style_idle_depressed": ("Depressed", "ID", "idle"),
    "style_idle_old": ("Old", "ID", "idle"),
    "style_idle_folded": ("ArmsFolded", "ID", "idle"),
    "style_idle_akimbo": ("Akimbo", "ID", "idle"),
    "style_idle_pockets": ("HandsInPockets", "ID", "idle"),
    "style_idle_stiff": ("Stiff", "ID", "idle"),
    "style_idle_heavy": ("Heavyset", "ID", "idle"),
    "style_idle_angry": ("Angry", "ID", "idle"),
    "style_idle_followed": ("Followed", "ID", "idle"),
    # a second sitting, the same evening: ways of standing and walking for particular people. Not the drunk: his standing
    # turns eighteen degrees in five seconds and his walk, cut to one stride, is not a stagger.
    "style_idle_behind": ("ArmsBehindBack", "ID", "idle"),
    "style_idle_wide": ("WideLegs", "ID", "idle"),
    "style_idle_apart": ("LegsApart", "ID", "idle"),
    "style_idle_leanback": ("LeanBack", "ID", "idle"),
    "style_idle_byside": ("ArmsBySide", "ID", "idle"),
    "style_idle_rushed": ("Rushed", "ID", "idle"),
    "style_idle_swing": ("SwingShoulders", "ID", "idle"),
    "style_walk_behind": ("ArmsBehindBack", "FW", "walk"),
    "style_walk_limp": ("LimpLeft", "FW", "walk"),
    "style_walk_heavy": ("Heavyset", "FW", "walk"),
    "style_walk_byside": ("ArmsBySide", "FW", "walk"),
    "style_walk_swing": ("SwingShoulders", "FW", "walk"),
}


def read_bvh(path):
    """The skeleton as it is written (kept word for word), each joint's parent and offset, and the frames as numbers."""
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

    return lines[:motion], names, parents, np.array(offsets), frames, float(lines[motion + 2].split()[-1])


def turns(frames, count):
    """Each joint's turn in each frame. The root's channels are three of place and then Y, X, Z; every other joint's are Y, X, Z."""
    every = []

    for joint in range(count):
        first = 3 + (joint * 3)
        every.append(Rotation.from_euler("YXZ", frames[:, first:first + 3], degrees=True))

    return every


def places(frames, parents, offsets, every):
    """Where every joint is in every frame, in the recording's own space."""
    count = len(parents)
    where = np.zeros((len(frames), count, 3))
    world = [None] * count

    for joint in range(count):
        if parents[joint] < 0:
            world[joint] = every[joint]
            where[:, joint] = frames[:, 0:3]
        else:
            world[joint] = world[parents[joint]] * every[joint]
            where[:, joint] = where[:, parents[joint]] + world[parents[joint]].apply(offsets[joint])

    return where


def heading(every):
    """Which way the hips face over the ground in each frame, as an angle about the upright (radians)."""
    forward = every[0].apply([0.0, 0.0, 1.0])

    return np.unwrap(np.arctan2(forward[:, 0], forward[:, 2]))


def shape(where, facing):
    """The body in each frame as its joints about its own hips, turned to face straight ahead: what two frames are compared by."""
    about = where - where[:, :1]
    cos, sin = np.cos(-facing), np.sin(-facing)

    x = (about[:, :, 0] * cos[:, None]) + (about[:, :, 2] * sin[:, None])
    z = (-about[:, :, 0] * sin[:, None]) + (about[:, :, 2] * cos[:, None])

    return np.stack([x, about[:, :, 1] + where[:, :1, 1], z], axis=2).reshape(len(where), -1)


def smooth(values, width):
    kernel = np.ones(width) / width

    return np.convolve(values, kernel, mode="same")


def find_walk(where, facing, first, last, rate):
    """A stretch of whole steps walked in a straight line at a steady pace: where it starts and how many frames it is."""
    hips = where[:, 0]
    step = np.gradient(hips, axis=0) * rate
    speed = smooth(np.hypot(step[:, 0], step[:, 2]), int(rate * 0.4))
    usual = np.median(speed[first:last])
    body = shape(where, facing)

    best = None

    for length in range(int(rate * 0.7), int(rate * 2.4)):
        starts = np.arange(first, last - length, 2)

        if len(starts) == 0:
            continue

        # the body at the end as it was at the beginning, having turned nowhere and kept its pace throughout
        unlike = np.linalg.norm(body[starts + length] - body[starts], axis=1) / np.sqrt(body.shape[1] / 3)
        turned = np.abs(facing[starts + length] - facing[starts])
        slow = np.array([speed[start:start + length].min() for start in starts]) < usual * 0.8
        wander = np.array([np.ptp(facing[start:start + length]) for start in starts])

        # and of two that close as well, the brisker: the room it was recorded in was four metres across, and nobody
        # strides out in that
        brisk = np.array([speed[start:start + length].mean() for start in starts])

        cost = unlike + (np.degrees(turned) * 0.6) + (np.degrees(wander) * 0.12) + (slow * 100.0) - (brisk * 0.03)
        at = int(np.argmin(cost))

        # a longer stretch that closes as well is two steps and not one: whole cycles, left and right, are what is wanted
        if best is None or cost[at] < best[0] - 0.05:
            best = (float(cost[at]), int(starts[at]), length, float(unlike[at]), float(np.degrees(turned[at])))

    return best


def find_idle(where, facing, first, last, rate):
    """A stretch of standing that ends as it began: where it starts and how many frames it is."""
    body = shape(where, facing)
    hips = where[:, 0]
    best = None

    for length in range(int(rate * 5.0), min(int(rate * 10.0), last - first - 2), int(rate * 0.25)):
        starts = np.arange(first, last - length, 6)

        if len(starts) == 0:
            continue

        unlike = np.linalg.norm(body[starts + length] - body[starts], axis=1) / np.sqrt(body.shape[1] / 3)
        strayed = np.hypot(hips[starts + length, 0] - hips[starts, 0], hips[starts + length, 2] - hips[starts, 2])
        turned = np.degrees(np.abs(facing[starts + length] - facing[starts]))

        # somebody who walks off in the middle of it is not standing
        roamed = np.array([np.hypot(np.ptp(hips[start:start + length, 0]), np.ptp(hips[start:start + length, 2])) for start in starts])

        cost = unlike + (strayed * 0.5) + (turned * 0.3) + np.maximum(roamed - 25.0, 0.0)
        at = int(np.argmin(cost))

        if best is None or cost[at] < best[0]:
            best = (float(cost[at]), int(starts[at]), length, float(unlike[at]), float(turned[at]))

    return best


def cut(name, style, take, kind):
    path = DATA / style / "{}_{}.bvh".format(style, take)
    header, names, parents, offsets, frames, frame_time = read_bvh(path)
    rate = round(1.0 / frame_time)

    with open(DATA / "Frame_Cuts.csv") as listed:
        cuts = {row["STYLE_NAME"]: row for row in csv.DictReader(listed)}

    first = int(cuts[style][take + "_START"])
    last = min(int(cuts[style][take + "_STOP"]), len(frames) - 1)

    every = turns(frames, len(names))
    where = places(frames, parents, offsets, every)
    facing = heading(every)

    found = (find_walk if kind == "walk" else find_idle)(where, facing, first, last, rate)

    if not found:
        sys.exit("{}: nothing to cut in {}".format(name, path.name))

    cost, start, length, unlike, turned = found
    stop = start + length

    kept = frames[start:stop + 1].copy()
    turn = [rotation[start:stop + 1] for rotation in every]
    count = len(kept)
    along = np.linspace(0.0, 1.0, count)

    # which way it was going (a walk) or facing (standing), and so how far to turn it to face straight ahead
    hips = kept[:, 0:3].copy()
    gone = hips[-1] - hips[0]
    way = np.arctan2(gone[0], gone[2]) if kind == "walk" else float(np.mean(facing[start:stop + 1]))
    ahead = Rotation.from_euler("Y", -way)

    speed = float(np.hypot(gone[0], gone[2]) / (length / rate)) if kind == "walk" else 0.0

    # the travel comes out: a walk's at its own steady pace, and whatever standing drifted by
    hips[:, 0] -= hips[0, 0] + (gone[0] * along)
    hips[:, 2] -= hips[0, 2] + (gone[2] * along)
    hips[:, 1] -= (hips[-1, 1] - hips[0, 1]) * along
    hips = ahead.apply(hips)

    kept[:, 0:3] = hips

    # every joint ends as it began: the difference between the two ends is spread over the whole of it
    for joint in range(len(names)):
        own = (ahead * turn[joint]) if joint == 0 else turn[joint]
        back = (own[0] * own[-1].inv()).as_rotvec()
        closed = Rotation.from_rotvec(along[:, None] * back[None, :]) * own

        kept[:, 3 + (joint * 3):6 + (joint * 3)] = closed.as_euler("YXZ", degrees=True)

    # thirty frames a second, which is what the game's other clips are; the last frame is the first again and is left off
    keep = kept[:-1:2] if rate >= 50 else kept[:-1]
    out_time = (2.0 if rate >= 50 else 1.0) / rate

    OUT.mkdir(parents=True, exist_ok=True)

    # The recordings rest their skeleton with its hips at the origin and its feet a metre under the floor. The engine
    # scales how far a body travels by how high its hips stand at rest, and from hips at nought it made that thirty times
    # too far. So the skeleton is rested standing on the floor: the hips' own offset is how high they stand.
    for index, line in enumerate(header):
        if line.split()[:1] == ["OFFSET"]:
            header[index] = line[:len(line) - len(line.lstrip())] + "OFFSET 0.000000 {:.6f} 0.000000".format(HIPS_STAND)
            break

    with open(OUT / (name + ".bvh"), "w") as file:
        file.write("\n".join(header) + "\nMOTION\nFrames: {}\nFrame Time: {:.6f}\n".format(len(keep), out_time))

        for frame in keep:
            file.write(" ".join("{:.5f}".format(value) for value in frame) + "\n")

    facts = {"clip": name, "source": "100STYLE {}".format(path.name), "first": start, "last": stop, "frames": len(keep),
             "seconds": round(len(keep) * out_time, 4), "loop": True, "ends_unlike_cm": round(unlike, 2), "turned_degrees": round(turned, 2)}

    if kind == "walk":
        facts["speed"] = round(speed, 1)

    with open(OUT / (name + ".json"), "w") as file:
        json.dump(facts, file, indent=1)

    print("CUT {}: {}".format(name, json.dumps(facts)))


def main():
    wanted = sys.argv[1:] or list(CLIPS)

    for name in wanted:
        if name not in CLIPS:
            sys.exit("No clip called {}".format(name))

        cut(name, *CLIPS[name])


if __name__ == "__main__":
    main()
