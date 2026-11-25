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
STYLE_FACTS = os.path.join(PROJECT, "ArtSource", "style", "export")

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

# Finishers that two people performed together (Brutal Finishers, RamsterZ): the pair's name, how far apart the pack's own
# demo stands the two (read from its map), when the blow lands, when the one it is done to is down and still, and how long
# the pair is. Both are played as performed, so the game's windup for that finisher becomes the time the blow takes to come.
# The times were read off the clips: the hand or what is in it at the other's head, and the other's head going to the floor.
PAIRED = {
    "club_skull_crack": ("bf_animseq_paired_h2h_brick_smashhead", 190.0, 1.0, 1.9, 3.0),
    "machete_throat_cut": ("bf_animseq_paired_knife_shove_neckslice_quick", 122.0, 1.55, 3.4, 4.07),
    "fists_beatdown": ("bf_animseq_paired_h2h_kneekick_punch", 207.0, 2.9, 3.3, 4.5),
}

# and what those three were timed at when each was one body acting on another (Tools/build_gut_content.py), to go back to
UNPAIRED_TIMING = {"club_skull_crack": (0.7, 0.5), "machete_throat_cut": (0.6, 0.5), "fists_beatdown": (1.6, 0.7)}

# how near the one being finished has to be for a finisher done with the hands or the rim of a shield to reach it (the gap in
# centimetres, as a fighter measures it). The game lets a finisher start from 170 away: from there the fighter walks in first.
REACH = {"fists_neck_snap": 55.0, "shield_rim_strike": 70.0}

# how much of a stun, a dodge and a backstep clip is played, stretched over the game's own time for each: seconds from the start
STUN_LENGTH = 0.7
# A dodge clip is made to cover the game's own dodge distance at a steady speed in its first 0.8 s, as the game moves the fighter.
DODGE_LENGTH = 0.8
BACKSTEP_LENGTH = 0.8

# out of a fight: ways of standing in a street, and small things done while standing
STANDING = ["civil_idle", "street_stand_guard", "street_stand_weight"]
FIDGETS = ["fidget_neck", "fidget_shoulders", "fidget_wipe", "fidget_scratch", "fidget_shift", "fidget_look_around", "fidget_cough"]

# cut from motion capture of a person (100STYLE, CC BY 4.0): the walk, how fast the town walks with it, and ways of standing
# for anybody who was not given one of their own
STYLE_WALK = "style_walk_neutral"
STROLL = 112.0
STYLE_STANDING = ["style_idle_neutral", "style_idle_pockets", "style_idle_strut", "style_idle_akimbo", "style_idle_proud", "style_idle_byside"]

# Out of a fight, since 22 November: performed motion from the packs bought on Fab (Tools/import_pack_motion.py,
# for-review/motion-sources.md). A walk, a jog and a run for a man and for a woman, each by somebody of that sex, and
# standing as families of clips from one stance: whoever stands that way goes from one clip of it to another.
PACK = json.load(open(os.path.join(PROJECT, "Tools", "gut_pack_clips.json")))["clips"]

PACES = {
    "at_ease_locomotion": {"walk": "mc_am_loco_walk_fwd", "run": "mc_am_loco_jog_fwd", "sprint": "mc_am_loco_run_fwd"},
    "at_ease_locomotion_woman": {"walk": "mc_af_loco_walk_fwd", "run": "mc_af_loco_jog_fwd", "sprint": "mc_af_loco_run_fwd"},
}

# the things done from plain standing and back to it
STAND_EXTRAS = (["mc_am_stand_idle_0{}".format(n) for n in ("3_lookaround", "4_lookaround", "6_scratcharm", "7_scratchleg", "8_thightap")]
                + ["mc_am_stand_idle_fidget_0{}".format(n) for n in ("1_scratchhead", "2_rubneck", "3_rubneck", "4_scratchhead", "5_scratchface", "6_scratchface", "7_fingesrtogether")]
                + ["mc_am_stand_idle_waiting_0{}_lookaround".format(n) for n in (4, 5, 6, 7, 8)]
                + ["mc_am_stand_idle_hot_0{}".format(n) for n in ("4_browrhand", "5_browlhand", "6_neckrhand", "7_necklhand")]
                + ["mc_am_stand_bodyache_0{}".format(n) for n in ("1_back", "2_shoulders", "4_arms")]
                + ["mc_am_stand_idle_brushoff_0{}".format(n) for n in ("5_arm", "6_arm", "8_chest")]
                + ["mc_am_stand_idle_tired_05_yawn", "mc_am_stand_idle_lookatnails_01"]
                # more of them, 23 November: of the 126 recordings of somebody standing, 33 were in use
                + ["mc_am_stand_bodyache_0{}".format(n) for n in ("3_hips", "5_upperbody", "6_upperbody")]
                + ["mc_am_stand_idle_brushoff_0{}".format(n) for n in ("1_legs", "2_leg", "3_legs", "4_leg", "7_arm", "9_chest")]
                + ["mc_am_stand_idle_lookatnails_0{}".format(n) for n in ("2", "3", "4_bite")]
                + ["mc_am_stand_idle_tired_0{}".format(n) for n in ("1", "2", "6_yawn")]
                + ["mc_am_stand_idle_hot_0{}".format(n) for n in ("1", "2_fanrhand", "3_fanlhand")]
                + ["mc_am_stand_lookat_0{}".format(n) for n in (1, 2, 3)] + ["mc_am_stand_crowd_lookaround_01"])

