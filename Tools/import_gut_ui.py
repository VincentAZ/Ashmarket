"""
Brings the surfaces the game's screens are drawn on into the project: photographs of scratched black steel, a wiped slate
and a hoarding, generated in Mixar (ArtSource/mixar/ui) and imported as they are into /Game/Gut/Art/UI.
The two typefaces (ArtSource/fonts, from Google Fonts under their open licences) are not imported: the game reads the
font files themselves from Content/Gut/Fonts.

Run through Tools/import_gut_ui.sh.
"""

import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "mixar", "ui")
TARGET = "/Game/Gut/Art/UI"


def log(message):
    unreal.log("[import_gut_ui] " + message)


def main():
    tasks = []

    for file in sorted(os.listdir(SOURCE)):
        if not file.endswith(".png"):
            continue

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SOURCE, file))
        task.set_editor_property("destination_path", TARGET)
        task.set_editor_property("destination_name", os.path.splitext(file)[0])
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for task in tasks:
        name = task.get_editor_property("destination_name")
        texture = unreal.load_asset(TARGET + "/" + name)

        if not texture:
            log("could not import " + name)
            continue

        # drawn flat on the screen at whatever size the panel is: no smaller copies, no streaming, colours as painted
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("srgb", True)

        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

        log("{}: {} x {}".format(name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))

    log("DONE")


main()
