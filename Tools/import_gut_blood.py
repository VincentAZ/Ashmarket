"""
Brings the blood's pictures into the project and makes what shows them. Run through Tools/import_gut_blood.sh.

Every picture is a photograph of red liquid made in Mixar, with nothing behind it (ArtSource/mixar/blood/<name>.png):
nothing here draws blood. This only says how each picture is shown:
  - blood_drop, blood_drop_2 ...   a drop in the air: the picture on a small card that faces the camera (M_Gut_BloodDrop).
                                   Until one is made, blood_splash stands in for it.
  - anything else                  laid on the floor or on a body as a decal, the way the pool and the splash already are
                                   (M_Gut_Decal, made by Tools/import_gut_art.py).
A picture already in the project is left alone unless its file has changed since.
"""

import os
import traceback

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

ROOT = "/Game/Gut/Art/Blood"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "mixar", "blood")


def log(text):
    unreal.log("[import_gut_blood] " + text)


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def drop_material():
    """What a drop in the air is shown with: its picture, cut out by the picture's own transparency, wet."""
    path = ROOT + "/M_Gut_BloodDrop"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    material = asset_tools.create_asset("M_Gut_BloodDrop", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.4)

    picture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    picture.set_editor_property("parameter_name", "Picture")
    wet = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 300)
    wet.set_editor_property("parameter_name", "Roughness")
    wet.set_editor_property("default_value", 0.15)

    mel.connect_material_property(picture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(picture, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(wet, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)

    log("made M_Gut_BloodDrop")

    return material


def picture(name):
    """A picture's texture: the one in the project, or the file brought in."""
    path = "{}/{}".format(ROOT, name)
    source = os.path.join(SOURCE, name + ".png")
    stamp = os.path.join(PROJECT, "Content", path[len("/Game/"):] + ".uasset")

    # the one in the project, unless the file has been changed since it was brought in
    if eal.does_asset_exist(path) and (not os.path.exists(source) or (os.path.exists(stamp) and os.path.getmtime(stamp) >= os.path.getmtime(source))):
        return unreal.load_asset(path)

    if not os.path.exists(source):
        return None

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])

    texture = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
    save(texture)

    return texture


def shown(name, parent, texture):
    path = "{}/MI_Gut_{}".format(ROOT, name)

    if eal.does_asset_exist(path):
        # there already: but if it shows another picture (the splash stood in for the drop until one was made), it is given its own
        instance = unreal.load_asset(path)

        if mel.get_material_instance_texture_parameter_value(instance, "Picture") != texture:
            mel.set_material_instance_texture_parameter_value(instance, "Picture", texture)
            mel.update_material_instance(instance)
            save(instance)

            log("MI_Gut_{} now shows its own picture".format(name))

        return False

    instance = asset_tools.create_asset("MI_Gut_" + name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(instance, "Picture", texture)
    mel.update_material_instance(instance)
    save(instance)

    log("made MI_Gut_" + name)

    return True


def main():
    # a picture kept as it was made, beside the one that was cleaned (Tools/clean_blood_picture.py), is not one to bring in
    names = sorted(os.path.splitext(file)[0] for file in os.listdir(SOURCE) if file.endswith(".png") and not file.endswith("_as_made.png")) if os.path.isdir(SOURCE) else []
    drops = [name for name in names if name.startswith("blood_drop")]

    for name in names:
        texture = picture(name)

        if not texture:
            continue

        if name in drops:
            shown(name, drop_material(), texture)
        elif not eal.does_asset_exist("{}/MI_Gut_{}".format(ROOT, name)):
            shown(name, unreal.load_asset(ROOT + "/M_Gut_Decal"), texture)

    # with no picture of a drop made yet, the splash stands in for one
    if not drops:
        splash = picture("blood_splash")

        if splash:
            shown("blood_drop", drop_material(), splash)

    log("DONE: {} pictures, {} of them drops".format(len(names), len(drops)))


try:
    main()
except Exception:
    log("FAILED\n" + traceback.format_exc())
