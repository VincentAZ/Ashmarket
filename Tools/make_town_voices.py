"""Has ElevenLabs speak what the town's people say.

    Tools/make_town_voices.sh            every line that has no recording yet, or whose words or voice have changed
    Tools/make_town_voices.sh --list     what would be made, and how many characters it comes to: nothing is asked of ElevenLabs
    Tools/make_town_voices.sh --only "Block Square/Margin"     one speaker
    Tools/make_town_voices.sh --force    everything again
    Tools/make_town_voices.sh --only "Main Street/Brass" --redesign     a new voice for one speaker, and their lines again

Reads the three places' lists (Tools/town_*.json) and Tools/town_voices.json, which says who speaks with which voice.
A speaker with a 'design' there has a voice made for them from that description (Vincent's choice of 2026-11-19, of
seven ways tried); which voice it made is kept in Tools/town_voices_designed.json. The account holds only a few voices
at once, so each is saved, speaks, and is let go when the room is wanted: those named under 'keep' are made last and
so are left saved.
A line is whatever somebody says to the player, and each call from the Block. A line with a figure written into it as it
is said ({held}, {owed}, {dealt}) is left as words: it is different every time.

Each recording is ArtSource/voices/<place>/<name>.mp3, and ArtSource/voices/manifest.json keeps what was said, by which
voice, so that a line is only made again when it has changed. Tools/import_town_voices.sh brings them into the project.

The key is never in this file: the ElevenLabs client reads ELEVENLABS_API_KEY from the environment, which is filled
from .env.local (git never sees that file) just before the client is made.
"""
import argparse
import base64
import glob
import hashlib
import json
import os
import re
import sys

import town_lists

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAST = os.path.join(PROJECT, "Tools", "town_voices.json")
DESIGNED = os.path.join(PROJECT, "Tools", "town_voices_designed.json")
LISTS = sorted(path for path in glob.glob(os.path.join(PROJECT, "Tools", "town_*.json")) if path not in (CAST, DESIGNED))
OURS = "Ashmarket: "
OUT = os.path.join(PROJECT, "ArtSource", "voices")
MANIFEST = os.path.join(OUT, "manifest.json")


# A voice that has lost its low end sounds as if it came out of a loudspeaker or an old radio ("speaker voice", 25 and 26
# November: Margin, Dolores, Pike). Measured, those three have under 3% of their sound below 300 Hz, where a voice that
# sounds like somebody in the room has 8 to 40%. So every voice that is designed is measured, and one under FULL is not kept.
FULL = 0.07

# and a speaker whose recordings have less than this is given a new voice by --thin --redesign. Lower than FULL: between the
# two are voices he has heard many times and said nothing against (Mother Ledger, Hasp), which are left as they are.
THIN = 0.036


