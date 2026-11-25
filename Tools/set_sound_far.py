"""
Sets recordings far off: the rumble under them taken away, their top taken off as distance and air take it off, and what
comes back off walls a moment later laid in. For the night's far gunfire and far screams, which were recorded nearer than
they are to be heard. Each result is written beside its recording as <name>_far.flac and into ArtSource/sound/manifest.json
with what was done to it.

    python3 Tools/set_sound_far.py far/shot_single_distant_pistol_preview.mp3=1800 far/scream_woman_2_preview.mp3=1400:5.0
        file under ArtSource/sound = the frequency above which it is taken off, in Hz [: the longest it may be, in seconds]
"""

import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOUND = ROOT / "ArtSource/sound"


def main():
    manifest_path = SOUND / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    known = {entry["file"]: entry for entry in manifest}

    for arg in sys.argv[1:]:
        file, how = arg.split("=", 1)
        top = float(how.split(":")[0])
        longest = float(how.split(":")[1]) if ":" in how else None

        source = SOUND / file
        result = file.replace("_preview.mp3", "").rsplit(".", 1)[0] + "_far.flac"

        chain = ["highpass=f=90", "lowpass=f={}".format(top), "lowpass=f={}".format(top)]

        if longest:
            chain += ["atrim=end={}".format(longest), "afade=t=out:st={}:d=0.5".format(longest - 0.5)]

        # what comes back off a wall 45, 100 and 180 metres away and back, each fainter and duller
        chain += ["apad=pad_dur=1.4", "aecho=0.85:0.6:130|290|520:0.30|0.19|0.10"]

        subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-ac", "1", "-ar", "48000", "-af", ",".join(chain), str(SOUND / result)], check=True)

        entry = dict(known.get(file, {}))
        entry.update({
            "file": result,
            "is_preview": False,
            "format": "flac",
            "channels": 1,
            "sample_rate": 48000,
            "bytes": (SOUND / result).stat().st_size,
            "is_original_file": False,
            "derivation": "Made from {} (freesound's preview of the same sound) by Tools/set_sound_far.py: nothing under 90 Hz, nothing much over {:.0f} Hz{}, and three fainter returns 0.13, 0.29 and 0.52 s after it, so that it is heard as from far off.".format(
                file, top, ", cut at {:.1f} s".format(longest) if longest else ""),
        })

        manifest = [one for one in manifest if one["file"] != result] + [entry]

        print("made", result)

    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n")


main()
