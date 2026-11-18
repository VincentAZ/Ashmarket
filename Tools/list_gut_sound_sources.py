"""Writes for-review/sound-sources.md: every kind of sound in the game and the recordings it is cut from, with who recorded
each, where it is published and under what licence. Read from Tools/gut_sounds.json and ArtSource/sound/manifest.json.

    python3 Tools/list_gut_sound_sources.py
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

plan = json.loads((ROOT / "Tools/gut_sounds.json").read_text())
known = {entry["file"]: entry for entry in json.loads((ROOT / "ArtSource/sound/manifest.json").read_text())}
cut = json.loads((ROOT / "ArtSource/sound/prepared/prepared.json").read_text())

lines = ["# Where the Gut's sounds come from", "",
         "Every sound is a recording published on freesound.org under CC0 1.0 (public domain: no credit owed, commercial use allowed).",
         "The files were fetched as the mono 48 kHz copies in the LAION-Audio-630k mirror on Hugging Face, or, where marked, as freesound's own stereo MP3 preview.",
         "The original files need a free freesound account to download; each page is linked.", ""]

used = set()

for kind, how in plan["kinds"].items():
    lines.append("## {} ({} cuts)".format(kind, len(cut.get(kind, []))))

    for source in how.get("from", []):
        entry = known.get(source["file"])

        if not entry:
            lines.append("- {} (not in the manifest)".format(source["file"]))
            continue

        used.add(source["file"])

        notes = []

        if entry.get("is_preview"):
            notes.append("stereo MP3 preview")

        if entry.get("original_is_lossy"):
            notes.append("original upload is lossy")

        if entry.get("caution"):
            notes.append(entry["caution"])

        lines.append("- `{}`: {} by {}, {} [{}]({}){}".format(
            source["file"], " ".join(entry["description"].split(" [Holds")[0].split())[:170].rstrip("."), entry["author"], entry["licence"], "page", entry["page_url"],
            ". Note: " + "; ".join(notes) if notes else ""))

    lines.append("")

lines.insert(5, "{} recordings are used, out of {} gathered.".format(len(used), len(known)))

(ROOT / "for-review/sound-sources.md").write_text("\n".join(lines) + "\n")

print("{} kinds, {} recordings used -> for-review/sound-sources.md".format(len(plan["kinds"]), len(used)))
