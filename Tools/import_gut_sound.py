"""
Brings the Gut's sounds into the project and tells the game which is which.

Run through Tools/import_gut_sound.sh (an editor with no window), after Tools/prepare_gut_sound.sh has cut them.

  1. imports every WAV in ArtSource/sound/prepared/<kind>/ into /Game/Gut/Sound/<kind>, and marks the ones that play without end
  2. makes SA_Gut (how a sound in the cage fades with distance) and RV_Gut (the ring of an empty tiled pool)
  3. makes DA_Gut_Sound, the list the game reads: each kind of sound, its recordings, how loud it is against the rest,
     how much its pitch is varied, and so on, all from Tools/gut_sounds.json
  4. notes which fighters' bodies have a woman's voice, and what each weapon sounds like
A WAV that is not newer than its imported copy is left alone.
"""

import json
import os

import unreal

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

ROOT = "/Game/Gut/Sound"

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PREPARED = os.path.join(PROJECT, "ArtSource", "sound", "prepared")
PLAN = json.load(open(os.path.join(PROJECT, "Tools", "gut_sounds.json")))

# what each weapon sounds like: part of the name of its sounds (swing_club, hit_club, block_club, drop_club ...)
WEAPONS = {"DA_Gut_Fists": "fists", "DA_Gut_Club": "club", "DA_Gut_Machete": "blade", "DA_Gut_Chain": "chain", "DA_Gut_Shield": "shield"}

# the material a man's body is given in place of the shirt a woman's keeps (Tools/build_gut_metahuman.py)
NOT_DRAWN = "/Game/Gut/Art/MetaHumans/M_Gut_NotDrawn"
BODIES = "/Game/Gut/Art/MetaHumans"


def log(message):
    unreal.log("[import_gut_sound] {}".format(message))


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def bring_in(path, folder, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    asset_tools.import_asset_tasks([task])

    return unreal.load_asset(folder + "/" + name)


def sounds_of(kind, loops):
    folder = os.path.join(PREPARED, kind)
    result = []

    if not os.path.isdir(folder):
        return result

    for file in sorted(os.listdir(folder)):
        name, extension = os.path.splitext(file)

        if extension != ".wav":
            continue

        target = "{}/{}/{}".format(ROOT, kind, name)
        stamp = os.path.join(PROJECT, "Content", target[len("/Game/"):] + ".uasset")
        source = os.path.join(folder, file)

        if eal.does_asset_exist(target) and os.path.exists(stamp) and os.path.getmtime(stamp) >= os.path.getmtime(source):
            wave = unreal.load_asset(target)
        else:
            wave = bring_in(source, "{}/{}".format(ROOT, kind), name)

        if not isinstance(wave, unreal.SoundWave):
            raise RuntimeError("{} did not come in as a sound".format(source))

        if bool(wave.get_editor_property("looping")) != loops:
            wave.set_editor_property("looping", loops)
            save(wave)

        result.append(wave)

    # a recording that is no longer wanted is taken out of the project too
    wanted = {wave.get_name() for wave in result}

    if eal.does_directory_exist("{}/{}".format(ROOT, kind)):
        for path in eal.list_assets("{}/{}".format(ROOT, kind), recursive=False):
            if path.split(".")[-1] not in wanted:
                eal.delete_asset(path.split(".")[0])

    return result


def falloff():
    """How a sound somewhere in the cage fades: full within a few metres, as real sound does beyond, and placed left or right."""
    path = ROOT + "/SA_Gut"
    asset = unreal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset("SA_Gut", ROOT, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())

    settings = asset.get_editor_property("attenuation")
    settings.set_editor_property("attenuate", True)
    settings.set_editor_property("spatialize", True)
    settings.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND)
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(350.0, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", 4500.0)
    settings.set_editor_property("d_b_attenuation_at_max", -36.0)
    asset.set_editor_property("attenuation", settings)

    save(asset)

    return asset


def ring():
    """The ring of the place: an empty tiled pool with a wire roof, open to the night. Hard, bright and short."""
    path = ROOT + "/RV_Gut"
    asset = unreal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset("RV_Gut", ROOT, unreal.ReverbEffect, unreal.ReverbEffectFactory())

    for name, value in PLAN.get("reverb", {}).items():
        if name != "volume":
            asset.set_editor_property(name, value)

    save(asset)

    return asset


def women():
    """The fighters whose bodies kept the shirt: every body that does not use the material a man's shirt is replaced with."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)

    men = set()

    for package in registry.get_referencers(NOT_DRAWN, options) or []:
        parts = str(package).split("/")

        if str(package).startswith(BODIES + "/") and len(parts) > 6:
            men.add(parts[5])

    everybody = set()

    for path in eal.list_assets(BODIES, recursive=True):
        name = path.split(".")[-1]

        if name.startswith("BP_") and path.split("/")[5] == name[3:]:
            everybody.add(name[3:])

    # only somebody who fights has a voice: the people on the deck are heard as a crowd. A fighter is a body that a rung of
    # the ladder or one of the fighter Blueprints asks for.
    def fights(name):
        users = registry.get_referencers("{0}/{1}/BP_{1}".format(BODIES, name), options) or []

        return any(str(user).startswith("/Game/Gut/Ladder") or str(user).startswith("/Game/Gut/Blueprints") for user in users)

    return sorted("BP_" + name for name in everybody - men if fights(name)), sorted(name for name in men if fights(name))


def main():
    kinds = {}
    empty = []

    for kind, how in PLAN["kinds"].items():
        waves = sounds_of(kind, bool(how.get("loop")))

        if not waves:
            empty.append(kind)
            continue

        one = unreal.GutSoundSet()
        one.set_editor_property("sounds", waves)
        one.set_editor_property("volume", float(how.get("volume", 1.0)))
        one.set_editor_property("low_pitch", float(how.get("pitch", [0.95, 1.05])[0]))
        one.set_editor_property("high_pitch", float(how.get("pitch", [0.95, 1.05])[1]))
        one.set_editor_property("everywhere", bool(how.get("everywhere")))
        one.set_editor_property("least_gap", float(how.get("gap", 0.0)))

        kinds[kind] = one

    path = ROOT + "/DA_Gut_Sound"

    if eal.does_asset_exist(path):
        data = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.GutSoundData)
        data = asset_tools.create_asset("DA_Gut_Sound", ROOT, unreal.GutSoundData, factory)

    with_shirts, without = women()

    data.set_editor_property("sets", kinds)
    data.set_editor_property("attenuation", falloff())
    data.set_editor_property("reverb", ring())
    data.set_editor_property("reverb_volume", float(PLAN.get("reverb", {}).get("volume", 0.5)))
    data.set_editor_property("women_bodies", with_shirts)
    data.set_editor_property("always", [kind for kind in PLAN.get("always", []) if kind in kinds])

    save(data)

    for asset_name, what in WEAPONS.items():
        weapon = unreal.load_asset("/Game/Gut/Weapons/" + asset_name)

        if weapon and str(weapon.get_editor_property("sound_kind")) != what:
            weapon.set_editor_property("sound_kind", what)
            save(weapon)

    log("{} kinds of sound, {} recordings".format(len(kinds), sum(len(one.get_editor_property("sounds")) for one in kinds.values())))
    log("with a woman's voice: {}".format(", ".join(with_shirts) or "nobody"))
    log("with a man's voice: {}".format(", ".join(without) or "nobody"))

    if empty:
        log("no recordings for: {}".format(", ".join(empty)))

    log("DONE")


main()
