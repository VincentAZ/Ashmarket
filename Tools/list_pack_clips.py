#!/usr/bin/env python3
"""
Writes Tools/gut_pack_clips.json: which clips of the bought motion capture packs are carried over to the fighting skeleton,
and the name each goes by in the game. Run after Tools/survey_motion_packs.py has listed the packs (Saved/motion_packs.json).

    python3 Tools/list_pack_clips.py

The names: mc_ MoCap Central (idles and core motion; am_ a man performed it, af_ a woman), kb_ Kubold's Fighting Animset Pro,
sw_ Kubold's Sword Animset Pro, pp_ MoCap Online's Punch Pro, bf_ RamsterZ's Brutal Finishers.
"""
import json
import os
import re

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PACKS = {
    "mc_idles": {"mesh": "/Game/MC_Idles_Pack/Demo/Characters/MCUE5v2/Meshes/SKM_MCUE5v2"},
    "mc_core": {"mesh": "/Game/MC_Core_Motion/Demo/Characters/MCPaul/Meshes/SKM_MCPaul"},
    "fight": {"mesh": "/Game/FightingAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin"},
    "sword": {"mesh": "/Game/SwordAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin"},
    "punch": {"mesh": "/Game/Punch_Pro/Character/Mesh/SK_Mannequin"},
    "finish": {"mesh": "/Game/Brutal_Finishers_Hand_and_Knife/Demo/Mannequin/Character/Mesh/SK_Mannequin"},
}

# not the game's: guns, grenades, lasers, jumping about, telephones, dancing
LEFT_OUT = re.compile(r"EyeLasers|Bazooka|Shotgun|Grenade|KB_Gun|KB_Mine|Projectile|KnifeThrow|Jump|KB_Land|Stretch$|Bindpose|Climb|Falling|"
                      r"Land_Generic|Dance|Phone|HoldSign|ListenMusic|NeedToilet|Silly|Creepy|FingerGuns|Superpunch|RootJump")


def main():
    listed = json.load(open(os.path.join(PROJECT, "Saved", "motion_packs.json")))
    clips = {}

    # how fast each clip that travels was going: the same clip held in place (its _NoRM twin) is walked at that speed
    speeds = {}

    for entry in listed:
        travel = entry.get("travel", [])

        if len(travel) == 3 and all(isinstance(part, (int, float)) for part in travel) and entry.get("length", 0.0) > 0.0:
            speeds[entry["path"]] = round(((travel[0] ** 2 + travel[1] ** 2) ** 0.5) / entry["length"], 1)

    for entry in listed:
        path = entry["path"]
        base = path.rsplit("/", 1)[1]

        if entry.get("failed") or LEFT_OUT.search(base):
            continue

        one = None

        if path.startswith("/Game/MC_Idles_Pack/Animations/"):
            one = ("mc_" + base.lower(), "mc_idles", False)
        elif path.startswith("/Game/MC_Core_Motion/Animations/"):
            folder = path.split("/")[4]

            # the walks, jogs and runs that hold their place (the game moves the body), from the newer set, and the fast run
            if folder == "LocomotionV2":
                if base.endswith("_NoRM") and "Crouch" not in base:
                    one = ("mc_" + base[:-len("_NoRM")].lower(), "mc_core", False)
            elif folder == "Locomotion":
                if base in ("am_Loco_RunFast_01_NoRM", "am_Loco_Run_01_NoRM", "am_Loco_Jog_01_NoRM") or (base.endswith("_NoRM") and "_Step_" in base):
                    one = ("mc_" + base[:-len("_NoRM")].lower() + ("_old" if "_Step_" not in base else ""), "mc_core", False)
            elif folder == "Turn":
                if base.endswith("_NoRM"):
                    one = ("mc_" + base[:-len("_NoRM")].lower(), "mc_core", False)
            elif folder in ("Conversation", "Idle", "Kneel", "LookAt", "Pickup", "ReactBump", "Ready", "Wave"):
                one = ("mc_" + base.lower(), "mc_core", folder == "ReactBump")
        elif path.startswith("/Game/FightingAnimsetPro/Animations/InPlace/"):
            one = ("kb_" + base[len("KB_"):].lower() if base.startswith("KB_") else "kb_" + base.lower(), "fight", False)
        elif path.startswith("/Game/SwordAnimsetPro/Animations/InPlace/"):
            one = ("sw_" + base.replace("Sword1h_", "").lower(), "sword", False)
        elif path.startswith("/Game/Punch_Pro/Animation/"):
            one = ("pp_" + base.lower(), "punch", True)
        elif path.startswith("/Game/Brutal_Finishers_Hand_and_Knife/Animations/"):
            one = ("bf_" + base.lower(), "finish", False)

        if not one:
            continue

        name, pack, lock = one
        name = re.sub(r"[^a-z0-9_]", "_", name)

        if name in clips:
            raise SystemExit("two clips would be called {}: {} and {}".format(name, clips[name]["from"], path))

        clips[name] = {"pack": pack, "from": path}

        if path.endswith("_NoRM") and speeds.get(path[:-len("_NoRM")], 0.0) > 5.0:
            clips[name]["speed"] = speeds[path[:-len("_NoRM")]]

        if lock:
            clips[name]["root_lock"] = True

    with open(os.path.join(PROJECT, "Tools", "gut_pack_clips.json"), "w") as handle:
        json.dump({"packs": PACKS, "clips": dict(sorted(clips.items()))}, handle, indent=1)

    counts = {}

    for entry in clips.values():
        counts[entry["pack"]] = counts.get(entry["pack"], 0) + 1

    print(len(clips), "clips:", ", ".join("{} {}".format(count, pack) for pack, count in sorted(counts.items())))


if __name__ == "__main__":
    main()
