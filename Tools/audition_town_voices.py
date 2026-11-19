"""Makes the same few lines several ways, for somebody to listen to and choose: an audition, not the game's voices.

    Tools/audition_town_voices.sh

Vincent, 2026-11-18, of the first voices: "sound like AI voices ... like they are reading from a script. Come up with
OPTIONS, try all and then show me your results." Each option here is one way of making a line less read:

  a_as_made      the pre-made voice saying the words as written (what the game has now)
  b_directed     the same voice, told how to say it in the text itself, and left freer
  c_older_model  the same direction through the older, more theatrical model, with its 'style' turned up
  d_designed     a voice made for the character from a description of who they are; two takes of each
  e_library      somebody's voice from ElevenLabs' shared library, picked by description, with the direction
  f_in_the_air   the directed line as it would be heard outdoors at a few paces, not at a microphone
  g_as_spoken    the line re-worded the way it would be said aloud (HIS WORDS CHANGED: for him to judge), directed

Everything is written to for-review/voices_audition/, with index.html to play them side by side. Nothing here
touches the game or ArtSource/voices.
"""
import base64
import json
import os
import shutil
import sys

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(PROJECT, "for-review", "voices_audition")
MADE = os.path.join(PROJECT, "ArtSource", "voices")

LINES = {
    "hasp": {
        "who": "Hasp, who racks the weapons at the Gut (Pool Street)",
        "voice": "Callum",
        "as_made": "pool_street/hasp_rack.mp3",
        "written": "Rack's inside. The club for most of them. The blade for anybody wearing rubber. And nobody ever thanked me for handing them a chain against a shield. That's free. Are you going in?",
        "directed": "[gruff, low, bored] Rack's inside. [flat] The club for most of them... the blade for anybody wearing rubber. [dry chuckle] And nobody ever thanked me for handing them a chain against a shield. [mutters] That's free. [short pause] Are you going in?",
        "spoken": "[gruff, low, bored] Rack's inside. Club for most of 'em... blade, if they're wearing rubber. [dry chuckle] And nobody ever thanked me for handing 'em a chain against a shield. [mutters] That one's free. [short pause] You going in?",
        "design": "A man in his fifties who hands out weapons at a pit-fighting cage in a desert shanty town. Gravelly, worn, a smoker's rasp, low and unhurried, bored, with dry humour underneath. Plain working American speech with no polish. He talks close and quiet, like somebody who has said this a thousand times. Not an announcer and not a narrator: a little mumbled, imperfect, real.",
        "search": {"search": "gravelly", "gender": "male"}, "pick": "cymHWdiF8WjUCg6vvFxx",
        "air": "street",
    },
    "ledger": {
        "who": "Mother Ledger, the lender (Block Square)",
        "voice": "Lily",
        "as_made": "block_square/mother_ledger_signed.mp3",
        "written": "People who signed. I have never collared anybody. People sign. I only remember it for them.",
        "directed": "[very quiet, unhurried, almost kind] People who signed. [pause] I have never collared anybody. [soft, patient] People sign. [barely above a whisper] I only remember it for them.",
        "spoken": "[very quiet, unhurried, almost kind] People who signed. [pause] I've never collared anybody... [soft] people sign. [barely above a whisper] I only remember it for 'em.",
        "design": "An old woman, late sixties, a moneylender who has never needed to raise her voice. Thin, dry, papery, very calm and slow, faintly amused, patient in a way that frightens people. She speaks softly and close, as if across a small table at night. Not a narrator and not an actress: plain, tired, exact.",
        "design_text": "People who signed. I have never collared anybody. People sign. I only remember it for them. I like to begin with the figures.",
        "search": {"search": "grandmother"}, "pick": "wGcFBfKz5yUQqhqr0mVy",
        "air": "street",
    },
    "caller": {
        "who": "Caller Strand, the auctioneer, calling bids over the crowd (Block Square)",
        "voice": "River",
        "as_made": "block_square/call_02.mp3",
        "written": "Thirty at the rope. Thirty-five, the Gut's man. Forty, at the back.",
        "directed": "[auctioneer's chant, fast, loud, projected over a crowd] Thirty at the rope! Thirty-five, the Gut's man! [rising] Forty, at the back!",
        "spoken": "[auctioneer's chant, fast, loud, projected over a crowd] Thirty at the rope, thirty! Thirty-five, the Gut's man, thirty-five... [rising] forty! Forty at the back!",
        "design": "An auctioneer calling bids from the back of a trailer at an open-air night market. An androgynous voice in its thirties, hoarse from shouting every night, nasal and hard, fast and rhythmic like a livestock auctioneer's chant, thrown over the heads of a crowd, tired underneath. Rough and loud, not smooth, not a radio voice.",
        "design_text": "Thirty at the rope. Thirty-five, the Gut's man. Forty, at the back. Forty-five? Forty-five, Mr Rask. Going at forty-five.",
        "search": {"search": "hoarse"}, "pick": "BVq2CupjBl5ZBjg6H0Vb",
        "air": "street",
    },
}


