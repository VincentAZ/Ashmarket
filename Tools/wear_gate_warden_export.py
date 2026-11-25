"""
First step of wearing the Gate Warden in (Tools/wear_gate_warden.sh): writes out, as PNG, every picture his outfit's
materials and his own face and body skin are drawn from, into ArtSource/worn/gate_warden/src, with a list of which
material uses which for what (pictures.json). Nothing in the project is changed.
"""

import json
import os

import unreal

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(PROJECT, "ArtSource", "worn", "gate_warden", "src")

WHO = "Mix04"
BODY = "/Game/Gut/Art/MetaHumans/{0}/BP_{0}".format(WHO)

library = unreal.MaterialEditingLibrary

PROPERTIES = {
    "base_color": unreal.MaterialProperty.MP_BASE_COLOR,
    "normal": unreal.MaterialProperty.MP_NORMAL,
    "roughness": unreal.MaterialProperty.MP_ROUGHNESS,
    "metallic": unreal.MaterialProperty.MP_METALLIC,
    "specular": unreal.MaterialProperty.MP_SPECULAR,
    "occlusion": unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
    "opacity_mask": unreal.MaterialProperty.MP_OPACITY_MASK,
}


def log(*words):
    unreal.log("[wear_gate_warden] " + " ".join(str(word) for word in words))


def samples(material, start, found, seen):
    if not start or start.get_path_name() in seen:
        return

    seen.add(start.get_path_name())

    if isinstance(start, unreal.MaterialExpressionTextureBase):
        texture = start.get_editor_property("texture")

        if texture:
            found.add(texture.get_path_name())

    for source in library.get_inputs_for_material_expression(material, start):
        samples(material, source, found, seen)


def export(path):
    texture = unreal.load_asset(path)
    name = path.split(".")[-1]
    target = os.path.join(OUT, name + ".png")

    if not os.path.exists(target):
        task = unreal.AssetExportTask()
        task.set_editor_property("object", texture)
        task.set_editor_property("filename", target)
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("exporter", unreal.TextureExporterPNG())

        if not unreal.Exporter.run_asset_export_task(task):
            log("could not write", name)
            return None

    return {"asset": path, "file": name + ".png", "srgb": bool(texture.get_editor_property("srgb")),
            "compression": str(texture.get_editor_property("compression_settings")), "width": texture.blueprint_get_size_x(), "height": texture.blueprint_get_size_y()}


def main():
    os.makedirs(OUT, exist_ok=True)

    blueprint = unreal.load_asset(BODY)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    listed = {"materials": [], "skin": []}
    done = set()

    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        thing = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(subsystem.k2_find_subobject_data_from_handle(handle))

        if not isinstance(thing, unreal.SkeletalMeshComponent) or thing.get_name() in done:
            continue

        done.add(thing.get_name())

        mesh = thing.get_editor_property("skeletal_mesh_asset")

        if not mesh:
            continue

        for slot, entry in enumerate(mesh.get_editor_property("materials")):
            material = entry.get_editor_property("material_interface")

            if isinstance(material, unreal.Material):
                uses = {}

                for role, which in PROPERTIES.items():
                    found = set()
                    samples(material, library.get_material_property_input_node(material, which), found, set())

                    if found:
                        uses[role] = sorted(found)

                pictures = {}

                for role, paths in uses.items():
                    for path in paths:
                        pictures.setdefault(path, {"roles": []})["roles"].append(role)

                for path in pictures:
                    pictures[path].update(export(path) or {})

                listed["materials"].append({"component": thing.get_name(), "slot": slot, "slot_name": str(entry.get_editor_property("material_slot_name")), "material": material.get_path_name(), "pictures": pictures})

                log("slot", slot, material.get_name(), {path.split(".")[-1]: facts["roles"] for path, facts in pictures.items()})
            elif isinstance(material, unreal.MaterialInstanceConstant) and ("Skin" in material.get_name() or "Body_Baked" in material.get_name()) and "LOD3" not in material.get_name() and "LOD5" not in material.get_name():
                for value in material.get_editor_property("texture_parameter_values"):
                    texture = value.get_editor_property("parameter_value")
                    name = str(value.get_editor_property("parameter_info").get_editor_property("name"))

                    if texture and "Baked" in name:
                        facts = export(texture.get_path_name()) or {}
                        facts.update({"material": material.get_path_name(), "parameter": name})
                        listed["skin"].append(facts)

                        log("skin", material.get_name(), name, facts.get("file"), facts.get("width"), facts.get("height"))

    with open(os.path.join(OUT, "pictures.json"), "w") as handle:
        json.dump(listed, handle, indent=1)

    log("DONE", len(listed["materials"]), "outfit materials,", len(listed["skin"]), "skin pictures")


main()