# somebody who fights for a living, standing in their owner's yard with a fight to come: they stretch, and they are sore
YARD_EXTRAS = (["mc_am_standwide_stretch_{}".format(n) for n in ("01_sidebend", "02_backbend", "03_quad", "04_hamstringrleg", "05_hamstringlleg", "06_backbend", "07_shoulderrarm",
                                                                  "08_shoulderlarm", "09_backtouchtoes", "10_warmup", "11_shoulders", "12_shoulders")]
               + ["mc_am_stand_bodyache_0{}".format(n) for n in ("1_back", "2_shoulders", "3_hips", "4_arms", "5_upperbody", "6_upperbody")]
               + ["mc_am_stand_idle_breathe_01_heavy", "mc_am_stand_idle_breathe_02_heavy", "mc_am_stand_idle_breathe_05_grabhead", "mc_am_stand_idle_tired_01", "mc_am_stand_idle_tired_02"])

# name, who is dealt it when they were given none (0 nobody, 1 men, 2 women, 3 anybody), the standing, the small things
FAMILIES = [
    ("stand", 1, ["mc_am_stand_idle_01", "mc_am_stand_idle_02"], STAND_EXTRAS),
    ("stand_f", 2, ["mc_af_stand_idle_01", "mc_af_stand_idle_02_weightshift", "mc_af_stand_idle_05_weightshift"],
     ["mc_af_stand_idle_0{}".format(n) for n in ("3_lookright", "4_lookleft", "6_lookfloor", "7_lookbehind", "8_lookbehind", "9_thightap")]
     + ["mc_af_stand_idle_10_hiplefthand", "mc_af_stand_idle_11_hiprighthand", "mc_af_stand_idle_12_hiphans"]),
    ("shift_l", 3, ["mc_am_stand_idle_shiftl_loop_0{}".format(n) for n in ("1", "2", "3_lookaround", "4_lookaround", "5_lookahead", "6_lookright", "7_lookleft")], []),
    ("shift_r", 3, ["mc_am_stand_idle_shiftr_loop_0{}".format(n) for n in ("1", "2", "3_wigglefingers", "4_lookaround", "5_lookaround")], []),
    ("crossed", 1, ["mc_am_armscrossed_idle_0{}".format(n) for n in ("1", "2", "3", "4", "5_lookaround", "6_lookaround", "7_lookaround")], []),
    ("crossed_b", 0, ["mc_am_armscrossedb_idle_0{}".format(n) for n in ("1", "2_lookaround", "3_shiftweight", "4_shiftweight")], []),
    ("hip_l", 3, ["mc_am_handhipl_idle_0{}".format(n) for n in ("1", "2_lookaround", "3", "4_lookaround", "5_fidget")], []),
    ("hip_r", 3, ["mc_am_handhipr_idle_0{}".format(n) for n in ("1", "2", "3_shiftweight", "4_lookaround")], []),
    ("hips", 1, ["mc_am_handship_idle_0{}".format(n) for n in ("1", "2", "3", "4_shiftweight")], []),
    ("clasp", 0, ["mc_am_handsclaspfront_idle_0{}".format(n) for n in ("1", "2", "3_shiftweight", "4_lookaround")], []),
    ("watch", 0, ["mc_am_stand_crowd_look_0{}".format(n) for n in (1, 2, 3, 4)], ["mc_am_stand_crowd_lookaround_01"]),
    ("yard", 0, ["mc_am_stand_idle_01", "mc_am_stand_idle_02"], YARD_EXTRAS),
]

# how a crowd stands: these of the families, dealt round. Twice as many plain watchers as anything else.
CROWD_FAMILIES = ["watch", "stand", "shift_l", "watch", "crossed", "hip_r", "watch", "shift_r", "hips", "stand_f", "hip_l", "clasp"]

EPIC_JOG = "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"
EPIC_JOG_SPEED = 300.0
# it holds two strides, and the walk one: mixed without knowing that, the feet of the two came down out of step
EPIC_JOG_CYCLES = 2

FIGHTERS = ["BP_GutPlayer", "BP_GutOpponent", "BP_GutDummy"]

FACE_LOOPS = "/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Expression_Loops"

# crowd_watch_restless is made and filmed but left out: its take holds both fists up by the head the whole time
# what the one left standing does: dealt by chance
VICTORY = ["victory_arms", "victory_roar", "victory_spit", "victory_fists"]