def main():
    from dotenv import load_dotenv

    load_dotenv(os.path.join(PROJECT, ".env.local"))

    from elevenlabs import EffectsSpecInput, ElevenLabs, VoiceSettings

    client = ElevenLabs(api_key=os.environ["ELEVENLABS_API_KEY"])
    voices = {voice.name.split(" - ")[0].strip(): voice.voice_id for voice in client.voices.get_all().voices}

    os.makedirs(OUT, exist_ok=True)

    notes = json.load(open(os.path.join(OUT, "notes.json"))) if os.path.exists(os.path.join(OUT, "notes.json")) else {}

    def keep(name, audio, note):
        path = os.path.join(OUT, name + ".mp3")

        with open(path, "wb") as out:
            if isinstance(audio, (bytes, bytearray)):
                out.write(audio)
            else:
                for chunk in audio:
                    out.write(chunk)

        notes[name] = note

        with open(os.path.join(OUT, "notes.json"), "w") as out:
            json.dump(notes, out, indent=1, sort_keys=True)

        print("made", name)

    def attempt(name, make):
        if os.path.exists(os.path.join(OUT, name + ".mp3")) and name in notes:
            return

        try:
            make()
        except Exception as error:
            notes[name] = {"failed": "{}: {}".format(type(error).__name__, str(error)[:400])}
            print("FAILED", name, notes[name]["failed"][:200])

    for key, line in LINES.items():
        voice = voices[line["voice"]]

        # a: what the game has now
        if not os.path.exists(os.path.join(OUT, key + "__a_as_made.mp3")):
            shutil.copy(os.path.join(MADE, line["as_made"]), os.path.join(OUT, key + "__a_as_made.mp3"))
            notes[key + "__a_as_made"] = {"voice": line["voice"], "model": "eleven_v4", "said": line["written"]}

        attempt(key + "__b_directed", lambda: keep(key + "__b_directed", client.text_to_speech.convert(
            text=line["directed"], voice_id=voice, model_id="eleven_v4", voice_settings=VoiceSettings(stability=0.2, similarity_boost=0.75)),
            {"voice": line["voice"], "model": "eleven_v4", "said": line["directed"], "settings": "stability 0.2"}))

        attempt(key + "__c_older_model", lambda: keep(key + "__c_older_model", client.text_to_speech.convert(
            text=line["directed"], voice_id=voice, model_id="eleven_v3", voice_settings=VoiceSettings(stability=0.0, similarity_boost=0.75, style=0.6)),
            {"voice": line["voice"], "model": "eleven_v3", "said": line["directed"], "settings": "stability 0 (its 'creative' setting), style 0.6"}))

        def designed():
            made = client.text_to_voice.design(voice_description=line["design"], model_id="eleven_ttv_v3", text=line.get("design_text", line["written"]))

            for index, preview in enumerate(made.previews[:2]):
                keep("{}__d_designed_{}".format(key, index + 1), base64.b64decode(preview.audio_base_64),
                     {"voice": "designed from a description", "description": line["design"], "said": line.get("design_text", line["written"]),
                      "generated_voice_id": preview.generated_voice_id, "model": "eleven_ttv_v3"})

        attempt(key + "__d_designed_1", designed)

        def library():
            # chosen from what the library answered to these words, by what each voice's owner says of it
            found = client.voices.get_shared(page_size=10, language="en", **line["search"])
            pick = next(voice for voice in found.voices if voice.voice_id == line["pick"])

            # a library voice is spoken with by its own number: this key may not add voices to the account, and need not
            use = pick.voice_id

            keep(key + "__e_library", client.text_to_speech.convert(text=line["directed"], voice_id=use, model_id="eleven_v4",
                                                                 voice_settings=VoiceSettings(stability=0.25, similarity_boost=0.75)),
                 {"voice": "library: {} ({})".format(pick.name, (pick.description or "")[:120]), "voice_id": use, "model": "eleven_v4", "said": line["directed"],
                  "others_found": [other.name for other in found.voices[1:5]]})

        attempt(key + "__e_library", library)

        attempt(key + "__f_in_the_air", lambda: keep(key + "__f_in_the_air", client.text_to_speech.convert(
            text=line["directed"], voice_id=voice, model_id="eleven_v4", voice_settings=VoiceSettings(stability=0.2, similarity_boost=0.75),
            audio_effects=EffectsSpecInput(environment_id=line["air"], distance=0.3)),
            {"voice": line["voice"], "model": "eleven_v4", "said": line["directed"], "settings": "stability 0.2; heard in a '{}', a little way off".format(line["air"])}))

        attempt(key + "__g_as_spoken", lambda: keep(key + "__g_as_spoken", client.text_to_speech.convert(
            text=line["spoken"], voice_id=voice, model_id="eleven_v4", voice_settings=VoiceSettings(stability=0.2, similarity_boost=0.75)),
            {"voice": line["voice"], "model": "eleven_v4", "said": line["spoken"], "settings": "stability 0.2", "words_changed": True}))

    with open(os.path.join(OUT, "notes.json"), "w") as out:
        json.dump(notes, out, indent=1, sort_keys=True)

    left = client.user.subscription.get()

    print("characters used {} of {}".format(left.character_count, left.character_limit))


main()
