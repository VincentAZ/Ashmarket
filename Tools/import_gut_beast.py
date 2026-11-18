"""
Brings the pit boss's dog into the project under /Game/Gut/Art/Beast and puts it to use in the cage.

It makes nothing itself. The dog is a model generated in Mixar from a reference picture, given a skeleton there by Mixar's
own rigging (Tripo), made lighter and exported as ArtSource/mixar/pit_dog/pit_dog.glb with the one clip Mixar has for a
four-legged skeleton, a walk. Its other clips come from ArtSource/beast (see SOURCES.md there): made by people on another
four-legged skeleton and carried over to this one by the engine's own retargeter. This script only imports, retargets and wires.

Run it through Tools/import_gut_beast.sh, which starts the editor with no window. GUT_BEAST_WIRE=1 also writes the two
data assets (the dog and its bite) and gives the dog to the cage's game mode; without it only the model and clips come in.
"""

import json
import os
import traceback

import unreal

ROOT = "/Game/Gut/Art/Beast"
DOG = ROOT + "/Dog"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MODEL = os.path.join(PROJECT, "ArtSource", "mixar", "pit_dog", "pit_dog.glb")

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log("[import_gut_beast] " + text)


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def of_class(folder, kind):
    if not eal.does_directory_exist(folder):
        return []

    found = []

    for path in eal.list_assets(folder, recursive=True):
        asset = unreal.load_asset(path)

        if isinstance(asset, kind):
            found.append(asset)

    return found


def dog_model():
    """The dog's skinned mesh, importing the glTF file through the engine's own importer if it is not in the project yet."""
    meshes = of_class(DOG, unreal.SkeletalMesh)

    if not meshes:
        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property("is_automated", True)
        parameters.set_editor_property("replace_existing", True)

        unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(DOG, unreal.InterchangeManager.create_source_data(MODEL), parameters)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

        meshes = of_class(DOG, unreal.SkeletalMesh)

    if len(meshes) != 1:
        raise RuntimeError("the dog came in as {} skinned meshes".format(len(meshes)))

    return meshes[0]


def describe(mesh):
    bounds = mesh.get_bounds()
    skeleton = mesh.get_editor_property("skeleton")

    log("dog: {} with skeleton {}, {:.0f} x {:.0f} x {:.0f} cm, centre {:.0f} {:.0f} {:.0f}, {} materials".format(
        mesh.get_path_name(), skeleton.get_name() if skeleton else None,
        bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0, bounds.origin.x, bounds.origin.y, bounds.origin.z,
        len(mesh.get_editor_property("materials"))))

    # where its bones are when it stands as it was made: which way it faces is read from this, not assumed
    pose = unreal.AnimationLibrary if False else None

    names = [str(name) for name in unreal.SkeletalMeshEditorSubsystem.get_default_object().get_all_socket_names(mesh)] if False else []

    reference = mesh.get_editor_property("skeleton").get_reference_pose()
    bones = [str(name) for name in reference.get_bone_names()]

    def place(bone):
        return reference.get_ref_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation

    for bone in bones:
        if "Head_2" in bone or "Spine_0" in bone or bone.endswith("Root") or "Limb_3" in bone:
            at = place(bone)
            log("  bone {}: {:.1f} {:.1f} {:.1f}".format(bone, at.x, at.y, at.z))

    log("  {} bones: {}".format(len(bones), ", ".join(bones)))

    for clip in of_class(DOG, unreal.AnimSequence):
        log("  clip {}: {:.2f} s".format(clip.get_path_name(), clip.get_play_length()))

    for material in mesh.get_editor_property("materials"):
        log("  material {}".format(material.get_editor_property("material_interface").get_path_name() if material.get_editor_property("material_interface") else None))


# where the dog's clips come from: models by other people that came with clips, on four-legged skeletons of their own.
# Only the clips are used; the models are brought in because a clip needs the skeleton it was made on.
SOURCES = {
    "Hyena": os.path.join(PROJECT, "ArtSource", "beast", "source", "objaverse_sketchfab_canines", "Hyena_Kapi777_f69e2799cada4e188701a7f3933138f3.glb"),
    "Wolf": os.path.join(PROJECT, "ArtSource", "beast", "source", "polypizza_quaternius_animals", "Wolf_P1gU3Qkr9r.glb"),
}


