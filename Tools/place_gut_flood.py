"""
The flooded floor: brings the photograph of standing water (generated in Mixar, ArtSource/mixar/pool_water) into the project,
makes the one material that shows it (the picture as it is, laid once over the whole floor, wet-smooth so it mirrors the lamps),
and puts the flood into the cage level over the pool floor. Safe to run again: what is there is kept.

Run through Tools/place_gut_flood.sh.
"""

import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "mixar", "pool_water", "pool_water.png")
TARGET = "/Game/Gut/Art/Mixar/pool_water"
LEVEL = "/Game/Gut/Lvl_Gut"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("[place_gut_flood] " + message)


def water_texture():
    path = TARGET + "/pool_water"

    if not eal.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", SOURCE)
        task.set_editor_property("destination_path", TARGET)
        task.set_editor_property("destination_name", "pool_water")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])

        texture = unreal.load_asset(path)
        texture.set_editor_property("srgb", True)
        eal.save_loaded_asset(texture, only_if_is_dirty=False)

        log("imported the photograph of the water")

    return unreal.load_asset(path)


def water_material(texture):
    path = TARGET + "/M_Gut_Water"

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    material = asset_tools.create_asset("M_Gut_Water", TARGET, unreal.Material, unreal.MaterialFactoryNew())

    # the photograph, once across the whole sheet, a little darker: standing water in a deep-sided pool at night
    picture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -600, -200)
    picture.set_editor_property("texture", texture)

    depth = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -600, 60)
    depth.set_editor_property("r", 0.55)

    colour = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(picture, "RGB", colour, "A")
    mel.connect_material_expressions(depth, "", colour, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # still water is as smooth as anything gets; the scum on it is a little less so, which the picture's own brightness says
    smooth = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -600, 200)
    smooth.set_editor_property("r", 0.04)

    scum = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 240)
    scale = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -600, 320)
    scale.set_editor_property("r", 0.9)
    mel.connect_material_expressions(picture, "G", scum, "A")
    mel.connect_material_expressions(scale, "", scum, "B")

    rough = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -100, 220)
    mel.connect_material_expressions(smooth, "", rough, "A")
    mel.connect_material_expressions(scum, "", rough, "B")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    eal.save_loaded_asset(material, only_if_is_dirty=False)

    log("made the water's material")

    return material


def main():
    material = water_material(water_texture())

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    floods = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.GutFlood)]

    if not floods:
        flood = actors.spawn_actor_from_class(unreal.GutFlood, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        flood.set_actor_label("Flood")
        floods = [flood]

        log("put the flood in the cage")

    # the floor it covers is the cage's own
    cages = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.GutCage)]

    for flood in floods:
        flood.set_editor_property("water_material", material)

        if cages:
            flood.set_editor_property("floor_size", cages[0].get_editor_property("floor_size"))
            flood.set_actor_location(cages[0].get_actor_location(), False, False)

        log("{}: floor {}, at {}".format(flood.get_actor_label(), flood.get_editor_property("floor_size"), flood.get_actor_location()))

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()

    log("DONE")


main()
