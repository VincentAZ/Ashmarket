"""
Brings the Gut's motion clips into the project and carries them over to the skeleton that does the fighting.

Run through Tools/import_gut_motion.sh (an editor with no window).

The clips are generated on NVIDIA's SOMA skeleton (Tools/make_gut_motion.sh) and converted to FBX
(Tools/kimodo_bvh_to_fbx.py) in ArtSource/kimodo/fbx. This script:
  1. imports SK_Soma.fbx once, the bare skeleton, into /Game/Gut/Art/Motion/Soma
  2. imports every clip's FBX onto that skeleton, into /Game/Gut/Art/Motion/Soma/Clips
  3. makes an IK rig for it and a retargeter from it to the mannequin (RTG_Gut_Soma)
  4. has the engine retarget each clip to the mannequin's skeleton, into /Game/Gut/Art/Motion
  5. gives each weapon its attacks' clips and its stance, and each fighter Blueprint its walking clips and its animation class
A clip whose FBX is not newer than its imported copy is left alone. Name clips on GUT_MOTION_CLIPS to redo just those,
or set GUT_MOTION_REDO=1 to make the rig and the retargeter again and redo them all.
"""

import json
import os

import unreal

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

ROOT = "/Game/Gut/Art/Motion"
SOMA = ROOT + "/Soma"
CLIPS = SOMA + "/Clips"

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "kimodo", "fbx")
FACTS = os.path.join(PROJECT, "ArtSource", "kimodo", "export")

MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_RIG = "/Game/Gut/Art/IK_Gut_Placeholder"

# the SOMA skeleton's limbs, named the way the mannequin's rig names its own, so the two map by name
CHAINS = [
    ("Spine", "Spine1", "Chest"),
    ("Neck", "Neck1", "Neck2"),
    ("Head", "Head", "Head"),
    ("LeftClavicle", "LeftShoulder", "LeftShoulder"),
    ("LeftArm", "LeftArm", "LeftHand"),
    ("RightClavicle", "RightShoulder", "RightShoulder"),
    ("RightArm", "RightArm", "RightHand"),
    # a chain is carried over bone for bone only when both skeletons have the same number of bones in it:
    # with the toe on the end of the leg, the mannequin's foot took the toe's turn and stood on tiptoe
    ("LeftLeg", "LeftLeg", "LeftFoot"),
    ("LeftFoot", "LeftToeBase", "LeftToeBase"),
    ("RightLeg", "RightLeg", "RightFoot"),
    ("RightFoot", "RightToeBase", "RightToeBase"),
]


def log(message):
    unreal.log("[import_gut_motion] {}".format(message))


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


def soma_mesh():
    path = SOMA + "/SK_Soma"

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

    made = fbx_task(os.path.join(SOURCE, "SK_Soma.fbx"), SOMA, "SK_Soma", options)

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


def soma_rig(mesh):
    path = SOMA + "/IK_Gut_Soma"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    rig = asset_tools.create_asset("IK_Gut_Soma", SOMA, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)

    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("the IK rig does not fit SK_Soma")

    controller.set_retarget_root("Hips")

    for chain, first, last in CHAINS:
        controller.add_retarget_chain(chain, first, last, "")

    save(rig)

    return rig