def source_model(name):
    folder = "{}/Source/{}".format(ROOT, name)
    meshes = of_class(folder, unreal.SkeletalMesh)

    if not meshes:
        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property("is_automated", True)
        parameters.set_editor_property("replace_existing", True)

        unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(folder, unreal.InterchangeManager.create_source_data(SOURCES[name]), parameters)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

        meshes = of_class(folder, unreal.SkeletalMesh)

    if not meshes:
        raise RuntimeError("{} came in with no skinned mesh".format(name))

    # the one with the most bones is the animal; anything else in the file is not wanted
    meshes.sort(key=lambda mesh: -len(mesh.get_editor_property("skeleton").get_reference_pose().get_bone_names()))

    return meshes[0]


def describe_source(name, mesh):
    folder = "{}/Source/{}".format(ROOT, name)
    skeleton = mesh.get_editor_property("skeleton")
    reference = skeleton.get_reference_pose()
    bounds = mesh.get_bounds()

    log("{}: {} with skeleton {}, {:.0f} x {:.0f} x {:.0f} cm".format(name, mesh.get_path_name(), skeleton.get_path_name(),
        bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0))

    for bone in reference.get_bone_names():
        at = reference.get_ref_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation
        parent = reference.get_bone_parent_name(bone) if hasattr(reference, "get_bone_parent_name") else ""
        log("  bone {} <- {}: {:.1f} {:.1f} {:.1f}".format(bone, parent, at.x, at.y, at.z))

    for clip in of_class(folder, unreal.AnimSequence):
        log("  clip {}: {:.2f} s on {}".format(clip.get_name(), clip.get_play_length(), clip.get_editor_property("skeleton").get_name() if hasattr(clip, "get_editor_property") else ""))


def bone_names(mesh):
    return [str(name) for name in mesh.get_editor_property("skeleton").get_reference_pose().get_bone_names()]


def find_bone(mesh, part):
    """The one bone whose name has this in it: the sources' bones carry prefixes and numbers that are nobody's business."""
    names = bone_names(mesh)

    if part in names:
        return part

    found = [name for name in names if part in name]

    if len(found) != 1:
        raise RuntimeError("{} bones match {} in {}: {}".format(len(found), part, mesh.get_name(), found))

    return found[0]


# the dog's skeleton is Mixar's (Tripo's): its named bones, and the unnamed ones that are its tail, its shoulder blades and,
# on the left, its whole hind leg (Tripo put that leg's name on four bones bunched at the hip that hold the belly, and its own
# walk never moves the leg: which is why that walk is not used). Tripo also left the root bone lying on the floor under the
# belly; since the spine, the hind legs and the tail all hang from it, it was moved in Mixar to where the hips are, between
# the hip joints, before the model was exported. Nothing else about the skeleton or the skin was touched.
DOG_ROOT = "tripo__Root"
DOG_CHAINS = [
    ("Spine", "tripo__Spine_0", "tripo__Spine_3"),
    ("Head", "tripo__Head_0", "tripo__Head_2"),
    ("FrontBladeRight", "bone_12", "bone_12"),
    ("FrontBladeLeft", "bone_17", "bone_17"),
    ("FrontRight", "tripo__0_Right_Limb_0", "tripo__0_Right_Limb_3"),
    ("FrontLeft", "tripo__0_Left_Limb_0", "tripo__0_Left_Limb_3"),
    ("HindRight", "tripo__1_Right_Limb_0", "tripo__1_Right_Limb_3"),
    ("HindLeft", "bone_30", "bone_33"),
    ("Tail", "bone_1", "bone_4"),
]

