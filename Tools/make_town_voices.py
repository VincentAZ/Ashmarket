"""Has ElevenLabs speak what the town's people say.

    Tools/make_town_voices.sh            every line that has no recording yet, or whose words or voice have changed
    Tools/make_town_voices.sh --list     what would be made, and how many characters it comes to: nothing is asked of ElevenLabs
    Tools/make_town_voices.sh --only "Block Square/Margin"     one speaker
    Tools/make_town_voices.sh --force    everything again

Reads the three places' lists (Tools/town_*.json) and Tools/town_voices.json, which says who speaks with which voice.
A line is whatever somebody says to the player, and each call from the Block. A line with a figure written into it as it
is said ({held}, {owed}, {dealt}) is left as words: it is different every time.

Each recording is ArtSource/voices/<place>/<name>.mp3, and ArtSource/voices/manifest.json keeps what was said, by which
voice, so that a line is only made again when it has changed. Tools/import_town_voices.sh brings them into the project.

The key is never in this file: the ElevenLabs client reads ELEVENLABS_API_KEY from the environment, which is filled
from .env.local (git never sees that file) just before the client is made.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LISTS = sorted(glob.glob(os.path.join(PROJECT, "Tools", "town_*.json")))
CAST = os.path.join(PROJECT, "Tools", "town_voices.json")
OUT = os.path.join(PROJECT, "ArtSource", "voices")
MANIFEST = os.path.join(OUT, "manifest.json")


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def lines_of(listed):
    """Everything said aloud in a place: (speaker as the cast names them, the recording's name, the words)."""
    place = listed["name"]
    seen = {}
    found = []

    for person in listed.get("people", []):
        if not person.get("lines"):
            continue

        # two people of one name in a place: the second is Name#2
        seen[person["name"]] = seen.get(person["name"], 0) + 1
        nth = seen[person["name"]]
        speaker = "{}/{}{}".format(place, person["name"], "" if nth == 1 else "#{}".format(nth))

        for index, line in enumerate(person["lines"]):
            if "{" in line["words"]:
                continue

            name = "{}{}_{}".format(slug(person["name"]), "" if nth == 1 else str(nth), slug(str(line.get("id", index))))
            found.append((speaker, name, line["words"]))

    for index, call in enumerate(listed.get("calls", [])):
        if call.get("words") and call.get("speaker") and "{" not in call["words"]:
            found.append(("{}/{}".format(place, call["speaker"]), "call_{:02d}".format(index), call["words"]))

    return place, found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--only", default="")
    options = parser.parse_args()

    cast = json.load(open(CAST))
    speakers = cast["speakers"]
    manifest = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else {}

    def voice_of(speaker):
        entry = speakers.get(speaker)

        if entry and entry.get("same_as"):
            entry = speakers.get(entry["same_as"])

        return entry

    wanted = []
    silent = []

    for path in LISTS:
        listed = json.load(open(path))

        if "people" not in listed or "name" not in listed:
            continue

        place, found = lines_of(listed)

        for speaker, name, words in found:
            if options.only and speaker != options.only:
                continue

            entry = voice_of(speaker)

            if not entry:
                silent.append(speaker)
                continue

            settings = entry.get("settings", cast["settings"])
            file = os.path.join(slug(place), name + ".mp3")
            mark = hashlib.sha1(json.dumps([words, entry["voice"], cast["model"], settings], sort_keys=True).encode()).hexdigest()

            if options.force or manifest.get(file, {}).get("mark") != mark or not os.path.exists(os.path.join(OUT, file)):
                wanted.append((file, speaker, entry["voice"], settings, words, mark))

    for speaker in sorted(set(silent)):
        print("no voice is given for {}: their lines stay words (Tools/town_voices.json)".format(speaker))

    print("{} lines to make, {} characters".format(len(wanted), sum(len(item[4]) for item in wanted)))

    if options.list or not wanted:
        for file, speaker, voice, _, words, _ in wanted:
            print("  {:44s} {:34s} {:9s} {}".format(file, speaker, voice, words[:60]))

        return

    # only now is anything asked of ElevenLabs. The key comes out of .env.local into the environment, and from there
    # the client takes it.
    from dotenv import load_dotenv
    from elevenlabs import ElevenLabs, VoiceSettings

    load_dotenv(os.path.join(PROJECT, ".env.local"))

    client = ElevenLabs()

    # the account's voices by the first part of their names: "Laura - Enthusiast, Quirky Attitude" is Laura
    voices = {voice.name.split(" - ")[0].strip(): voice.voice_id for voice in client.voices.get_all().voices}

    missing = sorted({item[2] for item in wanted} - set(voices))

    if missing:
        sys.exit("the account has no voice called {}: it has {}".format(", ".join(missing), ", ".join(sorted(voices))))

    made = 0

    for file, speaker, voice, settings, words, mark in wanted:
        os.makedirs(os.path.dirname(os.path.join(OUT, file)), exist_ok=True)

        audio = client.text_to_speech.convert(text=words, voice_id=voices[voice], model_id=cast["model"], output_format=cast["format"],
                                              voice_settings=VoiceSettings(stability=settings["stability"], similarity_boost=settings["similarity_boost"]))

        with open(os.path.join(OUT, file), "wb") as out:
            for chunk in audio:
                out.write(chunk)

        manifest[file] = {"speaker": speaker, "voice": voice, "voice_id": voices[voice], "model": cast["model"], "settings": settings, "words": words, "mark": mark,
                          "made_with": "ElevenLabs text to speech"}

        # written after every line, so that a run that stops half way has lost nothing it paid for
        with open(MANIFEST, "w") as out:
            json.dump(manifest, out, indent=1, sort_keys=True)

        made += 1
        print("[{}/{}] {}  ({}, {} characters)".format(made, len(wanted), file, voice, len(words)))

    print("made {} recordings in {}".format(made, OUT))


main()
