"""
Third step of wearing the Gate Warden in (Tools/wear_gate_warden.sh): brings the worn pictures in
(ArtSource/worn/gate_warden/out -> /Game/Gut/Art/Worn/GateWarden), makes a copy of each of his outfit's materials that is
drawn from them, and has his body, and only his, wear those; and has his own face's skin drawn from its worn pictures.
The outfit as bought is not touched: anybody else in it is as clean as before.

GATE_WARDEN_CLEAN=1 puts him back as he was.
"""

import json
import os

import unreal

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(PROJECT, "ArtSource", "worn", "gate_warden", "src")
OUT = os.path.join(PROJECT, "ArtSource", "worn", "gate_warden", "out")
WORN = "/Game/Gut/Art/Worn/GateWarden"

WHO = "Mix04"
BODY = "/Game/Gut/Art/MetaHumans/{0}/BP_{0}".format(WHO)
CLEAN = bool(os.environ.get("GATE_WARDEN_CLEAN"))

library = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

PROPERTIES = [unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL, unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialProperty.MP_METALLIC,
              unreal.MaterialProperty.MP_SPECULAR, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, unreal.MaterialProperty.MP_OPACITY_MASK]


def log(*words):
    unreal.log("[wear_gate_warden] " + " ".join(str(word) for word in words))


def bring_in(file, like):
    """The worn picture as a texture, kept the way the one it was made from is kept."""
    name = file[:-4]
    path = os.path.join(OUT, file)

    if not os.path.exists(path):
        return None

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", WORN)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.load_asset(WORN + "/" + name)
    original = unreal.load_asset(like)

    if not texture or not original:
        return None

    for setting in ("srgb", "compression_settings", "lod_group", "virtual_texture_streaming", "mip_gen_settings"):
        texture.set_editor_property(setting, original.get_editor_property(setting))

    eal.save_loaded_asset(texture, False)

    return texture


def swap(material, start, changes, seen):
    if not start or start.get_path_name() in seen:
        return 0

    seen.add(start.get_path_name())

    count = 0

    if isinstance(start, unreal.MaterialExpressionTextureBase):
        texture = start.get_editor_property("texture")

        if texture and texture.get_path_name() in changes:
            start.set_editor_property("texture", changes[texture.get_path_name()])
            count += 1

    for source in library.get_inputs_for_material_expression(material, start):
        count += swap(material, source, changes, seen)

    return count


def outfit_component(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        thing = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(subsystem.k2_find_subobject_data_from_handle(handle))

        if isinstance(thing, unreal.SkeletalMeshComponent):
            mesh = thing.get_editor_property("skeletal_mesh_asset")

            if mesh and "Outfits" in mesh.get_name():
                return thing

    return None


def main():
    listed = json.load(open(os.path.join(SRC, "pictures.json")))
    blueprint = unreal.load_asset(BODY)
    component = outfit_component(blueprint)

    if not component:
        log("FAILED: no outfit on", BODY)
        return

    mesh = component.get_editor_property("skeletal_mesh_asset")
    slots = len(mesh.get_editor_property("materials"))

    if CLEAN:
        component.set_editor_property("override_materials", [])
    else:
        made = {}
        overrides = [None] * slots

        for entry in listed["materials"]:
            if entry["component"] != component.get_name() and "SkeletalMesh" not in entry["component"]:
                continue

            source = entry["material"]

            if source not in made:
                changes = {}

                for path, facts in entry["pictures"].items():
                    worn = bring_in(facts["file"].replace(".png", "_Worn.png"), path)

                    if worn:
                        changes[path] = worn

                name = source.split(".")[-1] + "_Worn"
                target = WORN + "/" + name

                if eal.does_asset_exist(target):
                    eal.delete_asset(target)

                copy = eal.duplicate_asset(source, target)

                swapped = 0

                for which in PROPERTIES:
                    swapped += swap(copy, library.get_material_property_input_node(copy, which), changes, set())

                library.recompile_material(copy)
                eal.save_loaded_asset(copy, False)

                made[source] = copy

                log(name, "drawn from", swapped, "worn pictures")

            if entry["slot"] < slots:
                overrides[entry["slot"]] = made[source]

        component.set_editor_property("override_materials", overrides)

    # his face's skin, which is his alone
    for facts in listed["skin"]:
        if "Head" not in facts.get("file", "") or facts["parameter"] not in ("Basecolor Baked VT", "SRMF Baked VT"):
            continue

        instance = unreal.load_asset(facts["material"])
        texture = unreal.load_asset(facts["asset"]) if CLEAN else bring_in(facts["file"].replace(".png", "_Worn.png"), facts["asset"])

        if instance and texture:
            library.set_material_instance_texture_parameter_value(instance, facts["parameter"], texture)
            library.update_material_instance(instance)
            eal.save_loaded_asset(instance, False)

            log("face:", facts["parameter"], "is", texture.get_name())

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    eal.save_loaded_asset(blueprint, False)

    log("DONE:", "as bought" if CLEAN else "worn")


main()
