"""
Cuts the Gut's sounds out of the recordings in ArtSource/sound and gets them ready for the engine.

    <a python with numpy> Tools/prepare_gut_sound.py [kind ...]        (Tools/prepare_gut_sound.sh finds one)
    <a python with numpy> Tools/prepare_gut_sound.py --look <file>     list the separate sounds found in one recording

Every sound in the game is a recording of the real thing (ArtSource/sound/manifest.json says where each came from and
under what licence). Nothing is synthesised here. What this does to a recording is what an editor does with a razor:
  - finds the separate sounds in it, where it holds several, and cuts each out
  - trims the silence off the front and the tail, with a fade a few thousandths of a second long so the cut does not click
  - where a cut is to hold one blow and no more ("single"), cuts off whatever follows or comes before the loudest in it
  - brings every cut to the same peak level, so that how loud a kind of sound is in the game is set in one place
  - for something that plays without end, takes a stretch and folds its tail into its head so that the join cannot be heard
Tools/gut_sounds.json says which recording becomes which kind of sound and how. The cuts are written to
ArtSource/sound/prepared/<kind>/<kind>_NN.wav at 48 kHz, with prepared.json beside them listing what each was cut from.
"""

import json
import pathlib
import subprocess
import sys
import wave

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "ArtSource/sound"
OUT = SOURCE / "prepared"
PLAN = ROOT / "Tools/gut_sounds.json"

RATE = 48000


def read(path, channels):
    """A recording as numbers between -1 and 1, at 48 kHz, in one channel or two."""
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "f32le", "-ac", str(channels), "-ar", str(RATE), "-"],
                         capture_output=True, check=True).stdout

    return np.frombuffer(raw, dtype=np.float32).reshape(-1, channels).astype(np.float64)


def write(path, sound):
    path.parent.mkdir(parents=True, exist_ok=True)

    with wave.open(str(path), "wb") as out:
        out.setnchannels(sound.shape[1])
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((np.clip(sound, -1.0, 1.0) * 32767.0).astype(np.int16).tobytes())


def loudness(sound, window=0.005):
    """How loud the recording is in each 5 thousandths of a second, in decibels below full."""
    size = int(window * RATE)
    count = sound.shape[0] // size
    power = (sound[: count * size] ** 2).mean(axis=1).reshape(count, size).mean(axis=1)

    return 10.0 * np.log10(power + 1e-12), size


def find_events(sound, shortest=0.04, longest=3.0, apart=0.12):
    """The separate sounds in a recording: where each begins and ends, in samples, and how loud it gets."""
    level, size = loudness(sound)

    if level.size == 0:
        return []

    quiet = np.percentile(level, 15)
    top = level.max()

    # a sound starts where the level comes well up out of the quiet, and lasts until it has sunk most of the way back
    start_at = max(quiet + 16.0, top - 32.0)
    end_at = max(quiet + 6.0, top - 48.0)

    events = []
    index = 0

    while index < level.size:
        if level[index] < start_at:
            index += 1
            continue

        first = index

        # back to where it first lifted off the quiet
        while first > 0 and level[first - 1] > end_at and index - first < int(0.05 * RATE / size):
            first -= 1

        last = index
        below = 0

        while last < level.size - 1 and (last - first) * size < longest * RATE:
            last += 1
            below = below + 1 if level[last] < end_at else 0

            if below * size >= apart * RATE:
                last -= below
                break

        if (last - first) * size >= shortest * RATE:
            events.append((first * size, min((last + 1) * size, sound.shape[0]), float(level[first: last + 1].max())))

        index = last + int(apart * RATE / size) + 1

    return events


def trim(sound, lead=0.006, floor=-52.0):
    """A sound without the silence before it and after it."""
    level, size = loudness(sound, 0.002)

    if level.size == 0:
        return sound

    heard = np.nonzero(level > max(level.max() + floor, -75.0))[0]

    if heard.size == 0:
        return sound

    first = max(0, heard[0] * size - int(lead * RATE))
    last = min(sound.shape[0], (heard[-1] + 1) * size + int(0.03 * RATE))

    return sound[first:last]


def one_blow(sound, drop=12.0, rise=8.0, clear=0.05, within=18.0):
    """
    The one loudest blow in a cut, without whatever else was caught with it: a footfall's heel without the slap of the sole
    that follows it, or without the end of the step before. Cut where the level is lowest between the two, like the rest.
    Something else is a lift of at least `rise` decibels out of a hollow `drop` below the blow, to within `within` of it:
    a blow dying away wavers by less than that. Returns the cut and whether anything was taken off its end.
    """
    level, size = loudness(sound, 0.008)

    if level.size < 8:
        return sound, False

    peak = int(level.argmax())
    step = max(1, int(clear * RATE / size))

    def other(indices):
        lowest = None

        for index in indices:
            if lowest is None or level[index] < level[lowest]:
                lowest = index

            if level[lowest] <= level[peak] - drop and level[index] >= level[lowest] + rise and level[index] >= level[peak] - within:
                return lowest

        return None

    # forward from the blow, and back from it the same way
    after = other(range(peak + step, level.size))
    before = other(range(peak - step, -1, -1))

    first = 0 if before is None else before
    last = level.size if after is None else after

    return sound[first * size: min(last * size, sound.shape[0])], after is not None


