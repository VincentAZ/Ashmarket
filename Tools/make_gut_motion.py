"""
Makes the Gut's motion clips with Kimodo, NVIDIA's text-to-motion model (trained on 700 hours of motion capture).

Run it through Tools/make_gut_motion.sh, which uses the Kimodo install in ~/Repos/DigitalHumanStudio
(read and executed there, never written to). Everything it makes lands in ArtSource/kimodo/.

  make_gut_motion.sh generate [clip ...]     generate the samples of the clips named (all that have none yet, if none are named)
  make_gut_motion.sh pose <pose> <clip> <sample> <frame> [asis]
                                             keep one frame of a sample as a named pose other clips can start or end in,
                                             turned to look straight ahead unless asis is given
  make_gut_motion.sh pick [clip ...] [again] choose a sample for each clip that has none chosen, by measuring: a first guess to look at
  make_gut_motion.sh export [clip ...]       write the picked sample of each clip as ArtSource/kimodo/export/<clip>.bvh, ready for the engine
  make_gut_motion.sh sheet <clip> [every] [sample] [first frame] [last frame]
                                             redraw a clip's contact sheet, or one sample of it in closer steps

The clips are listed in Tools/gut_motion_clips.json. Each has:
  prompt, seconds, samples, seed
  start, end        names of poses the clip must begin and finish in (optional)
  feet              name of a pose whose feet and hips the clip keeps throughout (to try another upper body on the same legs)
  travel            metres forward the clip finishes at (the game's lunge), with the end pose moved there (optional)
  turn              degrees to the left the clip finishes turned, on the spot, with the end pose turned that far (optional)
  dash              [x, z, seconds]: the body goes that far (metres to its left, metres forward) at a steady speed over the first
                    `seconds` of the clip, still facing the way its start pose faces, and its end pose is there. The game moves a
                    dodging fighter at a steady speed too, so export takes exactly this out and the feet land where they landed.
  path              "line": the body covers `velocity` [x, z] metres a second throughout, facing the way its start pose faces
                    (for walk loops); "stay": it keeps to the spot; "own": export takes out whatever travel it did.
                    With none, the clip goes where it went: for somebody sinking to the knees or falling dead, whose feet
                    and knees must stay where they were put
  guidance          [text, constraints] strengths, if not the model's own

Fields read by export:
  pick              which sample to keep
  trim              [first, last] frames to keep (optional)
  hand              the joint whose fastest moment is the blow: RightHand unless said (attacks)
  impact            the frame of the blow, if the fastest moment is not it
  windup            seconds the game takes to wind this attack up: the clip's own windup is never cut shorter than that
  blow              "last": the clip has several blows and the last strong one is the one that counts
  loop              true: cut to a stretch that ends as it began and blend the join (idles, walks)
  source            the clip whose samples to read, if not this clip's own (to export several samples of one clip)
  origin            the frame that stands over the origin, if not the first one kept (for the second half of a take cut in two)
  face              true: turn the clip so that its first frame looks straight ahead (clips that did not start from a pose)

The game moves its fighters itself: a lunge is a push at a steady speed that lasts until the blow lands. So export
takes that same push out of the clip (and a walk's steady travel out of a walk), and what is left plays in place.

What comes out, per clip, in ArtSource/kimodo/raw/<clip>/: sNN.npz (Kimodo's own format, Y up, metres, 30 a second),
metrics.json (what each sample did), and ArtSource/kimodo/sheets/<clip>.png, every sample drawn as stick figures
from the side and from the front. The right arm is red and the left is blue.
"""

import argparse
import json
import os
import pathlib
import sys

import numpy as np
import torch
from PIL import Image, ImageDraw
from scipy.spatial.transform import Rotation

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "ArtSource/kimodo"
CLIPS = ROOT / "Tools/gut_motion_clips.json"

FPS = 30


def read_clips():
    return {clip["name"]: clip for clip in json.loads(CLIPS.read_text())}


def read_pose(name):
    return json.loads((OUT / "poses" / (name + ".json")).read_text())


def heading_of(pose, skeleton):
    """Which way a pose's hips point, the way Kimodo measures it: a [cos, sin] pair."""
    from kimodo.constraints import FullBodyConstraintSet

    held = FullBodyConstraintSet.from_dict(skeleton, {
        "frame_indices": [0], "local_joints_rot": [pose["local_joints_rot"]], "root_positions": [pose["root_positions"]]})

    return [float(value) for value in held.global_root_heading[0]]