# the same parts on each source skeleton, by a piece of each bone's name: the root, then first and last bone of each chain
SOURCE_CHAINS = {
    "Hyena": ("hyena_ROOT", [
        ("Spine", "Spine_01", "Spine_Top"),
        ("Head", "Neck_01", "Head_Top"),
        ("FrontBladeRight", "r_Clavicle", "r_Clavicle"),
        ("FrontBladeLeft", "l_Clavicle", "l_Clavicle"),
        ("FrontRight", "r_FrontLeg_Hip", "r_FrontLeg_Ball"),
        ("FrontLeft", "l_FrontLeg_Hip", "l_FrontLeg_Ball"),
        ("HindRight", "r_HindLeg_Hip", "r_HindLeg_Ankle"),
        ("HindLeft", "l_HindLeg_Hip", "l_HindLeg_Ankle"),
        ("Tail", "Tail_01_01", "Tail_01_05"),
    ]),

    # a stylised wolf, for the one thing the hyena does not do: die. Its legs have fewer bones than the dog's, so each is
    # carried along the whole leg and not joint for joint.
    "Wolf": ("Back", [
        ("Spine", "Torso", "Torso3"),
        ("Head", "Neck1", "Head"),
        ("FrontBladeRight", "FrontShoulder_R", "FrontShoulder_R"),
        ("FrontBladeLeft", "FrontShoulder_L", "FrontShoulder_L"),
        ("FrontRight", "FrontUpperLeg_R", "FrontLowerLeg_R"),
        ("FrontLeft", "FrontUpperLeg_L", "FrontLowerLeg_L"),
        ("HindRight", "BackLeg_R", "BackLowerLeg_R"),
        ("HindLeft", "BackLeg_L", "BackLowerLeg_L"),
        ("Tail", "Tail1", "Tail8"),
    ]),
}


def ik_rig(name, mesh, root, chains, feet=None):
    folder = mesh.get_path_name().rsplit("/", 2)[0]
    path = "{}/IK_Gut_{}".format(folder, name)

    # the launcher removes the rigs and retargeters before the editor starts: deleted from inside it, they come back broken
    rig = asset_tools.create_asset("IK_Gut_{}".format(name), folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)

    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("the IK rig does not fit {}".format(mesh.get_name()))

    if not controller.set_retarget_root(root):
        raise RuntimeError("{} has no bone {} to be its root".format(name, root))

    # the dog's four feet are held where the source's feet are by the engine's full-body solver: joint angles alone, carried from
    # an animal with other proportions, put a crouching dog's feet through the floor
    solver = None

    if feet:
        log("  solver types: {}".format(", ".join(str(kind) for kind in controller.get_solver_types())))

        solver = controller.add_solver(unreal.IKRigFullBodyIKSolver.static_struct().get_path_name())
        log("  {} solver: {} ({})".format(name, solver, controller.get_solver_type(solver) if solver is not None and solver >= 0 else None))

        if solver is None or solver < 0 or not controller.set_root_bone(root, solver):
            raise RuntimeError("no solver for {}".format(name))

    for chain, first, last in chains:
        goal = ""

        if feet and chain in feet:
            goal = str(controller.add_new_goal(chain + "Foot", last))

            if not controller.connect_goal_to_solver(goal, solver):
                raise RuntimeError("could not give {} to the solver".format(goal))

        made = controller.add_retarget_chain(chain, first, last, goal)
        log("  {} chain {}: {} to {} ({}){}".format(name, chain, first, last, made, ", foot held by " + goal if goal else ""))

    save(rig)

    return rig


