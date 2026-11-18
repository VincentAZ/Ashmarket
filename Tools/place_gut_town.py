"""
Stands a piece of the town round the Gut, so that somebody in the pool who looks up sees Ashmarket over the rim and not
an empty sky: shipping containers people live in, stacked two and three high; a gutted bus; the broken freeway overpass.

It makes nothing itself. The four models were generated in Mixar from reference pictures (ArtSource/mixar/town_*), the same
way as the floodlight trucks, and are brought in through the engine's own glTF importer. What is decided here is only where
each one stands, and where a few bare bulbs burn among them.

Run it through Tools/place_gut_town.sh, which starts the editor with no window. It is safe to run again: everything it
placed before (every actor whose label begins "Town_") is taken away first. GUT_TOWN=remove only takes it away.
"""

import os
import traceback

import unreal

ROOT = "/Game/Gut/Art/Mixar"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "mixar")

eal = unreal.EditorAssetLibrary

# Lengths in cm. The pool's floor is 14 m by 10 m about the middle of the level, long way along X; the deck runs 8 m back
# from its edge on every side, so to 15 m along X and 13 m along Y, and the ground beyond the deck is lower than the pool's floor.
# Each thing: the model, where it stands (x, y), what it stands on ("deck", "ground", or a height above the deck in cm for
# something stacked), and the way its long side is turned.
CONTAINER_HIGH = 259.0

TOWN = [
    # on the deck, behind the crowd and clear of the four floodlight trucks: the row along each long side
    ("town_container_red", -720.0, 1150.0, "deck", 2.0),
    ("town_container_blue", 880.0, 1160.0, "deck", -3.0),
    ("town_container_red", 700.0, 1150.0, CONTAINER_HIGH, 8.0),
    ("town_bus", 520.0, -1140.0, "deck", 178.0),
    ("town_container_red", -960.0, -1140.0, "deck", -4.0),
    ("town_container_blue", -1000.0, -1150.0, CONTAINER_HIGH, 3.0),

    # and at the two ends
    ("town_container_red", 1340.0, -820.0, "deck", 92.0),
    ("town_container_red", 1350.0, -790.0, CONTAINER_HIGH, 84.0),
    ("town_container_blue", -1350.0, 720.0, "deck", 88.0),

    # beyond the deck, on the ground: the stacks that make the skyline, three high
    ("town_container_blue", -400.0, 2250.0, "ground", 12.0),
    ("town_container_red", -420.0, 2260.0, "ground+1", 7.0),
    ("town_container_red", -150.0, 2240.0, "ground+2", 15.0),
    ("town_container_blue", 1300.0, 2100.0, "ground", -20.0),
    ("town_container_blue", 1320.0, 2080.0, "ground+1", -14.0),
    ("town_container_red", 1100.0, 2090.0, "ground+2", -24.0),
    ("town_container_red", 2250.0, 500.0, "ground", 80.0),
    ("town_container_blue", 2270.0, 520.0, "ground+1", 86.0),
    ("town_container_red", 2240.0, 300.0, "ground+2", 77.0),
    ("town_container_blue", -2300.0, -400.0, "ground", 95.0),
    ("town_container_red", -2290.0, -380.0, "ground+1", 101.0),
    ("town_container_red", -2320.0, -600.0, "ground+2", 92.0),
    ("town_bus", -1900.0, 1700.0, "ground", 40.0),
    ("town_container_red", 1900.0, -1900.0, "ground", -35.0),
    ("town_container_blue", 1880.0, -1880.0, "ground+1", -30.0),

    # the freeway: three spans in a line beyond the long side of the pool, the last of them the broken one
    ("town_overpass", -2900.0, -2550.0, "ground", 180.0),
    ("town_overpass", -100.0, -2550.0, "ground", 180.0),
    ("town_overpass", 2700.0, -2550.0, "ground", 180.0),
]