def constraints_for(clip, frames, skeleton=None):
    """The list Kimodo's own loader takes, built from the clip's start, end, travel and path."""
    result = []

    travel = float(clip.get("travel", 0.0))

    ends = []

    if clip.get("start"):
        ends.append((0, read_pose(clip["start"]), 0.0))

    if clip.get("end"):
        ends.append((frames - 1, read_pose(clip["end"]), travel))

    dash = clip.get("dash")

    for frame, pose, forward in ends:
        root = list(pose["root_positions"])
        root[2] += forward

        if dash and frame > 0:
            root[0] += dash[0]
            root[2] += dash[1]

        rotations = pose["local_joints_rot"]

        if clip.get("turn") and frame > 0:
            # a clip that turns on the spot ends in its end pose turned that many degrees to the left
            angle = np.radians(float(clip["turn"]))

            rotations = [list(row) for row in rotations]
            rotations[0] = [float(v) for v in Rotation.from_matrix(turn_matrix(angle) @ Rotation.from_rotvec(rotations[0]).as_matrix()).as_rotvec()]
            root = [float(v) for v in turned(np.array(root), angle)]

        result.append({
            "type": "fullbody",
            "frame_indices": [frame],
            "local_joints_rot": [rotations],
            "root_positions": [root],
            "smooth_root_2d": [[dash[0], dash[1]] if dash and frame > 0 else [0.0, forward]],
        })

    if dash:
        # there by the time the dash is over, by a straight line at a steady speed, and facing forward all the way
        last = int(round(dash[2] * FPS))
        indices = list(range(0, last + 1, 3)) + list(range(last + 3, frames, 3))
        heading = heading_of(read_pose(clip["start"]), skeleton) if clip.get("start") and skeleton is not None else [1.0, 0.0]

        result.append({
            "type": "root2d",
            "frame_indices": indices,
            "smooth_root_2d": [[dash[0] * min(i / last, 1.0), dash[1] * min(i / last, 1.0)] for i in indices],
            "global_root_heading": [heading for _ in indices],
        })

    if clip.get("feet"):
        # the feet and hips of a pose, held through the clip, while the rest of the body does what the prompt says
        pose = read_pose(clip["feet"])
        indices = sorted(set(list(range(0, frames, 15)) + [frames - 1]))

        for side in ("left-foot", "right-foot"):
            result.append({
                "type": side,
                "frame_indices": indices,
                "local_joints_rot": [pose["local_joints_rot"] for _ in indices],
                "root_positions": [pose["root_positions"] for _ in indices],
                "smooth_root_2d": [[0.0, 0.0] for _ in indices],
            })

    if clip.get("path") == "line":
        velocity = clip["velocity"]
        indices = list(range(frames))

        # facing the way its starting pose faces the whole way: going sideways is a sidestep, not a turn and a walk
        heading = heading_of(read_pose(clip["start"]), skeleton) if clip.get("start") and skeleton is not None else [1.0, 0.0]

        result.append({
            "type": "root2d",
            "frame_indices": indices,
            "smooth_root_2d": [[velocity[0] * i / FPS, velocity[1] * i / FPS] for i in indices],
            "global_root_heading": [heading for _ in indices],
        })

    elif clip.get("path") == "stay":
        # a few waypoints, not every frame: the body is free to shift its weight
        indices = list(range(0, frames, 10))

        result.append({
            "type": "root2d",
            "frame_indices": indices,
            "smooth_root_2d": [[0.0, 0.0] for _ in indices],
        })

    return result


def gaze_angle(sample, skeleton_names, frame=0):
    """Which way the head looks at a frame, as a turn about the vertical from straight ahead (+Z), in radians. Positive is to the left."""
    forward = sample["global_rot_mats"][frame, skeleton_names.index("Head")][:, 2]

    return float(np.arctan2(forward[0], forward[2]))


def turned(points, angle):
    """Points turned about the vertical by an angle, positive to the left."""
    cos, sin = np.cos(angle), np.sin(angle)

    result = points.copy()
    result[..., 0] = points[..., 0] * cos + points[..., 2] * sin
    result[..., 2] = -points[..., 0] * sin + points[..., 2] * cos

    return result


def turn_matrix(angle):
    cos, sin = np.cos(angle), np.sin(angle)

    return np.array([[cos, 0.0, sin], [0.0, 1.0, 0.0], [-sin, 0.0, cos]])


