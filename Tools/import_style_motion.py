"""
Brings the clips cut from the 100STYLE motion capture into the project and carries them over to the skeleton that does the
fighting, beside the generated ones: /Game/Gut/Art/Motion/style_*.

Run through Tools/import_style_motion.sh (an editor with no window), after Tools/make_style_motion.py and
Tools/style_bvh_to_fbx.py. It:
  1. imports SK_Style.fbx once, the dataset's bare skeleton, into /Game/Gut/Art/Motion/Style
  2. imports every clip's FBX onto that skeleton, into /Game/Gut/Art/Motion/Style/Clips
  3. makes an IK rig for it and a retargeter from it to the mannequin (RTG_Gut_Style), with a floor constraint on the legs
  4. has the engine retarget each clip to the mannequin's skeleton, into /Game/Gut/Art/Motion
What each clip is used for is said in Tools/import_gut_motion.py, which is run after this.
A clip whose FBX is not newer than its imported copy is left alone. STYLE_MOTION_REDO=1 makes the rig and the retargeter
again and redoes them all.
"""

import os

import unreal

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

ROOT = "/Game/Gut/Art/Motion"
STYLE = ROOT + "/Style"
CLIPS = STYLE + "/Clips"

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "style", "fbx")

MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_RIG = "/Game/Gut/Art/IK_Gut_Placeholder"

# the dataset's limbs, named the way the mannequin's rig names its own, so the two map by name. A chain is carried over
# bone for bone only when both skeletons have the same number of bones in it: the toe is its own chain.
CHAINS = [
    ("Spine", "Chest", "Chest4"),
    ("Neck", "Neck", "Neck"),
    ("Head", "Head", "Head"),
    ("LeftClavicle", "LeftCollar", "LeftCollar"),
    ("LeftArm", "LeftShoulder", "LeftWrist"),
    ("RightClavicle", "RightCollar", "RightCollar"),
    ("RightArm", "RightShoulder", "RightWrist"),
    ("LeftLeg", "LeftHip", "LeftAnkle"),
    ("LeftFoot", "LeftToe", "LeftToe"),
    ("RightLeg", "RightHip", "RightAnkle"),
    ("RightFoot", "RightToe", "RightToe"),
]


def log(message):
    unreal.log("[import_style_motion] {}".format(message))


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def fbx_task(path, folder, name, options):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)

    asset_tools.import_asset_tasks([task])

    return [str(path) for path in task.get_editor_property("imported_object_paths")]


def style_mesh():
    path = STYLE + "/SK_Style"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("create_physics_asset", False)

    made = fbx_task(os.path.join(SOURCE, "SK_Style.fbx"), STYLE, "SK_Style", options)

    log("skeleton imported: {}".format(", ".join(made)))

    return unreal.load_asset(path)


def import_clip(name, skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("skeleton", skeleton)

    data = options.get_editor_property("anim_sequence_import_data")
    data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property("import_bone_tracks", True)
    data.set_editor_property("remove_redundant_keys", False)

    made = fbx_task(os.path.join(SOURCE, name + ".fbx"), CLIPS, name, options)

    for path in made:
        asset = unreal.load_asset(path)

        if isinstance(asset, unreal.AnimSequence):
            return asset

    raise RuntimeError("{} did not come in as an animation: {}".format(name, made))


def style_rig(mesh):
    path = STYLE + "/IK_Gut_Style"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    rig = asset_tools.create_asset("IK_Gut_Style", STYLE, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)

    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("the IK rig does not fit SK_Style")

    controller.set_retarget_root("Hips")

    for chain, first, last in CHAINS:
        controller.add_retarget_chain(chain, first, last, "")

    save(rig)

    return rig


def retargeter(mesh):
    path = STYLE + "/RTG_Gut_Style"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    manny = unreal.load_asset(MANNY)
    manny_rig = unreal.load_asset(MANNY_RIG)

    asset = asset_tools.create_asset("RTG_Gut_Style", STYLE, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(asset)

    source = unreal.RetargetSourceOrTarget.SOURCE
    target = unreal.RetargetSourceOrTarget.TARGET

    controller.set_ik_rig(source, style_rig(mesh))
    controller.set_ik_rig(target, manny_rig)
    controller.set_preview_mesh(source, mesh)
    controller.set_preview_mesh(target, manny)
    controller.add_default_ops()

    # by exact name only: matched loosely, fingers have been given legs to follow
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    # The clips rest in a T-pose and the mannequin in an A-pose: the mannequin's arms and legs are stood the way the clips'
    # stand. Not its back or its neck. The recording's spine is a straight stick with the head straight above it, and the
    # mannequin's is modelled with a back's own curve and a neck that leans forward: lined up with the stick, its neck
    # leaned 45 degrees where the performer's leaned 23 (measured 21 November), and everybody craned.
    limbs = [unreal.Name(bone + side) for side in ("_l", "_r") for bone in ("clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball")]

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

    log("chain map: {}".format(", ".join("{}<-{}".format(key, value) for key, value in controller.get_chain_map().items())))

    save(asset)

    return asset


def retarget(clips, mesh, rtg):
    """The engine's own batch retarget. It writes beside the source clip, so each result is then moved up to ROOT."""
    manny = unreal.load_asset(MANNY)

    for clip in clips:
        name = clip.get_name()

        for old in (ROOT + "/" + name, CLIPS + "/" + name + "_Manny"):
            if eal.does_asset_exist(old):
                eal.delete_asset(old)

    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [unreal.AssetRegistryHelpers.create_asset_data(clip) for clip in clips], mesh, manny, rtg,
        search="", replace="", prefix="", suffix="_Manny", include_referenced_assets=False)

    results = []

    for data in made:
        made_path = str(data.package_name)
        name = made_path.rsplit("/", 1)[-1][:-len("_Manny")]

        if not eal.rename_asset(made_path, ROOT + "/" + name):
            raise RuntimeError("could not move {} to {}".format(made_path, ROOT))

        results.append(unreal.load_asset(ROOT + "/" + name))

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
    redo = bool(os.environ.get("STYLE_MOTION_REDO"))
    mesh = style_mesh()
    skeleton = mesh.get_editor_property("skeleton")

    names = sorted(file[:-4] for file in os.listdir(SOURCE) if file.endswith(".fbx") and file != "SK_Style.fbx")
    fresh = []

    for name in names:
        source_time = os.path.getmtime(os.path.join(SOURCE, name + ".fbx"))
        made = PROJECT + "Content" + (ROOT + "/" + name)[len("/Game"):] + ".uasset"

        if not redo and os.path.exists(made) and os.path.getmtime(made) >= source_time:
            continue

        fresh.append(import_clip(name, skeleton))

    if fresh:
        results = retarget(fresh, mesh, retargeter(mesh))

        for sequence in results:
            # a loop of whole steps or of standing: the game moves the body, and nothing in the clip does
            sequence.set_editor_property("enable_root_motion", False)
            save(sequence)

            log("{}: {:.2f} s; {}".format(sequence.get_name(), sequence.get_play_length(), describe(sequence, ["pelvis", "foot_l", "foot_r", "hand_r", "head"])))
    else:
        log("no new clips")

    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED " + traceback.format_exc())