def retargeter(name, source_mesh, source_rig, dog, dog_rig, turn, travel=False):
    path = "{}/RTG_Gut_{}_to_Dog".format(DOG, name)

    asset = asset_tools.create_asset("RTG_Gut_{}_to_Dog".format(name), DOG, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(asset)

    source = unreal.RetargetSourceOrTarget.SOURCE
    target = unreal.RetargetSourceOrTarget.TARGET

    controller.set_ik_rig(source, source_rig)
    controller.set_ik_rig(target, dog_rig)
    controller.set_preview_mesh(source, source_mesh)
    controller.set_preview_mesh(target, dog)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    log("  chain map: {}".format(", ".join("{}<-{}".format(key, value) for key, value in controller.get_chain_map().items())))

    # the two animals have to stand facing the same way before one can follow the other: the dog is turned to face as the source does
    if turn:
        controller.set_rotation_offset_for_retarget_pose_bone(DOG_ROOT, unreal.Rotator(roll=0.0, pitch=0.0, yaw=turn).quaternion(), target)

    for index in range(controller.get_num_retarget_ops()):
        op_name = str(controller.get_op_name(index))
        op_controller = controller.get_op_controller(index)
        settings = op_controller.get_settings()

        if op_name == "Pelvis Motion":
            # the clips are played on the spot: the game moves the dog, as it moves every fighter
            settings.set_editor_property("scale_horizontal", 1.0 if travel else 0.0)
            op_controller.set_settings(settings)

        if op_name == "Root Motion":
            # the dog's root bone is its hips: nothing else is to move it
            settings.set_editor_property("enabled", False)
            op_controller.set_settings(settings)

    log("  ops: {}".format(", ".join(str(controller.get_op_name(index)) for index in range(controller.get_num_retarget_ops()))))

    for index in range(controller.get_num_retarget_ops()):
        settings = controller.get_op_controller(index).get_settings()

        for prop in ("chains_to_retarget", "chains", "chains_to_affect"):
            try:
                log("  {} {}: {}".format(controller.get_op_name(index), prop, str(settings.get_editor_property(prop))[:900]))
            except BaseException:
                pass

    save(asset)

    return asset


def retarget(wanted, source_mesh, dog, rtg):
    """The engine's own batch retarget, for {dog clip name: source clip}. It writes beside the source clip; each result is moved to the dog's clips."""
    clips_folder = DOG + "/Clips"

    for name in wanted:
        if eal.does_asset_exist(clips_folder + "/" + name):
            eal.delete_asset(clips_folder + "/" + name)

    for clip in wanted.values():
        beside = clip.get_path_name().rsplit("/", 1)[0] + "/" + clip.get_name() + "_Dog"

        if eal.does_asset_exist(beside):
            eal.delete_asset(beside)

    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [unreal.AssetRegistryHelpers.create_asset_data(clip) for clip in wanted.values()], source_mesh, dog, rtg,
        search="", replace="", prefix="", suffix="_Dog", include_referenced_assets=False)

    by_source = {clip.get_name() + "_Dog": name for name, clip in wanted.items()}
    results = {}

    for data in made:
        made_path = str(data.package_name)
        name = by_source.get(made_path.rsplit("/", 1)[-1])

        if not name:
            continue

        if not eal.rename_asset(made_path, clips_folder + "/" + name):
            raise RuntimeError("could not move {} to {}".format(made_path, clips_folder))

        results[name] = unreal.load_asset(clips_folder + "/" + name)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    return results


WATCHED = [DOG_ROOT, "tripo__Spine_0", "tripo__Head_2", "tripo__0_Right_Limb_3", "tripo__0_Left_Limb_3", "tripo__1_Right_Limb_3", "bone_33"]


def describe_clip(name, sequence):
    """Where the hips, the head and the four feet are through a clip, in the body's own space: the lowest and highest each gets, and where it starts."""
    length = sequence.get_play_length()
    steps = 12
    seen = {bone: [] for bone in WATCHED}

    for step in range(steps + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, length * step / steps, unreal.AnimPoseEvaluationOptions())

        for bone in WATCHED:
            seen[bone].append(unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation)

    parts = []

    for bone in WATCHED:
        spots = seen[bone]
        parts.append("{} starts ({:.0f}, {:.0f}, {:.0f}), height {:.0f} to {:.0f}, strays {:.0f}".format(
            bone.replace("tripo__", ""), spots[0].x, spots[0].y, spots[0].z, min(spot.z for spot in spots), max(spot.z for spot in spots),
            max(((spot.x - spots[0].x) ** 2 + (spot.y - spots[0].y) ** 2) ** 0.5 for spot in spots)))

    log("clip {}: {:.2f} s; {}".format(name, length, "; ".join(parts)))


# the dog's clips: its name for each, the animal it was made on, and a piece of that clip's name there
CLIPS = {
    "dog_stand": ("Hyena", "f3loopFightIdle_"),
    "dog_idle": ("Hyena", "33138f3loopIdle_"),
    "dog_down": ("Hyena", "933138f3LieDown"),
    "dog_howl": ("Hyena", "38f3loopHowling"),
    "dog_death": ("Wolf", "P1gU3Qkr9rDeath"),
    "dog_wolf_idle": ("Wolf", "P1gU3Qkr9rIdle_2_HeadLow"),
    "dog_run": ("Hyena", "loopRunning"),
    "dog_walk": ("Hyena", "f3loopWalk_"),
    "dog_bite": ("Hyena", "loopAttackJaw"),
    "dog_leap": ("Hyena", "loopAttackJump"),
    "dog_hurt": ("Hyena", "loopHitLeft"),
    "dog_hurt_right": ("Hyena", "loopHitRight"),
    "dog_hurt_back": ("Hyena", "loopHitBack"),
}

