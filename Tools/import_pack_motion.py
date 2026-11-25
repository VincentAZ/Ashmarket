"""
Carries clips from the bought motion capture packs (for-review/motion-sources.md) over to the skeleton that does the
fighting, beside the generated ones and the ones cut from 100STYLE: /Game/Gut/Art/Motion/<name>.

Run through Tools/import_pack_motion.sh (an editor with no window). The packs themselves are in Content/ under their own
folders, as Fab gave them (Tools/fetch_fab_library.py), and are not in the repository: they are bought, not ours to give out.

Which clips, and under what names, is said in Tools/gut_pack_clips.json:
    { "packs": { "<pack>": { "mesh": "<the skeletal mesh its clips are shown on>" } },
      "clips": { "<name in the game>": { "pack": "<pack>", "from": "<asset path>", "root_lock": true } } }
`root_lock` holds a clip that travels in place (the game moves the body itself). A clip already carried over is left alone;
PACK_MOTION_REDO=1 makes the rigs and retargeters again and redoes them all, PACK_MOTION_CLIPS=a,b redoes just those.
"""

import json
import os

import unreal

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

ROOT = "/Game/Gut/Art/Motion"
PACKS = ROOT + "/Packs"

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LISTED = json.load(open(os.path.join(PROJECT, "Tools", "gut_pack_clips.json")))

MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_RIG = "/Game/Gut/Art/IK_Gut_Placeholder"

LIMBS = ("clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball")


def log(message):
    unreal.log("[import_pack_motion] {}".format(message))


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def chains_of(rig):
    controller = unreal.IKRigController.get_controller(rig)

    return {str(chain.get_editor_property("chain_name")): (str(controller.get_retarget_chain_start_bone(chain.get_editor_property("chain_name"))),
                                                          str(controller.get_retarget_chain_end_bone(chain.get_editor_property("chain_name"))))
            for chain in controller.get_retarget_chains()}


def pack_rig(pack, mesh):
    """An IK rig for a pack's own skeleton, with its limbs named as the mannequin's rig names its own."""
    name = "IK_Pack_" + pack
    path = PACKS + "/" + name

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    rig = asset_tools.create_asset(name, PACKS, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)

    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("the IK rig does not fit {}".format(mesh.get_name()))

    bones = {str(bone).lower(): str(bone) for bone in mesh.get_editor_property("skeleton").get_reference_pose().get_bone_names()}

    # The mannequin's own chains, by the bones they run between. Every pack's skeleton is one of Epic's two mannequins, and
    # names its bones as they do (some in capitals): where it has the same two bones, it gets the same chain. Where it has not
    # (the older mannequin has three bones in its back and one in its neck), the chain runs between what it has.
    stand_ins = {"spine_04": "spine_03", "spine_05": "spine_03", "neck_02": "neck_01"}
    made = []

    for chain, (first, last) in chains_of(unreal.load_asset(MANNY_RIG)).items():
        own = []

        for bone in (first, last):
            key = bone.lower()

            if key not in bones and key in stand_ins:
                key = stand_ins[key]

            own.append(bones.get(key))

        if not own[0] or not own[1]:
            continue

        controller.add_retarget_chain(chain, own[0], own[1], "")
        made.append("{} {}..{}".format(chain, own[0], own[1]))

    root = bones.get("pelvis")

    if not root or not controller.set_retarget_root(root):
        raise RuntimeError("{} has no pelvis to be the root of its retargeting".format(mesh.get_name()))

    save(rig)

    log("{}: {} chains: {}".format(name, len(made), ", ".join(made)))

    return rig


