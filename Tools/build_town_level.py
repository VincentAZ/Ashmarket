"""
Builds a place in the town that can be walked in, from its list: Tools/town_pool_street.json, Tools/town_block_square.json.

It makes nothing with a look of its own. The ground and the plain walls are flat slabs that carry photographed surfaces from
Poly Haven, exactly as the pool's own walls do; everything that is a thing is a model already brought into the project from
Mixar or Poly Haven; the night (sky, air, exposure) is the cage's own, kept from a copy of its level. This script only places
what the list says where the list says, and writes down who stands there and what they say.

Run it through Tools/build_town_level.sh, which starts the editor with no window. It is safe to run again: everything it
placed before (labelled Street_...) is taken away first. GUT_TOWN_LIST names another list.

Places in the list are in the map's metres (x east, y north; for-review/design/map-of-ashmarket.md). In the level, north is
+X and east is +Y, and the level's origin is the cage's (the middle of the pool floor), so the two levels line up.
"""

import json
import math
import os
import re
import struct
import sys
import random
import traceback

import unreal

# the place lists are read through Tools/town_lists.py, which grafts each piece of work into the places it touches
sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools"))

import importlib
import town_lists

importlib.reload(town_lists)

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LIST = os.environ.get("GUT_TOWN_LIST", os.path.join(PROJECT, "Tools", "town_pool_street.json"))
POLYHAVEN = os.path.join(PROJECT, "ArtSource", "polyhaven")

SURFACES = "/Game/Gut/Art/Surfaces"
MODELS = ("/Game/Gut/Art/Mixar", "/Game/Gut/Art/PolyHaven")
MIXAR = os.path.join(PROJECT, "ArtSource", "mixar")

# bills and painted words: pictures made in Mixar with nothing behind them, laid on as decals the way the pool's markings are
PICTURES = "/Game/Town/Pictures"

# what the town's people say, spoken: brought in by Tools/import_town_voices.sh
VOICES = "/Game/Town/Voices"
PICTURE_SOURCE = os.path.join(MIXAR, "street")
DECAL = "/Game/Gut/Art/Blood/M_Gut_Decal"
MOTION = "/Game/Gut/Art/Motion"
PEOPLE = "/Game/Gut/Art/MetaHumans"

# photographed leaks and damp from ambientCG, already made into decals for the pool by Tools/import_gut_art.py
GRIME = {"streaks": ["Leaking001", "Leaking003", "Leaking004"], "damp": ["Leaking007", "Leaking008", "Leaking009"], "rust": ["Leaking006"]}
CAGE = "/Game/Gut/Lvl_Gut"
PREFIX = "Street_"

# What is lower than this above the ground is in a walker's way, and how near a walker's middle comes to it: the place
# keeps where each thing stands so that the game can walk round it (ATownGameMode::WalkerRadius is the same number).
HEAD = 200.0
WALKER = 34.0

# a bare bulb is warm
BULB_COLOUR = unreal.Color(r=255, g=184, b=107, a=255)    # by name: the colour takes blue first

# what a light is the colour of, by what burns in it: a work lamp's glass, a bulb, embers in a drum, a lantern's wick
LIGHT_COLOURS = {"lamp": unreal.Color(r=255, g=226, b=190, a=255), "bulb": BULB_COLOUR,
                 "fire": unreal.Color(r=255, g=128, b=48, a=255), "lantern": unreal.Color(r=255, g=166, b=84, a=255)}

# the glass of a lamp that burns: the cage's own lamp glass
LAMP_GLASS = "/Game/Gut/Materials/MI_Gut_Lamp"

# a light with this tag wavers as a fire does (ATownGameMode::UpdateFires)
FIRE_TAG = "TownFire"

# What can be shoved by walking into it, and what each weighs in kilograms: light enough that a person's shin moves it.
# Anything not listed stays where it is put (a generator, a table, a lamp, a drum with a fire in it).
LOOSE = {"old_tyre": 9.0, "wooden_crate_02": 14.0, "metal_jerrycan": 7.0, "propane_tank": 16.0, "Barrel_01": 30.0, "barrel_03": 30.0,
         # the small things Tools/scatter_loose_things.py sets down (Poly Haven scans, Tools/import_loose_props.sh)
         "plastic_jerrycan": 1.5, "plastic_bottle_gallon": 0.6, "plastic_crate_02": 2.0, "can_rusted": 0.4, "oil_tin": 0.8, "metal_stool_02": 4.0,
         "plastic_monobloc_chair_01": 2.5, "cardboard_box_01": 1.2, "trashbag": 3.0, "cement_bag": 25.0}

# anything lighter than this (kg) never stops whoever walks: it is kicked aside or stepped past, and a can against a wall is not a wall
LOOSE_STOPS_NOBODY = 5.0

# an actor with this tag is one of those (ATownGameMode counts them and listens for them being struck)
LOOSE_TAG = "TownLoose"

# the bulb that hangs wherever a list says a bulb is 'hung'
HUNG_BULB = "main_bulb"

DEEDS = {"": unreal.TownDeed.NONE, "fight": unreal.TownDeed.FIGHT, "fight_afresh": unreal.TownDeed.FIGHT_AFRESH,
         "spar": unreal.TownDeed.SPAR, "bet": unreal.TownDeed.BET, "own": unreal.TownDeed.OWN, "go": unreal.TownDeed.GO,
         "buy": unreal.TownDeed.BUY, "borrow": unreal.TownDeed.BORROW, "repay": unreal.TownDeed.REPAY,
         "armor": unreal.TownDeed.ARMOR, "trade": unreal.TownDeed.TRADE, "pit": unreal.TownDeed.PIT, "rest": unreal.TownDeed.REST,
         "rack": unreal.TownDeed.RACK}

# the clip somebody with their wrists tied has their arms taken from (AGutFighter::BoundArms), which nobody else in a town stands in
BOUND_STANCE = "crowd_watch_hands_behind"

# what the cage's crowd is made of, which a crowd standing anywhere else is made of too: read from the cage's own crowd
CROWD_MAKINGS = ("spectator_mesh", "idle_animation", "cheer_animation", "idle_animations", "cheer_animations", "families", "tint_parameter",
                 "spectator_classes", "spectator_retargeter", "person_body_name", "cloth_parameters")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log("[build_town_level] " + text)


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def surface(ours, spec):
    """A photographed surface as a material: one already in the project, or one made here from Poly Haven's three maps
    the way Tools/import_gut_art.py makes the pool's."""
    if "existing" in spec:
        return unreal.load_asset("{}/{}".format(SURFACES, spec["existing"]))

    path = "{}/MI_Gut_{}".format(SURFACES, ours)

    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    name = spec["from"]
    textures = {}

    for kind, parameter in (("basecolor", "BaseColor"), ("normal", "Normal"), ("roughness", "Roughness")):
        source = os.path.join(POLYHAVEN, name, "{}_{}.png".format(name, kind))

        if not os.path.exists(source):
            raise RuntimeError("{} has not been fetched: Tools/fetch_polyhaven.py --surface {}".format(name, name))

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
    instance.set_editor_property("parent", unreal.load_asset(SURFACES + "/M_Gut_Surface"))

    for parameter, texture in textures.items():
        mel.set_material_instance_texture_parameter_value(instance, parameter, texture)

    mel.set_material_instance_scalar_parameter_value(instance, "Size", float(spec.get("size", 300)))
    mel.update_material_instance(instance)
    save(instance)

    log("surface {} from {}".format(instance.get_name(), name))

    return instance


def model(name):
    """A model by the name of its folder: its one static mesh. One exported from Mixar and not yet in the project is
    brought in through the engine's glTF importer, as Tools/place_gut_town.py brings in the containers."""
    def found():
        for root in MODELS:
            folder = "{}/{}".format(root, name)

            if not eal.does_directory_exist(folder):
                continue

            for path in eal.list_assets(folder, recursive=True, include_folder=False):
                asset = unreal.load_asset(path.split(".")[0])

                if isinstance(asset, unreal.StaticMesh):
                    return asset

        return None

    mesh = found()

    if mesh:
        return mesh

    source = os.path.join(MIXAR, name, name + ".gltf")

    if not os.path.exists(source):
        raise RuntimeError("There is no model called {} in the project".format(name))

    parameters = unreal.ImportAssetParameters()
    parameters.set_editor_property("is_automated", True)
    parameters.set_editor_property("replace_existing", True)

    unreal.InterchangeManager.get_interchange_manager_scripted().import_asset("{}/{}".format(MODELS[0], name), unreal.InterchangeManager.create_source_data(source), parameters)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    mesh = found()

    if not mesh:
        raise RuntimeError("{} did not come in as a model".format(name))

    # the importer turns Nanite on, which the materials it makes for a glTF file are not set up for
    meshes_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    nanite = meshes_editor.get_nanite_settings(mesh)

    if nanite.enabled:
        nanite.enabled = False
        meshes_editor.set_nanite_settings(mesh, nanite, apply_changes=True)
        save(mesh)

    box = mesh.get_bounding_box()

    log("brought in {}: {} triangles, {:.0f} x {:.0f} x {:.0f} cm".format(name, mesh.get_num_triangles(0),
        box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z))

    return mesh


