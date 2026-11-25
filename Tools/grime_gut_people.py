"""
Dirties what came too clean (Vincent, 21 November: "They can be more dirty / grimy").

  - the white T-shirt under the Survivor outfit's jacket (the yard hand, Brass): its colour picture, stained down with scans of
    leak stains (ArtSource/grime/T_Survivor_Tshirt_Grimed.png, made from the outfit's own picture), is put in the outfit's
    own material in place of the clean one. Everybody who wears the outfit wears the same shirt.
  - the tea seller's hair, which its preset dyes pink over bleached blonde: no dye, and the dull brown of hair that is not washed.

Run with Tools/grime_gut_people.sh, with the editor closed. It changes materials only and builds nobody.
"""

import os

import unreal

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRIME = "/Game/Gut/Art/Grime"
SHIRT = "/Game/Outfits/SurvivorOutfit/SurvivorOutfit/Materials/MI_Survivor_Tshirt"

# whose hair is dulled, and how dark: 0 is white, 1 is black
DULL_HAIR = {"Jelena": 0.62}

library = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(*words):
    unreal.log("[grime_gut_people] " + " ".join(str(word) for word in words))


def bring_in(name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(PROJECT, "ArtSource", "grime", name + ".png"))
    task.set_editor_property("destination_path", GRIME)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(GRIME + "/" + name)


def main():
    shirt = unreal.load_asset(SHIRT)
    picture = bring_in("T_Survivor_Tshirt_Grimed")

    if shirt and picture:
        library.set_material_instance_texture_parameter_value(shirt, "BaseTexture", picture)
        library.update_material_instance(shirt)
        eal.save_loaded_asset(shirt, only_if_is_dirty=False)
        log("the Survivor outfit's T-shirt is", picture.get_path_name())
    else:
        log("NOT DONE: the Survivor outfit's T-shirt", shirt, picture)

    for who, dark in DULL_HAIR.items():
        folder = "/Game/Gut/Art/MetaHumans/%s/Grooms" % who

        for path in sorted(eal.list_assets(folder, recursive=False)):
            instance = unreal.load_asset(path.split(".")[0])

            if not isinstance(instance, unreal.MaterialInstanceConstant) or "_Hair_S_" not in instance.get_name() and "MI_WI_Hair_" not in instance.get_name():
                continue

            vectors = {str(name) for name in library.get_vector_parameter_names(instance)}
            scalars = {str(name) for name in library.get_scalar_parameter_names(instance)}

            for name in ("hairDye", "OmbrehairDye", "HighlightshairDye", "RegionhairDye"):
                if name in vectors:
                    library.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

            for name, value in (("hairMelanin", dark), ("OmbreMelanin", dark - 0.06), ("HighlightsMelanin", dark - 0.03), ("RegionMelanin", dark),
                                ("hairRedness", 0.3), ("OmbreRedness", 0.3), ("HighlightsRedness", 0.3), ("RegionRedness", 0.3)):
                if name in scalars:
                    library.set_material_instance_scalar_parameter_value(instance, name, value)

            library.update_material_instance(instance)
            eal.save_loaded_asset(instance, only_if_is_dirty=False)
            log(who, instance.get_name(), "no dye, melanin", dark)

    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED", traceback.format_exc())