# how the crowd stands: as a person did (Tools/make_style_motion.py), where those clips are there. Their heads are turned to
# the fight by the game. The ones made for the deck (GENERATED_WATCHING) looped every four seconds and looked at their feet.
# Not style_idle_folded, _heavy or _followed: looked at on clothed bodies (21 November), folded arms hover crossed in front
# with flat open hands (the recording has no fingers and the arms are not the performer's length), "heavy" splays the arms
# like wings, and "followed" throws the head back.
WATCHING = ["style_idle_akimbo", "style_idle_pockets", "style_idle_neutral", "style_idle_proud",
            "style_idle_angry", "style_idle_stiff", "style_idle_strut", "style_idle_behind", "style_idle_byside", "style_idle_rushed"]
GENERATED_WATCHING = ["crowd_watch_arms_crossed", "crowd_watch_hands_on_hips", "crowd_watch_leaning",
                      "crowd_watch_hands_behind", "crowd_watch_relaxed", "crowd_watch_neck", "crowd_watch_tense"]
CHEERING = ["crowd_cheer_fists", "crowd_cheer_jump", "crowd_cheer_shout", "crowd_cheer_clap", "crowd_cheer_point"]

# and performed, from the idles pack's crowd set: arms up, one arm up, clapping, a fist shaken, a shout. Sixteen ways.
PACK_CHEERING = (["mc_am_stand_crowd_cheerarms_01", "mc_am_stand_crowd_clapa_01", "mc_am_stand_crowd_clapb_01", "mc_am_stand_crowd_fistshakes_01",
                  "mc_am_stand_crowd_shout_01", "mc_am_stand_crowd_shout_02"]
                 + ["mc_am_stand_crowd_cheer{}arm_0{}".format(side, n) for side in "lr" for n in (1, 2)]
                 + ["mc_am_stand_crowd_fistshake{}arm_0{}".format(side, n) for side in "lr" for n in (1, 2, 3)])


LISTED = {entry["name"]: entry for entry in json.load(open(os.path.join(PROJECT, "Tools", "gut_motion_clips.json")))}

# The fight, since 22 November: performed motion from the packs bought on Fab, in place of the generated clip of the same
# name wherever there is one. Bare hands and the shield are held as a boxer stands (Kubold's Fighting Animset Pro, left foot
# forward); the club, the machete and the chain as a one-handed sword is (Kubold's Sword Animset Pro, right foot forward).
# An attack names the limb that strikes and how the moment it lands is found in the clip: "reach", when that limb is furthest
# forward (a punch, a kick, a thrust), or "speed", when it is moving fastest in front of the body (a swing).
# GUT_MOTION_GENERATED=1 wires the generated clips again, to compare.
KB_STANCE = {
    "idle": "kb_idle_1", "block": "kb_block_loop", "block_hit": "pp_box1_m3_block_straight_hit_v1",
    "stun": "kb_hit_m_highfront_med", "stagger": "pp_box1_m3_idle_really_tired_v1",
    "dodge": "kb_skipfwd_1", "dodge_left": "kb_dodge_l", "dodge_right": "kb_dodge_r", "backstep": "kb_skipbwd_1",
    "hit_left": "kb_hit_p_highleft_weak", "hit_right": "kb_hit_p_highright_weak", "hit_gut": "kb_hit_p_midfront_weak",
    "hit_top": "kb_hit_m_midtop_med", "hit_back": "kb_hit_m_highback_weak", "hit_arm_l": "kb_hit_p_midleft_weak",
    "hit_arm_r": "kb_hit_p_midright_weak", "hit_leg_l": "kb_hit_p_lowleft_weak", "hit_leg_r": "kb_hit_p_lowright_weak",
}

SW_STANCE = {
    "idle": "sw_idle", "block": "sw_parry_mid", "block_hit": "sw_parry_mid2",
    "stun": "sw_hit_torso_front", "stagger": "pp_box1_m3_idle_really_tired_v1",
    "dodge": "sw_dodge_fwd", "dodge_left": "sw_dodge_left", "dodge_right": "sw_dodgle_right", "backstep": "sw_dodge",
    "hit_left": "sw_hit_head_left", "hit_right": "sw_hit_head_right", "hit_gut": "sw_hit_torso_front",
    "hit_top": "sw_hit_head_front", "hit_back": "sw_hit_torso_front", "hit_arm_l": "sw_hit_torso_left",
    "hit_arm_r": "sw_hit_torso_right", "hit_leg_l": "sw_hit_legs_left", "hit_leg_r": "sw_hit_legs_right",
}

# Other ways of doing each of those, performed: whenever the first would be played, it or one of these is, and never the
# same twice running. Which way a head goes in each was measured (23 November, head against hips over the clip): Kubold's
# "left" and "right" name the way the head goes, and the game's name where the blow comes from, so the two are crossed.
KB_STANCE.update({"hit_left": "kb_hit_p_highright_weak", "hit_right": "kb_hit_m_highleft_weak"})
SW_STANCE.update({"hit_left": "sw_hit_head_right", "hit_right": "sw_hit_head_leftdown", "hit_top": "sw_hit_head_front"})

PP = "pp_box1_m3_"

