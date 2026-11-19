"""
Brings art made elsewhere into the project under /Game/Gut/Art and puts it to use in the cage.

It makes nothing itself. What it brings in is in ArtSource/mixar (models generated in Mixar from a reference picture and
exported from it as glTF), in ArtSource/ambientcg (photographed stains, CC0, downloaded by Tools/fetch_ambientcg.py and laid
on the pool as decals) and in ArtSource/polyhaven, downloaded by Tools/fetch_polyhaven.py from Poly Haven
(scanned and artist-made, CC0):
- models, as glTF files: the engine's own glTF importer builds the meshes, textures and materials from the file;
- surfaces, as colour / normal / roughness maps: joined in an instance of one plain material that lays them on by world position;
- a photographed night sky, as one .hdr file, which lights the level and is seen behind it.

It then dresses the level with them: where each thing stands is decided here, what it looks like is not.

Run it through Tools/import_gut_art.sh, which starts the editor with no window. Assets already in the project are left as they are.
"""

import os
import random
import traceback

import unreal

ROOT = "/Game/Gut/Art"
MODELS = ROOT + "/PolyHaven"
SURFACES = ROOT + "/Surfaces"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "polyhaven")
MIXAR_MODELS = ROOT + "/Mixar"
MIXAR_SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "mixar")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Poly Haven's name for a surface -> our name for it, and the width in cm that one repeat of it covers in the world (Poly Haven's figure)
SURFACE_ASSETS = {
    "long_white_tiles": ("PoolWall", 127.0),
    "worn_tile_floor": ("PoolFloor", 200.0),
    "rust_coarse_01": ("Rust", 220.0),
    "concrete_floor_worn_001": ("Deck", 300.0),
    "metal_plate_02": ("Plate", 200.0),
}

SKY = "rogland_moonlit_night"

# weapon asset -> model, where it is gripped (cm along the model, from its own origin), its scale,
# and the turn about its long side that faces it the way the cage's weapons face.
# The models stand with their long side up, as the cage's weapons do, so only the grip and the facing have to be found.
WEAPONS = {
    "DA_Gut_Machete": ("machete", 11.0, 1.0, 0.0),
    "DA_Gut_Club": ("rebar_club", -9.0, 1.0, 0.0),
    "DA_Gut_Shield": ("stop_sign_shield", 0.0, 1.0, -90.0),
    "DA_Gut_Chain": ("tow_chain_fine", -3.9, 1.0, 0.0),
}

# the chain in the hand: the taped grip and the hook cut from the chain made in Mixar (Tools/cut_chain_ends.py), and the one link that
# is hung between them over and over, all three brought down to the size of a chain a person could swing by Tools/assemble_chain.py,
# which also sets them end to end as tow_chain_fine for the rack and the floor and prints these numbers. Then: how far up the grip
# the first link hangs (cm), how many links, the distance from one link's middle to the next (cm), and how long a link is (cm).
CHAIN = ("DA_Gut_Chain", "tow_chain_grip", "chain_link_small", "tow_chain_hook_small", 6.6, 31, 4.43, 7.0)

# the pool's own lights, let into its walls a little below where the water stood: how high (cm), and which of every five still burn
POOL_LIGHTS = (238.0, (0, 3))

# the collar: how the ring (which lies flat in its own space) sits round the neck bone, and its size against the model's
COLLAR = ("steel_collar", unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0), unreal.Vector(5.0, 0.0, 0.0), 1.15)

# the rope round tied wrists: exported lying with its opening along Z, its longer way along X and its knot toward UE's +Y
TIE = "wrist_rope"

OTHER_MODELS = ["worn_metal_rack", "portable_generator"]

# what stands about on the deck, behind the crowd and clear of the trucks: model, where (cm from the middle of the pool), and its turn
DECK_CLUTTER = [
    ("Barrel_01", 520.0, 900.0, 0.0), ("barrel_03", 590.0, 970.0, 70.0), ("metal_jerrycan", 470.0, 840.0, 25.0),
    ("old_tyre", -250.0, 930.0, 0.0), ("old_tyre", -330.0, 890.0, 40.0), ("wooden_crate_02", -540.0, 1000.0, 15.0),
    ("concrete_road_barrier", 880.0, 930.0, 45.0), ("concrete_road_barrier", -900.0, 880.0, -40.0),
    ("propane_tank", -660.0, -930.0, 0.0), ("barrel_03", 300.0, -900.0, 10.0), ("wooden_crate_02", 370.0, -980.0, -20.0),
    ("old_tyre", 1010.0, -500.0, 90.0), ("metal_jerrycan", 1160.0, 430.0, -30.0), ("Barrel_01", -1120.0, 320.0, 120.0),
]

# models made in Mixar
FROM_MIXAR = ["rebar_club", "stop_sign_shield", "tow_chain", "steel_collar", "spike_panel", "floor_cable",
              "pool_ladder", "pool_coping", "pool_drain",
              "rubber_sleeve", "rubber_vest", "sign_sleeve", "sign_vest",
              "floodlight_truck", "floodlight_flatbed", "floodlight_wagon", "floodlight_van",
              "chain_link", "tow_chain_handle", "tow_chain_hook", "pool_light",
              "tow_chain_fine", "tow_chain_grip", "chain_link_small", "tow_chain_hook_small", "wrist_rope"]

# armor asset -> the tube it is made of, how much wider than the old box the tube is made, and its depth against its width
ARMOR = {
    "DA_Gut_Armor_RubberVest": ("rubber_vest", 1.6, 0.74),
    "DA_Gut_Armor_RubberSleeves": ("rubber_sleeve", 1.45, 1.0),
    "DA_Gut_Armor_RubberGreaves": ("rubber_sleeve", 1.45, 1.0),
    "DA_Gut_Armor_SignPlate": ("sign_vest", 1.6, 0.74),
    "DA_Gut_Armor_SignBracers": ("sign_sleeve", 1.45, 1.0),
    "DA_Gut_Armor_SignGreaves": ("sign_sleeve", 1.45, 1.0),
}

