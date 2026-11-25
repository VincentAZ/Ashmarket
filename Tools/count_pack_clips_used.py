"""
Says how many of the clips carried over from the bought packs (Tools/gut_pack_clips.json) something in the game uses:
a weapon, a fighter, a crowd, a place. A clip is in use if anything other than another clip refers to it.

  UnrealEditor-Cmd Ashmarket.uproject -ExecutePythonScript=Tools/count_pack_clips_used.py -unattended -nosplash -RenderOffscreen -nosound
Writes Saved/pack_clips_used.json: the names in use and the names not, by pack.
"""

import collections
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ROOT = "/Game/Gut/Art/Motion"

clips = json.load(open(os.path.join(PROJECT, "Tools", "gut_pack_clips.json")))["clips"]
registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)

used = []
idle = []

for name in sorted(clips):
    users = registry.get_referencers(ROOT + "/" + name, options) or []
    users = [str(user) for user in users if not str(user).startswith(ROOT + "/") or str(user).count("/") > ROOT.count("/") + 1]

    (used if users else idle).append(name)

by_pack = collections.Counter(name.split("_")[0] for name in used)
all_pack = collections.Counter(name.split("_")[0] for name in clips)

json.dump({"used": used, "not": idle}, open(os.path.join(PROJECT, "Saved", "pack_clips_used.json"), "w"), indent=1)

unreal.log("[count_pack_clips] {} of {} in use: {}".format(len(used), len(clips), ", ".join("{} {} of {}".format(pack, by_pack[pack], all_pack[pack]) for pack in sorted(all_pack))))
