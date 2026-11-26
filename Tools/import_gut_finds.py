"""
Brings in the models of what lies about the town to be picked up (the flask, the scrap, the tin of smokes, the tag),
made in Mixar and written to ArtSource/mixar/find_<name>/find_<name>.glb.

Run headless, with the editor closed:
  UnrealEditor-Cmd Ashmarket.uproject -ExecutePythonScript=Tools/import_gut_finds.py -unattended -nosplash -RenderOffscreen -nosound
A model already in the project is left as it is: delete its folder under /Game/Gut/Art/Mixar to bring it in again.
"""

import os

import unreal

FINDS = ["flask", "scrap", "smokes", "tag"]
# what is seen on the yard's rack when it is kept there, where it is not something found: made in Mixar too, and written
# to ArtSource/mixar/rack_<name>/rack_<name>.gltf (ATownGameMode::ShowRack)
RACK = ["bread", "tea", "splint", "armor"]
ROOT = "/Game/Gut/Art/Mixar"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "mixar")

eal = unreal.EditorAssetLibrary


def log(*words):
    unreal.log("[import_gut_finds] " + " ".join(str(word) for word in words))


def pipelines():
    path = "/Game/Gut/Art/PolyHaven/Import/GLTF_OneMesh"

    if eal.does_asset_exist(path):
        assets = unreal.load_asset(path)
    else:
        assets = eal.duplicate_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline", path)
        assets.get_editor_property("mesh_pipeline").set_editor_property("combine_static_meshes_behavior", unreal.InterchangeCombineStaticMeshesBehavior.ALL)
        eal.save_loaded_asset(assets, only_if_is_dirty=False)

    return [unreal.SoftObjectPath(assets.get_path_name()), unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline")]


def meshes_in(folder):
    if not eal.does_directory_exist(folder):
        return []

    return [asset for asset in eal.list_assets(folder, recursive=True) if isinstance(unreal.load_asset(asset), unreal.StaticMesh)]


def main():
    # (and the bag the player is seen to carry: AGutFighter::WearBag)
    for name in ["find_" + good for good in FINDS] + ["rack_" + good for good in RACK] + ["worn_satchel", "bag_strap"]:
        folder = "{}/{}".format(ROOT, name)
        meshes = meshes_in(folder)

        if not meshes:
            path = os.path.join(SOURCE, name, name + ".glb")

            if not os.path.exists(path):
                path = os.path.join(SOURCE, name, name + ".gltf")

            if not os.path.exists(path):
                log("skipped {}: no {}".format(name, path))
                continue

            parameters = unreal.ImportAssetParameters()
            parameters.set_editor_property("is_automated", True)
            parameters.set_editor_property("replace_existing", True)
            parameters.set_editor_property("override_pipelines", pipelines())
            unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(folder, unreal.InterchangeManager.create_source_data(path), parameters)
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
            meshes = meshes_in(folder)

        if len(meshes) != 1:
            log("{} came in as {} meshes: {}".format(name, len(meshes), meshes))
            continue

        mesh = unreal.load_asset(meshes[0])

        # small things in the street do not need Nanite, and the materials made for a glTF file are not set up for it
        editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        nanite = editor.get_nanite_settings(mesh)

        if nanite.enabled:
            nanite.enabled = False
            editor.set_nanite_settings(mesh, nanite, apply_changes=True)
            eal.save_loaded_asset(mesh, only_if_is_dirty=False)

        bounds = mesh.get_bounds()
        log("{}: {} triangles, {} materials, {:.0f} x {:.0f} x {:.0f} cm, lowest point {:.1f}".format(
            name, mesh.get_num_triangles(0), len(mesh.static_materials), bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0,
            bounds.origin.z - bounds.box_extent.z))

    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED", traceback.format_exc())