# blood: photographs of red liquid with the white keyed out (Tools/key_blood_decal.py), as decals
BLOOD = {"blood_material": "blood_pool", "blood_splash_material": "blood_splash"}

# the floodlights: a vehicle on each side of the pool, parked beyond the crowd, its lamps turned on the cage.
# For each: which side, how far along that side it is parked (cm from the middle), and which vehicle.
# Every vehicle's lamps face its own +X.
TRUCKS = [("+y", 160.0, "floodlight_truck"), ("-y", -220.0, "floodlight_flatbed"), ("+x", 90.0, "floodlight_wagon"), ("-x", -130.0, "floodlight_van")]
# how far back from the pool's edge the deck runs, to give them somewhere to park
DECK_DEPTH = 800.0

# what years of standing empty leave on a pool: photographed decals from ambientCG, by where they go.
# Streaks run down from the rim, damp climbs from the foot of each wall and creeps out across the floor, rust runs from steel.
GRIME_SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "ambientcg")
GRIME = {"streaks": ["Leaking001", "Leaking003", "Leaking004"], "damp": ["Leaking007", "Leaking008", "Leaking009"], "rust": ["Leaking006"]}

# what was painted and tiled on the pool when it was a pool, and what has been painted on it since: pictures made in Mixar
# and exported from it cut out (Tools/key_marking.sh lets the tile show where the paint has chipped). Picture -> how shiny it is.
MARKINGS_SOURCE = os.path.join(MIXAR_SOURCE, "markings", "keyed")
MARKINGS = {"gut_decal_name": 0.35, "gut_decal_depth": 0.6, "gut_decal_no_diving": 0.6, "gut_decal_lane": 0.3}

# a model that is a kit of parts -> the one part of it that is used
KIT_PARTS = {"modular_chainlink_fence": "modular_chainlink_fence_double"}


