#!/usr/bin/env python3
"""
Cuts stretches out of the films in for-review and joins them into one short film, for-review/tonight_in_three_minutes.mp4.
Each stretch keeps its own sound and carries the name of the film it is from, small, in a corner. It makes nothing: ffmpeg cuts
and joins what Tools/film_gut_fight.sh filmed.

  python3 Tools/make_gut_reel.py

Tools/make_gut_review.sh runs this after it has made the films. A film made again is a different fight, so the stretches
below are either counted back from a film's end (its last blows and what follows) or early enough to be in any take; for
a better cut, look at the films and choose again.
"""
import subprocess, sys, os

REVIEW = "/home/vincent/Repos/Ashmarket/for-review"

def length(name):
    return float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", os.path.join(REVIEW, name)]).decode())

# film, start, end: a negative number counts back from the end of the film
CUTS = [
    ("fight_chain_v_club_last.mp4", 24, 44),
    ("fight_shield_v_machete_last.mp4", -18, -1),
    ("fight_against_the_dogs.mp4", 3, 19),
    ("fight_everything_the_pit_boss_has.mp4", 48, 68),
    ("fight_with_a_dog_at_heel.mp4", 8, 26),
    ("fight_two_on_two.mp4", -24, -4),
    ("fight_lost_to_the_dogs.mp4", -13, -2),
    ("fight_spared.mp4", -11.5, -0.2),
    ("fight_fists_v_fists_last.mp4", -15, -1),
]

inputs, parts, joins = [], [], []

for index, (name, start, end) in enumerate(CUTS):
    whole = length(name)
    start = whole + start if start < 0 else start
    end = whole + end if end < 0 else end

    # a film made again may be shorter than the one this list was written for: whatever is asked for is kept inside it
    end = min(end, whole - 0.1)
    start = max(0.0, min(start, end - 6.0))
    inputs += ["-ss", "{:.2f}".format(start), "-t", "{:.2f}".format(end - start), "-i", os.path.join(REVIEW, name)]
    parts.append("[{i}:v]fps=30,scale=1600:900,setsar=1,drawtext=text='{t}':x=24:y=h-44:fontsize=22:fontcolor=white@0.75:box=1:boxcolor=black@0.45:boxborderw=6,fade=t=in:st=0:d=0.25[v{i}];[{i}:a]aresample=48000,afade=t=in:st=0:d=0.2,afade=t=out:st={o:.2f}:d=0.25[a{i}]".format(
        i=index, t=name.replace(".mp4", "").replace("_", " "), o=max(end - start - 0.25, 0)))
    joins.append("[v{i}][a{i}]".format(i=index))

graph = ";".join(parts) + ";" + "".join(joins) + "concat=n={}:v=1:a=1[v][a]".format(len(CUTS))

subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error"] + inputs + ["-filter_complex", graph, "-map", "[v]", "-map", "[a]",
                       "-c:v", "libx264", "-crf", "20", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", os.path.join(REVIEW, "tonight_in_three_minutes.mp4")])
print("{:.0f} s".format(length("tonight_in_three_minutes.mp4")))
