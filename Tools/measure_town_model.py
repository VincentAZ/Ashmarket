"""Measures models exported from Mixar, and writes down where each one glows.

    $BLENDER -b --factory-startup -P Tools/measure_town_model.py -- <model.gltf> [<model.gltf> ...]

Prints each model's footprint in bands of height (what of it is lower than a knee, a waist, a head), which is what a
place's list needs to stand it: how far its awnings reach, where its counter is, where its feet are.
For a model whose glass or embers Tools/light_vehicle_lamps.py has made to glow, it also writes <model>.glow.json beside
the model: the middle of the glow and the way it looks, in Blender's own metres. Tools/build_town_level.py puts a
thing's light there, so that whatever lights the street is something that is seen to burn.
Needs a Blender whose Python can read glTF (the one Tools/light_vehicle_lamps.py uses).
"""
import bpy, sys, json, os
from mathutils import Vector
for path in sys.argv[sys.argv.index("--") + 1:]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    d = json.load(open(path))
    for scene in d.get("scenes", []):
        scene.setdefault("nodes", list(range(len(d["nodes"]))))
    tmp = path + ".reading.gltf"; json.dump(d, open(tmp, "w"))
    try: bpy.ops.import_scene.gltf(filepath=tmp)
    finally: os.remove(tmp)
    o = next(t for t in bpy.context.scene.objects if t.type == "MESH")
    pts = [o.matrix_world @ v.co for v in o.data.vertices]
    name = os.path.basename(path)[:-5]
    zmax = max(p.z for p in pts)
    print("[m]", name, "top", round(zmax, 2))
    for a, b in ((0, .3), (.3, 1.0), (1.0, 2.0), (2.0, 3.0), (3.0, 4.5), (4.5, 99)):
        s = [p for p in pts if a <= p.z < b]
        if s:
            print("[m]   z %.1f-%.1f  x %.2f..%.2f  y %.2f..%.2f  n %d" % (a, min(b, zmax), min(p.x for p in s), max(p.x for p in s), min(p.y for p in s), max(p.y for p in s), len(s)))
    # glowing faces, if any
    if len(o.data.materials) > 1:
        c = Vector(); n = Vector(); area = 0.0
        for f in o.data.polygons:
            if f.material_index == 1:
                c += (o.matrix_world @ f.center) * f.area; n += (o.matrix_world.to_3x3() @ f.normal) * f.area; area += f.area
        if area:
            c /= area
            print("[m]   glow centre %.3f %.3f %.3f  normal %.2f %.2f %.2f  area %.3f" % (c.x, c.y, c.z, *(n.normalized()), area))
            json.dump({"centre": [round(v, 4) for v in c], "look": [round(v, 3) for v in n.normalized()], "area": round(area, 4)}, open(path[:-5] + ".glow.json", "w"))