def glow_of(name):
    """Where a model made in Mixar glows, and which way that looks, in the model's own axes as the level has them
    (centimetres): written beside the model by Tools/light_vehicle_lamps.py's measuring. None for a model that does not.
    Blender's -Y is the level's +Y for these models, and its X and Z are the level's."""
    source = os.path.join(MIXAR, name, name + ".glow.json")

    if not os.path.exists(source):
        return None

    glow = json.load(open(source))
    centre, look = glow["centre"], glow["look"]

    return unreal.Vector(centre[0] * 100.0, -centre[1] * 100.0, centre[2] * 100.0), unreal.Vector(look[0], -look[1], look[2])


def painted(name, roughness):
    """A bill or painted words as a decal: the material, and how wide the picture is against its height."""
    path = "{}/MI_Town_{}".format(PICTURES, name)

    if eal.does_asset_exist(path):
        instance = unreal.load_asset(path)
        image = mel.get_material_instance_texture_parameter_value(instance, "Picture")
    else:
        source = os.path.join(PICTURE_SOURCE, name + ".png")

        if not os.path.exists(source):
            raise RuntimeError("There is no picture called {} in {}".format(name, PICTURE_SOURCE))

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", PICTURES)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])

        image = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
        image.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        image.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        save(image)

        instance = asset_tools.create_asset("MI_Town_" + name, PICTURES, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        instance.set_editor_property("parent", unreal.load_asset(DECAL))
        mel.set_material_instance_texture_parameter_value(instance, "Picture", image)
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
        mel.update_material_instance(instance)
        save(instance)

        log("picture {}".format(instance.get_name()))

    return instance, shape_of(name, image)


def shape_of(name, image):
    """How wide a picture is against its height, read from the file it was made from. The texture cannot be asked: in an
    editor with no window a texture that is already in the project says it is 32 by 32 until it has been drawn, so every
    picture came out square the second time a place was built."""
    source = os.path.join(PICTURE_SOURCE, name + ".png")

    if os.path.exists(source):
        with open(source, "rb") as file:
            head = file.read(24)

        if head[:8] == b"\x89PNG\r\n\x1a\n":
            wide, tall = struct.unpack(">II", head[16:24])

            if wide > 0 and tall > 0:
                return wide / float(tall)

    wide, tall = image.blueprint_get_size_x(), image.blueprint_get_size_y()

    if wide <= 32 and tall <= 32:
        raise RuntimeError("The picture {} has no file in {} to take its shape from, and its texture will not say".format(name, PICTURE_SOURCE))

    return wide / float(tall)


def footprint(mesh):
    """Where a model stands, seen from above, in its own centimetres (x from, x to, y from, y to): the box round every
    point of it that is lower than a head. A lamp post is its post, not the arm over the road."""
    box = mesh.get_bounding_box()
    xs = []
    ys = []

    try:
        described = mesh.get_static_mesh_description(0)
        wanted = described.get_vertex_count()
        seen = 0
        index = 0

        while seen < wanted and index < wanted * 4:
            vertex = unreal.VertexID(index)
            index += 1

            if not described.is_vertex_valid(vertex):
                continue

            seen += 1
            point = described.get_vertex_position(vertex)

            if point.z <= box.min.z + HEAD:
                xs.append(point.x)
                ys.append(point.y)
    except Exception as error:
        log("could not read the shape of {}: {}".format(mesh.get_name(), error))

    if not xs:
        return box.min.x, box.max.x, box.min.y, box.max.y

    return min(xs), max(xs), min(ys), max(ys)


def legs(mesh, head=HEAD, apart=110.0, cell=25.0):
    """Where a model that stands on legs stands, seen from above: a box (x from, x to, y from, y to, in its own
    centimetres) round each part of it that is lower than a head and is further than `apart` from every other such part,
    so that it is walked under between them. A gantry is its two legs and not the road between; a wing on pillars is its
    pillars. Measured from the model's own points, as footprint() is: so it is for a model made of many small triangles
    (anything out of Mixar), and not for one whose long flat sides have points only at their corners, which it would
    take for four legs. Asked for by name in a place's list ('legs': true); nothing is taken for legged unasked.
    `head` is how far up its own height a walker's head comes: less than a head's height where it stands on ground higher
    than the walker's (the wing on the Row's two banks, over the furrow). Returns one box round everything low where the
    shape cannot be read."""
    box = mesh.get_bounding_box()
    top = box.min.z + head
    filled = set()

    try:
        described = mesh.get_static_mesh_description(0)
        wanted = described.get_vertex_count()
        seen = 0
        index = 0

        while seen < wanted and index < wanted * 4:
            vertex = unreal.VertexID(index)
            index += 1

            if not described.is_vertex_valid(vertex):
                continue

            seen += 1
            point = described.get_vertex_position(vertex)

            if point.z <= top:
                filled.add((int(math.floor(point.x / cell)), int(math.floor(point.y / cell))))
    except Exception as error:
        log("could not read the legs of {}: {}".format(mesh.get_name(), error))
        filled = set()

    if not filled:
        return [footprint(mesh)]

    # what is nearer than `apart` is one part
    near = int(math.ceil(apart / cell))
    parts = []

    while filled:
        edge = [filled.pop()]
        part = list(edge)

        while edge:
            across, along = edge.pop()

            for step_across in range(-near, near + 1):
                for step_along in range(-near, near + 1):
                    other = (across + step_across, along + step_along)

                    if other in filled:
                        filled.discard(other)
                        edge.append(other)
                        part.append(other)

        parts.append((max(box.min.x, min(square[0] for square in part) * cell), min(box.max.x, (max(square[0] for square in part) + 1) * cell),
                      max(box.min.y, min(square[1] for square in part) * cell), min(box.max.y, (max(square[1] for square in part) + 1) * cell)))

    return sorted(parts)


def turned(box, yaw):
    """A box in a model's own axes (x from, x to, y from, y to) as the level sees it once the model is turned."""
    turn = math.radians(yaw)
    corners = [(x * math.cos(turn) - y * math.sin(turn), x * math.sin(turn) + y * math.cos(turn)) for x in box[:2] for y in box[2:]]

    return min(c[0] for c in corners), max(c[0] for c in corners), min(c[1] for c in corners), max(c[1] for c in corners)


def away(point, block):
    """How far a place is from something that stands on the ground: nought inside it."""
    middle_x, middle_y, half_x, half_y, yaw = block
    turn = math.radians(yaw)
    off_x, off_y = point[0] - middle_x, point[1] - middle_y
    own_x = off_x * math.cos(turn) + off_y * math.sin(turn)
    own_y = -off_x * math.sin(turn) + off_y * math.cos(turn)

    return math.hypot(max(abs(own_x) - half_x, 0.0), max(abs(own_y) - half_y, 0.0))


def open_edges(patches, blocks):
    """Every stretch of the edge of the ground where a walker would be stopped by nothing: the edge of the patches, taken
    together, wherever nothing that stands there is near enough to be what stops them. Patches are (x from, x to, y from,
    y to) in the level's centimetres. Returns each stretch as its two ends."""
    def inside(x, y):
        return any(p[0] + 1.0 < x < p[1] - 1.0 and p[2] + 1.0 < y < p[3] - 1.0 for p in patches)

    step = 10.0
    found = []

    for p in patches:
        sides = (((p[0], p[2]), (p[0], p[3]), (-1.0, 0.0)), ((p[1], p[2]), (p[1], p[3]), (1.0, 0.0)),
                 ((p[0], p[2]), (p[1], p[2]), (0.0, -1.0)), ((p[0], p[3]), (p[1], p[3]), (0.0, 1.0)))

        for begin, end, out in sides:
            length = math.hypot(end[0] - begin[0], end[1] - begin[1])
            stretch = None

            for index in range(int(length / step) + 1):
                share = min(index * step / length, 1.0)
                point = (begin[0] + (end[0] - begin[0]) * share, begin[1] + (end[1] - begin[1]) * share)

                # an edge that another patch carries on from is no edge, here or a walker's width to either side
                edge = not any(inside(point[0] + out[0] * 3.0 + along * out[1], point[1] + out[1] * 3.0 + along * out[0]) for along in (-3.0, 0.0, 3.0))
                bare = edge and min([away(point, block) for block in blocks] or [1e9]) > WALKER + 4.0

                if bare:
                    stretch = [point, point] if stretch is None else [stretch[0], point]
                elif stretch is not None:
                    found.append(stretch)
                    stretch = None

            if stretch is not None:
                found.append(stretch)

    return found


def level(listed):
    """Opens the place's level. The first time, it is made from a copy of the cage's with everything taken out of it but
    the night: the sky, the air and the camera's exposure are then the same in both."""
    path = listed["level"]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # a place with people standing about in it borrows what the cage's crowd is made of. That is read first, with the cage
    # open, and nothing in the cage is touched.
    makings = {}

    if listed.get("bystanders"):
        unreal.EditorLoadingAndSavingUtils.load_map(CAGE)

        for actor in actors.get_all_level_actors():
            if isinstance(actor, unreal.GutCrowd):
                # lists are copied out: the cage's own go when its level is closed
                makings = {name: actor.get_editor_property(name) for name in CROWD_MAKINGS}
                makings = {name: (list(value) if isinstance(value, unreal.Array) else value) for name, value in makings.items()}

        if not makings:
            raise RuntimeError("the cage has no crowd to make this place's bystanders from")

        # nobody free stands in the town with their hands behind their back: here that is how somebody tied stands
        makings["idle_animations"] = [clip for clip in makings["idle_animations"] if clip and clip.get_name() != BOUND_STANCE]

    def here():
        """Stops everything unless the level the editor has open is the place's own. Nothing below is safe in any other."""
        open_now = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split(".")[0]

        if open_now != path or path == CAGE:
            raise RuntimeError("the editor has {} open, not {}: nothing was touched".format(open_now, path))

    if not eal.does_asset_exist(path):
        # opened as the editor opens a level and saved under the new name. (Copied as an asset, the cage's world is left
        # in memory with nothing holding it, and the editor stops dead at the next level it is asked to open.)
        unreal.EditorLoadingAndSavingUtils.load_map(CAGE)

        if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), path):
            raise RuntimeError("Could not save the cage's level as {}".format(path))

        # saving under a new name writes a copy and leaves the editor in the level it had open: the cage. The copy has to
        # be opened before anything is taken out. (The first time this ran it emptied the cage and built the street in it.)
        unreal.EditorLoadingAndSavingUtils.load_map(path)
        here()

        kept = 0
        removed = 0

        for actor in actors.get_all_level_actors():
            night = (isinstance(actor, (unreal.ExponentialHeightFog, unreal.PostProcessVolume, unreal.DirectionalLight, unreal.SkyLight,
                                        unreal.SkyAtmosphere, unreal.VolumetricCloud, unreal.WorldSettings))
                     or actor.get_actor_label() == "NightSky")

            if night:
                kept += 1
            elif actors.destroy_actor(actor):
                removed += 1

        log("made {} from the cage's level: kept {} things of its night, took out {}".format(path, kept, removed))
    else:
        unreal.EditorLoadingAndSavingUtils.load_map(path)

    here()

    removed = 0

    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX) or isinstance(actor, unreal.PlayerStart):
            actors.destroy_actor(actor)
            removed += 1

    log("took away {} things placed before".format(removed))

    return actors, makings