def retargeter(mesh):
    path = SOMA + "/RTG_Gut_Soma"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    manny = unreal.load_asset(MANNY)
    manny_rig = unreal.load_asset(MANNY_RIG)

    log("mannequin chains: {}".format(", ".join(
        str(chain.get_editor_property("chain_name")) for chain in unreal.IKRigController.get_controller(manny_rig).get_retarget_chains())))

    asset = asset_tools.create_asset("RTG_Gut_Soma", SOMA, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(asset)

    source = unreal.RetargetSourceOrTarget.SOURCE
    target = unreal.RetargetSourceOrTarget.TARGET

    controller.set_ik_rig(source, soma_rig(mesh))
    controller.set_ik_rig(target, manny_rig)
    controller.set_preview_mesh(source, mesh)
    controller.set_preview_mesh(target, manny)
    controller.add_default_ops()

    # by exact name only. Matched loosely, the mannequin's fingers were given the clips' legs and collar bones to follow.
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    # the clips rest in a T-pose and the mannequin in an A-pose: stand the mannequin the way the clips stand
    controller.auto_align_all_bones(target)

    # the two skeletons' ankles are not the same height off the floor. A foot near the floor keeps the mannequin's own
    # height above it, so a planted foot is planted and not sunk or hovering.
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

    log("floor constraint on: {}".format(", ".join(
        "{} {}".format(chain.get_editor_property("target_chain_name"), chain.get_editor_property("enable_floor_constraint"))
        for chain in floor_controller.get_settings().get_editor_property("chains_to_affect"))))

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
    """Where a few bones are at the start, the middle and the end of a clip, in the body's own space."""
    length = sequence.get_play_length()
    lines = []

    for time in (0.0, length * 0.5, length):
        options = unreal.AnimPoseEvaluationOptions()
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        spots = []

        for bone in bones:
            spot = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation

            spots.append("{} ({:.0f}, {:.0f}, {:.0f})".format(bone, spot.x, spot.y, spot.z))

        lines.append("{:.2f} s: {}".format(time, ", ".join(spots)))

    return "; ".join(lines)


# which stance each weapon is held in, and the name its clips go by
WEAPONS = {
    "DA_Gut_Fists": ("fists", "fists"),
    "DA_Gut_Club": ("armed", "club"),
    "DA_Gut_Machete": ("armed", "machete"),
    "DA_Gut_Chain": ("chain", "chain"),
    "DA_Gut_Shield": ("shield", "shield"),
}

# each weapon's light attack that goes in low, by its label: a kick, a sweep at the legs, a cut down from the shoulder.
# The chain's are all low already, and the shield has none.
LOW = {"DA_Gut_Fists": "Kick", "DA_Gut_Club": "Leg Sweep", "DA_Gut_Machete": "Cut"}

# where a weapon gripped in the fist points while its holder stands ready, and with the guard up: forward, right, up.
# The clips are of empty hands and their wrists say nothing reliable, so the game turns the wrist to this.
# Ready: cocked up and back over the right shoulder, leaning out from the head. Guard: across in front of the face.
WEAPON_AIM = {"armed": ((-0.42, 0.3, 0.86), (0.25, -0.85, 0.45))}

# the ways of taking a blow: the field on the stance, and the name the clip goes by after the stance's own
HITS = {"hit_left": "hit_left", "hit_right": "hit_right", "hit_body": "hit_gut", "hit_top": "hit_top", "hit_back": "hit_back",
        "hit_left_arm": "hit_arm_l", "hit_right_arm": "hit_arm_r", "hit_left_leg": "hit_leg_l", "hit_right_leg": "hit_leg_r"}

# stepping round on the spot: the field on the fighter's walking clips, and the clip
TURNS = {"turn_left": "turn_left_45", "turn_right": "turn_right_45", "turn_left_far": "turn_left_90", "turn_right_far": "turn_right_90"}

# what the one a finisher is done to does while it is done, by the finisher's clip: without one it goes on swaying
TAKEN = {"chain_strangle": "taken_strangle", "fists_beatdown": "taken_cower", "club_execution": "taken_kneel",
         "shield_crush": "taken_kneel", "machete_butchery": "taken_hacked"}

# and how the body dies after the blow, where that is not by going limp at once
DEATHS = {"machete_throat_cut": "death_throat", "fists_neck_snap": "death_neck", "club_skull_crack": "death_skull",
          "shield_rim_strike": "death_rim", "chain_neck_break": "death_yank"}

# how near the one being finished has to be for a finisher done with the hands or the rim of a shield to reach it (the gap in
# centimetres, as a fighter measures it). The game lets a finisher start from 170 away: from there the fighter walks in first.
REACH = {"fists_neck_snap": 55.0, "shield_rim_strike": 70.0}

# how much of a stun, a dodge and a backstep clip is played, stretched over the game's own time for each: seconds from the start
STUN_LENGTH = 0.7
# A dodge clip is made to cover the game's own dodge distance at a steady speed in its first 0.8 s, as the game moves the fighter.
DODGE_LENGTH = 0.8
BACKSTEP_LENGTH = 0.8

FIGHTERS = ["BP_GutPlayer", "BP_GutOpponent", "BP_GutDummy"]

FACE_LOOPS = "/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Expression_Loops"

# crowd_watch_restless is made and filmed but left out: its take holds both fists up by the head the whole time
# what the one left standing does: dealt by chance
VICTORY = ["victory_arms", "victory_roar", "victory_spit", "victory_fists"]

WATCHING = ["crowd_watch_arms_crossed", "crowd_watch_hands_on_hips", "crowd_watch_leaning",
            "crowd_watch_hands_behind", "crowd_watch_relaxed", "crowd_watch_neck", "crowd_watch_tense"]
CHEERING = ["crowd_cheer_fists", "crowd_cheer_jump", "crowd_cheer_shout", "crowd_cheer_clap", "crowd_cheer_point"]


LISTED = {entry["name"]: entry for entry in json.load(open(os.path.join(PROJECT, "Tools", "gut_motion_clips.json")))}


def clip(name):
    path = ROOT + "/" + name

    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def facts(name):
    path = os.path.join(FACTS, name + ".json")

    return json.load(open(path)) if os.path.exists(path) else {}


def timed(entry, name):
    """Gives an attack or a finisher its clip, and where in the clip the blow begins and lands. Returns False if there is no such clip."""
    sequence = clip(name)
    known = facts(name)

    if not sequence or "impact" not in known:
        return False

    entry.set_editor_property("animation", sequence)
    entry.set_editor_property("anim_start_time", known["begin"])
    entry.set_editor_property("anim_impact_time", known["impact"])

    # where the blow comes in from, as the one it lands on sees it: that one reels accordingly
    strikes = LISTED.get(name, {}).get("strikes")

    if strikes:
        entry.set_editor_property("blow_from", getattr(unreal.GutBlowFrom, strikes.upper()))

    # a finisher's other half: the one it is done to, and the death that follows
    if isinstance(entry, unreal.GutFinisherDef):
        taken = TAKEN.get(name)
        death = DEATHS.get(name)

        entry.set_editor_property("taken_animation", clip(taken) if taken else None)
        entry.set_editor_property("taken_impact_time", LISTED.get(taken, {}).get("lands", 0.0) if taken and clip(taken) else 0.0)
        entry.set_editor_property("death_animation", clip(death) if death else None)
        entry.set_editor_property("death_limp_time", LISTED.get(death, {}).get("limp", 0.0) if death and clip(death) else 0.0)
        entry.set_editor_property("reach", REACH.get(name, 0.0))

    return True


def wire():
    """Puts the clips where the game looks for them: on each weapon, and on the three fighter Blueprints."""
    missing = []

    for asset_name, (stance, prefix) in WEAPONS.items():
        weapon = unreal.load_asset("/Game/Gut/Weapons/" + asset_name)

        def label_of(entry):
            return prefix + "_" + str(entry.get_editor_property("label")).lower().replace(" ", "_")

        # an entry read out of a list is a copy: change it and put it back
        lights = weapon.get_editor_property("light_attacks")

        for index in range(len(lights)):
            entry = lights[index]

            if not timed(entry, label_of(entry)):
                missing.append(label_of(entry))

            lights[index] = entry

        weapon.set_editor_property("light_attacks", lights)

        # the light attack that goes in low, which is what every light attack looks like when it is swung at a dog
        labels = [str(entry.get_editor_property("label")) for entry in lights]

        weapon.set_editor_property("low_light_attack", labels.index(LOW[asset_name]) if LOW.get(asset_name) in labels else -1)

        for field in ("heavy_attack", "clean_finisher", "slow_finisher"):
            entry = weapon.get_editor_property(field)

            if not timed(entry, label_of(entry)):
                missing.append(label_of(entry))

            weapon.set_editor_property(field, entry)

        held = unreal.GutStanceAnimations()
        held.set_editor_property("idle", clip(stance + "_idle"))
        held.set_editor_property("block", clip(stance + "_block"))
        held.set_editor_property("block_hit", clip(stance + "_block_hit"))
        held.set_editor_property("stun", clip(stance + "_stun"))
        held.set_editor_property("stun_length", STUN_LENGTH)
        held.set_editor_property("stagger", clip(stance + "_stagger"))
        held.set_editor_property("dodge", clip(stance + "_dodge"))
        held.set_editor_property("dodge_length", DODGE_LENGTH)
        held.set_editor_property("dodge_left", clip(stance + "_dodge_left"))
        held.set_editor_property("dodge_right", clip(stance + "_dodge_right"))
        held.set_editor_property("backstep", clip(stance + "_backstep"))
        held.set_editor_property("backstep_length", BACKSTEP_LENGTH)

        for field, suffix in HITS.items():
            held.set_editor_property(field, clip(stance + "_" + suffix))

            if not clip(stance + "_" + suffix):
                missing.append(stance + "_" + suffix)

        ready, guard = WEAPON_AIM.get(stance, ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
        held.set_editor_property("weapon_aim", unreal.Vector(*ready))
        held.set_editor_property("block_weapon_aim", unreal.Vector(*guard))

        # the walking clips are walked with bare fists up, which is how bare fists are held anyway
        held.set_editor_property("keep_upper_body_while_moving", stance != "fists")

        weapon.set_editor_property("stance", held)
        save(weapon)

    moving = unreal.GutLocomotionAnimations()

    for pace in ("walk", "run"):
        for way in ("forward", "right", "back", "left"):
            name = "move_{}_{}".format(pace, way)
            sequence = clip(name)

            if not sequence:
                missing.append(name)
                continue

            one = unreal.GutMoveClip()
            one.set_editor_property("animation", sequence)
            one.set_editor_property("speed", facts(name).get("speed", 150.0))

            moving.set_editor_property("{}_{}".format(pace, way), one)

    for field, name in TURNS.items():
        sequence = clip(name)
        known = facts(name)

        if not sequence or "turn_progress" not in known:
            missing.append(name)
            continue

        one = unreal.GutTurnClip()
        one.set_editor_property("animation", sequence)
        one.set_editor_property("degrees", known["turn_degrees"])
        one.set_editor_property("progress", known["turn_progress"])

        moving.set_editor_property(field, one)

    # somebody still standing when the other one is dead
    moving.set_editor_property("victory", [clip(name) for name in VICTORY if clip(name)])

    # beaten and let live, and the one who will not finish them
    moving.set_editor_property("yield_down", clip("yield_down"))
    moving.set_editor_property("yield_kneel", clip("yield_kneel"))
    moving.set_editor_property("spare_lower", clip("spare_lower"))
    moving.set_editor_property("spare_stand", clip("spare_stand"))

    # somebody the live cable kills
    moving.set_editor_property("death_shock", clip("death_shock"))
    moving.set_editor_property("death_shock_limp_time", LISTED.get("death_shock", {}).get("limp", 0.0) if clip("death_shock") else 0.0)

    # out of a fight altogether, in the street: standing at ease, a walk and a jog. Out of a fight a body turns to face
    # where it goes, so the forward clip is the only one ever seen and it stands in for every way.
    at_ease = unreal.GutStanceAnimations()
    at_ease.set_editor_property("idle", clip("civil_idle"))
    at_ease.set_editor_property("keep_upper_body_while_moving", False)

    strolling = unreal.GutLocomotionAnimations()

    for pace, name in (("walk", "civil_walk"), ("run", "civil_jog")):
        sequence = clip(name)

        if not sequence:
            missing.append(name)
            continue

        one = unreal.GutMoveClip()
        one.set_editor_property("animation", sequence)
        one.set_editor_property("speed", facts(name).get("speed", 150.0))

        for way in ("forward", "right", "back", "left"):
            strolling.set_editor_property("{}_{}".format(pace, way), one)

    for name in FIGHTERS:
        blueprint = unreal.load_asset("/Game/Gut/Blueprints/" + name)
        defaults = unreal.get_default_object(blueprint.generated_class())

        # what a face does when a blow lands on it, and when its owner is beaten: two more of the performances Epic captured
        # and ships with MetaHuman. The player's are the man's takes and the others' the woman's, like the anger they already wear.
        take = "m" if name == "BP_GutPlayer" else "f"
        folder = "B" if name == "BP_GutPlayer" else "A"

        defaults.set_editor_property("face_hurt", unreal.load_asset(FACE_LOOPS + "/Surprise{0}/mhc_mh002_fmn_f_facialloop_surprise_{1}_s001".format(folder, take)))
        defaults.set_editor_property("face_afraid", unreal.load_asset(FACE_LOOPS + "/Fear{0}/mhc_mh002_fmn_f_facialloop_fear_{1}_s001".format(folder, take)))

        defaults.set_editor_property("locomotion", moving)
        defaults.set_editor_property("at_ease_stance", at_ease)
        defaults.set_editor_property("at_ease_locomotion", strolling)
        defaults.get_editor_property("mesh").set_editor_property("anim_class", unreal.GutAnimInstance.static_class())

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)

    # the people on the deck: each is dealt one way of standing and watching, and cheers a different way each time
    watching = [clip(name) for name in WATCHING if clip(name)]
    cheering = [clip(name) for name in CHEERING if clip(name)]

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Gut/Lvl_Gut")

    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.GutCrowd):
            actor.set_editor_property("idle_animations", watching)
            actor.set_editor_property("cheer_animations", cheering)

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()

    log("the crowd watches {} ways and cheers {} ways".format(len(watching), len(cheering)))

    log("wired {} weapons and {} fighters{}".format(len(WEAPONS), len(FIGHTERS), "; no clip for: " + ", ".join(missing) if missing else ""))


