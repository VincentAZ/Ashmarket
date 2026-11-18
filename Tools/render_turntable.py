"""Review turntable of one exported art asset, rendered in Blender from the same FBX and textures Unreal imports.

    $BLENDER -b --factory-startup -P Tools/render_turntable.py -- <fbx> <texture prefix> <frames dir> [options]

Options: --frames N (one full turn), --only N (render just that frame), --top F / --bottom F (the slice of the
asset's height to frame, 0 = feet, 1 = head), --width W --height H, --samples S.
Needs a Blender whose Python has numpy (the FBX importer uses it); the system one does not.
Staging only: studio lighting and a grey floor for judging the asset. Nothing here goes into the game.
"""
import argparse
import math
import os
import sys

import bpy
from mathutils import Vector



def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("fbx")
    parser.add_argument("textures")
    parser.add_argument("out")
    parser.add_argument("--frames", type=int, default=144)
    parser.add_argument("--only", type=int, default=0)
    parser.add_argument("--top", type=float, default=1.0)
    parser.add_argument("--bottom", type=float, default=0.0)
    parser.add_argument("--width", type=int, default=1080)
    parser.add_argument("--height", type=int, default=1350)
    parser.add_argument("--samples", type=int, default=96)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:])


def image_node(nodes, path, colour):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(path)
    if not colour:
        node.image.colorspace_settings.name = "Non-Color"
    return node