def story():
    """Writes down the town's story where the game reads it: its powers, where its marks begin, and every piece of work
    there is, with how each stands at each figure of its mark. What is said and done in each piece is in the places."""
    told = town_lists.story()
    path = told.get("data", "/Game/Town/DA_Town_Story")
    folder, name = path.rsplit("/", 1)

    if eal.does_asset_exist(path):
        data = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TownStoryData)
        data = asset_tools.create_asset(name, folder, unreal.TownStoryData, factory)

    factions = []

    for entry in told.get("factions", []):
        faction = unreal.TownFaction()
        faction.set_editor_property("key", unreal.Name(entry["key"]))
        faction.set_editor_property("name", entry["name"])
        faction.set_editor_property("about", entry.get("about", ""))
        factions.append(faction)

    missions = []

    for entry in told.get("missions", []):
        mission = unreal.TownMission()
        mission.set_editor_property("key", unreal.Name(entry["key"]))
        mission.set_editor_property("name", entry["name"])
        mission.set_editor_property("faction", unreal.Name(entry.get("faction", "")))
        mission.set_editor_property("giver", entry.get("giver", ""))

        steps = []

        for at, words in entry.get("steps", []):
            step = unreal.TownMissionStep()
            step.set_editor_property("at", int(at))
            step.set_editor_property("words", words)
            steps.append(step)

        mission.set_editor_property("steps", steps)
        missions.append(mission)

    data.set_editor_property("factions", factions)
    data.set_editor_property("missions", missions)
    data.set_editor_property("starts", {unreal.Name(key): int(value) for key, value in told.get("starts", {}).items()})

    log("the story: {} powers, {} pieces of work".format(len(factions), len(missions)))