# the chains whose ends the engine's solver would hold where the source's are. Tried and not used: with these two skeletons it
# lifted the whole dog a third of a metre off the floor, and joint angles alone keep the feet within a few centimetres of it
# in every clip chosen. GUT_BEAST_FEET=1 tries it again.
FEET = ("FrontRight", "FrontLeft", "HindRight", "HindLeft") if os.environ.get("GUT_BEAST_FEET", "0") == "1" else None

# how each source has to be turned to face the way the dog's own model faces (degrees), and its hips' height (cm), for sizing travel
FACING = {"Hyena": -90.0, "Wolf": -90.0}


MOTION = "/Game/Gut/Art/Motion"
LISTED = {entry["name"]: entry for entry in json.load(open(os.path.join(PROJECT, "Tools", "gut_motion_clips.json")))}

# how the cage's brain fights for a dog: it never guards, it comes on fast and often, it will not stand off, and it is quick
# to go for somebody who has just missed
DOG_TUNING = dict(attack_chance=0.6, crowded_attack_chance=0.5, guard_chance=0.0, block_light_chance=0.0, block_heavy_chance=0.0,
                  guard_after_hit_chance=0.0, dodge_heavy_chance=0.35, counter_chance=0.0, punish_chance=0.7, heavy_chance=0.3,
                  heavy_chance_against_guard=0.5, max_light_chain=2, min_decision_interval=0.5, max_decision_interval=1.2,
                  approach_speed_scale=1.0, circle_speed_scale=0.5, retreat_stamina=8.0, recovered_stamina=40.0, max_retreat_time=1.5)


def bite_attack(label, zone, **numbers):
    result = unreal.GutAttackDef()
    result.set_editor_property("label", label)
    result.set_editor_property("hit_zone", zone)
    result.set_editor_property("damage_type", unreal.GutDamageType.BLADED)

    for name, value in numbers.items():
        result.set_editor_property(name, value)

    return result


def data_asset(name, folder, kind):
    path = "{}/{}".format(folder, name)

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", kind)

    return asset_tools.create_asset(name, folder, kind, factory)


# who on the ladder has dogs of its own: how many, the line its card gains, and its health with them at heel
HEEL = {"The Kennel Master": (1, "The Kennel Master does not come in alone.", 95.0)}


def heel():
    """Gives the fighters named above their dogs, on the ladders as they stand. Only when asked (GUT_BEAST_WIRE=1): it changes
    assets that every running copy of the game has open."""
    if not os.environ.get("GUT_BEAST_WIRE"):
        return

    for path in eal.list_assets("/Game/Gut/Ladder", recursive=False):
        ladder = unreal.load_asset(path)

        if not isinstance(ladder, unreal.GutLadderData):
            continue

        rungs = ladder.get_editor_property("rungs")
        changed = False

        for index in range(len(rungs)):
            rung = rungs[index]
            wanted = HEEL.get(str(rung.get_editor_property("opponent_name")))

            if not wanted:
                continue

            dogs, line, health = wanted
            card = [str(text) for text in rung.get_editor_property("card")]

            if line not in card:
                card.append(line)

            rung.set_editor_property("dogs", dogs)
            rung.set_editor_property("card", card)
            rung.set_editor_property("health", health)

            rungs[index] = rung
            changed = True

            log("{} brings {} dog(s), with {:.0f} health".format(rung.get_editor_property("opponent_name"), dogs, health))

        if changed:
            ladder.set_editor_property("rungs", rungs)
            save(ladder)