KB_OTHERS = {
    "stun": ["kb_hit_p_highfront_weak", "kb_hit_m_highfront_weak", "kb_hit_p_highupper_weak", PP + "hit_by_jab_v1_short", PP + "hit_by_jab_v2_short", PP + "hit_by_jab_v3",
             PP + "hit_by_jab_v4_short", PP + "hit_by_jab_v5_short", PP + "hit_by_cross_v3"],
    "hit_left": ["kb_hit_m_highright_med", PP + "hit_by_hook_v2_short", PP + "hit_by_hook_v3_short", PP + "hit_by_combo_v1_short"],
    "hit_right": ["kb_hit_m_highleft_med", "kb_hit_p_highleft_weak", PP + "hit_by_hook_v1_short", PP + "hit_by_hook_v5", PP + "hit_by_hook_v6_short", PP + "hit_by_hook_v7_short",
                  PP + "hit_by_hook_v8_short"],
    "hit_gut": ["kb_hit_m_midfront_weak", "kb_hit_m_midfront_med", PP + "hit_by_gutshot_v1_short", PP + "hit_by_gutshot_v2_short", PP + "hit_by_gutshot_v3", PP + "hit_by_kidneyshot_v1"],
    "hit_back": ["kb_hit_m_midback_med"],
    "hit_arm_l": ["kb_hit_m_midleft_weak", "kb_hit_m_midleft_med"],
    "hit_arm_r": ["kb_hit_m_midright_weak", "kb_hit_m_midright_med"],
    "hit_leg_l": ["kb_hit_m_lowleft_weak", "kb_hit_m_lowleft_med"],
    "hit_leg_r": ["kb_hit_m_lowright_weak", "kb_hit_m_lowright_med"],
    "block_hit": [PP + "block_straight_hit_v2", PP + "block_straight_hit_v3"],
    "stagger": [PP + "idle_really_tired_v2", PP + "idle_hurt_v1", PP + "idle_hurt_v2", PP + "idle_hurt_v3"],
    "dodge": ["kb_skipfwd_2"],
    "dodge_left": [PP + "left_weave", PP + "left_dodge", "kb_sidestep_l"],
    "dodge_right": [PP + "right_weave", PP + "right_dodge", "kb_sidestep_r"],
    "backstep": ["kb_skipbwd_2", PP + "back_dodge", PP + "back_weave"],
}

SW_OTHERS = {
    "stun": ["sw_hit_head_front", "sw_hit_head_left", "sw_hit_head_rightup", "sw_hit_head_leftup"],
    "hit_left": ["sw_hit_head_rightdown"],
    "block_hit": ["sw_parry_l", "sw_parry_l2", "sw_parry_r", "sw_parry_t", "sw_parry_rd", "sw_parry_lowright"],
    "stagger": [PP + "idle_really_tired_v2", PP + "idle_hurt_v1", PP + "idle_hurt_v2", PP + "idle_hurt_v3"],
    "dodge": ["sw_skip_fwd", "sw_shortdodge_mid", "sw_shortdodge_low2"],
    "dodge_left": ["sw_shortdodge_highmid"],
    "dodge_right": ["sw_shortdodge_highright"],
    "backstep": ["sw_skip_bwd", "sw_shortdodge_1", "sw_shortdodge_mid2", "sw_shortdodge_mid3", "sw_shortdodge_low"],
}

PACK_OTHERS = {"fists": KB_OTHERS, "shield": KB_OTHERS, "armed": SW_OTHERS, "chain": SW_OTHERS}

# and how whoever holds a blade or a club shows the crowd: a swordsman's own ways, and what they do to say "is that all"
SW_VICTORY = ["sw_vicotry_1", "sw_victory_2", "sw_victory_3", "sw_victory_4", "sw_taunt_cmon", "sw_taunt_what", "sw_taunt_lowersword", "sw_taunt_mark_1", "sw_taunt_mark_2",
              "sw_taunt_mark_3", "sw_taunt_mark_short", "sw_taunt_swings", "sw_taunt_taptap", "sw_taunt_wheel_short", "sw_taunt_wheels"]

PACK_FIGHT = {}

for stance, family in (("fists", KB_STANCE), ("shield", KB_STANCE), ("armed", SW_STANCE), ("chain", SW_STANCE)):
    for part, source in family.items():
        PACK_FIGHT[stance + "_" + part] = source

PACK_FIGHT.update({
    # chosen so that each lands, performed, about when the game's own windup for it runs out (GUT_MOTION_SURVEY=1 lists them)
    "fists_jab": ("kb_m_jab_l", "hand_l", "reach"),
    "fists_cross": ("kb_m_jab_r", "hand_r", "reach"),
    "fists_kick": ("kb_p_midkickstraight_r", "foot_r", "reach"),
    "fists_haymaker": ("kb_m_onetwo", "hand_r", "reach"),
    "shield_jab": ("kb_m_jab_r", "hand_r", "reach"),
    "shield_bash": ("kb_m_hook_l", "hand_l", "speed"),
    "shield_charge": ("kb_m_elbowround_l", "hand_l", "speed"),
    "club_swing": ("sw_attack_place_fast_rdown_1", "hand_r", "speed"),
    "club_backswing": ("sw_attack_place_fast_ldown_1", "hand_r", "speed"),
    "club_leg_sweep": ("sw_attack_place_fast_llow_1", "hand_r", "speed"),
    "club_overhead": ("sw_attack_move_slow_t_1", "hand_r", "speed"),
    "machete_cut": ("sw_attack_place_snap_r_1", "hand_r", "speed"),
    "machete_backcut": ("sw_attack_place_snap_ldown_1", "hand_r", "speed"),
    "machete_thrust": ("sw_attack_move_snap_point", "hand_r", "reach"),
    "machete_hack": ("sw_attack_move_slow_t_2", "hand_r", "speed"),
    "chain_lash": ("sw_attack_place_fast_rdown_2", "hand_r", "speed"),
    "chain_backlash": ("sw_attack_place_fast_lup_1", "hand_r", "speed"),
    "chain_whip_round": ("sw_attack_move_slow_whirl_l_1", "hand_r", "speed"),
})