def log(message):
    unreal.log("[import_gut_art] {}".format(message))


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def gltf_pipelines(one_mesh):
    """The importer's own two steps for a glTF file, with one setting fixed: a model in several parts comes in as one mesh, or as its parts."""
    path = MODELS + "/Import/" + ("GLTF_OneMesh" if one_mesh else "GLTF_Parts")
    if eal.does_asset_exist(path):
        assets = unreal.load_asset(path)
    else:
        assets = eal.duplicate_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline", path)
        behavior = unreal.InterchangeCombineStaticMeshesBehavior
        assets.get_editor_property("mesh_pipeline").set_editor_property("combine_static_meshes_behavior", behavior.ALL if one_mesh else behavior.DO_NOT_COMBINE)
        save(assets)
    return [unreal.SoftObjectPath(assets.get_path_name()), unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline")]


def import_model(name, part=None):
    """Returns the model's static mesh (or the named part of a kit), importing the glTF file if it is not in the project yet."""
    folder = "{}/{}".format(MIXAR_MODELS if name in FROM_MIXAR else MODELS, name)

    def found():
        if not eal.does_directory_exist(folder):
            return []
        meshes = [asset for asset in eal.list_assets(folder, recursive=True) if isinstance(unreal.load_asset(asset), unreal.StaticMesh)]
        return [asset for asset in meshes if not part or asset.split(".")[-1] == part]

    meshes = found()

    if not meshes:
        path = os.path.join(MIXAR_SOURCE if name in FROM_MIXAR else SOURCE, name, name + ".gltf")
        if not os.path.exists(path):
            log("skipped {}: not downloaded".format(name))
            return None

        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property("is_automated", True)
        parameters.set_editor_property("replace_existing", True)
        parameters.set_editor_property("override_pipelines", gltf_pipelines(part is None))
        unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(folder, unreal.InterchangeManager.create_source_data(path), parameters)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        meshes = found()

    if len(meshes) != 1:
        raise RuntimeError("{} came in as {} meshes: {}".format(name, len(meshes), meshes))

    mesh = unreal.load_asset(meshes[0])

    # the importer turns Nanite on, which the materials it makes for a glTF file are not set up for: hand props do not need it
    meshes_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    nanite = meshes_editor.get_nanite_settings(mesh)
    if nanite.enabled:
        nanite.enabled = False
        meshes_editor.set_nanite_settings(mesh, nanite, apply_changes=True)
        save(mesh)
    bounds = mesh.get_bounds()
    log("{}: {} triangles, {} materials, {:.0f} x {:.0f} x {:.0f} cm, centre {:.0f} {:.0f} {:.0f}".format(
        name, mesh.get_num_triangles(0), len(mesh.static_materials),
        bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0, bounds.origin.x, bounds.origin.y, bounds.origin.z))
    return mesh


def surface_material():
    """The one material every surface uses: its three maps, laid on by world position so a wall of any size is covered evenly."""
    path = SURFACES + "/M_Gut_Surface"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    material = asset_tools.create_asset("M_Gut_Surface", SURFACES, unreal.Material, unreal.MaterialFactoryNew())

    size = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 0)
    size.set_editor_property("parameter_name", "Size")
    size.set_editor_property("default_value", 100.0)

    def layer(parameter, function, y, sampler, default):
        texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -900, y)
        texture.set_editor_property("parameter_name", parameter)
        texture.set_editor_property("sampler_type", sampler)
        texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/" + default))
        node = mel.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -500, y)
        node.set_material_function(unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/" + function))
        mel.connect_material_expressions(texture, "", node, "TextureObject")
        mel.connect_material_expressions(size, "", node, "TextureSize")
        return node

    colour = layer("BaseColor", "WorldAlignedTexture", -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "DefaultDiffuse")
    normal = layer("Normal", "WorldAlignedNormal", 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "DefaultNormal")
    roughness = layer("Roughness", "WorldAlignedTexture", 300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, "DefaultWhiteGrid")
    mel.connect_material_property(colour, "XYZ Texture", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "XYZ Texture", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, "XYZ Texture", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)
    return material


def import_surface(name, ours, size):
    path = "{}/MI_Gut_{}".format(SURFACES, ours)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    textures = {}
    for kind, parameter in (("basecolor", "BaseColor"), ("normal", "Normal"), ("roughness", "Roughness")):
        source = os.path.join(SOURCE, name, "{}_{}.png".format(name, kind))
        if not os.path.exists(source):
            log("skipped {}: not downloaded".format(name))
            return None
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", "{}/{}".format(SURFACES, ours))
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
        texture.set_editor_property("srgb", kind == "basecolor")
        if kind == "normal":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            # Poly Haven's map has green up; the engine reads green down
            texture.set_editor_property("flip_green_channel", True)
        elif kind == "roughness":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        save(texture)
        textures[parameter] = texture

    instance = asset_tools.create_asset("MI_Gut_" + ours, SURFACES, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", surface_material())
    for parameter, texture in textures.items():
        mel.set_material_instance_texture_parameter_value(instance, parameter, texture)
    mel.set_material_instance_scalar_parameter_value(instance, "Size", size)
    mel.update_material_instance(instance)
    save(instance)
    log("surface {} from {}".format(instance.get_name(), name))
    return instance


def import_sky(name):
    """Returns the photographed sky as a cube map."""
    path = "{}/Sky/{}".format(ROOT, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    source = os.path.join(SOURCE, name, name + ".hdr")
    if not os.path.exists(source):
        log("skipped {}: not downloaded".format(name))
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT + "/Sky")
    task.set_editor_property("automated", True)
    asset_tools.import_asset_tasks([task])
    sky = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
    save(sky)
    log("sky {} ({})".format(sky.get_name(), sky.get_class().get_name()))
    return sky


def decal_material():
    """The one material every decal uses: a picture and its own transparency, laid on the floor. Wet, so it shines."""
    path = ROOT + "/Blood/M_Gut_Decal"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_Gut_Decal", ROOT + "/Blood", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    picture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    picture.set_editor_property("parameter_name", "Picture")
    wet = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 300)
    wet.set_editor_property("parameter_name", "Roughness")
    wet.set_editor_property("default_value", 0.12)
    mel.connect_material_property(picture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(picture, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(wet, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)
    return material


def import_decal(name):
    path = "{}/Blood/MI_Gut_{}".format(ROOT, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    source = os.path.join(MIXAR_SOURCE, "blood", name + ".png")
    if not os.path.exists(source):
        log("skipped {}: not made".format(name))
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT + "/Blood")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
    save(texture)
    instance = asset_tools.create_asset("MI_Gut_" + name, ROOT + "/Blood", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", decal_material())
    mel.set_material_instance_texture_parameter_value(instance, "Picture", texture)
    mel.update_material_instance(instance)
    save(instance)
    log("decal {}".format(instance.get_name()))
    return instance


def lid_fence(fence):
    """The fence as it is used for the roof: the same model, its wire throwing no shadow.
    Wire three millimetres thick, lit by a row of work lamps ten metres off, leaves no shadow that can be seen:
    drawn as hard lines across the fighters' skin it reads as cracks. The frame's shadow stays."""
    path = fence.get_path_name().split(".")[0] + "_roof"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    roof = eal.duplicate_asset(fence.get_path_name().split(".")[0], path)
    meshes_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for index, slot in enumerate(roof.static_materials):
        material = slot.material_interface
        if material and material.get_blend_mode() != unreal.BlendMode.BLEND_OPAQUE:
            meshes_editor.enable_section_cast_shadow(roof, False, 0, index)
            log("roof fence: the wire ({}) throws no shadow".format(material.get_name()))
    save(roof)
    return roof


def grime_material():
    """The one material every stain uses: its colour, how much of it there is, and what it does to the surface under it."""
    path = ROOT + "/Grime/M_Gut_Grime"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_Gut_Grime", ROOT + "/Grime", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    def picture(parameter, y, sampler, default):
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", parameter)
        node.set_editor_property("sampler_type", sampler)
        node.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/" + default))
        return node

    colour = picture("BaseColor", -400, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "DefaultDiffuse")
    opacity = picture("Opacity", -100, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, "DefaultWhiteGrid")
    normal = picture("Normal", 200, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "DefaultNormal")
    roughness = picture("Roughness", 500, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, "DefaultWhiteGrid")
    strength = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, -200)
    strength.set_editor_property("parameter_name", "Strength")
    strength.set_editor_property("default_value", 1.0)
    amount = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -150)
    mel.connect_material_expressions(opacity, "R", amount, "A")
    mel.connect_material_expressions(strength, "", amount, "B")
    mel.connect_material_property(colour, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(amount, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)
    return material


def import_grime(name):
    """Returns a photographed stain as a decal material, and how wide its picture is against its height."""
    path = "{}/Grime/MI_Gut_{}".format(ROOT, name)
    if eal.does_asset_exist(path):
        instance = unreal.load_asset(path)
        colour = mel.get_material_instance_texture_parameter_value(instance, "BaseColor")
        return instance, colour.blueprint_get_size_x() / float(colour.blueprint_get_size_y())

    textures = {}
    for kind, parameter in (("basecolor", "BaseColor"), ("opacity", "Opacity"), ("normal", "Normal"), ("roughness", "Roughness")):
        source = os.path.join(GRIME_SOURCE, name, "{}_{}.jpg".format(name, kind))
        if not os.path.exists(source):
            log("skipped {}: not downloaded".format(name))
            return None, 1.0
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", "{}/Grime/{}".format(ROOT, name))
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
        texture.set_editor_property("srgb", kind == "basecolor")
        if kind == "normal":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif kind != "basecolor":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        # a stain stops at the edge of its picture: it must not wrap round and start again
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        save(texture)
        textures[parameter] = texture

    instance = asset_tools.create_asset("MI_Gut_" + name, ROOT + "/Grime", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", grime_material())
    for parameter, texture in textures.items():
        mel.set_material_instance_texture_parameter_value(instance, parameter, texture)
    mel.update_material_instance(instance)
    save(instance)
    log("grime {}".format(instance.get_name()))
    colour = textures["BaseColor"]
    return instance, colour.blueprint_get_size_x() / float(colour.blueprint_get_size_y())


def import_marking(name, roughness):
    """Returns a painted or tiled marking as a decal material, and how wide its picture is against its height."""
    path = "{}/Markings/MI_Gut_{}".format(ROOT, name)
    if eal.does_asset_exist(path):
        instance = unreal.load_asset(path)
        picture = mel.get_material_instance_texture_parameter_value(instance, "Picture")
        return instance, picture.blueprint_get_size_x() / float(picture.blueprint_get_size_y())
    source = os.path.join(MARKINGS_SOURCE, name + ".png")
    if not os.path.exists(source):
        log("skipped {}: not made".format(name))
        return None, 1.0
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT + "/Markings")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    picture = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
    picture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    picture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    save(picture)
    instance = asset_tools.create_asset("MI_Gut_" + name, ROOT + "/Markings", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", decal_material())
    mel.set_material_instance_texture_parameter_value(instance, "Picture", picture)
    mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
    mel.update_material_instance(instance)
    save(instance)
    log("marking {}".format(instance.get_name()))
    return instance, picture.blueprint_get_size_x() / float(picture.blueprint_get_size_y())


def mark(actors, placed, half_x, half_y, deck):
    """Puts the pool's own markings back on it, and the name its new owners painted over them."""
    busy = []
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith("Mark_"):
            actors.destroy_actor(actor)
        elif isinstance(actor, (unreal.GutWeaponRack, unreal.GutSpikeWall)) or label == "Pool_Ladder" or (label.startswith("Pool_Light") and not label.endswith("Glow")):
            busy.append(actor.get_actor_location())

    marks = {name: import_marking(name, roughness) for name, roughness in MARKINGS.items()}
    if not all(found[0] for found in marks.values()):
        return

    def room(point):
        """How far a spot on a wall is from the nearest thing already hung there."""
        return min([unreal.Vector(point.x - thing.x, point.y - thing.y, 0.0).length() for thing in busy] or [10000.0])

    def best(spots):
        return max(spots, key=room)

    long_walls = [(unreal.Vector(0.0, half_y, 0.0), unreal.Vector(0.0, 1.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), half_x),
                  (unreal.Vector(0.0, -half_y, 0.0), unreal.Vector(0.0, -1.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), half_x)]
    short_walls = [(unreal.Vector(half_x, 0.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0), half_y),
                   (unreal.Vector(-half_x, 0.0, 0.0), unreal.Vector(-1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0), half_y)]

    def paint(label, name, middle, into, height, order=-1):
        material, shape = marks[name]
        # the wall's right, seen from inside the pool, is not the same way along it on every wall: lettering must read from inside
        decal = stain(actors, label, material, middle, into, height * shape, height)
        decal.get_editor_property("decal").set_editor_property("sort_order", order)
        return decal

    # the name, across whichever end wall has most room for it
    middle, into, along, half = max(short_walls, key=lambda wall: room(wall[0]))
    paint("Mark_name", "gut_decal_name", middle + unreal.Vector(0.0, 0.0, deck * 0.52), into, 180.0, order=2)

    # the depth, near the rim toward each end of both long walls and in the middle of the other end wall
    count = 0
    for middle, into, along, half in long_walls:
        for end in (-1.0, 1.0):
            spot = best([middle + along * (end * (half - offset)) for offset in (150.0, 260.0, 370.0)])
            if room(spot) > 140.0:
                paint("Mark_depth_{}".format(count), "gut_decal_depth", spot + unreal.Vector(0.0, 0.0, deck - 52.0), into, 46.0)
                count += 1
    middle, into, along, half = min(short_walls, key=lambda wall: room(wall[0]))
    spot = best([middle + along * offset for offset in (-300.0, 0.0, 300.0)])
    paint("Mark_depth_{}".format(count), "gut_decal_depth", spot + unreal.Vector(0.0, 0.0, deck - 52.0), into, 46.0)

    # the warning, once on each long wall where there is room
    for index, (middle, into, along, half) in enumerate(long_walls):
        spot = best([middle + along * offset for offset in (-380.0, -190.0, 0.0, 190.0, 380.0)])
        if room(spot) > 200.0:
            paint("Mark_warning_{}".format(index), "gut_decal_no_diving", spot + unreal.Vector(0.0, 0.0, deck - 125.0), into, 74.0)

    # lane lines of dark tile along the floor, three of them, each stopping short of the end walls
    material, shape = marks["gut_decal_lane"]
    length = 300.0
    pieces = int((half_x * 2.0 - 200.0) // length)
    for lane, y in enumerate((-half_y * 0.5, 0.0, half_y * 0.5)):
        for piece in range(pieces):
            x = (piece - (pieces - 1) * 0.5) * length
            # every other piece laid the other way round, so the same chipped tile does not come round in step
            turn = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(0.0, 0.0, -1.0), unreal.Vector(0.0, 1.0 if (piece + lane) % 2 else -1.0, 0.0))
            decal = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, 0.0), turn)
            decal.set_actor_label("Mark_lane_{}_{}".format(lane, piece))
            component = decal.get_editor_property("decal")
            component.set_decal_material(material)
            component.set_editor_property("decal_size", unreal.Vector(3.0, length / shape * 0.5, length * 0.5))
            component.set_editor_property("fade_screen_size", 0.0)
            component.set_editor_property("sort_order", -2)
    log("the pool is marked")


def stain(actors, label, material, middle, into, width, height, upside_down=False):
    """Lays one stain on a surface: where its middle is, the way into the surface, and its size along and up the surface."""
    # a decal is thrown along its own X, and its picture's top is toward its own -Y: rolled so that is up the wall
    turn = unreal.MathLibrary.make_rot_from_xy(into, unreal.Vector(0.0, 0.0, 1.0 if upside_down else -1.0))
    decal = actors.spawn_actor_from_class(unreal.DecalActor, middle, turn)
    decal.set_actor_label(label)
    component = decal.get_editor_property("decal")
    component.set_decal_material(material)
    component.set_editor_property("decal_size", unreal.Vector(12.0, height * 0.5, width * 0.5))
    component.set_editor_property("fade_screen_size", 0.0)
    return decal


def soil(actors, placed, half_x, half_y, deck):
    """Stains the pool. Where each stain goes is drawn from a fixed deal, so the pool looks the same every time it is dressed."""
    for actor in placed:
        if actor.get_actor_label().startswith("Grime_"):
            actors.destroy_actor(actor)

    grime = {kind: [found for found in (import_grime(name) for name in names) if found[0]] for kind, names in GRIME.items()}
    if not all(grime.values()):
        return

    deal = random.Random("the pool")
    count = 0

    # each wall: where its middle is, the way into it, and the way along it
    walls = [(unreal.Vector(0.0, half_y, 0.0), unreal.Vector(0.0, 1.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), half_x),
             (unreal.Vector(0.0, -half_y, 0.0), unreal.Vector(0.0, -1.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), half_x),
             (unreal.Vector(half_x, 0.0, 0.0), unreal.Vector(1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0), half_y),
             (unreal.Vector(-half_x, 0.0, 0.0), unreal.Vector(-1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0), half_y)]

    for middle, into, along, half in walls:
        # streaks down from the rim, side by side with the odd gap
        at = -half - deal.uniform(0.0, 120.0)
        while at < half:
            material, shape = deal.choice(grime["streaks"])
            width = deal.uniform(280.0, 460.0)
            height = min(width / shape * deal.uniform(1.0, 1.5), deck - 20.0)
            if deal.random() > 0.15:
                stain(actors, "Grime_{:03d}_streaks".format(count), material, middle + along * (at + width * 0.5) + unreal.Vector(0.0, 0.0, deck - height * 0.5), into, width, height)
                count += 1
            at += width * deal.uniform(0.7, 1.0)

        # damp climbing from the foot of the wall, and the same creeping out across the floor from it
        for flat in (False, True):
            at = -half - deal.uniform(0.0, 150.0)
            while at < half:
                material, shape = deal.choice(grime["damp"])
                width = deal.uniform(320.0, 520.0)
                height = width / shape * deal.uniform(0.9, 1.6)
                if deal.random() > 0.2:
                    if flat:
                        # on the floor: the picture's foot against the wall
                        place = middle + along * (at + width * 0.5) - into * (height * 0.5)
                        decal = actors.spawn_actor_from_class(unreal.DecalActor, place, unreal.MathLibrary.make_rot_from_xy(unreal.Vector(0.0, 0.0, -1.0), unreal.Vector(into.x, into.y, 0.0)))
                        decal.set_actor_label("Grime_{:03d}_floor".format(count))
                        component = decal.get_editor_property("decal")
                        component.set_decal_material(material)
                        component.set_editor_property("decal_size", unreal.Vector(4.0, height * 0.5, width * 0.5))
                        component.set_editor_property("fade_screen_size", 0.0)
                    else:
                        stain(actors, "Grime_{:03d}_damp".format(count), material, middle + along * (at + width * 0.5) + unreal.Vector(0.0, 0.0, height * 0.5), into, width, height)
                    count += 1
                at += width * deal.uniform(0.75, 1.05)

        # rust running down from the rim here and there
        for _ in range(deal.randint(2, 4)):
            material, shape = deal.choice(grime["rust"])
            width = deal.uniform(30.0, 55.0)
            height = width / shape * deal.uniform(0.7, 1.0)
            stain(actors, "Grime_{:03d}_rust".format(count), material, middle + along * deal.uniform(-half + 40.0, half - 40.0) + unreal.Vector(0.0, 0.0, deck - 6.0 - height * 0.5), into, width, height)
            count += 1

    log("the pool is stained: {} decals".format(count))


def light_the_night(placed):
    """Sets the night: work lamps that light the middle of the floor and leave the corners to the dark, air thick enough
    to show their beams, and a camera that sees it as a camera would."""
    for actor in placed:
        label = actor.get_actor_label()
        if isinstance(actor, unreal.SpotLight) and label.startswith("Floodlight"):
            lamp = actor.get_editor_property("light_component")
            lamp.set_editor_property("inner_cone_angle", 16.0)
            lamp.set_editor_property("outer_cone_angle", 40.0)
            lamp.set_editor_property("volumetric_scattering_intensity", 4.0)
        elif isinstance(actor, unreal.ExponentialHeightFog):
            fog = actor.get_editor_property("component")
            fog.set_editor_property("fog_density", 0.015)
            fog.set_editor_property("volumetric_fog_scattering_distribution", 0.7)
            fog.set_editor_property("volumetric_fog_extinction_scale", 1.0)
        elif isinstance(actor, unreal.PostProcessVolume) and label == "NightExposure":
            settings = actor.get_editor_property("settings")
            for name, value in (("bloom_intensity", 0.9), ("film_grain_intensity", 0.22), ("scene_fringe_intensity", 0.3), ("vignette_intensity", 0.6)):
                settings.set_editor_property("override_" + name, True)
                settings.set_editor_property(name, value)
            # a little less colour and a little more bite, as a night photograph has
            settings.set_editor_property("override_color_saturation", True)
            settings.set_editor_property("color_saturation", unreal.Vector4(0.92, 0.92, 0.92, 1.0))
            settings.set_editor_property("override_color_contrast", True)
            settings.set_editor_property("color_contrast", unreal.Vector4(1.08, 1.08, 1.08, 1.0))
            actor.set_editor_property("settings", settings)
        elif label == "NightSky":
            # the sky lights the pool a little and the air hardly at all, or the whole night goes milky
            for sky_light in actor.get_components_by_class(unreal.SkyLightComponent):
                sky_light.set_editor_property("volumetric_scattering_intensity", 0.15)
        elif isinstance(actor, unreal.DirectionalLight):
            actor.get_editor_property("light_component").set_editor_property("volumetric_scattering_intensity", 0.2)
    log("night lit")


def dress(models, surfaces, sky):
    """Stands the imported things in the level in place of the shapes that were built by code."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Gut/Lvl_Gut")
    placed = actors.get_all_level_actors()
    deck = 320.0

    for actor in placed:
        label = actor.get_actor_label()

        if isinstance(actor, unreal.GutCage):
            deck = actor.get_actor_location().z + actor.get_editor_property("wall_height")
            if surfaces.get("PoolWall") and surfaces.get("PoolFloor"):
                actor.set_editor_property("floor_material", surfaces["PoolFloor"])
                actor.set_editor_property("wall_material", surfaces["PoolWall"])
            if surfaces.get("Rust"):
                actor.set_editor_property("bar_material", surfaces["Rust"])
            if surfaces.get("Deck"):
                actor.set_editor_property("deck_material", surfaces["Deck"])
            if models.get("modular_chainlink_fence"):
                actor.set_editor_property("lid_panel_mesh", lid_fence(models["modular_chainlink_fence"]))
        elif isinstance(actor, unreal.GutWeaponRack) and models.get("modular_chainlink_fence"):
            actor.set_editor_property("backing_mesh", models["modular_chainlink_fence"])
        elif isinstance(actor, unreal.GutLiveCable) and models.get("portable_generator"):
            actor.set_editor_property("generator_mesh", models["portable_generator"])
        elif isinstance(actor, unreal.GutSpikeWall) and surfaces.get("Rust"):
            actor.set_editor_property("spike_material", surfaces["Rust"])
            # the plate the spikes come through: scanned steel sheet
            if surfaces.get("Plate"):
                actor.set_editor_property("plate_material", surfaces["Plate"])

    # the four floodlights: each becomes the lamps of a truck parked on the deck behind the crowd, in place of a pole and a box
    if all(models.get(name) for _, _, name in TRUCKS):
        for actor in placed:
            label = actor.get_actor_label()
            if label.startswith("Floodlight") and label.split("_")[-1] in ("Pole", "Head", "Lamp", "Truck"):
                actors.destroy_actor(actor)
            elif isinstance(actor, unreal.GutCage):
                actor.set_editor_property("wall_thickness", DECK_DEPTH)
                half_x, half_y = actor.get_editor_property("floor_size").x * 0.5, actor.get_editor_property("floor_size").y * 0.5

        def lamps_of(vehicle):
            """Where a vehicle's lamps are, in its own space: the middle of its lamp glass (Tools/light_vehicle_lamps.py marks it),
            or failing that the middle of the highest part of it."""
            for index, slot in enumerate(vehicle.static_materials):
                if "lamp_glass" in str(slot.material_slot_name) or (slot.material_interface and "lamp_glass" in slot.material_interface.get_name()):
                    glass = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(vehicle, 0, index)[0]
                    if glass:
                        return unreal.Vector(sum(v.x for v in glass) / len(glass), sum(v.y for v in glass) / len(glass), sum(v.z for v in glass) / len(glass))
            vertices = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(vehicle, 0, 0)[0]
            height = vehicle.get_bounds().box_extent.z * 2.0
            high = [vertex for vertex in vertices if vertex.z > height * 0.85]
            return unreal.Vector(sum(v.x for v in high) / len(high), sum(v.y for v in high) / len(high), sum(v.z for v in high) / len(high))

        lights = sorted((actor for actor in placed if isinstance(actor, unreal.SpotLight) and actor.get_actor_label().startswith("Floodlight")), key=lambda actor: actor.get_actor_label())
        for light_actor, (side, offset, vehicle_name) in zip(lights, TRUCKS):
            vehicle = models[vehicle_name]
            # just behind where the crowd stands (it keeps within 245 cm of the edge)
            back = 275.0 + vehicle.get_bounds().box_extent.x
            # the lamps face the vehicle's own +X: turn that toward the pool
            if side == "+y":
                location, yaw, aim = unreal.Vector(offset, half_y + back, deck), -90.0, unreal.Vector(offset * 0.3, -half_y * 0.35, 0.0)
            elif side == "-y":
                location, yaw, aim = unreal.Vector(offset, -half_y - back, deck), 90.0, unreal.Vector(offset * 0.3, half_y * 0.35, 0.0)
            elif side == "+x":
                location, yaw, aim = unreal.Vector(half_x + back, offset, deck), 180.0, unreal.Vector(-half_x * 0.35, offset * 0.3, 0.0)
            else:
                location, yaw, aim = unreal.Vector(-half_x - back, offset, deck), 0.0, unreal.Vector(half_x * 0.35, offset * 0.3, 0.0)

            parked = actors.spawn_actor_from_object(vehicle, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            parked.set_actor_label(light_actor.get_actor_label() + "_Truck")
            lamps = parked.get_actor_transform().transform_location(lamps_of(vehicle) + unreal.Vector(45.0, 0.0, 0.0))
            light_actor.set_actor_location(lamps, False, False)
            light_actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(lamps, aim), False)
            light = light_actor.get_editor_property("light_component")
            # a row of lamps is not a point: shadows soften with distance
            light.set_editor_property("source_radius", 25.0)
            light.set_editor_property("soft_source_radius", 25.0)
            light.set_editor_property("source_length", 150.0)
            # big work lamps, and farther from the floor than the poles were
            light.set_editor_property("intensity", 60000.0)
            log("{}: {} on the {} side, lamps at {:.0f} {:.0f} {:.0f}".format(light_actor.get_actor_label(), vehicle_name, side, lamps.x, lamps.y, lamps.z))

    for actor in placed:
        if isinstance(actor, unreal.GutSpikeWall) and models.get("spike_panel"):
            actor.set_editor_property("spike_panel_mesh", models["spike_panel"])
        elif isinstance(actor, unreal.GutLiveCable) and models.get("floor_cable"):
            actor.set_editor_property("cable_mesh", models["floor_cable"])

    # the pool's own furniture: coping stones round the rim, the ladder still hooked over it, the drain in the floor
    ladder, coping, drain = models.get("pool_ladder"), models.get("pool_coping"), models.get("pool_drain")
    for actor in placed:
        if actor.get_actor_label().startswith("Pool_"):
            actors.destroy_actor(actor)
    if ladder:
        # the ladder was made with its hooks toward +Y. Which way that came in tells how every model's Y came in.
        vertices = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(ladder, 0, 0)[0]
        height = ladder.get_bounds().box_extent.z * 2.0
        hooks = [vertex.y for vertex in vertices if vertex.z > height * 0.75]
        y_kept = sum(hooks) / max(len(hooks), 1) > 0
        log("models keep their Y on the way in: {}".format(y_kept))
        for actor in placed:
            if isinstance(actor, unreal.GutCage):
                half_x, half_y = actor.get_editor_property("floor_size").x * 0.5, actor.get_editor_property("floor_size").y * 0.5
                if coping:
                    actor.set_editor_property("coping_mesh", coping)
                    actor.set_editor_property("coping_turned", not y_kept)
        # hung on the long wall near a corner, hooks over the rim, feet well off the floor as a pool ladder's are
        depth = ladder.get_bounds().box_extent.y
        hung = actors.spawn_actor_from_object(ladder, unreal.Vector(-half_x + 150.0, half_y + 26.0 - depth, deck + 32.0 - height), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0 if y_kept else 180.0))
        hung.set_actor_label("Pool_Ladder")
        # the wall's stains are on the wall behind it, not on it
        hung.static_mesh_component.set_editor_property("receives_decals", False)
    if drain:
        set_in = actors.spawn_actor_from_object(drain, unreal.Vector(0.0, 0.0, 0.3), unreal.Rotator(0.0, 0.0, 0.0))
        set_in.set_actor_label("Pool_Drain")
        set_in.static_mesh_component.set_editor_property("receives_decals", False)

    # the pool's own lights, let into the walls. A few still burn off the generator; the rest are dead glass.
    fitting = models.get("pool_light")
    if fitting:
        height, burning = POOL_LIGHTS
        hung = [actor.get_actor_location() for actor in placed if isinstance(actor, (unreal.GutWeaponRack, unreal.GutSpikeWall))]
        hung += [actor.get_actor_location() for actor in actors.get_all_level_actors() if actor.get_actor_label() == "Pool_Ladder"]
        # each spot: where on the wall, and the way out of the wall into the pool
        spots = [(unreal.Vector(x, side * half_y, height), unreal.Vector(0.0, -side, 0.0)) for side in (1.0, -1.0) for x in (-half_x * 0.62, 0.0, half_x * 0.62)]
        spots += [(unreal.Vector(side * half_x, y, height), unreal.Vector(-side, 0.0, 0.0)) for side in (1.0, -1.0) for y in (-half_y * 0.6, half_y * 0.6)]
        count = 0
        for spot, out in spots:
            if any(unreal.Vector(spot.x - thing.x, spot.y - thing.y, 0.0).length() < 170.0 for thing in hung):
                continue
            # the fitting's glass looks along its own +X and its rim is at its own origin: turned to look out of the wall,
            # its body sunk in until only the rim and the glass stand proud
            turn = unreal.MathLibrary.make_rot_from_xz(out, unreal.Vector(0.0, 0.0, 1.0))
            set_in = actors.spawn_actor_from_object(fitting, spot + out * 1.0, turn)
            set_in.set_actor_label("Pool_Light_{}".format(count))
            component = set_in.static_mesh_component
            component.set_editor_property("receives_decals", False)
            lit = count % 5 in burning
            if lit:
                lamp = actors.spawn_actor_from_class(unreal.PointLight, spot + out * 14.0, unreal.Rotator(0.0, 0.0, 0.0))
                lamp.set_actor_label("Pool_Light_{}_Glow".format(count))
                light_component = lamp.get_editor_property("light_component")
                light_component.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
                light_component.set_editor_property("intensity", 260.0)
                light_component.set_editor_property("attenuation_radius", 700.0)
                light_component.set_editor_property("source_radius", 12.0)
                # old glass in front of an old bulb: a tired green-white
                light_component.set_editor_property("light_color", unreal.Color(r=205, g=255, b=225, a=255))
                light_component.set_editor_property("cast_shadows", False)
                light_component.set_editor_property("volumetric_scattering_intensity", 2.0)
            elif len(fitting.static_materials) > 1:
                # a dead one: its glass wears the same material as the rest of it, which does not glow
                component.set_material(1, fitting.static_materials[0].material_interface)
            count += 1
        log("pool lights: {} set in the walls".format(count))

    # years of standing empty
    for actor in placed:
        if isinstance(actor, unreal.GutCage):
            size = actor.get_editor_property("floor_size")
            mark(actors, placed, size.x * 0.5, size.y * 0.5, deck)
            soil(actors, placed, size.x * 0.5, size.y * 0.5, deck)
    # the list is stale after that much coming and going
    placed = actors.get_all_level_actors()

    # what stands about on the deck
    for actor in placed:
        if actor.get_actor_label().startswith("Deck_"):
            actors.destroy_actor(actor)
    for index, (model, x, y, turn) in enumerate(DECK_CLUTTER):
        if models.get(model):
            thing = actors.spawn_actor_from_object(models[model], unreal.Vector(x, y, deck + 2.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=turn))
            thing.set_actor_label("Deck_{:02d}_{}".format(index + 1, model))

    # the sky: the photographed one lights the level and is seen behind it, in place of the engine's built sky
    if sky:
        for actor in placed:
            if isinstance(actor, (unreal.SkyLight, unreal.SkyAtmosphere, unreal.VolumetricCloud)) or actor.get_actor_label() == "NightSky":
                actors.destroy_actor(actor)
        backdrop = actors.spawn_actor_from_class(unreal.load_class(None, "/HDRIBackdrop/Blueprints/HDRIBackdrop.HDRIBackdrop_C"), unreal.Vector(0.0, 0.0, -100.0), unreal.Rotator(0.0, 0.0, 0.0))
        # the photograph's ground lies under the pool: any higher and it would roof the pool over
        backdrop.set_actor_label("NightSky")
        backdrop.set_editor_property("Cubemap", sky)
        backdrop.set_editor_property("Intensity", float(os.environ.get("GUT_SKY_INTENSITY", "7.0")))
        backdrop.set_editor_property("Size", 300.0)
        # the haze that shows the floodlights' beams must stop short of the sky, or it hides it
        for actor in placed:
            if isinstance(actor, unreal.ExponentialHeightFog):
                actor.get_editor_property("component").set_editor_property("fog_cutoff_distance", 10000.0)
        log("sky: {}".format(sky.get_name()))

    light_the_night(actors.get_all_level_actors())

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("level dressed")


def wire(models, surfaces):
    """Puts the imported art to use: the weapons in the hand and on the rack, the tiles on the pool."""
    for weapon_name, (model, grip, scale, turn) in WEAPONS.items():
        if models.get(model):
            weapon = unreal.load_asset("/Game/Gut/Weapons/" + weapon_name)
            weapon.set_editor_property("mesh", models[model])
            # the model's own materials
            weapon.set_editor_property("material", None)
            # a shield is worn on the forearm, not carried by its edge
            weapon.set_editor_property("strap_to_forearm", weapon_name == "DA_Gut_Shield")
            weapon.set_editor_property("grip_transform", unreal.Transform(
                location=unreal.Vector(0.0, 0.0, grip * scale), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=turn), scale=unreal.Vector(scale, scale, scale)))
            save(weapon)
            log("{} is {}".format(weapon_name, model))

    # the chain hangs: its links are one real link, repeated
    weapon_name, handle, link, hook, start, count, pitch, length = CHAIN
    if models.get(handle) and models.get(link):
        weapon = unreal.load_asset("/Game/Gut/Weapons/" + weapon_name)
        weapon.set_editor_property("handle_mesh", models[handle])
        weapon.set_editor_property("link_mesh", models[link])
        weapon.set_editor_property("end_mesh", models.get(hook))
        weapon.set_editor_property("link_start", start)
        weapon.set_editor_property("link_count", count)
        weapon.set_editor_property("link_pitch", pitch)
        weapon.set_editor_property("link_scale", length / (models[link].get_bounds().box_extent.z * 2.0))
        save(weapon)
        log("{} hangs: {} links of {}".format(weapon_name, count, link))

    # armor: real tubes round the limbs and the chest in place of boxes
    for armor_name, (model, wider, depth) in ARMOR.items():
        if models.get(model):
            piece = unreal.load_asset("/Game/Gut/Armor/" + armor_name)
            piece.set_editor_property("plate_mesh", models[model])
            piece.set_editor_property("plate_material", None)
            piece.set_editor_property("plate_is_model", True)
            piece.set_editor_property("model_width_scale", wider)
            piece.set_editor_property("model_depth_share", depth)
            save(piece)
            log("{} is {}".format(armor_name, model))

    # blood: photographed, in place of the blot drawn by sums
    decals = {property_name: import_decal(name) for property_name, name in BLOOD.items()}
    if all(decals.values()):
        for blueprint_name in ("BP_GutPlayer", "BP_GutOpponent", "BP_GutDummy"):
            blueprint = unreal.load_asset("/Game/Gut/Blueprints/" + blueprint_name)
            defaults = unreal.get_default_object(blueprint.generated_class())
            for property_name, decal in decals.items():
                defaults.set_editor_property(property_name, decal)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint)
        log("blood is photographed")

    # the collar: the steel ring made in Mixar, round every fighter's neck
    collar_model, collar_turn, collar_place, collar_size = COLLAR
    if models.get(collar_model):
        for blueprint_name in ("BP_GutPlayer", "BP_GutOpponent", "BP_GutDummy"):
            blueprint = unreal.load_asset("/Game/Gut/Blueprints/" + blueprint_name)
            defaults = unreal.get_default_object(blueprint.generated_class())
            defaults.set_editor_property("collar_mesh", models[collar_model])
            # the model's own material
            defaults.set_editor_property("collar_material", None)
            defaults.set_editor_property("collar_transform", unreal.Transform(location=collar_place, rotation=collar_turn, scale=unreal.Vector(collar_size, collar_size, collar_size)))
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint)
        log("collar is {}".format(collar_model))

    # what ties the wrists of a lot on the Block, or of somebody bought and led away: the rope made in Mixar
    if models.get(TIE):
        for blueprint_name in ("BP_GutPlayer", "BP_GutOpponent", "BP_GutDummy"):
            blueprint = unreal.load_asset("/Game/Gut/Blueprints/" + blueprint_name)
            defaults = unreal.get_default_object(blueprint.generated_class())
            defaults.set_editor_property("tie_mesh", models[TIE])
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint)
        log("wrists are tied with {}".format(TIE))


def main():
    surfaces = {ours: import_surface(name, ours, size) for name, (ours, size) in SURFACE_ASSETS.items()}
    models = {name: import_model(name) for name in [model for model, _, _, _ in WEAPONS.values()] + OTHER_MODELS + sorted({model for model, _, _, _ in DECK_CLUTTER}) + [COLLAR[0], TIE, "spike_panel", "floor_cable", "pool_ladder", "pool_coping", "pool_drain", "pool_light", CHAIN[1], CHAIN[2], CHAIN[3]] + sorted({model for model, _, _ in ARMOR.values()}) + sorted({name for _, _, name in TRUCKS})}
    models.update({name: import_model(name, part) for name, part in KIT_PARTS.items()})

    # a fighter cannot walk through the generator
    generator = models.get("portable_generator")
    if generator and unreal.EditorStaticMeshLibrary.get_simple_collision_count(generator) == 0:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(generator, unreal.ScriptingCollisionShapeType.BOX)
        save(generator)

    wire(models, surfaces)
    dress(models, surfaces, import_sky(SKY))
    log("DONE")


try:
    main()
except Exception:
    unreal.log_error("[import_gut_art] FAILED\n" + traceback.format_exc())
