"""
Brings what the town's people say, as ElevenLabs spoke it (Tools/make_town_voices.sh), into the project: every recording
under ArtSource/voices/wav becomes a sound of the same name under /Game/Town/Voices/<place>. Tools/build_town_level.sh
then gives each line and call its recording.

Run through Tools/import_town_voices.sh, which first turns the MP3 files into the WAV files the engine reads.
"""

import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "ArtSource", "voices", "wav")
TARGET = "/Game/Town/Voices"


def log(message):
    unreal.log("[import_town_voices] " + message)


def main():
    tasks = []

    for place in sorted(os.listdir(SOURCE)):
        folder = os.path.join(SOURCE, place)

        if not os.path.isdir(folder):
            continue

        for file in sorted(os.listdir(folder)):
            if not file.endswith(".wav"):
                continue

            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(folder, file))
            task.set_editor_property("destination_path", "{}/{}".format(TARGET, place))
            task.set_editor_property("destination_name", os.path.splitext(file)[0])
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    made = 0

    for task in tasks:
        path = "{}/{}".format(task.get_editor_property("destination_path"), task.get_editor_property("destination_name"))

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            made += 1
        else:
            log("could not import " + path)

    log("{} of {} recordings are in {}".format(made, len(tasks), TARGET))
    log("DONE")


main()