def wire(mesh):
    """The dog's bite and the dog itself, as the two data assets the game reads. Rewritten each time: the numbers live here."""
    zone = unreal.GutHitZone

    # what it bites with. A snap at whatever is nearest, low or at the weapon arm, and a leap for the chest that knocks back.
    snap = dict(damage=12.0, stamina_cost=8.0, windup=0.3, active=0.12, recovery=0.42, reach=115.0, half_angle=50.0,
                lunge_distance=45.0, stun_time=0.35, knockback=15.0, block_stamina_damage=6.0)

    bite = data_asset("DA_Gut_Bite", "/Game/Gut/Weapons", unreal.GutWeaponData)
    bite.set_editor_property("display_name", "Teeth")
    bite.set_editor_property("description", "What a dog has.")
    bite.set_editor_property("sound_kind", "teeth")
    bite.set_editor_property("light_attacks", [
        bite_attack("Bite", zone.RIGHT_LEG, **snap),
        bite_attack("Bite", zone.LEFT_LEG, **snap),
        bite_attack("Bite", zone.RIGHT_ARM, **snap),
    ])
    bite.set_editor_property("heavy_attack", bite_attack("Leap", zone.TORSO,
        damage=24.0, stamina_cost=22.0, windup=0.45, active=0.15, recovery=0.6, reach=150.0, half_angle=45.0,
        lunge_distance=200.0, stun_time=0.7, knockback=120.0, block_stamina_damage=25.0))

    # somebody a dog has beaten is brought down by it: one captured take of a person pulled over onto the back, cut in two
    # where the back meets the floor. Until that take is in the project, the clips of the club's execution and the cut throat.
    taken, death = ("taken_mauled", "death_mauled") if eal.does_asset_exist(MOTION + "/death_mauled") else ("taken_kneel", "death_throat")

    throat = unreal.GutFinisherDef()
    throat.set_editor_property("label", "Throat")
    throat.set_editor_property("windup", 1.6)
    throat.set_editor_property("recovery", 0.6)

    # it goes in close first: the one it has beaten falls away from it
    throat.set_editor_property("reach", 45.0)
    throat.set_editor_property("taken_animation", unreal.load_asset(MOTION + "/" + taken))
    throat.set_editor_property("taken_impact_time", LISTED.get(taken, {}).get("lands", 0.0))
    throat.set_editor_property("death_animation", unreal.load_asset(MOTION + "/" + death))
    throat.set_editor_property("death_limp_time", LISTED.get(death, {}).get("limp", 0.0))

    bite.set_editor_property("clean_finisher", throat)
    bite.set_editor_property("slow_finisher", throat)
    save(bite)

    def clip(name):
        return unreal.load_asset(DOG + "/Clips/" + name) if eal.does_asset_exist(DOG + "/Clips/" + name) else None

    dog = data_asset("DA_Gut_Dog", "/Game/Gut/Beast", unreal.GutBeastData)
    dog.set_editor_property("display_name", "A dog")
    dog.set_editor_property("mesh", mesh)

    # a big dog: the model is a metre long as it came, and its clips face along the model's own side
    dog.set_editor_property("scale", 1.15)
    dog.set_editor_property("turn", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))

    # its own brindle, a darker dog, and a sandier one: the same coat, lighter or darker
    dog.set_editor_property("coats", [unreal.LinearColor(1.0, 1.0, 1.0, 1.0), unreal.LinearColor(0.42, 0.4, 0.4, 1.0), unreal.LinearColor(1.0, 0.86, 0.68, 1.0)])

    dog.set_editor_property("stand", clip("dog_stand"))
    dog.set_editor_property("move", clip("dog_run"))
    dog.set_editor_property("move_clip_speed", 480.0)
    dog.set_editor_property("walk", clip("dog_walk"))
    dog.set_editor_property("walk_clip_speed", 110.0)
    dog.set_editor_property("walk_under_speed", 200.0)
    dog.set_editor_property("strike", clip("dog_bite"))
    dog.set_editor_property("strike_impact_time", 0.35)
    dog.set_editor_property("leap", clip("dog_leap"))
    dog.set_editor_property("leap_impact_time", 0.36)
    dog.set_editor_property("maul", clip("dog_bite"))
    dog.set_editor_property("hurt", clip("dog_hurt"))
    # it dies as the wolf's clip has a wolf die: over onto its side. The wolf stands taller in its clips than the skeleton it
    # was made on, so the clip begins with the dog's hips a hand too high: it is begun at the moment they are back at the dog's own height.
    death = clip("dog_death") or clip("dog_down")
    death_start = 0.0

    if death and death.get_name() == "dog_death":
        standing = mesh.get_editor_property("skeleton").get_reference_pose().get_ref_bone_pose(DOG_ROOT, unreal.AnimPoseSpaces.WORLD).translation.z
        length = death.get_play_length()

        for step in range(61):
            at = length * step / 60.0
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(death, at, unreal.AnimPoseEvaluationOptions())

            if unreal.AnimPoseExtensions.get_bone_pose(pose, DOG_ROOT, unreal.AnimPoseSpaces.WORLD).translation.z <= standing + 1.0:
                death_start = at
                break

        log("the dog's death is begun {:.2f} s into the wolf's, of {:.2f} s".format(death_start, length))

    dog.set_editor_property("death", death)
    dog.set_editor_property("death_start_time", death_start)
    dog.set_editor_property("bite", bite)
    dog.set_editor_property("health", 80.0)
    dog.set_editor_property("speed_scale", 1.25)

    tuning = dog.get_editor_property("tuning")

    for name, value in DOG_TUNING.items():
        tuning.set_editor_property(name, value)

    dog.set_editor_property("tuning", tuning)
    dog.set_editor_property("voice", "dog")
    dog.set_editor_property("idle_sound", "dog_growl")
    dog.set_editor_property("idle_sound_gap", 3.0)
    save(dog)

    missing = [name for name in ("stand", "move", "walk", "strike", "leap", "hurt", "death") if not dog.get_editor_property(name)]

    log("the dog: {} cm long, health {:.0f}, bites for {:.0f} and leaps for {:.0f}{}".format(
        round(100 * 1.15), dog.get_editor_property("health"), snap["damage"], 24.0, "; MISSING clips: " + ", ".join(missing) if missing else ""))

    heel()

    # the cage finds the dog by where it is kept. Naming it on the game mode as well is done only when asked, because that
    # changes an asset every running copy of the game has open.
    if os.environ.get("GUT_BEAST_WIRE"):
        blueprint = unreal.load_asset("/Game/Gut/Blueprints/BP_GutGameMode")
        defaults = unreal.get_default_object(blueprint.generated_class())
        defaults.set_editor_property("dog", dog)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
        log("the game mode names the dog")


