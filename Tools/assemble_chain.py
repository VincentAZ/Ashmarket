"""Puts the tow chain together again at the size of a real one, out of the pieces made in Mixar.

    $BLENDER -b --factory-startup -P Tools/assemble_chain.py -- [--link 7.0] [--links 31] [--grip 0.55] [--hook 0.6]

The chain as it came from Mixar has links 18 cm long, which no chain a person could swing has. This reads the three pieces
already cut from it and made beside it (tow_chain_handle, chain_link, tow_chain_hook) and writes, in ArtSource/mixar:
  chain_link_small      the one link, at the given length and with fewer faces
  tow_chain_grip        the taped end, made smaller: what the fist closes on
  tow_chain_hook_small  the hook, made smaller
  tow_chain_fine        the whole chain for the rack and the floor: the grip, a run of the link at the given length (cm), the hook
Nothing is modelled here. Every piece is the Mixar model's own geometry and picture, made smaller and set end to end,
each link a quarter turn from the last as the links of a chain lie. The game hangs the same link the same way while the chain is in a hand.
It prints the numbers Tools/import_gut_art.py needs for CHAIN.
"""
import argparse
import json
import math
import os
import sys

import bpy

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ArtSource", "mixar")

# how much of a link's length lies between the middle of one link and the middle of the next: the rest is the two thicknesses of bar
PITCH_SHARE = 11.4 / 18.0

# how far up the grip, as it was made, the first link hangs from (cm)
GRIP_TOP = 12.0


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--link", type=float, default=7.0)
    parser.add_argument("--links", type=int, default=31)
    parser.add_argument("--grip", type=float, default=0.55)
    parser.add_argument("--hook", type=float, default=0.6)
    parser.add_argument("--faces", type=float, default=0.16)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])


def read(name):
    """One of the pieces, as one object with its shape as made."""
    path = os.path.join(HERE, name, name + ".gltf")
    # the file as Mixar wrote it lists a scene with nothing in it, which this Blender's reader cannot take: read a copy without that
    with open(path) as source:
        described = json.load(source)
    for scene in described.get("scenes", []):
        scene.setdefault("nodes", list(range(len(described["nodes"]))))
    readable = path + ".reading.gltf"
    with open(readable, "w") as out:
        json.dump(described, out)
    before = set(bpy.context.scene.objects)
    try:
        bpy.ops.import_scene.gltf(filepath=readable)
    finally:
        os.remove(readable)
    piece = next(thing for thing in bpy.context.scene.objects if thing not in before and thing.type == "MESH")
    for thing in list(bpy.context.scene.objects):
        if thing not in before and thing is not piece:
            bpy.data.objects.remove(thing)
    select([piece])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return piece


def select(things):
    bpy.ops.object.select_all(action="DESELECT")
    for thing in things:
        thing.select_set(True)
    bpy.context.view_layer.objects.active = things[0]


def copy(piece, scale, height, turn=0.0):
    made = piece.copy()
    made.data = piece.data.copy()
    bpy.context.scene.collection.objects.link(made)
    made.scale = (scale, scale, scale)
    made.rotation_euler = (0.0, 0.0, turn)
    made.location = (0.0, 0.0, height)
    select([made])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return made


def write(things, name):
    select(things)
    if len(things) > 1:
        bpy.ops.object.join()
    whole = bpy.context.view_layer.objects.active
    whole.name = name
    whole.data.name = name
    select([whole])
    folder = os.path.join(HERE, name)
    os.makedirs(folder, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(folder, name + ".gltf"), export_format="GLTF_SEPARATE", export_image_format="JPEG",
                              export_texture_dir="textures", use_selection=True)
    print("[chain] {}: {} faces, {:.1f} x {:.1f} x {:.1f} cm".format(name, len(whole.data.polygons), *(size * 100.0 for size in whole.dimensions)))
    return whole


options = arguments()

bpy.ops.wm.read_factory_settings(use_empty=True)

grip = read("tow_chain_handle")
link = read("chain_link")
hook = read("tow_chain_hook")

# a link 18 cm long was made with enough faces to be looked at that size. At this size a sixth of them is plenty,
# and thirty of them hang on every chain.
select([link])
thinner = link.modifiers.new("Decimate", "DECIMATE")
thinner.ratio = options.faces
bpy.ops.object.modifier_apply(modifier=thinner.name)

link_made = link.dimensions.z * 100.0
pitch = options.link * PITCH_SHARE
start = GRIP_TOP * options.grip

write([copy(link, options.link / link_made, 0.0)], "chain_link_small")
write([copy(grip, options.grip, 0.0)], "tow_chain_grip")
write([copy(hook, options.hook, 0.0)], "tow_chain_hook_small")

pieces = [copy(grip, options.grip, 0.0)]

for index in range(options.links):
    # the link is made standing on its middle
    pieces.append(copy(link, options.link / link_made, (start + pitch * (index + 0.5)) / 100.0, math.radians(90.0) * ((index + 1) % 2)))

pieces.append(copy(hook, options.hook, (start + pitch * options.links) / 100.0))

write(pieces, "tow_chain_fine")

print("[chain] CHAIN numbers: first link hangs {:.2f} cm up the grip, {} links, {:.2f} cm from one to the next, each {:.1f} cm long; the fist closes {:.1f} cm up".format(
    start, options.links, pitch, options.link, 7.0 * options.grip))
