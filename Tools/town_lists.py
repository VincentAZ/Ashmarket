"""Reads a place's list with the town's work grafted into it.

A place's own list (Tools/town_main_street.json) says what stands there and who is always there. A piece of work
(Tools/missions/w1_gate_duty.json) says, for each place it touches, who is there because of it, what the people who
were there already now have to say, and which ways are open. Both the builder (Tools/build_town_level.py) and the voice
tool (Tools/make_town_voices.py) read places through here, so that a mission's lines are built and spoken like any other.

A mission's list:

    "key": "w1"                 its mark: nought is not begun, 100 and over is ended
    "name", "faction", "giver"  for the bag's page
    "steps": [[1, "words"]]     how it stands when its mark has reached each figure
    "places": {"Gate Yard": {
        "people": [...]         people who are there for it: as a place's own, with "when", "ways"
        "lines": {"Sergeant Rook": [...]}    lines added to somebody the place already has ("Warden#2" is the second of that name)
        "doors": [...]          ways added
    }}

Words that can be said, and what they ask and do, are the same everywhere (see ATownGameMode::Holds and ::Apply):
    "opens": true               a talk can begin at this line...
    "when": "w1=1 !wynn"        ...while this holds; an answer or a person with "when" is only there while it holds
    "needs": "water>=30"        an answer that is always shown, and refused with what is lacking
    "does": "st.wardens+10 water+25 w1=100 walk:through"
    "log": "..."                a line in the player's own account of what it has done
"""

import glob
import hashlib
import json
import math
import os

TOOLS = os.path.dirname(os.path.abspath(__file__))
STORY = os.path.join(TOOLS, "town_story.json")
BUMPS = os.path.join(TOOLS, "town_bumps.json")


def bumped(listed):
    """What each person says when walked into, and whose they are: see Tools/town_bumps.json."""
    told = json.load(open(BUMPS)) if os.path.exists(BUMPS) else {}

    for person in listed["people"]:
        name = person["name"]

        if person.get("lot"):
            continue

        if "of" not in person and name in told.get("of", {}):
            person["of"] = told["of"][name]

        if "bumped" not in person:
            mouth = told.get("mouths", {}).get(told.get("who", {}).get(name, "plain"), [])
            # the same three for the same name, wherever they stand: and not the same three as the next of that mouth
            first = int(hashlib.sha1(name.encode()).hexdigest(), 16) % max(1, len(mouth))
            step = next((step for step in (3, 2, 5, 7) if math.gcd(step, len(mouth)) == 1), 1) if mouth else 1
            person["bumped"] = [mouth[(first + (index * step)) % len(mouth)] for index in range(min(3, len(mouth)))]


def missions():
    return [json.load(open(path)) for path in sorted(glob.glob(os.path.join(TOOLS, "missions", "*.json")))]


def story():
    """The town's powers, where its marks begin, and every piece of work there is."""
    told = json.load(open(STORY)) if os.path.exists(STORY) else {}
    told["missions"] = missions()
    return told


def place(path):
    """A place's list, with whatever each mission has in that place added to it."""
    listed = json.load(open(path))

    if "people" not in listed or "name" not in listed:
        return listed

    for mission in missions():
        graft = mission.get("places", {}).get(listed["name"])

        if not graft:
            continue

        for person in graft.get("people", []):
            listed["people"].append(dict(person, mission=mission["key"]))

        for speaker, lines in graft.get("lines", {}).items():
            name, _, nth = speaker.partition("#")
            same = [person for person in listed["people"] if person["name"] == name and person.get("lines") is not None]
            which = int(nth or 1) - 1

            if which >= len(same):
                raise RuntimeError("{}: nobody called {} in {} to say its lines".format(mission["key"], speaker, listed["name"]))

            taken = {line.get("id") for line in same[which]["lines"]}

            for line in lines:
                if line.get("id") in taken:
                    raise RuntimeError("{}: {} in {} already has a line called {}".format(mission["key"], speaker, listed["name"], line.get("id")))

            same[which]["lines"] = same[which]["lines"] + lines

        listed.setdefault("doors", []).extend(graft.get("doors", []))

    bumped(listed)

    return listed