# Other performances of the same blows: thrown one way or another each time, never the same way twice running. Each is
# come into late enough that it lands when the first does, so nothing about the fight's timing depends on which is thrown.
ATTACK_OTHERS = {
    "fists_jab": ["kb_p_jab_l_1", PP + "left_jab", PP + "left_jab_a", PP + "left_jab_b", PP + "left_jab_c", PP + "left_jab_hard"],
    "fists_cross": ["kb_p_jab_r_1", PP + "right_cross", PP + "right_cross_a", PP + "right_cross_b", PP + "right_cross_c", PP + "right_cross__hard"],
    "fists_kick": ["kb_m_midkickstraight_r", "kb_p_midkickfront_r", "kb_midkickstraightlong"],
    "fists_haymaker": ["kb_p_onetwo", "kb_m_overhand_r"],
    "shield_jab": ["kb_p_jab_r_1", PP + "right_cross", PP + "right_cross_a", PP + "right_cross_b"],
    "shield_bash": ["kb_p_hook_l", PP + "left_hook", PP + "left_hook_v2", PP + "left_hook__hard"],
    "shield_charge": ["kb_m_elbowround_l2", "kb_p_elbow_l"],
    "club_swing": ["sw_attack_move_fast_rdown_1", "sw_attack_move_med_rdown_1", "sw_attack_move_med_rdown_2", "sw_attack_place_slow_rdown_1"],
    "club_backswing": ["sw_attack_move_med_ldown_1", "sw_attack_move_med_ldown_2", "sw_attack_place_slow_ldown_1", "sw_attack_place_snap_ldown_2"],
    "club_overhead": ["sw_attack_move_med_t_1", "sw_attack_move_fast_t_1", "sw_attack_place_fast_t_1", "sw_attack_place_snap_t_1"],
    "machete_cut": ["sw_attack_place_med_r_1", "sw_attack_place_med_r_2", "sw_attack_move_med_rhigh_1"],
    "machete_backcut": ["sw_attack_place_snap_ldown_2", "sw_attack_move_med_l_1", "sw_attack_move_slow_l_1", "sw_attack_move_med_lhi_1"],
    "machete_thrust": ["sw_attack_move_fast_pommel"],
    "machete_hack": ["sw_attack_move_strong_rdown_1", "sw_attack_move_slow_rdown_1"],
    "chain_lash": ["sw_attack_move_fast_rdown_1", "sw_attack_place_fast_rdown_1"],
    "chain_backlash": ["sw_attack_move_slow_lup"],
    "chain_whip_round": ["sw_attack_move_slow_whirl_l_2"],
}

# which stances those are, and the walking and running that goes with the second: a sword's own, at the speeds it was performed
PACK_STANCES = {"fists": "kb", "shield": "kb", "armed": "sw", "chain": "sw"}
SW_MOVES = {"walk": ("sw_walkfwdloop", "sw_straferightloop", "sw_walkbwdloop", "sw_strafeleftloop", 203.0),
            "run": ("sw_runfwdloop", "sw_runrtloop", "sw_runbwdloop", "sw_runltloop", 401.0)}

# and everybody's legs in a fight: a walk and a jog each way, and a run forward, with the stance's arms kept over them
FIGHT_MOVES = {"walk": ("mc_am_loco_walk_fwd", "mc_am_loco_walk_right", "mc_am_loco_walk_bwd", "mc_am_loco_walk_left"),
               "run": ("mc_am_loco_jog_fwd", "mc_am_loco_jog_right", "mc_am_loco_jog_bwd", "mc_am_loco_jog_left"),
               "sprint": "mc_am_loco_run_fwd"}

PACK_VICTORY = ["pp_box1_m3_win_v{}".format(n) for n in range(1, 8)]

GENERATED_ONLY = bool(os.environ.get("GUT_MOTION_GENERATED"))


def own(name):
    """The clip of exactly this name, whatever it was made from"""
    path = ROOT + "/" + name

    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def performed(name):
    """The pack clip that stands in for a generated one of this name, if there is one and it is in the project"""
    mapped = PACK_FIGHT.get(name)

    if GENERATED_ONLY or not mapped:
        return None

    return own(mapped if isinstance(mapped, str) else mapped[0])


def clip(name):
    return performed(name) or own(name)


