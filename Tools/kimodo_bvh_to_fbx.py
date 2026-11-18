"""
Turns the motion clips Tools/make_gut_motion.sh exported (BVH on NVIDIA's SOMA skeleton) into FBX the engine can read.

Run with the Blender that has numpy:
  ~/Repos/scene-maker/.data/tools/blender/blender -b --factory-startup -P Tools/kimodo_bvh_to_fbx.py -- [clip ...]

With no names, every BVH in ArtSource/kimodo/export that is newer than its FBX is converted.
It also writes SK_Soma.fbx once: the skeleton in its T-pose with a speck of mesh on the hips,
because the engine will not take a skeleton that has no mesh. That speck is never shown.

The BVH is in centimetres, so the scene's unit is set to a centimetre and nothing is scaled:
a skeleton exported from a scene in metres arrives in the engine a hundred times too big.
"""

import pathlib
import sys

import bpy

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "ArtSource/kimodo/export"
TARGET = ROOT / "ArtSource/kimodo/fbx"


def empty_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)

    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 0.01


def read_bvh(path):
    bpy.ops.import_anim.bvh(filepath=str(path), axis_forward="-Z", axis_up="Y", global_scale=1.0, frame_start=0,
                            use_fps_scale=False, update_scene_fps=True, update_scene_duration=True, rotate_mode="NATIVE")

    armature = bpy.context.object

    # an armature called anything else arrives in the engine as one more bone above the root
    armature.name = "Armature"
    armature.data.name = "Armature"

    return armature


def write_fbx(path, objects, animated):
    bpy.ops.object.select_all(action="DESELECT")

    for item in objects:
        item.select_set(True)

    bpy.context.view_layer.objects.active = objects[0]

    bpy.ops.export_scene.fbx(
        filepath=str(path), use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
        bake_anim=animated, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, bake_anim_step=1.0,
        mesh_smooth_type="FACE", use_armature_deform_only=False, axis_forward="-Z", axis_up="Y")


def base(any_bvh):
    empty_scene()

    armature = read_bvh(any_bvh)
    armature.animation_data_clear()

    for bone in armature.pose.bones:
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_euler = (0.0, 0.0, 0.0)
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)

    bpy.context.view_layer.update()

    hips = armature.data.bones["Hips"]

    bpy.ops.mesh.primitive_cube_add(size=2.0, location=armature.matrix_world @ hips.head_local)

    speck = bpy.context.object
    speck.name = "SomaSpeck"

    group = speck.vertex_groups.new(name="Hips")
    group.add(list(range(len(speck.data.vertices))), 1.0, "REPLACE")

    speck.parent = armature
    speck.modifiers.new("Armature", "ARMATURE").object = armature

    write_fbx(TARGET / "SK_Soma.fbx", [armature, speck], False)

    feet = [armature.data.bones[name].head_local for name in ("LeftFoot", "RightFoot", "LeftToeBase", "LeftHand", "RightHand", "Head")]

    print("BASE SK_Soma.fbx: {} bones; hips at {}; left foot {}, right foot {}, left toe {}, left hand {}, right hand {}, head {}".format(
        len(armature.data.bones), tuple(round(v, 1) for v in hips.head_local), *[tuple(round(v, 1) for v in spot) for spot in feet]))


def clip(path):
    empty_scene()

    armature = read_bvh(path)

    scene = bpy.context.scene
    first, last = (int(v) for v in armature.animation_data.action.frame_range)

    scene.frame_start = first
    scene.frame_end = last

    write_fbx(TARGET / (path.stem + ".fbx"), [armature], True)

    print("CLIP {}.fbx: frames {} to {} at {} a second".format(path.stem, first, last, scene.render.fps))


def main():
    names = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    TARGET.mkdir(parents=True, exist_ok=True)

    sources = sorted(SOURCE.glob("*.bvh"))

    if not sources:
        sys.exit("Nothing in {}: run Tools/make_gut_motion.sh export first".format(SOURCE))

    if not (TARGET / "SK_Soma.fbx").exists():
        base(sources[0])

    for path in sources:
        target = TARGET / (path.stem + ".fbx")

        if names and path.stem not in names:
            continue

        if not names and target.exists() and target.stat().st_mtime >= path.stat().st_mtime:
            continue

        clip(path)


main()