def main():
    wanted = [name for name in os.environ.get("GUT_MOTION_CLIPS", "").split(",") if name]
    redo = bool(os.environ.get("GUT_MOTION_REDO"))

    # GUT_MOTION_REDO=1: the launcher has thrown the rig and the retargeter away, so they are made again here,
    # and every clip is carried over again

    mesh = soma_mesh()
    skeleton = mesh.get_editor_property("skeleton")
    rtg = retargeter(mesh)

    fresh = []

    for file in sorted(os.listdir(SOURCE)):
        name, extension = os.path.splitext(file)

        if extension != ".fbx" or name == "SK_Soma":
            continue

        if wanted and name not in wanted:
            continue

        done = ROOT + "/" + name

        if not wanted and not redo and eal.does_asset_exist(done):
            stamp = os.path.join(PROJECT, "Content", done[len("/Game/"):] + ".uasset")

            if os.path.exists(stamp) and os.path.getmtime(stamp) >= os.path.getmtime(os.path.join(SOURCE, file)):
                continue

        fresh.append(import_clip(name, skeleton))

    if not fresh:
        log("no new clips")
    else:
        for sequence in retarget(fresh, mesh, rtg):
            log("{}: {:.2f} s; {}".format(sequence.get_name(), sequence.get_play_length(),
                                         describe(sequence, ["pelvis", "foot_l", "foot_r", "hand_r", "head"])))

    wire()

    log("DONE")


main()
