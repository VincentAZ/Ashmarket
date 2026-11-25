"""
Lists every animation in the bought motion capture packs (for-review/motion-sources.md): its length, the skeleton it is
on, whether it travels, and how far. Written to Saved/motion_packs.json, for choosing which the game uses.

Run in an editor with no window:
    UnrealEditor-Cmd Ashmarket.uproject -ExecutePythonScript=Tools/survey_motion_packs.py -unattended -nosplash -RenderOffscreen -nosound
"""

import json
import os

import unreal

PACKS = ["/Game/MC_Idles_Pack", "/Game/MC_Core_Motion", "/Game/FightingAnimsetPro", "/Game/SwordAnimsetPro", "/Game/Punch_Pro",
         "/Game/Brutal_Finishers_Hand_and_Knife"]

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def log(message):
    unreal.log("[survey_motion_packs] {}".format(message))


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    skeletons = {}

    for pack in PACKS:
        assets = registry.get_assets_by_path(pack, recursive=True)
        kinds = {}

        for data in assets:
            kind = str(data.asset_class_path.asset_name)
            kinds[kind] = kinds.get(kind, 0) + 1

            if kind != "AnimSequence":
                continue

            sequence = data.get_asset()

            if not sequence:
                found.append({"path": str(data.package_name), "failed": True})
                continue

            skeleton = sequence.get_editor_property("skeleton")
            skeleton_path = skeleton.get_path_name() if skeleton else ""
            length = sequence.get_play_length()

            # how far the root goes over the whole clip
            travel = [0.0, 0.0, 0.0]
            turn = 0.0

            try:
                options = unreal.AnimPoseEvaluationOptions()
                first = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options), "root", unreal.AnimPoseSpaces.WORLD)
                last = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, length, options), "root", unreal.AnimPoseSpaces.WORLD)
                moved = last.translation - first.translation
                travel = [round(moved.x, 1), round(moved.y, 1), round(moved.z, 1)]
                turn = round(last.rotation.rotator().yaw - first.rotation.rotator().yaw, 1)
            except Exception as error:
                travel = [str(error)[:60]]

            found.append({
                "path": str(data.package_name),
                "length": round(length, 3),
                "frames": sequence.get_editor_property("number_of_sampled_keys") if hasattr(sequence, "number_of_sampled_keys") else 0,
                "skeleton": skeleton_path,
                "root_motion": bool(sequence.get_editor_property("enable_root_motion")),
                "travel": travel,
                "turn": turn,
            })

            skeletons[skeleton_path] = skeletons.get(skeleton_path, 0) + 1

        log("{}: {}".format(pack, ", ".join("{} {}".format(count, kind) for kind, count in sorted(kinds.items()))))

    for path, count in sorted(skeletons.items()):
        skeleton = unreal.load_asset(path.split(".")[0]) if path else None
        bones = []

        if skeleton:
            try:
                bones = [str(name) for name in skeleton.get_reference_pose().get_bone_names()]
            except Exception as error:
                bones = [str(error)[:80]]

        log("skeleton {}: {} clips, {} bones: {}".format(path, count, len(bones), " ".join(bones[:90])))

    # the meshes each pack shows its clips on
    for pack in PACKS:
        for data in registry.get_assets_by_path(pack, recursive=True):
            if str(data.asset_class_path.asset_name) in ("SkeletalMesh", "Skeleton", "IKRigDefinition", "IKRetargeter"):
                log("{} {}".format(data.asset_class_path.asset_name, data.package_name))

    with open(os.path.join(PROJECT, "Saved", "motion_packs.json"), "w") as handle:
        json.dump(found, handle, indent=1)

    log("{} animations listed".format(len(found)))
    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED " + traceback.format_exc())