def asset_material(prefix):
    material = bpy.data.materials.new("Review")
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    shader = nodes["Principled BSDF"]
    links.new(image_node(nodes, prefix + "_basecolor.png", True).outputs["Color"], shader.inputs["Base Color"])
    links.new(image_node(nodes, prefix + "_roughness.png", False).outputs["Color"], shader.inputs["Roughness"])
    links.new(image_node(nodes, prefix + "_metallic.png", False).outputs["Color"], shader.inputs["Metallic"])
    normal = nodes.new("ShaderNodeNormalMap")
    links.new(image_node(nodes, prefix + "_normal.png", False).outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    return material


def bounds(meshes):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    low, high = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for mesh in meshes:
        evaluated = mesh.evaluated_get(depsgraph)
        for corner in evaluated.bound_box:
            point = evaluated.matrix_world @ Vector(corner)
            low = Vector(map(min, low, point))
            high = Vector(map(max, high, point))
    return low, high


def area_light(name, location, target, energy, size, colour):
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.size, data.color = energy, size, colour
    light = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    light.rotation_euler = (Vector(target) - Vector(location)).to_track_quat("-Z", "Y").to_euler()


def world():
    scene_world = bpy.data.worlds.new("Studio")
    scene_world.use_nodes = True
    nodes, links = scene_world.node_tree.nodes, scene_world.node_tree.links
    environment = nodes.new("ShaderNodeTexEnvironment")
    environment.image = bpy.data.images.load(os.path.join(bpy.utils.system_resource("DATAFILES"), "studiolights", "world", "studio.exr"))
    lit = nodes["Background"]
    lit.inputs["Strength"].default_value = 0.3
    links.new(environment.outputs["Color"], lit.inputs["Color"])
    # the camera sees plain dark grey behind the asset; everything else is lit by the studio image
    backdrop = nodes.new("ShaderNodeBackground")
    backdrop.inputs["Color"].default_value = (0.03, 0.03, 0.035, 1.0)
    path = nodes.new("ShaderNodeLightPath")
    mix = nodes.new("ShaderNodeMixShader")
    links.new(path.outputs["Is Camera Ray"], mix.inputs["Fac"])
    links.new(lit.outputs["Background"], mix.inputs[1])
    links.new(backdrop.outputs["Background"], mix.inputs[2])
    links.new(mix.outputs["Shader"], nodes["World Output"].inputs["Surface"])
    bpy.context.scene.world = scene_world


def main():
    args = arguments()
    scene = bpy.context.scene
    for existing in list(bpy.data.objects):
        bpy.data.objects.remove(existing)

    bpy.ops.import_scene.fbx(filepath=args.fbx)
    imported = list(scene.objects)
    meshes = [item for item in imported if item.type == "MESH"]
    material = asset_material(args.textures)
    for mesh in meshes:
        mesh.data.materials.clear()
        mesh.data.materials.append(material)
        for polygon in mesh.data.polygons:
            polygon.use_smooth = True

    bpy.context.view_layer.update()
    low, high = bounds(meshes)
    tall = high.z - low.z
    print("[turntable] size %.3f x %.3f x %.3f m" % (high.x - low.x, high.y - low.y, tall))

    pivot = bpy.data.objects.new("Pivot", None)
    scene.collection.objects.link(pivot)
    pivot.location = ((low.x + high.x) / 2, (low.y + high.y) / 2, low.z)
    bpy.context.view_layer.update()
    for item in imported:
        if item.parent is None:
            item.parent = pivot
            item.matrix_parent_inverse = pivot.matrix_world.inverted()
    pivot.rotation_euler = (0, 0, 0)
    pivot.keyframe_insert("rotation_euler", frame=1)
    pivot.rotation_euler = (0, 0, math.tau)
    pivot.keyframe_insert("rotation_euler", frame=args.frames + 1)
    for curve in pivot.animation_data.action.layers[0].strips[0].channelbags[0].fcurves:
        for key in curve.keyframe_points:
            key.interpolation = "LINEAR"

    floor = bpy.data.meshes.new("Floor")
    reach = tall * 6
    floor.from_pydata([(-reach, -reach, 0), (reach, -reach, 0), (reach, reach, 0), (-reach, reach, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("Floor", floor)
    ground.location = (pivot.location.x, pivot.location.y, low.z)
    scene.collection.objects.link(ground)
    grey = bpy.data.materials.new("Floor")
    grey.use_nodes = True
    grey.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.05, 0.05, 0.055, 1.0)
    grey.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.7
    floor.materials.append(grey)

    # frame the chosen slice of the asset, with a margin, from straight in front
    slice_low = low.z + tall * args.bottom
    slice_high = low.z + tall * args.top
    focus = Vector((pivot.location.x, pivot.location.y, (slice_low + slice_high) / 2))
    wide = max(high.x - low.x, high.y - low.y)
    lens, sensor = 85.0, 36.0
    aspect = args.width / args.height
    sensor_tall = sensor / aspect if aspect > 1 else sensor
    sensor_wide = sensor if aspect > 1 else sensor * aspect
    need_tall = (slice_high - slice_low) * 1.18
    need_wide = min(wide, (slice_high - slice_low) * 1.6) * 1.18
    distance = max(need_tall * lens / sensor_tall, need_wide * lens / sensor_wide)
    camera_data = bpy.data.cameras.new("Camera")
    camera_data.lens, camera_data.sensor_width, camera_data.sensor_height = lens, sensor, sensor
    camera_data.sensor_fit = "HORIZONTAL" if aspect > 1 else "VERTICAL"
    camera = bpy.data.objects.new("Camera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = focus + Vector((0, -distance, tall * 0.04))
    camera.rotation_euler = (focus - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera

    world()
    span = max(tall, 0.6)
    area_light("Key", focus + Vector((-span * 1.1, -span * 1.6, span * 0.9)), focus, 130 * span * span, span * 0.9, (1.0, 0.96, 0.9))
    area_light("Rim", focus + Vector((span * 1.3, span * 1.5, span * 0.8)), focus, 110 * span * span, span * 0.6, (0.85, 0.92, 1.0))

    scene.render.engine = "CYCLES"
    preferences = bpy.context.preferences.addons["cycles"].preferences
    preferences.compute_device_type = "OPTIX"
    preferences.get_devices()
    for device in preferences.devices:
        device.use = device.type == "OPTIX"
    scene.cycles.device = "GPU"
    scene.cycles.samples = args.samples
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = args.out.rstrip("/") + "/f_"
    scene.frame_start, scene.frame_end = (args.only, args.only) if args.only else (1, args.frames)
    bpy.ops.render.render(animation=True)
    print("[turntable] DONE")


main()