def fade(sound, start=0.003, end=0.04):
    result = sound.copy()

    rise = min(int(start * RATE), result.shape[0] // 2)
    fall = min(int(end * RATE), result.shape[0] // 2)

    if rise > 0:
        result[:rise] *= np.linspace(0.0, 1.0, rise)[:, None]

    if fall > 0:
        result[-fall:] *= np.linspace(1.0, 0.0, fall)[:, None]

    return result


def level_to(sound, peak=-1.0):
    top = np.abs(sound).max()

    return sound if top < 1e-6 else sound * (10.0 ** (peak / 20.0) / top)


def make_loop(sound, join=0.75):
    """A stretch that can be played end to end for ever: its last part is folded under its first, each fading as the other comes up."""
    fold = min(int(join * RATE), sound.shape[0] // 4)
    body = sound[: sound.shape[0] - fold].copy()
    tail = sound[sound.shape[0] - fold:]

    # equal power, so the join is as loud as the rest
    mix = np.linspace(0.0, 1.0, fold)[:, None]

    body[:fold] = body[:fold] * np.sin(mix * np.pi / 2.0) + tail * np.cos(mix * np.pi / 2.0)

    return body


def steadiest(sound, seconds):
    """The stretch of that length whose loudness changes least: the part of a long recording with nothing going on in it but the thing itself."""
    level, size = loudness(sound, 0.25)
    span = int(seconds * RATE / size)

    if level.size <= span:
        return 0

    best = min(range(0, level.size - span, 2), key=lambda first: level[first: first + span].std() - 0.05 * level[first: first + span].mean())

    return best * size


def prepare(kinds):
    plan = json.loads(PLAN.read_text())
    report = {}

    # made afresh each time: a cut that is no longer wanted must not be left lying about to be brought in
    for kind in plan["kinds"]:
        if (not kinds or kind in kinds) and (OUT / kind).is_dir():
            for old in (OUT / kind).glob("*.wav"):
                old.unlink()

    for kind, how in plan["kinds"].items():
        if kinds and kind not in kinds:
            continue

        channels = 2 if how.get("everywhere") else 1
        loops = bool(how.get("loop"))
        made = []

        for source in how.get("from", []):
            path = SOURCE / source["file"]

            if not path.exists():
                print("MISSING {}: {}".format(kind, source["file"]))
                continue

            sound = read(path, channels)

            if "span" in source:
                sound = sound[int(source["span"][0] * RATE): int(source["span"][1] * RATE)]

            cuts = []

            if loops:
                seconds = float(source.get("seconds", 12.0))
                first = int(source["at"] * RATE) if "at" in source else steadiest(sound, seconds + 0.75)

                cuts.append((make_loop(sound[first: first + int((seconds + 0.75) * RATE)]), "{:.1f} s from {:.1f} s".format(seconds, first / RATE)))

            elif source.get("cut") == "events":
                found = find_events(sound, float(source.get("shortest", 0.04)), float(source.get("longest", 3.0)), float(source.get("apart", 0.12)))

                # the loudest are the cleanest: the quiet ones are mostly handling noise
                keep = sorted(sorted(found, key=lambda event: -event[2])[: int(source.get("most", 4))])

                for first, last, top in keep:
                    # a sound cut off at the longest it may be was still ringing: it is faded out over its last quarter, not chopped
                    capped = (last - first) >= float(source.get("longest", 3.0)) * RATE * 0.98
                    tail = float(source.get("fade", max(0.1, 0.25 * (last - first) / RATE) if capped else 0.04))

                    cut = trim(sound[first:last])

                    # "single": one blow to a cut. What is taken off the end of it is faded short, since the next thing was coming.
                    if source.get("single"):
                        cut, shortened = one_blow(cut)
                        tail = 0.02 if shortened else tail

                    cuts.append((fade(cut, end=tail), "{:.2f} to {:.2f} s".format(first / RATE, last / RATE)))

            else:
                whole = trim(sound)

                capped = "longest" in source and whole.shape[0] > int(float(source["longest"]) * RATE)

                if capped:
                    whole = whole[: int(float(source["longest"]) * RATE)]

                cuts.append((fade(whole, end=float(source.get("fade", max(0.1, 0.25 * whole.shape[0] / RATE) if capped else 0.04))), "whole"))

            for cut, where in cuts:
                if cut.shape[0] < int(0.06 * RATE):
                    continue

                name = "{}_{:02d}.wav".format(kind, len(made) + 1)

                write(OUT / kind / name, level_to(cut, float(how.get("peak", -1.0))))

                made.append({"file": "{}/{}".format(kind, name), "from": source["file"], "cut": where, "seconds": round(cut.shape[0] / RATE, 3)})

        report[kind] = made

        print("{}: {} sounds, {}".format(kind, len(made), ", ".join("{:.2f} s".format(entry["seconds"]) for entry in made) or "NONE"))

    known = json.loads((OUT / "prepared.json").read_text()) if (OUT / "prepared.json").exists() else {}
    known.update(report)

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "prepared.json").write_text(json.dumps(known, indent=1) + "\n")


def look(file):
    sound = read(SOURCE / file, 1)
    level, _ = loudness(sound)

    print("{}: {:.2f} s, peak {:.1f} dB, quiet {:.1f} dB".format(file, sound.shape[0] / RATE, 20.0 * np.log10(np.abs(sound).max() + 1e-9), np.percentile(level, 15)))

    for first, last, top in find_events(sound):
        print("  {:7.2f} to {:7.2f} s  ({:.2f} s)  {:.1f} dB".format(first / RATE, last / RATE, (last - first) / RATE, top))


if len(sys.argv) > 2 and sys.argv[1] == "--look":
    for name in sys.argv[2:]:
        look(name)
else:
    prepare(sys.argv[1:])