def main():
    mesh = dog_model()

    describe(mesh)

    dog_rig = ik_rig("Dog", mesh, DOG_ROOT, DOG_CHAINS, feet=FEET)

    reference = mesh.get_editor_property("skeleton").get_reference_pose()
    dog_hips = reference.get_ref_bone_pose("tripo__Spine_0", unreal.AnimPoseSpaces.WORLD).translation.z
    dog_root = reference.get_ref_bone_pose(DOG_ROOT, unreal.AnimPoseSpaces.WORLD).translation.z

    for name, (root, chains) in SOURCE_CHAINS.items():
        source = source_model(name)

        named = [(chain, find_bone(source, first), find_bone(source, last)) for chain, first, last in chains]
        root_bone = find_bone(source, root)
        rig = ik_rig(name, source, root_bone, named)

        source_hips = source.get_editor_property("skeleton").get_reference_pose().get_ref_bone_pose(root_bone, unreal.AnimPoseSpaces.WORLD).translation.z

        log("{}: hips at {:.0f} cm against the dog's {:.0f}".format(name, source_hips, dog_root))

        rtg = retargeter(name, source, rig, mesh, dog_rig, FACING.get(name, 0.0))

        folder = "{}/Source/{}".format(ROOT, name)
        there = of_class(folder, unreal.AnimSequence)
        wanted = {}

        for dog_name, (animal, part) in CLIPS.items():
            if animal != name:
                continue

            found = [clip for clip in there if part in clip.get_name() and not clip.get_name().endswith("_Dog")]

            if len(found) != 1:
                raise RuntimeError("{} clips of {} match {}: {}".format(len(found), name, part, [clip.get_name() for clip in found]))

            wanted[dog_name] = found[0]

        for dog_name, clip in sorted(retarget(wanted, source, mesh, rtg).items()):
            describe_clip(dog_name, clip)

    wire(mesh)

    log("DONE")


try:
    main()
except Exception:
    for line in traceback.format_exc().splitlines():
        log("ERROR " + line)
