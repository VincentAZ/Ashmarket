"""Gives a floodlight vehicle or a lamp made in Mixar a second material on its glass, so the engine can light the glass.

    $BLENDER -b --factory-startup -P Tools/light_vehicle_lamps.py -- <vehicle.gltf> [--check picture.png] [--strength 40]

The model and its textures are not changed: the faces that are lamp glass are found (high on the model, facing the way the
lamps face, pale in the model's own colour picture, in a patch big enough to be a lamp) and handed to a copy of the model's
material that also glows with that same colour picture. The file is written back where it was read from.
With --check nothing is written: a picture is saved of the model from the front with the chosen faces painted pink.
Needs a Blender whose Python has numpy; the system one does not.
"""
import argparse
import json
import os
import sys

import bmesh
import bpy
import numpy
from mathutils import Vector


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("gltf")
    parser.add_argument("--check", default="")
    parser.add_argument("--strength", type=float, default=40.0)
    # where the lamps are: the share of the model's height above which to look, how squarely a face must look along +X,
    # how pale it must be, and the smallest patch (square metres) that counts as a lamp
    parser.add_argument("--above", type=float, default=0.78)
    parser.add_argument("--facing", type=float, default=0.55)
    parser.add_argument("--pale", type=float, default=0.22)
    parser.add_argument("--patch", type=float, default=0.012)
    # for a single lamp that looks along +X from the origin: the glass is only what lies within this many metres of that line
    parser.add_argument("--within", type=float, default=0.0)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:])


def main():
    options = arguments()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # the file as Mixar wrote it lists its one model in no scene, which this Blender's reader cannot take: read a copy that does
    with open(options.gltf) as source:
        described = json.load(source)
    for scene in described.get("scenes", []):
        scene.setdefault("nodes", list(range(len(described["nodes"]))))
    readable = options.gltf + ".reading.gltf"
    with open(readable, "w") as out:
        json.dump(described, out)
    try:
        bpy.ops.import_scene.gltf(filepath=readable)
    finally:
        os.remove(readable)
    model = next(thing for thing in bpy.context.scene.objects if thing.type == "MESH")
    mesh = model.data
    material = mesh.materials[0]
    colour_node = next(node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED").inputs["Base Color"].links[0].from_node
    picture = colour_node.image
    width, height = picture.size
    pixels = numpy.array(picture.pixels[:], dtype=numpy.float32).reshape(height, width, picture.channels)

    turn = model.matrix_world.to_3x3()
    heights = [(model.matrix_world @ vertex.co).z for vertex in mesh.vertices]
    low, high = min(heights), max(heights)
    # the middle of the model as seen from the front, for a lamp whose glass is round that middle
    sides = [(model.matrix_world @ vertex.co).y for vertex in mesh.vertices]
    across = ((min(sides) + max(sides)) * 0.5, (low + high) * 0.5)

    work = bmesh.new()
    work.from_mesh(mesh)
    work.faces.ensure_lookup_table()
    layer = work.loops.layers.uv.active

    def is_glass(face):
        middle = model.matrix_world @ face.calc_center_median()
        if middle.z < low + (high - low) * options.above:
            return False
        if (turn @ face.normal).normalized().x < options.facing:
            return False
        if options.within > 0.0 and ((middle.y - across[0]) ** 2 + (middle.z - across[1]) ** 2) ** 0.5 > options.within:
            return False
        u = sum(loop[layer].uv.x for loop in face.loops) / len(face.loops)
        v = sum(loop[layer].uv.y for loop in face.loops) / len(face.loops)
        r, g, b = pixels[int(v % 1.0 * (height - 1)), int(u % 1.0 * (width - 1)), :3]
        return 0.2126 * r + 0.7152 * g + 0.0722 * b > options.pale

    chosen = {face.index for face in work.faces if is_glass(face)}

    # keep only patches big enough to be a lamp: a pale scratch on a pipe is not one
    kept, seen = set(), set()
    for start in chosen:
        if start in seen:
            continue
        patch, queue = [], [start]
        seen.add(start)
        while queue:
            index = queue.pop()
            patch.append(index)
            for edge in work.faces[index].edges:
                for other in edge.link_faces:
                    if other.index in chosen and other.index not in seen:
                        seen.add(other.index)
                        queue.append(other.index)
        # ...and neither is the pale edge of a roof or a length of pipe: a lamp's glass is about as tall as it is wide
        middles = [model.matrix_world @ work.faces[index].calc_center_median() for index in patch]
        wide = max(middle.y for middle in middles) - min(middle.y for middle in middles)
        tall = max(middle.z for middle in middles) - min(middle.z for middle in middles)
        # (a single lamp marked out by --within needs neither test: everything inside its ring is glass)
        if options.within > 0.0 or (sum(work.faces[index].calc_area() for index in patch) >= options.patch and max(wide, tall) < 3.5 * max(min(wide, tall), 0.01)):
            kept.update(patch)
    print("[lamps] {}: {} faces of {} are lamp glass, {:.3f} m2".format(os.path.basename(options.gltf), len(kept), len(work.faces), sum(work.faces[index].calc_area() for index in kept)))

    glass = material.copy()
    glass.name = material.name + "_lamp_glass"
    mesh.materials.append(glass)
    for index in kept:
        work.faces[index].material_index = 1
    work.to_mesh(mesh)
    work.free()

    shader = next(node for node in glass.node_tree.nodes if node.type == "BSDF_PRINCIPLED")

    if options.check:
        shader.inputs["Emission Color"].default_value = (1.0, 0.0, 0.6, 1.0)
        shader.inputs["Emission Strength"].default_value = 5.0
        scene = bpy.context.scene
        camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
        scene.collection.objects.link(camera)
        scene.camera = camera
        size = max(model.dimensions)
        camera.location = Vector((size * 1.6, -size * 0.9, high + size * 0.15)) * (3.0 if options.within > 0.0 else 1.0)
        aim = Vector((0.0, 0.0, low + (high - low) * 0.6)) - camera.location
        camera.rotation_euler = aim.to_track_quat("-Z", "Y").to_euler()
        sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        sun.data.energy = 3.0
        sun.rotation_euler = camera.rotation_euler
        scene.collection.objects.link(sun)
        scene.render.engine = "BLENDER_EEVEE_NEXT"
        scene.render.resolution_x, scene.render.resolution_y = 1280, 900
        scene.render.filepath = options.check
        bpy.ops.render.render(write_still=True)
        return

    # the glass glows with its own colour picture
    glass.node_tree.links.new(shader.inputs["Base Color"].links[0].from_socket, shader.inputs["Emission Color"])
    shader.inputs["Emission Strength"].default_value = options.strength
    bpy.ops.export_scene.gltf(filepath=options.gltf, export_format="GLTF_SEPARATE", export_image_format="JPEG", export_texture_dir="textures", export_keep_originals=True, use_selection=False, export_apply=False)


main()
