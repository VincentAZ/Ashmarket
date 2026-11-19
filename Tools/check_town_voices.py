"""Reads back what the town's people were recorded saying, with a speech recogniser, and says where it differs from the words.

    <a python with transformers and torch> Tools/check_town_voices.py

Nobody who works on this from a terminal can hear. This is the next best thing: every recording in ArtSource/voices is
written down again by Whisper and set beside the line it was made from (ArtSource/voices/manifest.json). A line that comes
back with words missing or changed is listed, with how long it runs. It cannot say whether a voice suits who speaks.
"""
import difflib
import json
import os
import re
import subprocess
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(PROJECT, "ArtSource", "voices")


NUMBERS = {"zero": 0, "one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10, "eleven": 11,
           "twelve": 12, "thirteen": 13, "fourteen": 14, "fifteen": 15, "sixteen": 16, "seventeen": 17, "eighteen": 18, "nineteen": 19, "twenty": 20,
           "thirty": 30, "forty": 40, "fifty": 50, "sixty": 60, "seventy": 70, "eighty": 80, "ninety": 90}


def plain(text):
    """The words alone, as either the writer or the recogniser would put them: figures as digits, litres however spelt."""
    words = re.sub(r"[^a-z0-9' ]+", " ", text.lower().replace("’", "'").replace("-", " ")).split()
    out = []

    for word in words:
        word = {"liters": "litres", "liter": "litre", "mr": "mister", "armour": "armor"}.get(word, word)

        if word in NUMBERS:
            # "forty two" is one number
            if out and out[-1].isdigit() and int(out[-1]) % 10 == 0 and 20 <= int(out[-1]) < 100 and NUMBERS[word] < 10:
                out[-1] = str(int(out[-1]) + NUMBERS[word])
            else:
                out.append(str(NUMBERS[word]))
        else:
            out.append(word)

    return out


def main():
    import numpy as np
    from transformers import pipeline

    manifest = json.load(open(os.path.join(OUT, "manifest.json")))
    hear = pipeline("automatic-speech-recognition", model="openai/whisper-large-v3-turbo", device=0)

    worst = []
    total = 0.0

    for file, entry in sorted(manifest.items()):
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", os.path.join(OUT, file), "-f", "f32le", "-ac", "1", "-ar", "16000", "-"], capture_output=True, check=True).stdout
        sound = np.frombuffer(raw, dtype=np.float32)
        seconds = sound.shape[0] / 16000.0
        total += seconds

        heard = hear({"raw": sound, "sampling_rate": 16000}, generate_kwargs={"language": "en"})["text"].strip()
        alike = difflib.SequenceMatcher(None, plain(entry["words"]), plain(heard)).ratio()
        level = 20.0 * np.log10(np.abs(sound).max() + 1e-9)

        worst.append((alike, file, seconds, level, entry["words"], heard))

    worst.sort()

    print("{} recordings, {:.0f} s in all".format(len(worst), total))

    for alike, file, seconds, level, words, heard in worst:
        if alike < 0.9:
            print("\n{:.2f}  {}  ({:.1f} s, peak {:.0f} dB)\n   written: {}\n   heard:   {}".format(alike, file, seconds, level, words, heard))

    print("\n{} of {} came back as written (nine words in ten or better)".format(sum(1 for item in worst if item[0] >= 0.9), len(worst)))
    print("longest {:.1f} s, shortest {:.1f} s, quietest peak {:.0f} dB".format(max(item[2] for item in worst), min(item[2] for item in worst), min(item[3] for item in worst)))


main()