def fullness(path):
    """The share of a recording's sound that lies between 80 and 300 Hz: its body. None if it cannot be measured."""
    import subprocess

    def mean(filters):
        out = subprocess.run(["ffmpeg", "-nostdin", "-i", path, "-af", filters + "volumedetect", "-f", "null", "-"], capture_output=True, text=True).stderr
        found = re.search(r"mean_volume: (-?[0-9.]+) dB", out)

        return float(found.group(1)) if found else None

    whole = mean("highpass=f=80,lowpass=f=8000,")
    low = mean("highpass=f=80,highpass=f=80,lowpass=f=300,lowpass=f=300,")

    if whole is None or low is None:
        return None

    return min(1.0, 10.0 ** ((low - whole) / 10.0))


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def saved_names(client):
    return [voice.name for voice in client.voices.get_all().voices]


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

        # and what they say when they are walked into (Tools/town_bumps.json)
        for index, words in enumerate(person.get("bumped", [])):
            found.append((speaker, "{}{}_bump_{}".format(slug(person["name"]), "" if nth == 1 else str(nth), index + 1), words))

    for index, call in enumerate(listed.get("calls", [])):
        if call.get("words") and call.get("speaker") and "{" not in call["words"]:
            found.append(("{}/{}".format(place, call["speaker"]), "call_{:02d}".format(index), call["words"]))

    # what a caller says while a lot is bid for, in the pieces it is said in: each a recording of its own
    bidding = listed.get("bidding") or {}

    for key, words in (bidding.get("clips") or {}).items():
        found.append(("{}/{}".format(place, bidding["speaker"]), "bid_" + key, words))

    return place, found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--redesign", action="store_true")
    parser.add_argument("--only", default="")
    parser.add_argument("--usage", action="store_true", help="what the account has used and is allowed, and nothing else")
    parser.add_argument("--thin", action="store_true", help="measure every recording and name the speakers whose voices are thin; with --redesign, give those new voices")
    options = parser.parse_args()

    if options.redesign and not options.only and not options.thin:
        sys.exit("--redesign gives a speaker a new voice: name the speaker with --only, or give every thin voice a new one with --thin")

    cast = json.load(open(CAST))
    speakers = cast["speakers"]
    manifest = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else {}
    designed = json.load(open(DESIGNED)) if os.path.exists(DESIGNED) else {}

    def voice_of(speaker):
        """Whose voice a speaker has (Margin at the table is Margin), and what the cast says of it."""
        entry = speakers.get(speaker)

        if entry and entry.get("same_as"):
            speaker = entry["same_as"]
            entry = speakers.get(speaker)

        return speaker, entry

    wanted = []
    silent = []
    said = {}
    every = []

    # whoever is to have a new voice: the speaker named, or (--thin --redesign) everybody whose voice measures thin
    again = set()

    for path in LISTS:
        # with whatever each piece of work has its people say there (Tools/town_lists.py)
        listed = town_lists.place(path)

        if "people" not in listed or "name" not in listed:
            continue

        place, found = lines_of(listed)

        for speaker, name, words in found:
            whose, entry = voice_of(speaker)

            if not entry:
                silent.append(speaker)
                continue

            said.setdefault(whose, []).append(words)

            if options.only and whose != options.only and speaker != options.only:
                continue

            settings = entry.get("settings", cast["settings"])
            # uncompressed where the cast asks for it ("format": "pcm_44100", which the account's plan has to allow): a
            # WAV file, and not the same recording as the MP3 there was of the same words
            pcm = cast["format"].startswith("pcm_")
            file = os.path.join(slug(place), name + (".wav" if pcm else ".mp3"))
            voice = entry.get("design") or entry["voice"]
            mark = hashlib.sha1(json.dumps([words, voice, cast["model"], settings] + ([cast["format"]] if pcm else []), sort_keys=True).encode()).hexdigest()

            every.append((file, speaker, whose, entry, settings, words, mark))

    if options.thin:
        # each speaker's recordings measured: the middle one of them is the speaker's
        heard = {}

        for file, speaker, whose, entry, _, _, _ in every:
            if os.path.exists(os.path.join(OUT, file)) and entry.get("design") and len(heard.setdefault(whose, [])) < 7:
                body = fullness(os.path.join(OUT, file))

                if body is not None:
                    heard[whose].append(body)

        print("{:46s} below 300 Hz".format("speaker"))

        for whose, bodies in sorted(heard.items(), key=lambda item: sorted(item[1])[len(item[1]) // 2] if item[1] else 1.0):
            if not bodies:
                continue

            middle = sorted(bodies)[len(bodies) // 2]
            meant = speakers[whose].get("thin_is_right")
            thin = middle < THIN and not meant

            print("{:46s} {:5.1%}  {}".format(whose, middle, "meant to be" if meant and middle < THIN else ("THIN" if thin else "")))

            if thin:
                again.add(whose)

        print("{} speakers are thin".format(len(again)))

        if not options.redesign:
            return
    elif options.redesign:
        again = {voice_of(options.only)[0]}

    for file, speaker, whose, entry, settings, words, mark in every:
        if options.force or whose in again or manifest.get(file, {}).get("mark") != mark or not os.path.exists(os.path.join(OUT, file)):
            wanted.append((file, speaker, whose, entry, settings, words, mark))

    for speaker in sorted(set(silent)):
        print("no voice is given for {}: their lines stay words (Tools/town_voices.json)".format(speaker))

    # those whose voices are to stay saved come last
    keep = cast.get("keep", [])
    wanted.sort(key=lambda item: (keep.index(item[2]) if item[2] in keep else -1, item[2], item[0]))

    print("{} lines to make, {} characters".format(len(wanted), sum(len(item[5]) for item in wanted)))

    if (options.list or not wanted) and not options.usage:
        for file, speaker, whose, entry, _, words, _ in wanted:
            print("  {:44s} {:34s} {:9s} {}".format(file, speaker, "designed" if entry.get("design") else entry["voice"], words[:60]))

        return

    # only now is anything asked of ElevenLabs. The key comes out of .env.local into the environment, and from there
    # the client takes it.
    from dotenv import load_dotenv

    # before the SDK is so much as imported: it looks for the key once, as it is read in
    load_dotenv(os.path.join(PROJECT, ".env.local"))

    from elevenlabs import ElevenLabs, VoiceSettings

    if not os.environ.get("ELEVENLABS_API_KEY"):
        sys.exit("there is no ELEVENLABS_API_KEY in .env.local: run npx -y @elevenlabs/cli@latest onboard init")

    client = ElevenLabs(api_key=os.environ["ELEVENLABS_API_KEY"])

    if options.usage:
        # what the plan allows and how much of it is gone: asked for when a limit is met, to say when it comes back
        plan = client.user.subscription.get()

        for name in ("tier", "status", "character_count", "character_limit", "voice_slots_used", "voice_limit", "voice_add_edit_counter",
                     "max_voice_add_edits", "professional_voice_slots_used", "professional_voice_limit", "next_character_count_reset_unix", "billing_period"):
            value = getattr(plan, name, None)

            if value is not None:
                print("{}: {}".format(name, value))

        reset = getattr(plan, "next_character_count_reset_unix", None)

        if reset:
            import datetime

            print("resets: {}".format(datetime.datetime.fromtimestamp(reset).strftime("%Y-%m-%d %H:%M")))

        print("saved now: {}".format(", ".join(sorted(name for name in saved_names(client)))))
        return

    def saved():
        return {voice.voice_id: voice.name for voice in client.voices.get_all().voices}

    def design(speaker, entry):
        """A new voice for somebody, from what the cast says of them: heard first saying their own lines."""
        text = entry.get("design_text") or " ".join(said[speaker])

        if len(text) < 100:
            sys.exit("{} says under a hundred characters: give them a 'design_text' of a hundred or more in Tools/town_voices.json".format(speaker))

        # no more than a thousand, and whole sentences
        while len(text) > 1000:
            text = text[:text.rstrip(".?! ").rfind(". ") + 1]

        os.makedirs(os.path.join(OUT, "designed"), exist_ok=True)

        # Every voice offered is listened to with a meter, and the fullest is kept: asked for again, up to six times, until
        # one of them has the body of somebody in the room (FULL). Whatever the description says, about one voice in three
        # comes back thin.
        best = None

        for asked in range(6):
            # (asked the same way twice, the same three voices come back: each asking after the first has a seed of its own)
            if asked == 0:
                made = client.text_to_voice.design(voice_description=entry["design"], model_id=cast["design_model"], text=text)
            else:
                made = client.text_to_voice.design(voice_description=entry["design"], model_id=cast["design_model"], text=text, seed=(asked * 1009) + 11)

            for number, preview in enumerate(made.previews):
                heard = os.path.join(OUT, "designed", "{}_try{}_{}.mp3".format(slug(speaker), asked, number))

                with open(heard, "wb") as out:
                    out.write(base64.b64decode(preview.audio_base_64))

                body = fullness(heard)

                print("  {} voice {}.{}: {} of its sound is below 300 Hz".format(speaker, asked + 1, number + 1, "?" if body is None else "{:.0%}".format(body)))

                if best is None or (body or 0.0) > best[0]:
                    best = (body or 0.0, preview, heard)

            if best[0] >= FULL:
                break

        body, preview, heard = best

        os.replace(heard, os.path.join(OUT, "designed", slug(speaker) + ".mp3"))

        for left in glob.glob(os.path.join(OUT, "designed", slug(speaker) + "_try*.mp3")):
            os.remove(left)

        if body < FULL:
            print("  {}: none of the voices offered was full (the best has {:.0%} below 300 Hz): kept, and to be looked at".format(speaker, body))

        designed[speaker] = {"description": entry["design"], "generated_voice_id": preview.generated_voice_id, "first_said": text, "body": round(body, 3)}

        with open(DESIGNED, "w") as out:
            json.dump(designed, out, indent=1, sort_keys=True)
            out.write("\n")

        print("designed a voice for {} ({})".format(speaker, preview.generated_voice_id))

    def room():
        """Sees that the account can hold one more voice, letting an earlier one of ours go if it is full."""
        held = saved()
        plan = client.user.subscription.get()

        if plan.voice_slots_used < plan.voice_limit:
            return

        # whoever is to stay saved is let go last of all
        ours = sorted((voice for voice, name in held.items() if name.startswith(OURS)), key=lambda voice: held[voice][len(OURS):] in keep)

        if not ours:
            sys.exit("the account's {} voices are all somebody else's: nothing of ours to let go".format(plan.voice_limit))

        try:
            client.voices.delete(voice_id=ours[0])
        except Exception as error:
            sys.exit("the account holds {} voices and is full ({}), and this key may not let one go: {}\n"
                     "either the plan needs more voices than the {} speakers here, or the key needs 'voices_write'".format(
                         plan.voice_limit, ", ".join(held[voice] for voice in ours), str(error)[-220:], len({voice_of(name)[0] for name in speakers})))

        print("let go of {} to make room".format(held[ours[0]]))

    def save(speaker, entry):
        """Has the account hold a designed voice, so that it can speak."""
        return client.text_to_voice.create(voice_name=OURS + speaker, voice_description=entry["design"],
                                           generated_voice_id=designed[speaker]["generated_voice_id"]).voice_id

    def voice_for(speaker, entry):
        if not entry.get("design"):
            # the account's own voices by the first part of their names: "Laura - Enthusiast, Quirky Attitude" is Laura
            named = {name.split(" - ")[0].strip(): voice for voice, name in saved().items()}

            if entry["voice"] not in named:
                sys.exit("the account has no voice called {}: it has {}".format(entry["voice"], ", ".join(sorted(named))))

            return named[entry["voice"]]

        if speaker in again or designed.get(speaker, {}).get("description") != entry["design"]:
            design(speaker, entry)
            again.discard(speaker)

        if designed[speaker]["generated_voice_id"] in saved():
            return designed[speaker]["generated_voice_id"]

        room()

        try:
            return save(speaker, entry)
        except Exception as error:
            # a voice that was made a while ago and never saved is forgotten at their end: make them another
            print("the voice made for {} cannot be saved any more ({}): designing again".format(speaker, str(error)[-200:]))
            design(speaker, entry)

            return save(speaker, entry)

    made = 0
    voices = {}

    for file, speaker, whose, entry, settings, words, mark in wanted:
        os.makedirs(os.path.dirname(os.path.join(OUT, file)), exist_ok=True)

        if whose not in voices:
            voices[whose] = voice_for(whose, entry)

        audio = client.text_to_speech.convert(text=words, voice_id=voices[whose], model_id=cast["model"], output_format=cast["format"],
                                              voice_settings=VoiceSettings(stability=settings["stability"], similarity_boost=settings["similarity_boost"]))

        if file.endswith(".wav"):
            import wave

            with wave.open(os.path.join(OUT, file), "wb") as out:
                out.setnchannels(1)
                out.setsampwidth(2)
                out.setframerate(int(cast["format"].split("_")[1]))
                out.writeframes(b"".join(audio))
        else:
            with open(os.path.join(OUT, file), "wb") as out:
                for chunk in audio:
                    out.write(chunk)

        manifest[file] = {"speaker": speaker, "voice": "designed for {}".format(whose) if entry.get("design") else entry["voice"], "voice_id": voices[whose],
                          "model": cast["model"], "settings": settings, "words": words, "mark": mark, "made_with": "ElevenLabs text to speech"}

        # written after every line, so that a run that stops half way has lost nothing it paid for
        with open(MANIFEST, "w") as out:
            json.dump(manifest, out, indent=1, sort_keys=True)

        made += 1
        print("[{}/{}] {}  ({}, {} characters)".format(made, len(wanted), file, whose, len(words)))

    plan = client.user.subscription.get()

    print("made {} recordings in {}".format(made, OUT))
    print("the account has used {} of {} characters".format(plan.character_count, plan.character_limit))


main()