def struck(sequence, bone, how):
    """When in a clip a limb lands its blow: furthest forward ("reach"), or fastest while it is in front of the body ("speed")"""
    length = sequence.get_play_length()
    count = max(int(length * 60.0), 8)
    spots = []

    for index in range(count + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, length * index / count, unreal.AnimPoseEvaluationOptions())
        limb = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
        hips = unreal.AnimPoseExtensions.get_bone_pose(pose, "pelvis", unreal.AnimPoseSpaces.WORLD).translation

        # the mannequin faces along +Y
        spots.append((limb.x, limb.y, limb.z, limb.y - hips.y))

    if how == "reach":
        best = max(range(count + 1), key=lambda index: spots[index][3])
    else:
        speeds = [0.0] + [sum((spots[index][axis] - spots[index - 1][axis]) ** 2 for axis in range(3)) ** 0.5 for index in range(1, count + 1)]
        ahead = [index for index in range(count + 1) if spots[index][3] > 15.0] or list(range(count + 1))
        best = max(ahead, key=lambda index: speeds[index])

    return length * best / count


def facts(name):
    # a performed attack: the moment it lands is found in the clip, and the clip is come into late enough that the stretch up
    # to that moment is about as long as the game gives the blow to wind up. Sped up to fit, a real swing looks thrown.
    mapped = PACK_FIGHT.get(name)

    if isinstance(mapped, tuple) and performed(name):
        impact = struck(performed(name), mapped[1], mapped[2])
        windup = float(LISTED.get(name, {}).get("windup", 0.3))
        begin = max(0.0, impact - (windup * 1.15))

        log("{} is {}: lands at {:.2f} s of {:.2f}, come into at {:.2f} for a windup of {:.2f}".format(
            name, mapped[0], impact, performed(name).get_play_length(), begin, windup))

        return {"begin": begin, "impact": impact}

    # what is known of a clip: beside the generated ones, or beside the ones cut from motion capture (Tools/make_style_motion.py)
    for folder in (FACTS, STYLE_FACTS):
        path = os.path.join(folder, name + ".json")

        if os.path.exists(path):
            return json.load(open(path))

    return {}


def strides(sequence):
    """
    How many whole cycles (a left step and a right) a walking clip holds, and where in the first of them the left foot
    passes the right going forward, as a share of a cycle: found by reading the two feet through the clip.
    """
    length = sequence.get_play_length()
    count = max(int(length * 60.0), 8)
    gaps = []

    for index in range(count):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, length * index / count, unreal.AnimPoseEvaluationOptions())
        left = unreal.AnimPoseExtensions.get_bone_pose(pose, "foot_l", unreal.AnimPoseSpaces.WORLD).translation
        right = unreal.AnimPoseExtensions.get_bone_pose(pose, "foot_r", unreal.AnimPoseSpaces.WORLD).translation

        # the mannequin faces along +Y
        gaps.append(left.y - right.y)

    passes = [index for index in range(count) if gaps[index - 1] < 0.0 <= gaps[index]]

    if not passes:
        return 1, 0.0

    cycles = len(passes)

    return cycles, (passes[0] / count) * cycles % 1.0


def move_clip(name):
    """A walking clip from the packs, with the speed it was walked at and how its steps fall"""
    sequence = clip(name)

    if not sequence:
        return None

    cycles, phase = strides(sequence)

    one = unreal.GutMoveClip()
    one.set_editor_property("animation", sequence)
    one.set_editor_property("speed", float(PACK.get(name, {}).get("speed", 150.0)))
    one.set_editor_property("cycles", cycles)
    one.set_editor_property("phase", phase)

    log("{}: {:.0f} cm/s, {} cycles in {:.2f} s, the left foot passes at {:.2f}".format(name, one.get_editor_property("speed"), cycles, sequence.get_play_length(), phase))

    return one


def families(names=None):
    made = []

    for name, dealt_to, loops, extras in FAMILIES:
        if names is not None and name not in names:
            continue

        one = unreal.GutStandFamily()
        one.set_editor_property("name", name)
        one.set_editor_property("dealt_to", dealt_to)
        one.set_editor_property("loops", [clip(loop) for loop in loops if clip(loop)])
        one.set_editor_property("extras", [clip(extra) for extra in extras if clip(extra)])

        absent = [each for each in loops + extras if not clip(each)]

        if absent:
            log("family {}: no clip for {}".format(name, ", ".join(absent)))

        if one.get_editor_property("loops"):
            made.append(one)

    if names is not None:
        made.sort(key=lambda one: names.index(str(one.get_editor_property("name"))))

        # dealt round as listed, the plain watchers more than once
        by_name = {str(one.get_editor_property("name")): one for one in made}
        made = [by_name[name] for name in names if name in by_name]

    return made