def main():
    # with whatever each piece of work has in this place (Tools/town_lists.py)
    listed = town_lists.place(LIST)

    origin = listed["origin"]
    street = float(listed["street_height"])
    alley = listed["alley"]

    def height(y):
        """How high the ground is at a place: the street is level, and the alley climbs northward from it."""
        return street + max(0.0, y - alley["from_y"]) * alley["rise_per_metre"]

    def place(x, y, z=None):
        """A place on the map, in the level's own centimetres."""
        z = height(y) if z is None else z

        return unreal.Vector((y - origin["y"]) * 100.0, (x - origin["x"]) * 100.0, (z - origin["z"]) * 100.0)

    surfaces = {ours: surface(ours, spec) for ours, spec in listed["surfaces"].items()}
    actors, makings = level(listed)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.TownGameMode)

    # The night sky is a photograph on the inside of a dome, which the cage's level centres on the pool and makes about
    # 130 m in radius, with the photograph's own ground a metre under the pool's floor. A place further from the pool than
    # that is outside its own sky: whoever stands near the dome's wall sees the photograph's hills a few metres off, and
    # nothing beyond them. 'sky' centres the dome on the place, makes it big enough to hold everything that is seen from
    # it, and puts its ground under the place's lowest.
    if listed.get("sky"):
        for actor in actors.get_all_level_actors():
            if actor.get_actor_label() == "NightSky":
                actor.set_actor_location(place(listed["sky"]["x"], listed["sky"]["y"], listed["sky"]["z"]), False, False)
                actor.set_editor_property("Size", float(listed["sky"]["size"]))
                log("the sky is centred on ({}, {}), its ground at {} m, size {}".format(listed["sky"]["x"], listed["sky"]["y"], listed["sky"]["z"], listed["sky"]["size"]))

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    counts = {}

    # everything that stands on the ground and is walked round: (middle x, middle y, half length, half width, yaw), in the level
    blocks = []

    def label(actor, kind, name):
        counts[kind] = counts.get(kind, 0) + 1
        actor.set_actor_label("{}{}_{:02d}_{}".format(PREFIX, kind, counts[kind], name))

    def slab(name, kind, x, y, bottom, top, covering, slope=False):
        """A flat slab between two corners of the map, carrying a photographed surface. With slope it follows the ground."""
        south, north = y
        west, east = x
        long = north - south

        middle = place((west + east) * 0.5, (south + north) * 0.5, 0.0)
        tilt = 0.0
        lift = 0.0

        if slope:
            rise = height(north) - height(south)
            tilt = math.degrees(math.atan2(rise, long))
            lift = (height(south) + height(north)) * 0.5 - street
            bottom += lift
            top += lift
            long = math.hypot(long, rise)

        middle.z = ((bottom + top) * 0.5 - origin["z"]) * 100.0

        actor = actors.spawn_actor_from_object(cube, middle, unreal.Rotator(roll=0.0, pitch=tilt, yaw=0.0))
        actor.set_actor_scale3d(unreal.Vector(long, east - west, top - bottom))

        part = actor.get_editor_property("static_mesh_component")
        part.set_material(0, surfaces[covering])
        part.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

        label(actor, kind, name)

        # a wall that comes down to where people are is walked round
        if kind == "Wall" and (bottom - street - lift) * 100.0 < HEAD:
            blocks.append((middle.x, middle.y, (north - south) * 50.0, (east - west) * 50.0, 0.0))

        return actor

    # the ground: its top is where the list says the ground is
    for entry in listed["ground"]:
        top = entry.get("z", street)
        slab(entry["label"], "Ground", entry["x"], entry["y"], top - 0.3, top, entry["surface"], entry.get("slope", False))

    for entry in listed["walls"]:
        base = street + entry.get("raise", 0.0)
        slab(entry["label"], "Wall", entry["x"], entry["y"], base, base + entry["height"], entry["surface"], entry.get("slope", False))

    # the plain walls are stained as the pool's are: streaks down from the top, damp at the foot and out across the ground
    # from it, a run of rust here and there. Where each goes is drawn from a fixed deal, so the place is the same every time.
    grime = {}

    for sort, names in GRIME.items():
        grime[sort] = []

        for name in names:
            stuff = unreal.load_asset("/Game/Gut/Art/Grime/MI_Gut_" + name)
            picture = mel.get_material_instance_texture_parameter_value(stuff, "BaseColor") if stuff else None

            if picture:
                grime[sort].append((stuff, picture.blueprint_get_size_x() / float(picture.blueprint_get_size_y())))

    deal = random.Random(listed["name"])

    def stain(sort, where, into, width, tall, flat=False):
        """Lays one stain: where its middle is, the way into the surface it lies on, and its size along and up that surface."""
        if flat:
            # on the ground, the picture's foot against the wall it creeps out from
            turn = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(0.0, 0.0, -1.0), into)
        else:
            # a decal is thrown along its own X, and its picture's top is toward its own -Y: rolled so that is up the wall
            turn = unreal.MathLibrary.make_rot_from_xy(into, unreal.Vector(0.0, 0.0, -1.0))

        decal = actors.spawn_actor_from_class(unreal.DecalActor, where, turn)
        part = decal.get_editor_property("decal")
        part.set_decal_material(sort)
        part.set_editor_property("decal_size", unreal.Vector(4.0 if flat else 12.0, tall * 0.5, width * 0.5))
        part.set_editor_property("fade_screen_size", 0.0)
        label(decal, "Stain", "floor" if flat else "wall")

    for entry in listed["walls"] if all(grime.values()) else []:
        west, east = entry["x"]
        south, north = entry["y"]
        tall = entry["height"] * 100.0

        # each face: where it begins and ends on the map, and the way into the wall in the level (its X is north)
        faces = {"south": ((west, south), (east, south), unreal.Vector(1.0, 0.0, 0.0)), "north": ((west, north), (east, north), unreal.Vector(-1.0, 0.0, 0.0)),
                 "west": ((west, south), (west, north), unreal.Vector(0.0, 1.0, 0.0)), "east": ((east, south), (east, north), unreal.Vector(0.0, -1.0, 0.0))}

        for side in entry.get("stained", []):
            begin, end, into = faces[side]
            long = math.hypot(end[0] - begin[0], end[1] - begin[1]) * 100.0
            top = (street + entry.get("raise", 0.0) + entry["height"] - origin["z"]) * 100.0

            def on_face(along, up=None):
                """A place on the face, so far along it: on the ground there, or so far up from it."""
                share = min(max(along / long, 0.0), 1.0)
                foot = place(begin[0] + (end[0] - begin[0]) * share, begin[1] + (end[1] - begin[1]) * share)

                return unreal.Vector(foot.x, foot.y, foot.z if up is None else up)

            at = -deal.uniform(0.0, 120.0)

            while at < long:
                stuff, shape = deal.choice(grime["streaks"])
                width = deal.uniform(280.0, 460.0)
                drop = min(width / shape * deal.uniform(1.0, 1.5), tall - 20.0)

                if deal.random() > 0.3:
                    stain(stuff, on_face(at + width * 0.5, top - drop * 0.5), into, width, drop)

                at += width * deal.uniform(0.7, 1.1)

            for flat in (False, True):
                at = -deal.uniform(0.0, 150.0)

                while at < long:
                    stuff, shape = deal.choice(grime["damp"])
                    width = deal.uniform(320.0, 520.0)
                    reach = width / shape * deal.uniform(0.9, 1.6)

                    if deal.random() > 0.3:
                        foot = on_face(at + width * 0.5)

                        if flat:
                            stain(stuff, foot - into * (reach * 0.5), into, width, reach, True)
                        else:
                            stain(stuff, unreal.Vector(foot.x, foot.y, foot.z + reach * 0.5), into, width, reach)

                    at += width * deal.uniform(0.75, 1.1)

            for _ in range(max(1, int(long / 700.0))):
                stuff, shape = deal.choice(grime["rust"])
                width = deal.uniform(30.0, 55.0)
                drop = width / shape * deal.uniform(0.7, 1.0)
                stain(stuff, on_face(deal.uniform(40.0, long - 40.0), top - 6.0 - drop * 0.5), into, width, drop)

    meshes = {}
    feet = {}
    on_legs = {}

    def known(name):
        if name not in meshes:
            meshes[name] = model(name)
            feet[name] = footprint(meshes[name])

        return meshes[name]

    def put(name, where, yaw, kind, blocking, scale=1.0, pitch=0.0):
        """Puts a model at a place in the level, turned, and made bigger or smaller all ways alike. With blocking, the place
        keeps where it stands. `pitch` tips its own front end up, for something long that lies up a slope."""
        actor = actors.spawn_actor_from_object(known(name), where, unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        part = actor.get_editor_property("static_mesh_component")

        if scale != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))

        # whoever walks is kept out of it by the place's own list of what stands where, and what is painted on the pool
        # stays on the pool
        part.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        part.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

        label(actor, kind, name)

        # blocking "legs": it stands on legs and is walked under between them, each leg a thing of its own to walk round
        # (a number with it is how far above the walker's ground it stands)
        legged = isinstance(blocking, tuple)

        if legged and (name, blocking[1]) not in on_legs:
            on_legs[(name, blocking[1])] = legs(meshes[name], max(40.0, HEAD - blocking[1]))
            log("{} stands on {} legs: {}".format(name, len(on_legs[(name, blocking[1])]),
                                                  ", ".join("{:.1f} by {:.1f} m".format((x1 - x0) / 100.0, (y1 - y0) / 100.0) for x0, x1, y0, y1 in on_legs[(name, blocking[1])])))

        for foot in (on_legs[(name, blocking[1])] if legged else [feet[name]] if blocking else []):
            x0, x1, y0, y1 = [edge * scale for edge in foot]
            turn = math.radians(yaw)
            middle_x, middle_y = (x0 + x1) * 0.5, (y0 + y1) * 0.5

            blocks.append((where.x + middle_x * math.cos(turn) - middle_y * math.sin(turn),
                           where.y + middle_x * math.sin(turn) + middle_y * math.cos(turn), (x1 - x0) * 0.5, (y1 - y0) * 0.5, yaw))

        return actor

    def stand(name, x, y, yaw, base=None, kind="Thing", blocking=True, scale=1.0, pitch=0.0):
        """Stands a model with the middle of what it covers at a place on the map and its lowest point on the ground there
        (or on `base`), `scale` times its own size. Returns the actor, how high it then reaches, and half its size."""
        box = known(name).get_bounding_box()
        x0, x1, y0, y1 = [edge * scale for edge in turned((box.min.x, box.max.x, box.min.y, box.max.y), yaw)]
        low, high = box.min.z * scale, box.max.z * scale

        # on a slope a thing stands on the lowest ground under it, and is sunk into what is higher: nothing hangs in the air
        if base is None:
            reach = (x1 - x0) / 200.0
            base = min(height(y - reach), height(y + reach))

        want = place(x, y, base)
        where = unreal.Vector(want.x - (x0 + x1) * 0.5, want.y - (y0 + y1) * 0.5, want.z - low)
        rise = want.z - place(x, y).z
        actor = put(name, where, yaw, kind, (("legs", round(max(0.0, rise))) if blocking == "legs" else blocking) if rise < HEAD else False, scale, pitch)

        return actor, (want.z + high - low) / 100.0 + origin["z"], unreal.Vector((x1 - x0) * 0.5, (y1 - y0) * 0.5, (high - low) * 0.5)

    # rows of containers end to end along a line, stacked: each stack one kind of container, the kinds taking turns along
    # the row. The row is given by the line its near faces stand on and the side of that line it lies on, so that the
    # containers make one wall whatever their sizes, and by where it starts and ends, which the containers are spread between.
    for entry in listed["rows"]:
        east_west = entry["along"] == "x"
        kinds = entry["model"]
        span = (entry["to"] - entry["from"]) * 100.0

        def size(name):
            """How long a container is along the row, where its whole shape begins along it, and where what stands of it
            begins and ends across it."""
            box = known(name).get_bounding_box()
            whole = turned((box.min.x, box.max.x, box.min.y, box.max.y), entry["yaw"])
            foot = turned(feet[name], entry["yaw"])

            return (whole[3] - whole[2], whole[2], foot[0], foot[1]) if east_west else (whole[1] - whole[0], whole[0], foot[2], foot[3])

        chosen = []
        filled = 0.0

        while filled + size(kinds[len(chosen) % len(kinds)])[0] + 10.0 * len(chosen) <= span:
            chosen.append(kinds[len(chosen) % len(kinds)])
            filled += size(chosen[-1])[0]

        gap = (span - filled) / (len(chosen) - 1) if len(chosen) > 1 else 0.0
        far_side = entry["side"] in ("north", "east")
        cursor = entry["from"] * 100.0

        for name in chosen:
            length, begins, foot_from, foot_to = size(name)
            middle = (cursor + length * 0.5) / 100.0

            # the level's X is the map's north and its Y the map's east
            if east_west:
                ground = place(middle, entry["face"])
                where = unreal.Vector(ground.x - (foot_from if far_side else foot_to), (cursor - origin["x"] * 100.0) - begins, 0.0)
            else:
                # along a street that climbs, a stack stands on the ground at its lower end
                ground = place(entry["face"], middle, min(height(cursor / 100.0), height((cursor + length) / 100.0)))
                where = unreal.Vector((cursor - origin["y"] * 100.0) - begins, ground.y - (foot_from if far_side else foot_to), 0.0)

            box = known(name).get_bounding_box()
            where.z = ground.z - box.min.z

            for storey in range(entry["high"]):
                put(name, unreal.Vector(where.x, where.y, where.z + storey * (box.max.z - box.min.z)), entry["yaw"], entry["label"], storey == 0)

            cursor += length + gap

        log("row {}: {} stacks, {:.2f} m between them".format(entry["label"], len(chosen), gap / 100.0))

    # a run of one model end to end between two places: a fence, a row of barriers
    for entry in listed["runs"]:
        (x0, y0), (x1, y1) = entry["from"], entry["to"]
        length = math.hypot(x1 - x0, y1 - y0)

        # which way the run goes, as a compass bearing, and the model turned so that its long side lies along it
        bearing = math.degrees(math.atan2(x1 - x0, y1 - y0))

        box = known(entry["model"]).get_bounding_box()
        long_is_x = box.max.x - box.min.x >= box.max.y - box.min.y
        piece = max(box.max.x - box.min.x, box.max.y - box.min.y) / 100.0
        thick = min(box.max.x - box.min.x, box.max.y - box.min.y)

        yaw = (bearing if long_is_x else bearing - 90.0) + entry.get("turn", 0.0)
        pieces = max(1, int(round(length / piece)))

        for index in range(pieces):
            share = (index + 0.5) / pieces
            middle_y = y0 + (y1 - y0) * share

            # 'slope': a long piece lies along the ground it stands on, where that climbs, and is not stepped up it: its
            # middle on the ground under its middle, its north end tipped up as the ground is
            if entry.get("slope") and long_is_x and middle_y > alley["from_y"]:
                tip = math.degrees(math.atan(alley["rise_per_metre"])) * math.cos(math.radians(yaw))
                piece_actor = stand(entry["model"], x0 + (x1 - x0) * share, middle_y, yaw, height(middle_y), entry["label"], False, pitch=tip)[0]
            else:
                piece_actor = stand(entry["model"], x0 + (x1 - x0) * share, middle_y, yaw, None, entry["label"], False)[0]

            # 'hollow': walked under, and looked through by the camera that follows whoever walks: the model's own solid
            # shape is one box round all of it
            if entry.get("hollow"):
                piece_actor.get_editor_property("static_mesh_component").set_collision_profile_name("NoCollision")

        begin, end = place(x0, y0), place(x1, y1)
        lies = math.degrees(math.atan2(end.y - begin.y, end.x - begin.x))

        # something carried on posts is walked under between them: each piece's 'posts' are where they stand along it from
        # its middle, and 'post' is half a post's size along the run and across it
        if entry.get("posts"):
            half_along, half_across = [size * 100.0 for size in entry["post"]]
            along = unreal.Vector(end.x - begin.x, end.y - begin.y, 0.0) * (1.0 / max(1.0, math.hypot(end.x - begin.x, end.y - begin.y)))

            for index in range(pieces):
                share = (index + 0.5) / pieces

                for post in entry["posts"]:
                    blocks.append((begin.x + (end.x - begin.x) * share + along.x * post * 100.0, begin.y + (end.y - begin.y) * share + along.y * post * 100.0,
                                   half_along, half_across, lies))

            continue

        # the whole run is one thing to walk round
        blocks.append(((begin.x + end.x) * 0.5, (begin.y + end.y) * 0.5, length * 50.0, thick * 0.5, lies))

    lamps = []
    lit = []

    def light(where, spec, look=None):
        """A light, as a list describes it: how bright (candelas), how far it reaches (metres), what burns in it, whether it
        throws shadows, whether it wavers. A 'spot' looks the way given; anything else shines all round."""
        spot = spec.get("kind") == "spot" and look is not None
        turn = unreal.MathLibrary.make_rot_from_x(look) if spot else unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
        made = actors.spawn_actor_from_class(unreal.SpotLight if spot else unreal.PointLight, where, turn)
        part = made.get_editor_property("light_component")
        part.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        part.set_editor_property("intensity", float(spec["candela"]))
        part.set_editor_property("light_color", LIGHT_COLOURS[spec.get("colour", "bulb")])
        part.set_editor_property("attenuation_radius", float(spec["reach"]) * 100.0)
        part.set_editor_property("source_radius", float(spec.get("size", 0.05)) * 100.0)
        part.set_editor_property("cast_shadows", bool(spec.get("shadows", False)))
        part.set_editor_property("volumetric_scattering_intensity", float(spec.get("haze", 0.8)))

        if spot:
            part.set_editor_property("inner_cone_angle", float(spec.get("cone", 60.0)) * 0.45)
            part.set_editor_property("outer_cone_angle", float(spec.get("cone", 60.0)))

        if spec.get("flicker"):
            made.set_editor_property("tags", [unreal.Name(FIRE_TAG)])

        label(made, "Light", spec.get("note", spec.get("colour", "bulb")).replace(" ", "_").replace("'", ""))

        return made

    loose_count = 0

    for entry in listed["things"]:
        # 'z' is the ground it stands on where that is not the street's, and 'scale' how much bigger than itself it is made
        own_block = entry.get("block")
        size = float(entry.get("scale", 1.0))
        # What a person could shove with a foot is loose: see below. It is not on the place's list of what stands where,
        # which is what keeps a walker out of things: it is walked into.
        heavy = LOOSE.get(entry["model"])
        loose = bool(heavy) and entry.get("loose", True) and not entry.get("hollow") and not entry.get("lamp") and not entry.get("light") and not own_block and entry.get("z") is None

        actor, top, _ = stand(entry["model"], entry["x"], entry["y"], entry.get("yaw", (entry["x"] * 37.0 + entry["y"] * 91.0) % 360.0),
                              base=entry.get("z"), scale=size,
                              blocking="legs" if entry.get("legs") else own_block is None and entry.get("blocking", True) and not loose)
        part = actor.get_editor_property("static_mesh_component")

        # 'block' is what of it stops a walker, where that is not everything of it that is lower than a head: a shop is
        # walked up to under its awnings (x from and to, y from and to, on the map)
        # (a list of them where it stands on legs and is walked under between: the gantry across the Gate Yard, which as one
        # box shut the whole road, 25 November)
        for one in (own_block if isinstance(own_block, list) else [own_block] if own_block else []):
            south_west, north_east = place(one["x"][0], one["y"][0]), place(one["x"][1], one["y"][1])
            blocks.append(((south_west.x + north_east.x) * 0.5, (south_west.y + north_east.y) * 0.5,
                           abs(north_east.x - south_west.x) * 0.5, abs(north_east.y - south_west.y) * 0.5, 0.0))

        # somebody stands inside it (the window's stall): the model's own solid shape, which stops the camera at everything
        # else, would push them out of it
        # (and so is anything on legs: its solid shape is that one box, which stopped a walker under the gantry after the
        # place's own list had let him through, 25 November)
        if entry.get("hollow") or entry.get("legs"):
            part.set_collision_profile_name("NoCollision")

        # What a person could shove with a foot is loose: walked into, it goes (23 November: "objects are moveable - I kick,
        # walk into an object and it moves"). It weighs what the thing would weigh, so a tyre skids and a full barrel only
        # grudges an inch. 'loose': false on a thing keeps it where it is; -GutNoLoose in the game keeps them all.
        if loose:
            part.set_mobility(unreal.ComponentMobility.MOVABLE)
            part.set_collision_profile_name("PhysicsActor")
            part.set_simulate_physics(True)
            part.set_mass_override_in_kg(unreal.Name("None"), heavy * size * size * size, True)
            part.set_linear_damping(0.35)
            part.set_angular_damping(0.9)
            part.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)

            if heavy < LOOSE_STOPS_NOBODY:
                part.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponseType.ECR_IGNORE)

            # asleep until something touches it: things stood against each other or a little into the ground do not
            # shoulder each other apart when the place opens
            body = part.get_editor_property("body_instance")
            body.set_editor_property("start_awake", False)
            part.set_editor_property("body_instance", body)
            actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(LOOSE_TAG)])
            loose_count += 1

        if entry.get("lamp"):
            lamps.append((entry["x"], entry["y"], top, actor))

        # 'light' is what the thing burns with, where the model itself glows (a work lamp's glass, the embers in a drum):
        # the light is put at the glow, so nothing is lit by something that looks dark
        if entry.get("light"):
            glow = glow_of(entry["model"])

            if not glow:
                raise RuntimeError("{} is given a light, and the model has nothing that glows to put it at".format(entry["model"]))

            centre, look = glow
            turn = actor.get_actor_transform()
            where = unreal.MathLibrary.transform_location(turn, centre)
            way = unreal.MathLibrary.transform_direction(turn, look)
            spec = entry["light"]

            # a little out from the glass, or up from the embers, so that the glow itself is not what shades it
            where = where + way * (float(spec.get("off", 0.12)) * 100.0)

            # 'aim' turns a lamp on something: a place on the map and how high up it (x, y, metres over the ground there)
            if spec.get("aim"):
                aimed = place(spec["aim"][0], spec["aim"][1], height(spec["aim"][1]) + spec["aim"][2]) - where
                way = aimed * (1.0 / max(1.0, aimed.length()))

            light(where, spec, way)

            # whatever burns in it does not throw its own shadow
            part.set_editor_property("cast_shadow", not spec.get("shadows", False))
            lit.append(entry["model"])

    # bills and painted words. Each is a picture thrown onto whatever stands there: 'faces' is the way the surface looks,
    # 'out' how far that surface stands toward the looker from the place given, 'up' the height of the picture's middle.
    into_from = {"north": unreal.Vector(-1.0, 0.0, 0.0), "south": unreal.Vector(1.0, 0.0, 0.0),
                 "east": unreal.Vector(0.0, -1.0, 0.0), "west": unreal.Vector(0.0, 1.0, 0.0)}
    pictures = {}

    def hang(name, x, y, faces, up, tall, lean=0.0, out=0.0, deep=0.12, rough=0.9):
        if name not in pictures:
            pictures[name] = painted(name, rough)

        stuff, shape = pictures[name]
        foot = place(x, y)

        if faces == "up":
            # painted on something that is stood on: thrown straight down onto it, turned as it leans
            decal = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(foot.x, foot.y, foot.z + up * 100.0), unreal.Rotator(roll=0.0, pitch=-90.0, yaw=lean))
            part = decal.get_editor_property("decal")
            part.set_decal_material(stuff)
            part.set_editor_property("decal_size", unreal.Vector(deep * 100.0, tall * 50.0, tall * shape * 50.0))
            part.set_editor_property("fade_screen_size", 0.0)
            part.set_editor_property("sort_order", 3)
            label(decal, "Picture", name)
            return

        into = into_from[faces]
        where = unreal.Vector(foot.x, foot.y, foot.z + up * 100.0) - into * (out * 100.0)

        # a decal is thrown along its own X, and its picture's top is toward its own -Y: that is up the surface, leaned a little
        down = unreal.Vector(0.0, 0.0, -1.0)
        across = into.cross(down)
        tilt = math.radians(lean)
        turn = unreal.MathLibrary.make_rot_from_xy(into, down * math.cos(tilt) + across * math.sin(tilt))

        decal = actors.spawn_actor_from_class(unreal.DecalActor, where, turn)
        part = decal.get_editor_property("decal")
        part.set_decal_material(stuff)
        part.set_editor_property("decal_size", unreal.Vector(deep * 100.0, tall * 50.0, tall * shape * 50.0))
        part.set_editor_property("fade_screen_size", 0.0)
        part.set_editor_property("sort_order", 3)
        label(decal, "Picture", name)

    for entry in listed.get("pictures", []):
        hang(entry["picture"], entry["x"], entry["y"], entry["faces"], entry["up"], entry["tall"], entry.get("lean", 0.0),
             entry.get("out", 0.0), entry.get("deep", 0.12), entry.get("rough", 0.9))

    # bills pasted along a line, one after another: which, how big, how high and how crooked from a fixed deal
    pasting = random.Random(listed["name"] + ", pasted")

    for entry in listed.get("pasted", []):
        (x0, y0), (x1, y1) = entry["from"], entry["to"]
        long = math.hypot(x1 - x0, y1 - y0)
        at = pasting.uniform(*entry["every"]) * 0.5
        last = None
        count = 0

        while at < long - 0.5:
            name = pasting.choice([name for name in entry["pictures"] if name != last])
            last = name
            share = at / long

            hang(name, x0 + (x1 - x0) * share, y0 + (y1 - y0) * share, entry["faces"], pasting.uniform(*entry["up"]),
                 pasting.uniform(*entry["tall"]), pasting.uniform(-entry.get("lean", 4.0), entry.get("lean", 4.0)),
                 entry.get("out", 0.0), entry.get("deep", 0.12))

            at += pasting.uniform(*entry["every"])
            count += 1

        log("pasted {} bills along {}".format(count, entry["label"]))

    # light: bare bulbs, and the street lamps that still burn
    hung = glow_of(HUNG_BULB) if any(entry.get("hung") for entry in listed["bulbs"]) else None

    for entry in listed["bulbs"]:
        where = place(entry["x"], entry["y"], height(entry["y"]) + entry["up"])

        # a bulb that is 'hung' is seen: the bulb on its flex, with its glass where the light is. It wants something over it
        # to hang from, which is for whoever writes the list to see to.
        if entry.get("hung") and hung:
            bulb_model = actors.spawn_actor_from_object(known(HUNG_BULB), where - hung[0], unreal.Rotator(roll=0.0, pitch=0.0, yaw=(entry["x"] * 53.0 + entry["y"] * 29.0) % 360.0))
            shape = bulb_model.get_editor_property("static_mesh_component")
            shape.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            shape.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
            shape.set_editor_property("cast_shadow", False)
            label(bulb_model, "Thing", HUNG_BULB)
        bulb = actors.spawn_actor_from_class(unreal.PointLight, where, unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        part = bulb.get_editor_property("light_component")
        part.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        part.set_editor_property("intensity", float(entry["candela"]))
        part.set_editor_property("light_color", BULB_COLOUR)
        part.set_editor_property("attenuation_radius", float(entry["reach"]) * 100.0)
        part.set_editor_property("source_radius", 4.0)
        part.set_editor_property("cast_shadows", bool(entry.get("shadows", False)))
        part.set_editor_property("volumetric_scattering_intensity", 0.6)
        label(bulb, "Bulb", entry.get("note", "bulb").replace(" ", "_").replace("'", ""))

    # A street lamp that burns is a lantern on a post: its bulb glows, and the light is in the lantern, where the bulb is.
    # (Until 2026-11-18 the light hung a metre to one side of a lamp whose glass was dark, and lit the street from nothing.)
    glass = unreal.load_asset(LAMP_GLASS)

    for x, y, top, post in lamps:
        shape = post.get_editor_property("static_mesh_component")
        names = [str(name) for name in shape.get_material_slot_names()]
        bulbs = [index for index, name in enumerate(names) if "bulb" in name.lower()]

        if not bulbs or not glass:
            raise RuntimeError("{} has no bulb to light: its materials are {}".format(post.get_actor_label(), names))

        for index in bulbs:
            shape.set_material(index, glass)

        # the lantern's own frame would shut the light in
        shape.set_editor_property("cast_shadow", False)

        lamp = actors.spawn_actor_from_class(unreal.PointLight, place(x, y, top - 0.55), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        part = lamp.get_editor_property("light_component")
        part.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        part.set_editor_property("intensity", 12000.0)
        part.set_editor_property("light_color", unreal.Color(r=255, g=205, b=150, a=255))
        part.set_editor_property("attenuation_radius", 2400.0)
        part.set_editor_property("source_radius", 9.0)
        part.set_editor_property("cast_shadows", True)
        part.set_editor_property("volumetric_scattering_intensity", 1.5)
        label(lamp, "Lamp", "street_lamp")

    if lit:
        log("lit where they glow: {}".format(", ".join("{} {}".format(lit.count(name), name) for name in sorted(set(lit)))))

    # where the player comes in
    start = listed["start"]
    begin = actors.spawn_actor_from_class(unreal.PlayerStart, place(start["x"], start["y"], height(start["y"]) + 1.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(start["facing"])))
    begin.set_actor_label(PREFIX + "Start")

    # who is here and what they say, the ways out, and where the ground ends
    data_path = listed["data"]
    folder, name = data_path.rsplit("/", 1)

    if eal.does_asset_exist(data_path):
        data = unreal.load_asset(data_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TownPlaceData)
        data = asset_tools.create_asset(name, folder, unreal.TownPlaceData, factory)

    people = []

    # What is said aloud: a recording for a line or a call, if Tools/make_town_voices.sh has made one and
    # Tools/import_town_voices.sh has brought it in. The names are that tool's: the speaker's name, a number for the
    # second of two people of one name, and the line's own name.
    def worded(text):
        return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")

    spoken = [0]
    named = {}

    def voice(name):
        path = "{}/{}/{}".format(VOICES, worded(listed["name"]), name)

        if not eal.does_asset_exist(path):
            return None

        spoken[0] += 1

        return unreal.load_asset(path)

    for entry in listed["people"]:
        if entry.get("lines"):
            named[entry["name"]] = named.get(entry["name"], 0) + 1

        nth = named.get(entry["name"], 1)

        person = unreal.TownPerson()
        person.set_editor_property("name", entry["name"])
        person.set_editor_property("prompt", entry.get("prompt", ""))
        person.set_editor_property("body", unreal.load_asset("{0}/{1}/BP_{1}".format(PEOPLE, entry["body"])).generated_class())
        person.set_editor_property("cloth", unreal.LinearColor(entry["cloth"][0], entry["cloth"][1], entry["cloth"][2], 1.0))
        person.set_editor_property("place", place(entry["x"], entry["y"]))
        person.set_editor_property("facing", float(entry["facing"]))
        # how they stand: a family of clips from one stance (Tools/import_gut_motion.py, FAMILIES), or failing that one clip
        person.set_editor_property("stand", unreal.Name(entry.get("stand", "")))
        person.set_editor_property("idle", unreal.load_asset("{}/{}".format(MOTION, entry["idle"])) if entry.get("idle") else None)
        person.set_editor_property("stands_on_what_is_there", bool(entry.get("on")))
        person.set_editor_property("collared", bool(entry.get("collared")))
        person.set_editor_property("lot", int(entry.get("lot", 0)))
        # somebody who is only there while the town's marks say so, and the ways they are walked along by name
        person.set_editor_property("when", entry.get("when", ""))

        ways = []

        for called, along in entry.get("ways", {}).items():
            way = unreal.TownWay()
            way.set_editor_property("name", unreal.Name(called))
            way.set_editor_property("points", [place(x, y) for x, y in along["points"]])
            way.set_editor_property("facing", float(along.get("facing", entry["facing"])))
            ways.append(way)

        person.set_editor_property("ways", ways)
        # their own way of walking: a clip cut from motion capture, and the speed it was walked at (Tools/make_style_motion.py)
        if entry.get("walk"):
            known = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "style", "export", entry["walk"] + ".json")

            person.set_editor_property("walk", unreal.load_asset("{}/{}".format(MOTION, entry["walk"])))
            person.set_editor_property("walk_speed", float(json.load(open(known)).get("speed", 90.0)) if os.path.exists(known) else 90.0)

        person.set_editor_property("sells", [unreal.Name(good) for good in entry.get("sells", [])])
        person.set_editor_property("buys", [unreal.Name(good) for good in entry.get("buys", [])])

        # a round they walk: each stop is [x, y], then how long they stand there, then which way they look while they do
        beat = []

        for at in entry.get("beat", []):
            stop = unreal.TownBeatStop()
            stop.set_editor_property("place", place(at[0], at[1]))
            stop.set_editor_property("wait", float(at[2]) if len(at) > 2 else 0.0)
            stop.set_editor_property("facing", float(at[3]) if len(at) > 3 else -1.0)
            beat.append(stop)

        person.set_editor_property("beat", beat)
        person.set_editor_property("pace", float(entry.get("pace", 0.85)))

        ids = {line["id"]: index for index, line in enumerate(entry.get("lines", []))}
        lines = []

        for said in entry.get("lines", []):
            line = unreal.TownLine()
            line.set_editor_property("words", said["words"])
            line.set_editor_property("id", str(said.get("id", "")))
            line.set_editor_property("note", said.get("note", ""))
            line.set_editor_property("next", ids.get(said.get("next"), -1))
            line.set_editor_property("opens", bool(said.get("opens")))
            line.set_editor_property("when", said.get("when", ""))
            line.set_editor_property("does", said.get("does", ""))
            line.set_editor_property("log", said.get("log", ""))

            if said.get("next") and said["next"] not in ids:
                raise RuntimeError("{} has no line called {} to go on to".format(entry["name"], said["next"]))

            line.set_editor_property("voice", voice("{}{}_{}".format(worded(entry["name"]), "" if nth == 1 else str(nth), worded(str(said.get("id", len(lines)))))))

            answers = []

            for back in said.get("answers", []):
                answer = unreal.TownAnswer()
                answer.set_editor_property("words", back["words"])
                answer.set_editor_property("deed", DEEDS[back.get("deed", "")])
                answer.set_editor_property("next", ids.get(back.get("next"), -1))
                answer.set_editor_property("when", back.get("when", ""))
                answer.set_editor_property("needs", back.get("needs", ""))
                answer.set_editor_property("does", back.get("does", ""))
                answer.set_editor_property("log", back.get("log", ""))

                if back.get("next") and back["next"] not in ids:
                    raise RuntimeError("{} has no line called {} to answer with".format(entry["name"], back["next"]))
                answers.append(answer)

            line.set_editor_property("answers", answers)
            lines.append(line)

        person.set_editor_property("lines", lines)
        people.append(person)

    doors = []

    for entry in listed["doors"]:
        door = unreal.TownDoor()
        door.set_editor_property("prompt", entry["prompt"])
        door.set_editor_property("place", place(entry["x"], entry["y"]))
        door.set_editor_property("deed", DEEDS[entry.get("deed", "")])
        door.set_editor_property("shut", entry.get("shut", ""))
        door.set_editor_property("to", entry.get("to", ""))
        door.set_editor_property("arrive", entry.get("arrive", ""))
        door.set_editor_property("walked_through", bool(entry.get("walk_through")))
        door.set_editor_property("reach", float(entry.get("reach", 0.0)) * 100.0)
        door.set_editor_property("when", entry.get("when", ""))
        door.set_editor_property("does", entry.get("does", ""))
        door.set_editor_property("said", entry.get("said", ""))
        door.set_editor_property("log", entry.get("log", ""))

        # a way as wide as a street is gone through anywhere across its mouth
        if entry.get("mouth"):
            low, high = place(entry["mouth"]["x"][0], entry["mouth"]["y"][0]), place(entry["mouth"]["x"][1], entry["mouth"]["y"][1])

            mouth = unreal.Box2D()
            mouth.set_editor_property("min", unreal.Vector2D(min(low.x, high.x), min(low.y, high.y)))
            mouth.set_editor_property("max", unreal.Vector2D(max(low.x, high.x), max(low.y, high.y)))
            mouth.set_editor_property("is_valid", True)
            door.set_editor_property("mouth", mouth)
        doors.append(door)

    # what lies about to be found and picked up: each is a good, where it lies, and which way it is turned
    finds = []

    for entry in listed.get("finds", []):
        find = unreal.TownFind()
        find.set_editor_property("good", unreal.Name(entry["good"]))
        find.set_editor_property("place", place(entry["x"], entry["y"]))
        find.set_editor_property("facing", float(entry.get("facing", 0.0)))
        finds.append(find)

    data.set_editor_property("finds", finds)

    # the ways in from other places
    arrivals = []

    for entry in listed.get("arrivals", []):
        arrival = unreal.TownArrival()
        arrival.set_editor_property("name", entry["name"])
        arrival.set_editor_property("place", place(entry["x"], entry["y"]))
        arrival.set_editor_property("facing", float(entry["facing"]))
        arrivals.append(arrival)

    # in a place that is the player's own yard: where their people stand
    stalls = []

    for entry in listed.get("stalls", []):
        stall = unreal.TownArrival()
        stall.set_editor_property("name", "stall_{}".format(len(stalls) + 1))
        stall.set_editor_property("place", place(entry["x"], entry["y"]))
        stall.set_editor_property("facing", float(entry["facing"]))
        stalls.append(stall)

    # what is called out over the place
    calls = []

    for entry in listed.get("calls", []):
        call = unreal.TownCall()
        call.set_editor_property("speaker", entry["speaker"])
        call.set_editor_property("words", entry["words"])
        call.set_editor_property("voice", voice("call_{:02d}".format(len(calls))))
        call.set_editor_property("seconds", float(entry.get("seconds", 3.5)))
        call.set_editor_property("stirs_the_crowd", bool(entry.get("stirs")))
        call.set_editor_property("sells", int(entry.get("sells", 0)))
        call.set_editor_property("about", int(entry.get("lot", 0)))
        call.set_editor_property("brings_up", bool(entry.get("brings_up")))
        calls.append(call)

    # people standing about: so many of them in a stretch of ground, no two nearer than a walker needs to get between them,
    # none on top of anything that stands there or in a way that is to be kept clear, each looking at what they came to see.
    # Where each stands is drawn from a fixed deal, so the place is the same every time. Each is something to walk round.
    for entry in listed.get("bystanders", []):
        dealt = random.Random(listed["name"] + ", " + entry["label"])
        apart = float(entry.get("apart", 1.3)) * 100.0
        low, high = place(entry["x"][0], entry["y"][0]), place(entry["x"][1], entry["y"][1])
        look = place(entry["look"][0], entry["look"][1])
        clear = [(place(c["x"][0], c["y"][0]), place(c["x"][1], c["y"][1])) for c in entry.get("clear", [])]
        kept = [place(p["x"], p["y"]) for p in listed["people"]] + [place(d["x"], d["y"]) for d in listed["doors"]]
        stood = []
        tries = 0

        while len(stood) < int(entry["count"]) and tries < 20000:
            tries += 1

            # `toward` draws them in to one side: 0 spreads them evenly, 1 packs them against the far end of x
            share = dealt.random() ** (1.0 / (1.0 + float(entry.get("toward", 0.0)) * 2.0))
            spot = (dealt.uniform(low.x, high.x), low.y + (high.y - low.y) * share)

            if any(math.hypot(spot[0] - other[0], spot[1] - other[1]) < apart for other in stood):
                continue

            if any(away(spot, block) < 60.0 for block in blocks):
                continue

            if any(math.hypot(spot[0] - k.x, spot[1] - k.y) < 190.0 for k in kept):
                continue

            if any(min(a.x, b.x) < spot[0] < max(a.x, b.x) and min(a.y, b.y) < spot[1] < max(a.y, b.y) for a, b in clear):
                continue

            stood.append(spot)

        places = []

        for spot in stood:
            # the ground where they stand, which on a street that climbs is not the same for all of them
            ground = (height(spot[0] / 100.0 + origin["y"]) - origin["z"]) * 100.0
            yaw = math.degrees(math.atan2(look.y - spot[1], look.x - spot[0])) + dealt.uniform(-18.0, 18.0)
            places.append(unreal.Transform(location=unreal.Vector(spot[0], spot[1], ground), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), scale=unreal.Vector(1.0, 1.0, 1.0)))
            blocks.append((spot[0], spot[1], 20.0, 20.0, 0.0))

        crowd = actors.spawn_actor_from_class(unreal.GutCrowd, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))

        for name, value in makings.items():
            crowd.set_editor_property(name, value)

        crowd.set_editor_property("standing_places", places)

        # heard by how near they are, not the same from one end of the place to the other
        crowd.set_editor_property("murmur_by_nearness", True)
        label(crowd, "Crowd", entry["label"])

        log("{}: {} people standing about, of {} asked for".format(entry["label"], len(places), entry["count"]))

    ground = []
    patches = []

    for entry in listed["walk"]:
        low = place(entry["x"][0], entry["y"][0])
        high = place(entry["x"][1], entry["y"][1])
        patches.append((min(low.x, high.x), max(low.x, high.x), min(low.y, high.y), max(low.y, high.y)))

        patch = unreal.Box2D()
        patch.set_editor_property("min", unreal.Vector2D(min(low.x, high.x), min(low.y, high.y)))
        patch.set_editor_property("max", unreal.Vector2D(max(low.x, high.x), max(low.y, high.y)))
        patch.set_editor_property("is_valid", True)
        ground.append(patch)

    data.set_editor_property("name", listed["name"])
    data.set_editor_property("errand", listed.get("errand", []))
    data.set_editor_property("beds", [unreal.Name(bed) for bed in listed.get("beds", [])])

    if spoken[0]:
        log("{} of the place's lines and calls are spoken".format(spoken[0]))
    data.set_editor_property("people", people)
    data.set_editor_property("doors", doors)
    data.set_editor_property("arrivals", arrivals)
    data.set_editor_property("stalls", stalls)
    data.set_editor_property("calls", calls)

    # the way a sale's lots come up and go down, and how far behind their marks they walk
    data.set_editor_property("lot_way", [place(x, y) for x, y in listed.get("lot_way", [])])
    data.set_editor_property("lot_down", [place(x, y) for x, y in listed.get("lot_down", [])])
    data.set_editor_property("lot_aisle", float(listed.get("lot_aisle", 0.7)) * 100.0)
    data.set_editor_property("ground", ground)

    # how high the ground is, for whoever is walked along it
    data.set_editor_property("street_z", (street - origin["z"]) * 100.0)
    data.set_editor_property("climb_from", (alley["from_y"] - origin["y"]) * 100.0)
    data.set_editor_property("climb_rise", float(alley["rise_per_metre"]))

    standing = []

    for middle_x, middle_y, half_x, half_y, yaw in blocks:
        block = unreal.TownBlock()
        block.set_editor_property("middle", unreal.Vector2D(middle_x, middle_y))
        block.set_editor_property("half_size", unreal.Vector2D(half_x, half_y))
        block.set_editor_property("yaw", yaw)
        standing.append(block)

    data.set_editor_property("blocks", standing)
    save(data)

    # nobody is to be stopped by nothing: wherever the ground ends, something has to stand
    # except at an open way to another place, which is where the ground is meant to go on
    ways = [place(d["x"], d["y"]) for d in listed["doors"] if d.get("walk_through") and not d.get("mouth")]
    mouths = [(place(d["mouth"]["x"][0], d["mouth"]["y"][0]), place(d["mouth"]["x"][1], d["mouth"]["y"][1])) for d in listed["doors"] if d.get("mouth")]

    def at_a_way(begin, end):
        middle = ((begin[0] + end[0]) * 0.5, (begin[1] + end[1]) * 0.5)

        if any(math.hypot(middle[0] - way.x, middle[1] - way.y) < 320.0 for way in ways):
            return True

        # the far edge of a mouth, and a metre round it
        return any(min(a.x, b.x) - 100.0 <= middle[0] <= max(a.x, b.x) + 100.0 and min(a.y, b.y) - 100.0 <= middle[1] <= max(a.y, b.y) + 100.0 for a, b in mouths)

    bare = [(begin, end) for begin, end in open_edges(patches, blocks) if not at_a_way(begin, end)]

    for begin, end in bare:
        log("OPEN EDGE: nothing stands along the ground's edge from ({:.1f}, {:.1f}) to ({:.1f}, {:.1f}) on the map, {:.1f} m".format(
            begin[1] / 100.0 + origin["x"], begin[0] / 100.0 + origin["y"], end[1] / 100.0 + origin["x"], end[0] / 100.0 + origin["y"],
            math.hypot(end[0] - begin[0], end[1] - begin[1]) / 100.0))

    # and the town's story, which every place's people speak from
    story()

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    log("placed: " + ", ".join("{} {}".format(count, kind) for kind, count in sorted(counts.items())))
    log("{}: {} people, {} doors, {} patches of ground, {} things to walk round, {} loose things".format(listed["name"], len(people), len(doors), len(ground), len(blocks), loose_count))

    if bare:
        log("OPEN EDGES: {}. The level is saved so that they can be looked at, but it is not fit to walk: stand something along each".format(len(bare)))

    log("DONE")


try:
    main()
except Exception:
    log("FAILED\n" + traceback.format_exc())