# bare bulbs among them: where (x, y), how high above what it hangs over ("deck" or "ground"), and how bright in candelas.
# Warm, small, and throwing no shadows: they are there to be seen by, like the bulbs strung through the town.
BULBS = [
    (-720.0, 1040.0, "deck", 230.0, 420.0), (880.0, 1050.0, "deck", 240.0, 380.0), (520.0, -1010.0, "deck", 250.0, 450.0),
    (-960.0, -1030.0, "deck", 230.0, 360.0), (1230.0, -820.0, "deck", 240.0, 380.0), (-1240.0, 720.0, "deck", 240.0, 400.0),
    (-380.0, 2120.0, "ground", 560.0, 700.0), (1290.0, 1960.0, "ground", 820.0, 700.0), (2120.0, 480.0, "ground", 560.0, 650.0),
    (-2170.0, -420.0, "ground", 820.0, 650.0), (-1780.0, 1600.0, "ground", 330.0, 500.0), (1780.0, -1790.0, "ground", 560.0, 600.0),
    (-1500.0, -2550.0, "ground", 820.0, 2600.0), (1300.0, -2550.0, "ground", 820.0, 2600.0), (-100.0, -2300.0, "ground", 500.0, 1600.0),
]


# fires and lamps down among the shacks, seen only by what they light: each a warm wash thrown up a face of something big,
# from (x, y, height above the ground) at a point (x, y, height above the ground), with its brightness in candelas and its spread
WASHES = [
    ((-1700.0, -1750.0, 60.0), (-1900.0, -2550.0, 1150.0), 60000.0, 48.0),
    ((300.0, -1800.0, 60.0), (-100.0, -2550.0, 1150.0), 70000.0, 50.0),
    ((2000.0, -1800.0, 60.0), (2600.0, -2550.0, 1150.0), 50000.0, 48.0),
    ((-900.0, 1750.0, 40.0), (-350.0, 2250.0, 520.0), 9000.0, 55.0),
    ((1750.0, 1500.0, 40.0), (1250.0, 2090.0, 520.0), 9000.0, 55.0),
    ((1800.0, 150.0, 40.0), (2250.0, 430.0, 520.0), 8000.0, 55.0),
    ((-1850.0, -150.0, 40.0), (-2300.0, -450.0, 520.0), 8000.0, 55.0),
]


# the freeway model has a street lamp standing on it. One of the three still burns: which span (counted along the line), and
# how bright in candelas. It is hung just under the lamp's own head, which is found from the model and not written down here.
LAMPS = {"town_overpass": {1: 26000.0, 2: 9000.0}}


def log(text):
    unreal.log("[place_gut_town] " + text)


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def model(name):
    """The model's mesh, brought in from its glTF file if it is not in the project yet."""
    folder = "{}/{}".format(ROOT, name)

    def found():
        if not eal.does_directory_exist(folder):
            return []

        return [asset for asset in eal.list_assets(folder, recursive=True) if isinstance(unreal.load_asset(asset), unreal.StaticMesh)]

    meshes = found()

    if not meshes:
        path = os.path.join(SOURCE, name, name + ".gltf")

        if not os.path.exists(path):
            log("skipped {}: not exported from Mixar yet".format(name))
            return None

        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property("is_automated", True)
        parameters.set_editor_property("replace_existing", True)

        unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(folder, unreal.InterchangeManager.create_source_data(path), parameters)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

        meshes = found()

    if len(meshes) != 1:
        raise RuntimeError("{} came in as {} meshes".format(name, len(meshes)))

    mesh = unreal.load_asset(meshes[0])

    # the importer turns Nanite on, which the materials it makes for a glTF file are not set up for
    meshes_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    nanite = meshes_editor.get_nanite_settings(mesh)

    if nanite.enabled:
        nanite.enabled = False
        meshes_editor.set_nanite_settings(mesh, nanite, apply_changes=True)
        save(mesh)

    bounds = mesh.get_bounds()

    log("{}: {} triangles, {:.0f} x {:.0f} x {:.0f} cm, lowest point {:.0f}".format(name, mesh.get_num_triangles(0),
        bounds.box_extent.x * 2.0, bounds.box_extent.y * 2.0, bounds.box_extent.z * 2.0, bounds.origin.z - bounds.box_extent.z))

    return mesh