def timed(entry, name):
    """Gives an attack or a finisher its clip, and where in the clip the blow begins and lands. Returns False if there is no such clip."""
    pair = None if GENERATED_ONLY else PAIRED.get(name)

    if pair and isinstance(entry, unreal.GutFinisherDef) and own(pair[0] + "_att") and own(pair[0] + "_vic"):
        stem, apart, blow, down, length = pair

        entry.set_editor_property("animation", own(stem + "_att"))
        entry.set_editor_property("anim_start_time", 0.0)
        entry.set_editor_property("anim_impact_time", blow)
        entry.set_editor_property("taken_animation", own(stem + "_vic"))
        entry.set_editor_property("taken_impact_time", blow)
        entry.set_editor_property("death_animation", None)
        entry.set_editor_property("death_limp_time", down)
        entry.set_editor_property("pair_apart", apart)
        entry.set_editor_property("windup", blow)
        entry.set_editor_property("recovery", max(length - blow - 0.3, 0.5))
        entry.set_editor_property("reach", 0.0)

        return True

    if isinstance(entry, unreal.GutFinisherDef):
        entry.set_editor_property("pair_apart", 0.0)

        if name in UNPAIRED_TIMING:
            entry.set_editor_property("windup", UNPAIRED_TIMING[name][0])
            entry.set_editor_property("recovery", UNPAIRED_TIMING[name][1])

    sequence = clip(name)
    known = facts(name)

    if not sequence or "impact" not in known:
        return False

    entry.set_editor_property("animation", sequence)
    entry.set_editor_property("anim_start_time", known["begin"])
    entry.set_editor_property("anim_impact_time", known["impact"])

    # the other performances of it, each come into as late as the first is: only those with as much of a lead up to the
    # blow as the game's windup is long, so none is hurried
    mapped = PACK_FIGHT.get(name)

    if isinstance(entry, unreal.GutAttackDef) and isinstance(mapped, tuple) and performed(name):
        windup = float(LISTED.get(name, {}).get("windup", 0.3))
        ways = []

        for other in ATTACK_OTHERS.get(name, []):
            another = own(other)

            if not another:
                continue

            impact = struck(another, mapped[1], mapped[2])

            if impact < windup * 0.95 or impact > another.get_play_length() - 0.1:
                log("{} is not thrown as {}: it lands at {:.2f} s, and the windup is {:.2f}".format(name, other, impact, windup))
                continue

            way = unreal.GutAttackWay()
            way.set_editor_property("animation", another)
            way.set_editor_property("anim_start_time", max(0.0, impact - (windup * 1.15)))
            way.set_editor_property("anim_impact_time", impact)
            ways.append(way)

        entry.set_editor_property("others", ways)

        if ways:
            log("{} is thrown {} other ways".format(name, len(ways)))

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

        # a sword's guard is one moment of a parry, held: the clip is of the guard going up and coming down again
        packed = PACK_STANCES.get(stance) if performed(stance + "_idle") else None

        held.set_editor_property("block_hold", struck(clip(stance + "_block"), "hand_r", "reach") if packed == "sw" and clip(stance + "_block") else -1.0)
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

        # the other ways of each, where the stance is a performed one
        others = []

        for part, names in (PACK_OTHERS.get(stance, {}) if packed else {}).items():
            first = clip(stance + "_" + part)
            ways = [own(name) for name in names if own(name)]

            if first and ways:
                one = unreal.GutOtherWays()

                try:
                    one.set_editor_property("for", first)
                except Exception:
                    one.set_editor_property("for_", first)

                one.set_editor_property("ways", ways)
                others.append(one)

        held.set_editor_property("others", others)
        held.set_editor_property("victory", [own(name) for name in SW_VICTORY if own(name)] if packed == "sw" else [])

        # the generated clips are of empty hands whose wrists say nothing reliable, so the game turns the wrist. A performed
        # sword stance was performed with a sword in the hand: its wrist is left as it is.
        ready, guard = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)) if packed else WEAPON_AIM.get(stance, ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
        held.set_editor_property("weapon_aim", unreal.Vector(*ready))
        held.set_editor_property("block_weapon_aim", unreal.Vector(*guard))

        # The generated walking clips are walked with bare fists up, which is how bare fists are held anyway. The performed
        # ones are a walk and a jog with the arms swinging: every stance keeps its arms over those, but a sword's, which is
        # walked and run with on its own clips.
        held.set_editor_property("keep_upper_body_while_moving", (packed != "sw") if packed else (stance != "fists"))

        weapon.set_editor_property("stance", held)

        own_moves = unreal.GutLocomotionAnimations()

        if packed == "sw" and own(SW_MOVES["walk"][0]):
            for pace, (forward, right, back, left, speed) in SW_MOVES.items():
                for way, name in (("forward", forward), ("right", right), ("back", back), ("left", left)):
                    one = move_clip(name)
                    one.set_editor_property("speed", speed)

                    own_moves.set_editor_property("{}_{}".format(pace, way), one)

        weapon.set_editor_property("moves", own_moves)
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

    # performed legs, where they are in the project: a walk and a jog each way and a run forward (FIGHT_MOVES)
    if not GENERATED_ONLY and all(own(name) for name in FIGHT_MOVES["walk"] + FIGHT_MOVES["run"]):
        for pace in ("walk", "run"):
            for way, name in zip(("forward", "right", "back", "left"), FIGHT_MOVES[pace]):
                moving.set_editor_property("{}_{}".format(pace, way), move_clip(name))

        if own(FIGHT_MOVES["sprint"]):
            moving.set_editor_property("sprint", move_clip(FIGHT_MOVES["sprint"]))

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
    moving.set_editor_property("victory", [own(name) for name in (PACK_VICTORY if not GENERATED_ONLY and own(PACK_VICTORY[0]) else VICTORY) if own(name)])

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
        speed = facts(name).get("speed", 150.0)

        # the walk is a person's: cut from the 100STYLE motion capture (Tools/make_style_motion.py), where there is one.
        # It was walked in a room four metres across, so it is a stroll: the game plays it a little faster than it was walked.
        quickened = 1.0
        cycles = 1

        if pace == "walk" and clip(STYLE_WALK):
            sequence = clip(STYLE_WALK)
            quickened = STROLL / facts(STYLE_WALK).get("speed", 90.0)
            speed = STROLL

        # the jog is Epic's own, captured from a performer and shipped with the engine's template, on the fighting skeleton
        # already: 300 cm/s, held in place by its root lock. Their set has nothing as slow as a walk.
        if pace == "run" and unreal.EditorAssetLibrary.does_asset_exist(EPIC_JOG):
            sequence = unreal.load_asset(EPIC_JOG)
            speed = EPIC_JOG_SPEED
            cycles = EPIC_JOG_CYCLES

        if not sequence:
            missing.append(name)
            continue

        one = unreal.GutMoveClip()
        one.set_editor_property("animation", sequence)
        one.set_editor_property("speed", speed)
        one.set_editor_property("quickened", quickened)
        one.set_editor_property("cycles", cycles)

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

        # performed motion from the packs, where it is in the project: a walk, a jog and a run for a man and for a woman
        paced = {}

        for field, paces in PACES.items():
            clips = {pace: move_clip(name) for pace, name in paces.items()}

            if not all(clips.values()):
                continue

            walked = unreal.GutLocomotionAnimations()

            for way in ("forward", "right", "back", "left"):
                walked.set_editor_property("walk_" + way, clips["walk"])
                walked.set_editor_property("run_" + way, clips["run"])

            walked.set_editor_property("sprint", clips["sprint"])

            defaults.set_editor_property(field, walked)
            paced[field] = clips

        if "at_ease_locomotion" in paced:
            for pace in ("walk", "jog", "run"):
                defaults.set_editor_property("at_ease_{}_speed".format(pace), paced["at_ease_locomotion"][{"walk": "walk", "jog": "run", "run": "sprint"}[pace]].get_editor_property("speed"))

        defaults.set_editor_property("at_ease_families", families())

        # how somebody stands who was not given a way of their own, and the small things anybody standing about does
        defaults.set_editor_property("at_ease_idles", [clip(name) for name in (STYLE_STANDING if clip(STYLE_STANDING[0]) else STANDING) if clip(name)])
        if "at_ease_locomotion" not in paced:
            defaults.set_editor_property("at_ease_walk_speed", STROLL if clip(STYLE_WALK) else 150.0)

        if clip(STYLE_STANDING[0]):
            at_ease.set_editor_property("idle", clip(STYLE_STANDING[0]))
            defaults.set_editor_property("at_ease_stance", at_ease)

        if clip("mc_am_stand_idle_01"):
            at_ease.set_editor_property("idle", clip("mc_am_stand_idle_01"))
            defaults.set_editor_property("at_ease_stance", at_ease)

        defaults.set_editor_property("at_ease_fidgets", [clip(name) for name in FIDGETS if clip(name)])
        defaults.get_editor_property("mesh").set_editor_property("anim_class", unreal.GutAnimInstance.static_class())

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)

    # the people on the deck: each is dealt one way of standing and watching, and cheers a different way each time
    watching = [clip(name) for name in WATCHING if clip(name)] or [clip(name) for name in GENERATED_WATCHING if clip(name)]
    cheering = [clip(name) for name in CHEERING if clip(name)]

    if not GENERATED_ONLY and own(PACK_CHEERING[0]):
        cheering = [own(name) for name in PACK_CHEERING if own(name)]

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Gut/Lvl_Gut")

    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.GutCrowd):
            actor.set_editor_property("idle_animations", watching)
            actor.set_editor_property("cheer_animations", cheering)
            actor.set_editor_property("families", families(CROWD_FAMILIES))

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

    # GUT_MOTION_SURVEY=1: when each of the packs' blows lands, by both ways of finding it, to choose among them
    if os.environ.get("GUT_MOTION_SURVEY"):
        for name in sorted(PACK):
            if not (name.startswith(("kb_m_", "kb_p_", "sw_attack", "sw_2xattack", "sw_kick")) and own(name)):
                continue

            sequence = own(name)
            found = []

            for bone in (("hand_r",) if name.startswith("sw_") else ("hand_l", "hand_r", "foot_l", "foot_r")):
                found.append("{} reach {:.2f} speed {:.2f}".format(bone, struck(sequence, bone, "reach"), struck(sequence, bone, "speed")))

            log("blow {} {:.2f} s: {}".format(name, sequence.get_play_length(), "; ".join(found)))

        log("DONE")
        return

    wire()

    log("DONE")


main()