def retargeter(pack, mesh):
    name = "RTG_Pack_" + pack
    path = PACKS + "/" + name

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    manny = unreal.load_asset(MANNY)
    manny_rig = unreal.load_asset(MANNY_RIG)

    asset = asset_tools.create_asset(name, PACKS, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(asset)

    source = unreal.RetargetSourceOrTarget.SOURCE
    target = unreal.RetargetSourceOrTarget.TARGET

    controller.set_ik_rig(source, pack_rig(pack, mesh))
    controller.set_ik_rig(target, manny_rig)
    controller.set_preview_mesh(source, mesh)
    controller.set_preview_mesh(target, manny)
    controller.add_default_ops()

    # by exact name only: matched loosely, fingers have been given legs to follow
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    # Arms and legs are stood the way the pack's stand (the older mannequin rests with its arms lower than the newer). Not the
    # back or the neck: lined up bone for bone across skeletons with different numbers of bones in them, necks have craned.
    limbs = [unreal.Name(bone + side) for side in ("_l", "_r") for bone in LIMBS]

    controller.auto_align_bones(limbs, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN, target)

    # a foot near the floor keeps the mannequin's own height above it, so a planted foot is planted
    floor = controller.add_retarget_op("/Script/IKRig.IKRetargetFloorConstraintOp")
    controller.set_parent_op_by_name(controller.get_op_name(floor), "Run IK Rig")
    controller.run_op_initial_setup(floor)

    floor_controller = controller.get_op_controller(controller.get_index_of_op_by_name("Floor Constraint"))
    settings = floor_controller.get_settings()
    chains = settings.get_editor_property("chains_to_affect")

    for index in range(len(chains)):
        chain = chains[index]

        if str(chain.get_editor_property("target_chain_name")) in ("LeftLeg", "RightLeg"):
            chain.set_editor_property("enable_floor_constraint", True)
            chain.set_editor_property("maintain_height_offset", 1.0)
            chains[index] = chain

    settings.set_editor_property("chains_to_affect", chains)
    floor_controller.set_settings(settings)

    mapped = controller.get_chain_map()

    log("{}: {}".format(name, ", ".join("{}<-{}".format(key, value) for key, value in mapped.items())))

    save(asset)

    return asset


def retarget(pack, names, mesh, rtg):
    """
    The engine's own batch retarget. It writes each result at the top of /Game under the source's own name, so each is then
    moved to ROOT under the name the game knows it by. Two clips of one name in different folders of a pack are done apart.
    """
    manny = unreal.load_asset(MANNY)
    by_folder = {}

    for name in names:
        path = LISTED["clips"][name]["from"]
        source = unreal.load_asset(path)

        if not isinstance(source, unreal.AnimSequence):
            log("{}: no such animation as {}".format(name, path))
            continue

        if eal.does_asset_exist(ROOT + "/" + name):
            eal.delete_asset(ROOT + "/" + name)

        by_folder.setdefault(path.rsplit("/", 1)[0], {}).setdefault(path.rsplit("/", 1)[1], []).append(name)

    results = []

    for folder, sources in sorted(by_folder.items()):
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            [unreal.AssetRegistryHelpers.create_asset_data(unreal.load_asset(folder + "/" + source)) for source in sources], mesh, manny, rtg,
            search="", replace="", prefix="", suffix="_Gut", include_referenced_assets=False)

        for data in made:
            made_path = str(data.package_name)
            wanted = sources.get(made_path.rsplit("/", 1)[1][:-len("_Gut")], [])

            if not wanted:
                log("made {} and do not know what for".format(made_path))
                continue

            # one source clip may go by several names in the game
            for extra in wanted[1:]:
                eal.duplicate_asset(made_path, ROOT + "/" + extra)
                results.append(extra)

            if not eal.rename_asset(made_path, ROOT + "/" + wanted[0]):
                raise RuntimeError("could not move {} to {}".format(made_path, ROOT))

            results.append(wanted[0])

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    return results


def describe(sequence, bones):
    length = sequence.get_play_length()
    lines = []

    for time in (0.0, length * 0.5):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, unreal.AnimPoseEvaluationOptions())
        spots = []

        for bone in bones:
            spot = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation

            spots.append("{} ({:.0f}, {:.0f}, {:.0f})".format(bone, spot.x, spot.y, spot.z))

        lines.append("{:.2f} s: {}".format(time, ", ".join(spots)))

    return "; ".join(lines)


def main():
    redo = bool(os.environ.get("PACK_MOTION_REDO"))
    only = [name for name in os.environ.get("PACK_MOTION_CLIPS", "").split(",") if name]

    by_pack = {}

    for name, entry in LISTED["clips"].items():
        if only and name not in only:
            continue

        if not only and not redo and eal.does_asset_exist(ROOT + "/" + name):
            continue

        by_pack.setdefault(entry["pack"], []).append(name)

    total = 0

    for pack, names in sorted(by_pack.items()):
        mesh = unreal.load_asset(LISTED["packs"][pack]["mesh"])

        if not mesh:
            log("{}: its mesh {} is not in the project. Fetch the pack (Tools/fetch_fab_library.py).".format(pack, LISTED["packs"][pack]["mesh"]))
            continue

        rtg = retargeter(pack, mesh)

        # in lots, so that a long run that dies has not lost everything
        for start in range(0, len(names), 60):
            lot = sorted(names)[start:start + 60]

            for name in retarget(pack, lot, mesh, rtg):
                sequence = unreal.load_asset(ROOT + "/" + name)
                entry = LISTED["clips"][name]

                # the game moves the body: nothing in a clip does. A clip that travels is held where it starts.
                sequence.set_editor_property("enable_root_motion", False)
                sequence.set_editor_property("force_root_lock", bool(entry.get("root_lock")))
                save(sequence)

                total += 1

                if len(names) <= 12 or entry.get("describe"):
                    log("{}: {:.2f} s; {}".format(name, sequence.get_play_length(), describe(sequence, ["pelvis", "foot_l", "foot_r", "hand_l", "hand_r", "head"])))

            log("{}: {} of {} carried over".format(pack, min(start + 60, len(names)), len(names)))

    log("{} clips carried over".format(total))
    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED " + traceback.format_exc())