def measure(sample, skeleton_names):
    """What a sample did, in numbers: where each hand was fastest, how far the body went, how much the planted feet slid."""
    joints = sample["posed_joints"]

    def speed(name):
        track = joints[:, skeleton_names.index(name)]

        return np.linalg.norm(np.diff(track, axis=0), axis=1) * FPS

    right = speed("RightHand")
    left = speed("LeftHand")
    right_foot = speed("RightFoot")
    left_foot = speed("LeftFoot")

    root = sample["smooth_root_pos"]
    contacts = sample["foot_contacts"].astype(bool)

    slide = []

    for foot, columns in (("LeftFoot", contacts[:, :2]), ("RightFoot", contacts[:, 2:4])):
        planted = columns.any(axis=1)[1:]
        moved = speed(foot)

        slide.append(float(moved[planted].mean()) if planted.any() else 0.0)

    return {
        "frames": int(joints.shape[0]),
        "looks_degrees_left": [round(float(np.degrees(gaze_angle(sample, skeleton_names, frame)))) for frame in (0, joints.shape[0] // 2, joints.shape[0] - 1)],
        "right_hand_peak_frame": int(right.argmax()) + 1,
        "right_hand_peak_speed": round(float(right.max()), 2),
        "left_hand_peak_frame": int(left.argmax()) + 1,
        "left_hand_peak_speed": round(float(left.max()), 2),
        "right_foot_peak_frame": int(right_foot.argmax()) + 1,
        "right_foot_peak_speed": round(float(right_foot.max()), 2),
        "left_foot_peak_frame": int(left_foot.argmax()) + 1,
        "left_foot_peak_speed": round(float(left_foot.max()), 2),
        "root_end": [round(float(root[-1, 0] - root[0, 0]), 3), round(float(root[-1, 2] - root[0, 2]), 3)],
        "root_farthest": round(float(np.linalg.norm((root - root[0])[:, [0, 2]], axis=1).max()), 3),
        "hips_lowest": round(float(joints[:, 0, 1].min()), 3),
        "planted_foot_slide": [round(value, 3) for value in slide],
    }


def draw_sheet(clip_name, samples, names, parents, every=None, only=None, first=0, last=None):
    """Every sample as two rows of stick figures: from its right side (so it faces right) and from in front.
    Front is the way the head looks in the first frame drawn, which in a fighting stance is not the way the hips point."""
    frames = samples[0]["posed_joints"].shape[0]
    last = frames if last is None else min(last, frames)

    if every is None:
        every = max(1, -(-(last - first) // 20))

    picks = list(range(first, last, every))

    cell_w, cell_h = 150, 230
    scale = 105.0

    numbered = list(enumerate(samples))

    if only is not None:
        numbered = [numbered[only]]

    sheet = Image.new("RGB", (cell_w * len(picks) + 60, cell_h * 2 * len(numbered) + 20), (245, 245, 242))
    draw = ImageDraw.Draw(sheet)

    def colour(name):
        if name.startswith("Right") and ("Arm" in name or "Hand" in name or "Shoulder" in name):
            return (210, 40, 40)

        if name.startswith("Left") and ("Arm" in name or "Hand" in name or "Shoulder" in name):
            return (40, 80, 210)

        if name.startswith("Right"):
            return (120, 60, 60)

        if name.startswith("Left"):
            return (60, 60, 120)

        return (50, 50, 50)

    # fingers only clutter a figure this small
    drawn = [i for i, name in enumerate(names) if parents[i] >= 0 and not any(
        part in name for part in ("Thumb", "Index", "Middle", "Ring", "Pinky", "Eye", "Jaw"))]

    for row, (s, sample) in enumerate(numbered):
        look = gaze_angle(sample, names, first)

        joints = turned(sample["posed_joints"], -look)
        root = turned(sample["smooth_root_pos"], -look)

        length = float(os.environ.get("GUT_SHEET_WEAPON", "0"))
        weapon = weapon_line(joints.astype(np.float64), names, length) if length > 0.0 else None

        for view in range(2):
            top = (row * 2 + view) * cell_h + 10

            draw.text((4, top + 4), "s{:02d} {}".format(s, "side" if view == 0 else "front"), fill=(0, 0, 0))
            draw.line((0, top + cell_h - 14, sheet.width, top + cell_h - 14), fill=(200, 200, 195))

            if view == 0:
                draw.text((4, top + 18), "looks {:.0f} left".format(np.degrees(look)), fill=(120, 120, 120))

            for c, frame in enumerate(picks):
                left = 60 + c * cell_w + cell_w / 2
                floor = top + cell_h - 14

                # each figure is drawn about where the body is, so travel does not walk it off its cell
                here = joints[frame] - np.array([root[frame, 0], 0.0, root[frame, 2]])

                def point(joint):
                    x, y, z = here[joint]
                    across = z if view == 0 else x

                    return (left + across * scale, floor - y * scale)

                for joint in drawn:
                    draw.line((*point(parents[joint]), *point(joint)), fill=colour(names[joint]), width=3)

                head = point(names.index("Head"))
                draw.ellipse((head[0] - 7, head[1] - 12, head[0] + 7, head[1] + 2), outline=(50, 50, 50), width=2)

                if weapon is not None:
                    # where a bar in the right fist would lie, in green
                    ends = []

                    for end in weapon[:2]:
                        x, y, z = end[frame] - np.array([root[frame, 0], 0.0, root[frame, 2]])
                        ends.append((left + (z if view == 0 else x) * scale, floor - y * scale))

                    draw.line((*ends[0], *ends[1]), fill=(30, 150, 60), width=2)

                if view == 0:
                    draw.text((left - 10, top), str(frame), fill=(90, 90, 90))

    (OUT / "sheets").mkdir(parents=True, exist_ok=True)

    path = OUT / "sheets" / (clip_name + ("" if only is None else "_s{:02d}".format(only)) + ".png")
    sheet.save(path)

    return path


def load_samples(clip_name):
    folder = OUT / "raw" / clip_name

    return [dict(np.load(path)) for path in sorted(folder.glob("s*.npz"))]


def skeleton_tables():
    from kimodo.skeleton.definitions import SOMASkeleton77

    skeleton = SOMASkeleton77()

    return list(skeleton.bone_order_names), [int(parent) for parent in skeleton.joint_parents]


def generate(names):
    from kimodo import load_model
    from kimodo.constraints import load_constraints_lst
    from kimodo.exports.motion_io import save_kimodo_npz
    from kimodo.tools import seed_everything

    clips = read_clips()

    if not names:
        names = [name for name, clip in clips.items() if "source" not in clip and not (OUT / "raw" / name / "s00.npz").exists()]

    for name in names:
        if name not in clips:
            sys.exit("No clip called {} in {}".format(name, CLIPS))

    if not names:
        print("Nothing to generate: every clip has samples. Name a clip to make it again.")
        return

    model = load_model("Kimodo-SOMA-RP-v1.2", device="cuda:0", default_family="Kimodo")
    bone_names, parents = skeleton_tables()

    for name in names:
        clip = clips[name]
        frames = int(round(clip["seconds"] * FPS))
        samples = int(clip.get("samples", 4))

        constraints = constraints_for(clip, frames, model.skeleton)
        constraint_lst = load_constraints_lst(constraints, model.skeleton) if constraints else []

        seed_everything(int(clip.get("seed", 0)))

        extra = {}

        if clip.get("guidance"):
            extra = {"cfg_type": "separated", "cfg_weight": [float(value) for value in clip["guidance"]]}

        output = model(
            [clip["prompt"]],
            [frames],
            constraint_lst=constraint_lst,
            num_denoising_steps=int(clip.get("steps", 100)),
            num_samples=samples,
            multi_prompt=True,
            post_processing=True,
            return_numpy=True,
            **extra,
        )

        folder = OUT / "raw" / name
        folder.mkdir(parents=True, exist_ok=True)

        for old in folder.glob("s*.npz"):
            old.unlink()

        metrics = []

        for i in range(samples):
            single = {key: (value[i] if hasattr(value, "shape") and len(value.shape) > 0 and value.shape[0] == samples else value)
                      for key, value in output.items()}

            save_kimodo_npz(str(folder / "s{:02d}.npz".format(i)), single)

        loaded = load_samples(name)

        for i, sample in enumerate(loaded):
            metrics.append(dict(sample=i, **measure(sample, bone_names)))

        (folder / "metrics.json").write_text(json.dumps({"clip": clip, "samples": metrics}, indent=1) + "\n")

        sheet = draw_sheet(name, loaded, bone_names, parents)

        print("CLIP {}: {} samples of {} frames, sheet {}".format(name, samples, frames, sheet))

        for entry in metrics:
            print("   ", json.dumps(entry))


def keep_pose(pose_name, clip_name, sample, frame, face=True):
    data = load_samples(clip_name)[sample]
    bone_names, _ = skeleton_tables()

    matrices = data["local_rot_mats"][frame].astype(np.float64).copy()

    # the pose stands over the origin wherever the clip had wandered to, and looks straight ahead
    root = data["root_positions"][frame].astype(float)
    smooth = data["smooth_root_pos"][frame].astype(float)

    root[0] -= smooth[0]
    root[2] -= smooth[2]

    # a clip made on the feet of another pose already faces the way that pose faces, wherever its head is turned
    angle = -gaze_angle(data, bone_names, frame) if face else 0.0

    matrices[0] = turn_matrix(angle) @ matrices[0]
    root = turned(root, angle)

    rotations = Rotation.from_matrix(matrices.reshape(-1, 3, 3)).as_rotvec()

    (OUT / "poses").mkdir(parents=True, exist_ok=True)

    (OUT / "poses" / (pose_name + ".json")).write_text(json.dumps({
        "from": {"clip": clip_name, "sample": sample, "frame": frame, "turned_degrees": round(float(np.degrees(angle)), 1)},
        "local_joints_rot": [[round(float(v), 6) for v in row] for row in rotations],
        "root_positions": [round(float(v), 5) for v in root],
    }) + "\n")

    print("Pose {} kept from {} s{:02d} frame {}: hips at {}".format(pose_name, clip_name, sample, frame, root.round(3)))


def turn_angles(sample):
    """How far the hips have turned about the vertical since the first frame, frame by frame, in radians, positive to the left:
    the same sense as turn_matrix and turned."""
    hips = sample["global_rot_mats"][:, 0].astype(np.float64)

    # the hips' own forward, flat on the floor
    forward = hips[:, :, 2]
    angle = np.unwrap(np.arctan2(forward[:, 0], forward[:, 2]))

    return angle - angle[0]


def slerp_rotations(a, b, weight):
    """Part of the way from one set of joint rotations to another."""
    first = Rotation.from_matrix(a.reshape(-1, 3, 3))
    turn = (first.inv() * Rotation.from_matrix(b.reshape(-1, 3, 3))).as_rotvec()

    return (first * Rotation.from_rotvec(turn * weight)).as_matrix().reshape(a.shape)


def find_loop(joints, root, shortest, longest, lead):
    """The two frames of a clip most alike in pose and in movement, far enough apart to hold a whole cycle."""
    local = joints - root[:, None, :]
    moving = np.diff(local, axis=0, append=local[-1:])

    best = None

    for first in range(lead, joints.shape[0] - shortest):
        for last in range(first + shortest, min(first + longest, joints.shape[0] - 1) + 1):
            cost = np.linalg.norm(local[first] - local[last]) + 4.0 * np.linalg.norm(moving[first] - moving[last])

            if best is None or cost < best[0]:
                best = (cost, first, last)

    return best


def export(names):
    from kimodo.exports.bvh import save_motion_bvh
    from kimodo.skeleton.definitions import SOMASkeleton77

    clips = read_clips()
    skeleton = SOMASkeleton77()
    bone_names = list(skeleton.bone_order_names)

    if not names:
        names = [name for name, clip in clips.items() if "pick" in clip]

    (OUT / "export").mkdir(parents=True, exist_ok=True)

    for name in names:
        clip = clips[name]
        data = load_samples(clip.get("source", name))[int(clip["pick"])]

        rotations = data["local_rot_mats"].astype(np.float64)
        hips = data["root_positions"].astype(np.float64)
        joints = data["posed_joints"].astype(np.float64)
        smooth = data["smooth_root_pos"].astype(np.float64)

        first, last = clip.get("trim", [0, rotations.shape[0] - 1])

        # a loop is the stretch of the clip that ends most nearly as it began. Found first, because everything after
        # is measured from where the kept part starts: a clip that turned round before its loop began once came out
        # with its back to the opponent.
        looped = None

        if clip.get("loop"):
            lead = int(clip.get("blend", 8))
            cycle = clip.get("cycle", [0.6, 1.6])

            looped = find_loop(joints, smooth, int(cycle[0] * FPS), int(cycle[1] * FPS), max(lead, first))
            first = looped[1]

        # a clip that was free to stand as it liked is turned so that it looks straight ahead where it begins:
        # in a fighting stance the hips point off to one side of the opponent
        if clip.get("face"):
            angle = -gaze_angle(data, bone_names, first)

            rotations[:, 0] = turn_matrix(angle) @ rotations[:, 0]
            hips = turned(hips, angle)
            joints = turned(joints, angle)
            smooth = turned(smooth, angle)
        frames = np.arange(rotations.shape[0])

        # where the game will have carried the fighter by each frame
        carried = np.zeros((rotations.shape[0], 3))
        facts = {}

        if clip.get("path") == "line":
            carried[:, 0] = clip["velocity"][0] * frames / FPS
            carried[:, 2] = clip["velocity"][1] * frames / FPS

        if "hand" in clip or "impact" in clip or "travel" in clip:
            track = joints[:, bone_names.index(clip.get("hand", "RightHand"))]
            speed = np.linalg.norm(np.diff(track, axis=0), axis=1) * FPS

            impact = int(clip["impact"]) if "impact" in clip else int(speed[first:last].argmax()) + first + 1

            if clip.get("blow") == "last" and "impact" not in clip:
                # several blows: the one that counts is the last that is nearly as fast as the fastest
                strong = np.nonzero(speed[first:last] > 0.7 * speed[impact - 1])[0]
                impact = int(strong[-1]) + first + 1

            # the blow begins where the hand first gets going
            begin = impact

            while begin > first and speed[begin - 1] > 0.12 * speed[impact - 1]:
                begin -= 1

            begin = max(first, begin - 2)

            # the game plays the clip so that this stretch takes as long as its own windup. Where the game's windup is the
            # longer, the stretch starts earlier, on whatever led up to the blow, rather than be played in slow motion.
            if "windup" in clip:
                begin = max(first, min(begin, impact - int(round(float(clip["windup"]) * FPS))))

            begin = int(clip.get("begin", begin))

            travel = float(clip.get("travel", 0.0))
            share = np.clip((frames - begin) / max(impact - begin, 1), 0.0, 1.0)

            carried[:, 2] += travel * share

            facts.update(begin=(begin - first) / FPS, impact=(impact - first) / FPS)

        if clip.get("turn"):
            # the game turns its fighters itself as well. What the body turned is taken out frame by frame, so the clip steps
            # round on the spot facing the way it began, and how far round it had got at each frame is kept: the game turns
            # the body back by what is left, and the feet land where they landed.
            angle = turn_angles(data)
            total = angle[-1]

            for frame in range(rotations.shape[0]):
                about = smooth[frame].copy()

                rotations[frame, 0] = turn_matrix(-angle[frame]) @ rotations[frame, 0]
                hips[frame] = turned(hips[frame] - about, -angle[frame]) + about
                joints[frame] = turned(joints[frame] - about, -angle[frame]) + about

            carried[:, 0] = smooth[:, 0] - smooth[first, 0]
            carried[:, 2] = smooth[:, 2] - smooth[first, 2]

            facts.update(turn_degrees=round(float(np.degrees(total)), 2), turn_progress=[round(float(value / total), 4) for value in angle])

        if clip.get("dash"):
            # the game carries a dodging fighter at a steady speed for as long as the dodge lasts: that, and nothing else, comes out
            across, along, seconds = clip["dash"]
            share = np.clip((frames - first) / max(seconds * FPS, 1.0), 0.0, 1.0)

            carried[:, 0] += across * share
            carried[:, 2] += along * share

            facts.update(dash=[across, along], dash_seconds=seconds)

        if clip.get("path") == "own":
            # wherever the body wandered, it wandered on the spot: the game decides where the fighter is
            carried[:, 0] = smooth[:, 0] - smooth[first, 0]
            carried[:, 2] = smooth[:, 2] - smooth[first, 2]

        hips = hips - carried
        joints = joints - carried[:, None, :]
        smooth = smooth - carried

        if looped:
            cost, first, last = looped

            rotations = rotations[: last + 1].copy()
            hips = hips[: last + 1].copy()

            # the frames before the join lean toward the frames before the start, so the last frame leads into the first
            for step in range(lead):
                weight = (step + 1) / lead
                here = last - lead + 1 + step
                there = first - lead + 1 + step

                rotations[here] = slerp_rotations(rotations[here], rotations[there], weight)
                hips[here] = hips[here] + (hips[there] - hips[here]) * weight

            facts.update(loop_cost=round(float(cost), 3))

            # the last frame is now the first again, so it is left off
            last -= 1

        rotations = rotations[first: last + 1]
        hips = hips[first: last + 1].copy()

        if clip.get("loop") and clip.get("path") == "line":
            # every walking loop starts at the same point in its step, the left foot as far ahead of the right
            # as it gets along the way the body is going, so that two of them can be mixed without the legs fighting
            way = np.array([clip["velocity"][0], 0.0, clip["velocity"][1]])
            way = way / np.linalg.norm(way)

            feet = joints[first: last + 1, bone_names.index("LeftFoot")] - joints[first: last + 1, bone_names.index("RightFoot")]
            start = int((feet @ way).argmax())

            rotations = np.roll(rotations, -start, axis=0)
            hips = np.roll(hips, -start, axis=0)

            facts.update(step_starts=start, speed=round(float(np.linalg.norm(clip["velocity"])) * 100.0, 1))

        # the clip stands over the origin where it begins, or where the frame named by `origin` was: the second half of a take
        # cut in two begins where the first half left the body, not back over the spot
        over = int(clip.get("origin", first))

        hips[:, 0] -= smooth[over, 0]
        hips[:, 2] -= smooth[over, 2]

        if "travel" in clip:
            facts.update(travel=float(clip["travel"]))

        target = OUT / "export" / (name + ".bvh")

        save_motion_bvh(str(target), torch.from_numpy(rotations).float(), torch.from_numpy(hips).float(),
                        skeleton=skeleton, fps=FPS, standard_tpose=True)

        facts.update(clip=name, sample=int(clip["pick"]), first=int(first), last=int(last),
                     frames=int(rotations.shape[0]), seconds=round(rotations.shape[0] / FPS, 4), prompt=clip["prompt"])

        target.with_suffix(".json").write_text(json.dumps(facts, indent=1) + "\n")

        print("EXPORT {}: {}".format(target.name, json.dumps({key: value for key, value in facts.items() if key != "prompt"})))


def autopick(names, again=False):
    """Chooses a sample for every clip that has a role and no pick yet, by measuring, and writes the choice into the clip list.
    It is a first guess to be looked at, not a verdict: a pick written by hand is never changed unless `again` is given."""
    listed = json.loads(CLIPS.read_text())
    bone_names, _ = skeleton_tables()

    for clip in listed:
        name = clip["name"]
        role = clip.get("role")

        if not role or (names and name not in names) or ("pick" in clip and not again and not names):
            continue

        samples = load_samples(clip.get("source", name))
        scores = []

        for index, sample in enumerate(samples):
            joints = sample["posed_joints"]
            smooth = sample["smooth_root_pos"]
            frames = joints.shape[0]
            facts = measure(sample, bone_names)

            def speed(joint):
                return np.linalg.norm(np.diff(joints[:, bone_names.index(joint)], axis=0), axis=1) * FPS

            if role == "attack":
                track = speed(clip.get("hand", "RightHand"))
                impact = int(track.argmax()) + 1
                travel = float(clip.get("travel", 0.0))

                # a blow that lands before the body has had time to wind up, or after the clip is nearly over, is no use
                timely = 0.2 * FPS <= impact <= 0.7 * frames
                carried = (smooth[impact, 2] - smooth[0, 2]) / travel if abs(travel) > 0.05 else 1.0

                score = float(track.max()) * (1.0 if timely else 0.3) * (1.0 if carried > 0.4 else 0.6)
                note = "blow at frame {} at {:.1f} m/s, {:.0f}% of the lunge covered by then".format(impact, track.max(), carried * 100.0)

            elif role == "stun":
                track = speed("Head")
                score = float(track[:12].max()) - 0.5 * float(track[25:].max())
                note = "head moves at {:.1f} m/s in the first 12 frames".format(track[:12].max())

            elif role in ("dodge", "backstep") and clip.get("dash"):
                across, along, seconds = clip["dash"]
                last = int(round(seconds * FPS))
                share = np.clip(np.arange(frames) / last, 0.0, 1.0)

                # how far the body strays from the straight dash the game will make of it: that much is how far the feet will slide
                off = np.hypot(smooth[:, 0] - smooth[0, 0] - across * share, smooth[:, 2] - smooth[0, 2] - along * share)

                # still facing its opponent, and on its feet
                turned_away = float(np.degrees(np.abs([gaze_angle(sample, bone_names, f) - gaze_angle(sample, bone_names, 0) for f in range(0, frames, 3)]).max()))
                low = float(joints[:, 0, 1].min())

                score = -float(off.max()) - 0.01 * max(0.0, turned_away - 35.0) - (5.0 if low < 0.5 else 0.0) - 0.5 * float(np.mean(facts["planted_foot_slide"]))
                note = "strays {:.2f} m from the straight dash at most, head turns {:.0f} degrees at most, hips never below {:.2f} m, planted feet slide {:.2f} m/s".format(
                    off.max(), turned_away, low, float(np.mean(facts["planted_foot_slide"])))

            elif role in ("dodge", "backstep"):
                moved = np.linalg.norm(np.diff(smooth[:, [0, 2]], axis=0), axis=1) * FPS
                score = float(moved[:15].mean())
                note = "body moves at {:.1f} m/s over the first half second".format(score)

            elif role == "idle" and clip.get("loop"):
                cycle = clip.get("cycle", [4.0, 8.0])
                cost, first, last = find_loop(joints, smooth, int(cycle[0] * FPS), int(cycle[1] * FPS), int(clip.get("blend", 8)))

                moving = float(speed("RightHand")[first:last].mean() + speed("LeftHand")[first:last].mean() + speed("Head")[first:last].mean())
                stepping = float(max(speed("RightFoot")[first:last].max(), speed("LeftFoot")[first:last].max()))

                # how far a weapon in the right hand comes to the neck, for the stances that hold one
                near = clearance(joints.astype(np.float64), bone_names)[first:last]

                # standing ready is not shadow boxing: no hand as fast as a punch or as far out as one, and no ducking
                fastest = float(max(speed("RightHand")[first:last].max(), speed("LeftHand")[first:last].max()))
                reach = max(float(np.linalg.norm((joints[first:last, bone_names.index(hand)] - joints[first:last, 0]) - (joints[0, bone_names.index(hand)] - joints[0, 0]), axis=1).max())
                            for hand in ("RightHand", "LeftHand"))
                head = joints[first:last, bone_names.index("Head"), 1]
                ducked = float(joints[0, bone_names.index("Head"), 1] - head.min())

                # alive, the livelier the better up to a point, but standing its ground, and a loop that closes
                score = (min(moving, 1.0) - 0.25 * cost - 1.0 * max(0.0, stepping - 0.9) - 1.0 * max(0.0, fastest - 1.6)
                         - 3.0 * max(0.0, reach - 0.28) - 4.0 * max(0.0, ducked - 0.12))
                note = ("loop of frames {} to {} at cost {:.2f}; hands and head average {:.2f} m/s, fastest hand {:.1f} m/s, a hand goes {:.2f} m from where the stance has it, "
                        "head dips {:.2f} m, fastest foot {:.2f} m/s; weapon line {:.2f} m from the neck at nearest").format(
                    first, last, cost, moving, fastest, reach, ducked, stepping, near.min())

            elif role == "idle":
                moving = float(speed("RightHand").mean() + speed("LeftHand").mean())
                stepping = max(facts["right_foot_peak_speed"], facts["left_foot_peak_speed"])

                # alive but not walking about: some movement in the arms, feet where they are
                score = min(moving, 0.25) - 2.0 * max(0.0, stepping - 0.8)
                note = "hands average {:.2f} m/s, fastest foot {:.2f} m/s".format(moving, stepping)

            elif role == "block":
                cycle = clip.get("cycle", [1.6, 3.2])
                cost, first, last = find_loop(joints, smooth, int(cycle[0] * FPS), int(cycle[1] * FPS), int(clip.get("blend", 8)))

                # a guard that stays up: the hands never far from where the guard pose has them
                strayed = max(float(np.linalg.norm(joints[first:last, bone_names.index(hand)] - joints[0, bone_names.index(hand)], axis=1).max())
                              for hand in ("RightHand", "LeftHand"))
                stepping = float(max(speed("RightFoot")[first:last].max(), speed("LeftFoot")[first:last].max()))

                score = -cost - 4.0 * max(0.0, strayed - 0.12) - 1.0 * max(0.0, stepping - 0.5)
                note = "loop of frames {} to {} at cost {:.2f}; a hand strays {:.2f} m from the guard at most, fastest foot {:.2f} m/s".format(first, last, cost, strayed, stepping)

            elif role == "blockhit":
                # a jolt that is seen: the head and the hands knocked back early, and the guard still up at the end of it
                look = gaze_angle(sample, bone_names, 0)
                head = turned(joints[:, bone_names.index("Head")] - smooth, -look)
                back = float(-(head[2:14, 2] - head[0, 2]).min())
                jolt = float(speed("Head")[:12].max())
                strayed = max(float(np.linalg.norm(joints[:, bone_names.index(hand)] - joints[0, bone_names.index(hand)], axis=1).max()) for hand in ("RightHand", "LeftHand"))

                score = min(jolt, 1.5) + 2.0 * min(back, 0.15) - 2.0 * max(0.0, strayed - 0.3) - 1.0 * max(0.0, facts["root_farthest"] - 0.3)
                note = "head knocked at {:.1f} m/s and {:.2f} m back, a hand strays {:.2f} m from the guard, body wanders {:.2f} m".format(jolt, back, strayed, facts["root_farthest"])

            elif role == "turn":
                angle = turn_angles(sample)
                share = angle / angle[-1]

                # the turn should go one way, without swinging past and coming back, and the feet should step, not hop
                back = float(np.clip(-np.diff(share), 0.0, None).sum())
                over = float(max(share.max() - 1.0, 0.0))
                contacts = sample["foot_contacts"].astype(bool)
                airborne = int((~contacts[:, :2].any(axis=1) & ~contacts[:, 2:4].any(axis=1)).sum())

                score = -3.0 * back - 3.0 * over - 0.05 * airborne - 1.0 * facts["root_farthest"]
                note = "turns {:.0f} degrees, goes back {:.0f}% of it on the way, past it by {:.0f}%, {} frames with both feet up, wanders {:.2f} m".format(
                    np.degrees(angle[-1]), back * 100.0, over * 100.0, airborne, facts["root_farthest"])

            elif role == "hit":
                # which way the head should go in the first half second, in the body's own left, up and forward
                wanted = {"left": (-1.0, -0.2, -0.3), "right": (1.0, -0.2, -0.3), "gut": (0.0, -1.0, 0.2), "top": (0.0, -1.0, 0.0),
                          "back": (0.0, -0.6, 1.0), "arm_l": (-1.0, -0.2, 0.0), "arm_r": (1.0, -0.2, 0.0),
                          "leg_l": (0.3, -1.0, 0.0), "leg_r": (-0.3, -1.0, 0.0)}[clip["strike"]]
                wanted = np.array(wanted) / np.linalg.norm(wanted)

                look = gaze_angle(sample, bone_names, 0)
                head = turned(joints[:, bone_names.index("Head")] - smooth, -look)
                hips = turned(joints[:, bone_names.index("Hips")] - smooth, -look)

                went = head[4:19] - head[0]
                along = went @ wanted
                peak = int(along.argmax()) + 4

                # a leg that gives is seen in the hips, not the head
                height = joints[:, 0, 1]
                drop = float(height[0] - height[4:22].min()) if clip["strike"].startswith("leg") else 0.0
                wander = float(np.linalg.norm((smooth[-1] - smooth[0])[[0, 2]]))

                score = float(along.max()) + 2.0 * drop - 0.3 * max(0.0, wander - 0.4) - (5.0 if joints[:, 0, 1].min() < 0.55 else 0.0)
                note = "head goes {:.2f} m the right way by frame {} (left {:+.2f}, up {:+.2f}, forward {:+.2f}), hips drop {:.2f} m, walks {:.2f} m".format(
                    along.max(), peak, went[peak - 4, 0], went[peak - 4, 1], went[peak - 4, 2], drop, wander)

            elif role == "victory":
                # somebody celebrating: on the spot, on its feet, and with its hands going up or out. Picked by looking at the sheet.
                head = joints[:, bone_names.index("Head")]
                reach = np.maximum(joints[:, bone_names.index("LeftHand"), 1], joints[:, bone_names.index("RightHand"), 1]) - head[:, 1]
                wander = facts["root_farthest"]

                score = float(reach.max()) - 1.0 * max(0.0, wander - 0.5) - (5.0 if facts["hips_lowest"] < 0.6 else 0.0)
                note = "a hand gets {:+.2f} m above the head, above it for {} frames, wanders {:.2f} m, hips lowest {:.2f} m".format(
                    float(reach.max()), int((reach > 0.0).sum()), wander, facts["hips_lowest"])

            elif role in ("taken", "death"):
                # somebody being finished, or dying on their own: it should keep to its spot, and a death should end on the floor.
                # This is only a first guess: these are picked by looking at the sheet.
                head = joints[:, bone_names.index("Head")]
                hands = 0.5 * (np.linalg.norm(joints[:, bone_names.index("LeftHand")] - head, axis=1)
                               + np.linalg.norm(joints[:, bone_names.index("RightHand")] - head, axis=1))
                wander = facts["root_farthest"]
                ends = float(head[-10:, 1].mean())
                lively = float(speed("Head").mean())

                score = lively - 1.5 * max(0.0, wander - 0.5) - (2.0 * ends if role == "death" else 0.0)
                note = "wanders {:.2f} m, hips lowest {:.2f} m, head ends at {:.2f} m, hands {:.2f} m from the head on average, head moves at {:.2f} m/s".format(
                    wander, float(joints[:, 0, 1].min()), ends, float(hands.mean()), lively)

            elif role == "stagger":
                cycle = clip.get("cycle", [2.5, 5.0])
                cost, first, last = find_loop(joints, smooth, int(cycle[0] * FPS), int(cycle[1] * FPS), int(clip.get("blend", 8)))
                low = float(joints[:, 0, 1].min())

                # it has to stay on its feet, and it has to move
                score = float(speed("Head")[first:last].mean()) - cost * 0.2 - (5.0 if low < 0.55 else 0.0)
                note = "loop of frames {} to {} at cost {:.2f}, hips never below {:.2f} m".format(first, last, cost, low)

            elif role == "watch":
                cycle = clip.get("cycle", [4.0, 8.0])
                cost, first, last = find_loop(joints, smooth, int(cycle[0] * FPS), int(cycle[1] * FPS), int(clip.get("blend", 8)))
                wandered = facts["root_farthest"]

                # somebody standing and watching: on their feet, on their spot, and the loop closing well
                score = -cost - 2.0 * max(0.0, wandered - 0.3) - (5.0 if facts["hips_lowest"] < 0.75 else 0.0)
                note = "loop of frames {} to {} at cost {:.2f}, wanders {:.2f} m".format(first, last, cost, wandered)

            elif role == "cheer":
                lively = float(speed("RightHand").mean() + speed("LeftHand").mean())
                wandered = facts["root_farthest"]

                score = lively - 2.0 * max(0.0, wandered - 0.4)
                note = "hands average {:.1f} m/s, wanders {:.2f} m".format(lively, wandered)

            elif role == "move":
                cycle = clip.get("cycle", [0.6, 1.6])
                velocity = np.array([clip["velocity"][0], 0.0, clip["velocity"][1]])
                carried = np.outer(np.arange(frames) / FPS, velocity)
                cost, first, last = find_loop(joints - carried[:, None, :], smooth - carried, int(cycle[0] * FPS), int(cycle[1] * FPS), int(clip.get("blend", 8)))

                went = (smooth[-1] - smooth[0]) / (frames / FPS)
                off = float(np.linalg.norm(went - velocity) / np.linalg.norm(velocity))
                turn = abs(gaze_angle(sample, bone_names, frames // 2) - gaze_angle(sample, bone_names, 0))

                score = -cost - 3.0 * off - 2.0 * turn
                note = "loop of frames {} to {} at cost {:.2f}, speed off by {:.0f}%, head turned {:.0f} degrees".format(first, last, cost, off * 100.0, np.degrees(turn))

            else:
                continue

            scores.append((score, index, note))

        if not scores:
            continue

        scores.sort(reverse=True)
        clip["pick"] = scores[0][1]

        print("PICK {}: s{:02d} ({}); others: {}".format(name, scores[0][1], scores[0][2], ", ".join("s{:02d} {:.2f}".format(i, sc) for sc, i, _ in scores[1:])))

    CLIPS.write_text(json.dumps(listed, indent=1) + "\n")


def weapon_line(joints, names, length):
    """Where a bar gripped in the right fist lies, frame by frame: its two ends. A fist closes round a bar so that the bar
    runs along the knuckles, out past the thumb side. The fingers of these takes are not posed, so the knuckle line is
    taken from the knuckles of the index and little fingers, which are where they are however the fingers curl."""
    hand = joints[:, names.index("RightHand")]
    index = joints[:, names.index("RightHandIndex2")]
    little = joints[:, names.index("RightHandPinky2")]

    along = index - little
    along = along / np.linalg.norm(along, axis=1, keepdims=True)

    grip = (index + little) * 0.5

    return grip - along * 0.12, grip + along * length, hand


def segment_distance(a0, a1, b0, b1):
    """The shortest distance between two line segments."""
    u, v, w = a1 - a0, b1 - b0, a0 - b0
    a, b, c, d, e = u @ u, u @ v, v @ v, u @ w, v @ w
    den = a * c - b * b

    s = np.clip((b * e - c * d) / den, 0.0, 1.0) if den > 1e-9 else 0.0
    t = np.clip((b * s + e) / c, 0.0, 1.0) if c > 1e-9 else 0.0
    s = np.clip((b * t - d) / a, 0.0, 1.0) if a > 1e-9 else 0.0

    return float(np.linalg.norm((a0 + u * s) - (b0 + v * t)))


def clearance(joints, names, length=0.75):
    """How far the weapon's line stays from the middle of the neck and head at each frame, in metres.
    A neck is about 6 cm from its middle to its skin and a head about 10, a rebar club 2: under about 0.11 it is in the flesh."""
    low, high, _ = weapon_line(joints, names, length)

    neck = joints[:, names.index("Neck1")]
    head = joints[:, names.index("Head")]

    # the skull's middle is above the joint the head turns on
    up = head - neck
    up = up / np.linalg.norm(up, axis=1, keepdims=True)
    crown = head + up * 0.13

    return np.array([segment_distance(low[f], high[f], neck[f], crown[f]) for f in range(joints.shape[0])])


def hold(names_wanted, length):
    """Prints, for each sample of the clips named, how close the weapon's line comes to the neck and head."""
    names, _ = skeleton_tables()

    for clip_name in names_wanted:
        for index, sample in enumerate(load_samples(clip_name)):
            near = clearance(sample["posed_joints"].astype(np.float64), names, length)
            low, high, hand = weapon_line(sample["posed_joints"].astype(np.float64), names, length)
            tip = (high - low)[near.argmin()]

            print("HOLD {} s{:02d}: nearest {:.3f} m at frame {}, first frame {:.3f}, mean {:.3f}; frames clear by 0.13: {}/{}; best frame {} at {:.3f}".format(
                clip_name, index, near.min(), int(near.argmin()), near[0], near.mean(), int((near > 0.13).sum()), near.shape[0], int(near.argmax()), near.max()))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["generate", "pose", "sheet", "export", "pick", "hold", "names"])
    parser.add_argument("rest", nargs="*")

    args = parser.parse_args()

    if args.action == "generate":
        generate(args.rest)

    elif args.action == "export":
        export(args.rest)

    elif args.action == "pick":
        autopick([name for name in args.rest if name != "again"], again="again" in args.rest)

    elif args.action == "pose":
        pose_name, clip_name, sample, frame = args.rest[:4]

        keep_pose(pose_name, clip_name, int(sample), int(frame), face="asis" not in args.rest[4:])

    elif args.action == "hold":
        hold(args.rest, float(os.environ.get("GUT_WEAPON_LENGTH", "0.75")))

    elif args.action == "names":
        print(skeleton_tables()[0])

    elif args.action == "sheet":
        bone_names, parents = skeleton_tables()

        numbers = [int(value) for value in args.rest[1:]] + [None] * 4
        every, only, first, last = numbers[:4]

        print(draw_sheet(args.rest[0], load_samples(args.rest[0]), bone_names, parents, every or None, only, first or 0, last))


if __name__ == "__main__":
    main()