def lamp_head(mesh):
    """Where the head of the model's street lamp is, in the model's own space: the middle of its highest half metre."""
    points = []

    for section in range(mesh.get_num_sections(0)):
        points.extend(unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)[0])

    if not points:
        return None

    top = max(point.z for point in points)
    head = [point for point in points if point.z > top - 50.0]

    return unreal.Vector(sum(point.x for point in head) / len(head), sum(point.y for point in head) / len(head), top)


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Gut/Lvl_Gut")

    deck = 320.0
    ground = -100.0

    removed = 0

    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith("Town_"):
            actors.destroy_actor(actor)
            removed += 1
        elif isinstance(actor, unreal.GutCage):
            deck = actor.get_actor_location().z + actor.get_editor_property("wall_height")

    log("took away {} things placed before; the deck is at {:.0f}".format(removed, deck))

    if os.environ.get("GUT_TOWN") != "remove":
        models = {name: model(name) for name in sorted({entry[0] for entry in TOWN})}

        def height(on):
            if on == "deck":
                return deck

            if isinstance(on, str) and on.startswith("ground"):
                return ground + (int(on.split("+")[1]) * CONTAINER_HIGH if "+" in on else 0.0)

            return deck + float(on)

        placed = 0
        lamps = 0
        seen = {}

        for index, (name, x, y, on, yaw) in enumerate(TOWN):
            mesh = models.get(name)

            if not mesh:
                continue

            seen[name] = seen.get(name, 0) + 1

            thing = actors.spawn_actor_from_object(mesh, unreal.Vector(x, y, height(on)), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            thing.set_actor_label("Town_{:02d}_{}".format(index + 1, name))

            part = thing.get_editor_property("static_mesh_component")

            # nobody walks out there, nothing is thrown that far, and paint on the pool stays on the pool
            part.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            part.set_editor_property("receives_decals", False)
            part.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

            placed += 1

            # a street lamp on the model that still burns: light thrown down from under its head onto the road it stands on
            candelas = LAMPS.get(name, {}).get(seen[name])
            head = lamp_head(mesh) if candelas else None

            if head:
                where = thing.get_actor_transform().transform_location(unreal.Vector(head.x, head.y, head.z - 70.0))

                lamp = actors.spawn_actor_from_class(unreal.SpotLight, where, unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
                lamp.set_actor_label("Town_Lamp_{:02d}".format(index + 1))

                light = lamp.get_editor_property("light_component")
                light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
                light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
                light.set_editor_property("intensity", candelas)
                light.set_editor_property("attenuation_radius", 2600.0)
                light.set_editor_property("inner_cone_angle", 40.0)
                light.set_editor_property("outer_cone_angle", 72.0)
                light.set_editor_property("source_radius", 14.0)
                light.set_editor_property("light_color", unreal.Color(r=255, g=158, b=72, a=255))
                light.set_editor_property("cast_shadows", False)
                light.set_editor_property("volumetric_scattering_intensity", 2.0)

                log("{}: its lamp burns at ({:.0f}, {:.0f}, {:.0f})".format(thing.get_actor_label(), where.x, where.y, where.z))

                lamps += 1

        for index, (x, y, on, above, candelas) in enumerate(BULBS):
            bulb = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, height(on) + above), unreal.Rotator(0.0, 0.0, 0.0))
            bulb.set_actor_label("Town_Bulb_{:02d}".format(index + 1))

            light = bulb.get_editor_property("light_component")
            light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
            light.set_editor_property("intensity", candelas)
            light.set_editor_property("attenuation_radius", 1500.0 if candelas < 1000.0 else 3200.0)
            light.set_editor_property("source_radius", 6.0)
            light.set_editor_property("light_color", unreal.Color(r=255, g=176, b=96, a=255))
            light.set_editor_property("cast_shadows", False)
            light.set_editor_property("volumetric_scattering_intensity", 1.5)

        for index, (start, at, candelas, spread) in enumerate(WASHES):
            where = unreal.Vector(start[0], start[1], ground + start[2])
            aim = unreal.Vector(at[0], at[1], ground + at[2])

            wash = actors.spawn_actor_from_class(unreal.SpotLight, where, unreal.MathLibrary.find_look_at_rotation(where, aim))
            wash.set_actor_label("Town_Wash_{:02d}".format(index + 1))

            light = wash.get_editor_property("light_component")
            light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
            light.set_editor_property("intensity", candelas)
            light.set_editor_property("attenuation_radius", 3600.0)
            light.set_editor_property("inner_cone_angle", spread * 0.5)
            light.set_editor_property("outer_cone_angle", spread)
            light.set_editor_property("light_color", unreal.Color(r=255, g=150, b=70, a=255))
            light.set_editor_property("cast_shadows", False)
            light.set_editor_property("volumetric_scattering_intensity", 0.0)

        log("placed {} things, {} bulbs, {} washes and {} street lamps".format(placed, len(BULBS), len(WASHES), lamps))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    log("DONE")


try:
    main()
except Exception:
    for line in traceback.format_exc().splitlines():
        log("ERROR " + line)
