"""Cuts the two ends off the tow chain made in Mixar, so that the middle of it can be hung from real links.

    $BLENDER -b --factory-startup -P Tools/cut_chain_ends.py -- [--handle 0.165] [--hook 1.49]

Reads ArtSource/mixar/tow_chain/tow_chain.gltf and writes two models beside it with the same textures:
tow_chain_handle (everything below the first cut, where it was) and tow_chain_hook (everything above the second,
moved down so that the cut is at its foot). The cuts are closed with a flat face, and the stub of the link each cut went through is taken away.
Nothing is modelled here: both pieces are the chain's own geometry.
"""
import argparse
import json
import os
import sys

import bmesh
import bpy

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ArtSource", "mixar")


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--handle", type=float, default=0.165)
    parser.add_argument("--hook", type=float, default=1.49)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])


def read():
    """The whole chain, as one object standing on the origin."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = os.path.join(HERE, "tow_chain", "tow_chain.gltf")
    # the file as Mixar wrote it lists a scene with nothing in it, which this Blender's reader cannot take: read a copy without that
    with open(path) as source:
        described = json.load(source)
    for scene in described.get("scenes", []):
        scene.setdefault("nodes", list(range(len(described["nodes"]))))
    readable = path + ".reading.gltf"
    with open(readable, "w") as out:
        json.dump(described, out)
    try:
        bpy.ops.import_scene.gltf(filepath=readable)
    finally:
        os.remove(readable)
    chain = next(thing for thing in bpy.context.scene.objects if thing.type == "MESH")
    bpy.context.view_layer.objects.active = chain
    chain.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return chain


def cut(name, height, keep_above):
    chain = read()
    work = bmesh.new()
    work.from_mesh(chain.data)
    # the reader splits the surface along every seam of its picture: join it up again so that the cut leaves one clean rim
    bmesh.ops.remove_doubles(work, verts=work.verts, dist=0.00001)
    everything = work.verts[:] + work.edges[:] + work.faces[:]
    result = bmesh.ops.bisect_plane(work, geom=everything, plane_co=(0.0, 0.0, height), plane_no=(0.0, 0.0, -1.0 if keep_above else 1.0), clear_outer=True)
    rim = [element for element in result["geom_cut"] if isinstance(element, bmesh.types.BMEdge)]
    bmesh.ops.holes_fill(work, edges=rim)
    # the cut also leaves the stub of whichever link it went through. A real link hangs there now, so the stub goes:
    # it is any separate piece that does not reach well past the cut into the end being kept
    work.verts.ensure_lookup_table()
    seen, stubs = set(), []
    for start in work.verts:
        if start.index in seen:
            continue
        piece, queue = [], [start]
        seen.add(start.index)
        while queue:
            vertex = queue.pop()
            piece.append(vertex)
            for edge in vertex.link_edges:
                other = edge.other_vert(vertex)
                if other.index not in seen:
                    seen.add(other.index)
                    queue.append(other)
        reach = max(vertex.co.z for vertex in piece) - height if keep_above else height - min(vertex.co.z for vertex in piece)
        if reach < (0.10 if keep_above else height * 0.4):
            stubs.extend(piece)
    bmesh.ops.delete(work, geom=stubs, context="VERTS")
    if keep_above:
        bmesh.ops.translate(work, verts=work.verts, vec=(0.0, 0.0, -height))
    work.to_mesh(chain.data)
    work.free()
    chain.name = name
    chain.data.name = name
    folder = os.path.join(HERE, name)
    os.makedirs(folder, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(folder, name + ".gltf"), export_format="GLTF_SEPARATE", export_image_format="JPEG", export_texture_dir="textures")
    print("[cut] {}: {} faces, {:.1f} x {:.1f} x {:.1f} cm".format(name, len(chain.data.polygons), *(size * 100.0 for size in chain.dimensions)))


options = arguments()
cut("tow_chain_handle", options.handle, False)
cut("tow_chain_hook", options.hook, True)
