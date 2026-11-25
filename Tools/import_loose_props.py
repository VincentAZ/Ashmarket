"""
Brings the small things that lie about the town's places into the project: scanned props from Poly Haven (CC0), fetched
as published by Tools/fetch_polyhaven.py into ArtSource/polyhaven/<name>/. Each comes in through the engine's own glTF
importer as one mesh, is given a simple shape to be struck and to fall over with if the importer gave it none, and its
size is logged (Tools/build_town_level.py's LOOSE gives each its weight).

    Tools/import_loose_props.sh

Nothing is made here: the models and their pictures are as Poly Haven publishes them.
"""
import os

import unreal

MODELS = "/Game/Gut/Art/PolyHaven"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "polyhaven")

# (metal_trash_can is three bins in a row as one model, and rusted_wheel_rim_01 does not come in as a mesh: neither is used)
PROPS = ["plastic_jerrycan", "plastic_bottle_gallon", "plastic_crate_02", "can_rusted", "oil_tin", "metal_stool_02", "plastic_monobloc_chair_01",
         "cardboard_box_01", "trashbag", "cement_bag"]

# What was loose already (Tools/build_town_level.py's LOOSE): given the same kind of shape, so that all of them are struck
# and land alike.
ALREADY = ["old_tyre", "wooden_crate_02", "metal_jerrycan", "propane_tank", "Barrel_01", "barrel_03"]

# square things are struck as a box; the rest as a hull of 26 faces, which is near enough round to roll
BOXES = {"wooden_crate_02", "plastic_crate_02", "cardboard_box_01", "metal_jerrycan", "plastic_jerrycan", "oil_tin", "cement_bag"}

eal = unreal.EditorAssetLibrary


def log(message):
    unreal.log("[import_loose_props] {}".format(message))


def found(folder):
    if not eal.does_directory_exist(folder):
        return []

    return [path for path in eal.list_assets(folder, recursive=True) if isinstance(unreal.load_asset(path), unreal.StaticMesh)]


def main():
    meshes_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    pipelines = [unreal.SoftObjectPath(MODELS + "/Import/GLTF_OneMesh.GLTF_OneMesh"), unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline")]

    for name in PROPS + ALREADY:
        folder = "{}/{}".format(MODELS, name)
        meshes = found(folder)

        if not meshes:
            path = os.path.join(SOURCE, name, name + ".gltf")

            if not os.path.exists(path):
                log("skipped {}: not downloaded".format(name))
                continue

            parameters = unreal.ImportAssetParameters()
            parameters.set_editor_property("is_automated", True)
            parameters.set_editor_property("replace_existing", True)
            parameters.set_editor_property("override_pipelines", pipelines)
            unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(folder, unreal.InterchangeManager.create_source_data(path), parameters)
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
            meshes = found(folder)

        if len(meshes) != 1:
            log("{} came in as {} meshes: left alone".format(name, len(meshes)))
            continue

        mesh = unreal.load_asset(meshes[0])

        nanite = meshes_editor.get_nanite_settings(mesh)

        if nanite.enabled:
            nanite.enabled = False
            meshes_editor.set_nanite_settings(mesh, nanite, apply_changes=True)

        meshes_editor.remove_collisions(mesh)
        meshes_editor.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX if name in BOXES else unreal.ScriptCollisionShapeType.NDOP26)

        shapes = meshes_editor.get_simple_collision_count(mesh)

        eal.save_loaded_asset(mesh, only_if_is_dirty=False)

        bounds = mesh.get_bounds()

        log("{}: {} triangles, {} simple shapes, {:.0f} x {:.0f} x {:.0f} cm, centre {:.0f} {:.0f} {:.0f}".format(
            name, mesh.get_num_triangles(0), shapes, bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0, bounds.origin.x, bounds.origin.y, bounds.origin.z))

    log("DONE")


main()
