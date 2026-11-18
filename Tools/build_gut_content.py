"""
Builds the Gut prototype content under /Game/Gut:
materials, input assets, fighter / controller / game mode Blueprints and the cage level.

Everything it creates is data only. All behaviour lives in Source/Ashmarket/Gut.

It only adds what is missing. An asset that already exists is kept, so values tuned by hand in the editor
survive a re-run. It changes existing assets in these narrow ways only:
- on a Blueprint, it fills in a reference or a list that is still empty (a weapon, an animation, an input action, the rack, the ladder),
  or a list left with a hole in it by an asset that was deleted to be made again
- on a weapon, it fills in a description that is still empty
- on an input mapping context, it adds bindings for an action that has none
- in the cage level, it makes sure the fighter standing opposite the player is the current opponent,
  that there is a weapon rack on the wall, that the hazards are in place (spikes in a wall and a live cable),
  and that there is a crowd on the deck

Run it through Tools/build_gut_content.sh, which starts the editor with no window.
"""

import traceback

import unreal

ROOT = "/Game/Gut"

MANNEQUIN_ANIMS = "/Game/Characters/Mannequins/Anims"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("[build_gut_content] {}".format(message))


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: {}".format(path))
    return asset


def create(name, folder, asset_class, factory):
    """Returns (asset, True) for a new asset, or (existing asset, False) if it is already there."""
    path = "{}/{}".format(folder, name)
    if eal.does_asset_exist(path):
        log("kept {}".format(path))
        return load(path), False
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create {}".format(path))
    return asset, True


def save(asset):
    if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))
    log("saved {}".format(asset.get_path_name()))


def key(name):
    result = unreal.Key()
    result.import_text(name)
    return result


def is_empty(value):
    """True for no reference, no text, a list with nothing in it, or a list with a hole where a deleted asset used to be."""
    if value is None:
        return True
    if isinstance(value, unreal.Text):
        return len(str(value)) == 0
    if isinstance(value, unreal.Array):
        return len(value) == 0 or any(item is None for item in value)
    return False


def fill_empty(target, values):
    """Sets each property that is still empty. Returns True if anything changed."""
    changed = False
    for name, value in values.items():
        if is_empty(target.get_editor_property(name)):
            target.set_editor_property(name, value)
            changed = True
    return changed


# --- materials ---------------------------------------------------------------

def build_materials():
    folder = ROOT + "/Materials"

    # one flat, tintable material for every grey box that is not on the prototype grid
    flat, created = create("M_Gut_Flat", folder, unreal.Material, unreal.MaterialFactoryNew())

    if created:
        build_flat_material(flat)

    def instance(name, rgb):
        mi, created = create(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if created:
            mi.set_editor_property("parent", flat)
            mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            mel.update_material_instance(mi)
            save(mi)
        return mi

    blood, created = create("M_Gut_Blood", folder, unreal.Material, unreal.MaterialFactoryNew())

    if created:
        build_blood_material(blood)

    # the art pass: surfaces made from sums on world position, so they need no textures and no unwrapping
    def surface(name, builder):
        material, created = create(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        if created:
            builder(material)
            mel.recompile_material(material)
            save(material)
        return material

    def scanned(name, fallback, builder):
        # the pool is tiled with scanned surfaces once Tools/import_gut_art.sh has brought them in
        path = "/Game/Gut/Art/Surfaces/" + name
        return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else surface(fallback, builder)

    pool_wall = scanned("MI_Gut_PoolWall", "M_Gut_PoolWall", build_pool_wall_material)
    pool_floor = scanned("MI_Gut_PoolFloor", "M_Gut_PoolFloor", build_pool_floor_material)
    rust = surface("M_Gut_Rust", build_rust_material)
    glow = surface("M_Gut_Glow", build_glow_material)

    def glowing(name, rgb, strength):
        mi, created = create(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if created:
            mi.set_editor_property("parent", glow)
            mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            mel.set_material_instance_scalar_parameter_value(mi, "Glow", strength)
            mel.update_material_instance(mi)
            save(mi)
        return mi

    return {
        "flat": flat,
        "blood": blood,
        "pool_wall": pool_wall,
        "pool_floor": pool_floor,
        "rust": rust,
        "cable_hot": glowing("MI_Gut_CableHot", (1.0, 0.7, 0.15), 30.0),
        "lamp": glowing("MI_Gut_Lamp", (1.0, 0.93, 0.8), 60.0),
        "bars": instance("MI_Gut_Bars", (0.09, 0.05, 0.035)),
        "guard": instance("MI_Gut_GuardPlate", (0.32, 0.36, 0.42)),
        "weapon": instance("MI_Gut_Weapon", (0.05, 0.05, 0.06)),

        # one flat color for each weapon, so they can be told apart across the cage
        "blade": instance("MI_Gut_Blade", (0.55, 0.6, 0.62)),
        "chain": instance("MI_Gut_Chain", (0.5, 0.17, 0.03)),
        "shield": instance("MI_Gut_Shield", (0.6, 0.05, 0.04)),
        "rack": instance("MI_Gut_Rack", (0.42, 0.33, 0.22)),

        # armor: what it is cut from
        "tire_rubber": instance("MI_Gut_TireRubber", (0.02, 0.02, 0.02)),
        "road_sign": instance("MI_Gut_RoadSign", (0.7, 0.5, 0.03)),

        # the hazards: dull while they are off, loud once they are live
        "spike_plate": instance("MI_Gut_SpikePlate", (0.1, 0.09, 0.08)),
        "spikes": instance("MI_Gut_Spikes", (0.32, 0.27, 0.22)),
        "spikes_live": instance("MI_Gut_SpikesLive", (0.8, 0.03, 0.02)),
        "generator": instance("MI_Gut_Generator", (0.2, 0.22, 0.08)),
        "cable": instance("MI_Gut_Cable", (0.015, 0.015, 0.015)),
        "cable_live": instance("MI_Gut_CableLive", (1.0, 0.9, 0.2)),
    }


def build_flat_material(flat):
    color = mel.create_material_expression(flat, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.3, 0.3, 0.3, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = mel.create_material_expression(flat, unreal.MaterialExpressionScalarParameter, -400, 250)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.8)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # the cage lid is an instanced static mesh
    flat.set_editor_property("used_with_instanced_static_meshes", True)

    mel.recompile_material(flat)
    save(flat)


class Graph:
    """Builds a material's expressions one after another, wiring each to the ones it is given. Numbers are taken as constants."""

    def __init__(self, material):
        self.material = material
        self.count = 0

    def node(self, expression_class, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -2600 + (self.count // 8) * 230, (self.count % 8) * 150)
        self.count += 1
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def binary(self, expression_class, a, b):
        expression = self.node(expression_class)
        for value, pin, constant in ((a, "A", "const_a"), (b, "B", "const_b")):
            if isinstance(value, (int, float)):
                expression.set_editor_property(constant, float(value))
            else:
                mel.connect_material_expressions(value, "", expression, pin)
        return expression

    def unary(self, expression_class, a):
        expression = self.node(expression_class)
        mel.connect_material_expressions(a, "", expression, "")
        return expression

    def add(self, a, b): return self.binary(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.binary(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.binary(unreal.MaterialExpressionMultiply, a, b)
    def div(self, a, b): return self.binary(unreal.MaterialExpressionDivide, a, b)
    def max(self, a, b): return self.binary(unreal.MaterialExpressionMax, a, b)
    def frac(self, a): return self.unary(unreal.MaterialExpressionFrac, a)
    def abs(self, a): return self.unary(unreal.MaterialExpressionAbs, a)
    def saturate(self, a): return self.unary(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.unary(unreal.MaterialExpressionOneMinus, a)

    def lerp(self, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate)
        for value, pin in ((a, "A"), (b, "B"), (alpha, "Alpha")):
            mel.connect_material_expressions(value, "", expression, pin)
        return expression

    def channel(self, a, name):
        expression = self.node(unreal.MaterialExpressionComponentMask, r=name == "r", g=name == "g", b=name == "b", a=False)
        mel.connect_material_expressions(a, "", expression, "")
        return expression

    def color(self, rgb): return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    def scalar(self, value): return self.node(unreal.MaterialExpressionConstant, r=value)
    def noise(self, scale, low, high): return self.node(unreal.MaterialExpressionNoise, scale=scale, output_min=low, output_max=high)
    def world(self): return self.node(unreal.MaterialExpressionWorldPosition)

    def out(self, expression, material_property):
        mel.connect_material_property(expression, "", material_property)

    def grout(self, coordinate, tile_size, share=0.07):
        """One where the given coordinate falls on the joint between two tiles of the given size, zero on the tile."""
        across = self.frac(self.div(coordinate, tile_size))
        from_middle = self.mul(self.abs(self.sub(across, 0.5)), 2.0)
        return self.saturate(self.mul(self.sub(from_middle, 1.0 - share), 40.0))


def build_pool_wall_material(material):
    """
    The walls of an emptied pool: small glazed tiles, patchy, filthy toward the bottom, with a stain where the water used to stand.
    Tiles run along the wall whichever way it faces, since along a wall only one of X and Y changes.
    """
    g = Graph(material)

    position = g.world()
    along = g.add(g.channel(position, "r"), g.channel(position, "g"))
    up = g.channel(position, "b")

    joints = g.max(g.grout(along, 15.0), g.grout(up, 15.0))

    tile = g.mul(g.color((0.42, 0.55, 0.56)), g.noise(0.02, 0.75, 1.1))
    surface = g.lerp(tile, g.color((0.1, 0.1, 0.09)), joints)

    # filth settles toward the floor, and the old water line is a band of scale
    low = g.saturate(g.one_minus(g.div(up, 150.0)))
    water_line = g.saturate(g.one_minus(g.div(g.abs(g.sub(up, 250.0)), 14.0)))
    dirt = g.saturate(g.mul(g.add(g.mul(low, 0.7), g.mul(water_line, 0.55)), g.noise(0.012, 0.45, 1.25)))

    g.out(g.lerp(surface, g.color((0.11, 0.08, 0.05)), dirt), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.lerp(g.scalar(0.3), g.scalar(0.9), g.max(joints, dirt)), unreal.MaterialProperty.MP_ROUGHNESS)


def build_pool_floor_material(material):
    """The floor of the pool: larger tiles, the dark lane lines still there, rust-brown where water sat longest."""
    g = Graph(material)

    position = g.world()
    x = g.channel(position, "r")
    y = g.channel(position, "g")

    joints = g.max(g.grout(x, 20.0), g.grout(y, 20.0))

    # a lane every two and a half metres, its line down the middle
    from_line = g.mul(g.abs(g.sub(g.frac(g.div(y, 250.0)), 0.5)), 250.0)
    lane_line = g.saturate(g.mul(g.sub(13.0, from_line), 0.6))

    tile = g.mul(g.lerp(g.color((0.5, 0.6, 0.6)), g.color((0.02, 0.04, 0.09)), lane_line), g.noise(0.02, 0.75, 1.1))
    surface = g.lerp(tile, g.color((0.09, 0.09, 0.08)), joints)

    stains = g.saturate(g.mul(g.sub(g.noise(0.005, 0.0, 1.5), 1.28), 1.2))

    g.out(g.lerp(surface, g.color((0.14, 0.08, 0.04)), stains), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.lerp(g.scalar(0.25), g.scalar(0.9), g.max(joints, stains)), unreal.MaterialProperty.MP_ROUGHNESS)


def build_rust_material(material):
    """Rusted steel, for the bars of the lid and the spikes."""
    g = Graph(material)

    g.out(g.lerp(g.color((0.06, 0.025, 0.015)), g.color((0.33, 0.13, 0.04)), g.saturate(g.noise(0.07, -0.2, 1.2))), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar(0.85), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.scalar(0.6), unreal.MaterialProperty.MP_METALLIC)

    # the lid and the spikes are instanced static meshes
    material.set_editor_property("used_with_instanced_static_meshes", True)


def build_glow_material(material):
    """Something that gives off light of its own: a lamp, or the cable when it is live."""
    g = Graph(material)

    color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.9, 0.7, 1.0))
    strength = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=20.0)

    g.out(g.mul(color, strength), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(0.0), unreal.MaterialProperty.MP_BASE_COLOR)


def build_blood_material(blood):
    """A decal for blood on the floor: dark, wet, round, with a ragged edge that is different wherever it lands."""
    blood.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    blood.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    color = mel.create_material_expression(blood, unreal.MaterialExpressionConstant3Vector, -400, -200)
    color.set_editor_property("constant", unreal.LinearColor(0.07, 0.002, 0.002, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = mel.create_material_expression(blood, unreal.MaterialExpressionConstant, -400, -50)
    roughness.set_editor_property("r", 0.15)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # how far from the middle of the decal, from one at the center to nothing at the edge
    uv = mel.create_material_expression(blood, unreal.MaterialExpressionTextureCoordinate, -1400, 200)

    centered = mel.create_material_expression(blood, unreal.MaterialExpressionSubtract, -1200, 200)
    centered.set_editor_property("const_b", 0.5)
    mel.connect_material_expressions(uv, "", centered, "A")

    distance = mel.create_material_expression(blood, unreal.MaterialExpressionLength, -1000, 200)
    mel.connect_material_expressions(centered, "", distance, "")

    doubled = mel.create_material_expression(blood, unreal.MaterialExpressionMultiply, -850, 200)
    doubled.set_editor_property("const_b", 2.0)
    mel.connect_material_expressions(distance, "", doubled, "A")

    falloff = mel.create_material_expression(blood, unreal.MaterialExpressionOneMinus, -700, 200)
    mel.connect_material_expressions(doubled, "", falloff, "")

    # noise in the world, so no two splatters have the same edge
    noise = mel.create_material_expression(blood, unreal.MaterialExpressionNoise, -850, 400)
    noise.set_editor_property("scale", 0.09)
    noise.set_editor_property("output_min", 0.25)
    noise.set_editor_property("output_max", 1.5)

    ragged = mel.create_material_expression(blood, unreal.MaterialExpressionMultiply, -550, 300)
    mel.connect_material_expressions(falloff, "", ragged, "A")
    mel.connect_material_expressions(noise, "", ragged, "B")

    gained = mel.create_material_expression(blood, unreal.MaterialExpressionMultiply, -400, 300)
    gained.set_editor_property("const_b", 5.0)
    mel.connect_material_expressions(ragged, "", gained, "A")

    lifted = mel.create_material_expression(blood, unreal.MaterialExpressionSubtract, -250, 300)
    lifted.set_editor_property("const_b", 0.8)
    mel.connect_material_expressions(gained, "", lifted, "A")

    mask = mel.create_material_expression(blood, unreal.MaterialExpressionSaturate, -100, 300)
    mel.connect_material_expressions(lifted, "", mask, "")
    mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(blood)
    save(blood)


# --- meshes ------------------------------------------------------------------

class Shape:
    """
    A mesh put together from simple solids and saved as a static mesh. Lengths are centimetres.
    Everything is made standing up its own Z axis, since that is the way a weapon lies in the hand.
    Needs the Geometry Script plugin, which the project enables for the editor only.
    """

    def __init__(self):
        self.mesh = unreal.DynamicMesh()
        self.options = unreal.GeometryScriptPrimitiveOptions()

    @staticmethod
    def at(location=(0.0, 0.0, 0.0), roll=0.0, pitch=0.0, yaw=0.0, scale=(1.0, 1.0, 1.0)):
        return unreal.Transform(location=unreal.Vector(*location), rotation=unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw), scale=unreal.Vector(*scale))

    def cylinder(self, radius, height, steps=12, **where):
        unreal.GeometryScript_Primitives.append_cylinder(self.mesh, self.options, self.at(**where), radius=radius, height=height, radial_steps=steps)

    def box(self, x, y, z, **where):
        unreal.GeometryScript_Primitives.append_box(self.mesh, self.options, self.at(**where), dimension_x=x, dimension_y=y, dimension_z=z)

    def ring(self, major, minor, major_steps=14, minor_steps=6, **where):
        unreal.GeometryScript_Primitives.append_torus(self.mesh, self.options, self.at(**where), unreal.GeometryScriptRevolveOptions(),
                                                      major_radius=major, minor_radius=minor, major_steps=major_steps, minor_steps=minor_steps,
                                                      origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)

    def plate(self, outline, thickness, **where):
        """A flat shape with the given outline, counter-clockwise in X and Y, and the given thickness up Z."""
        unreal.GeometryScript_Primitives.append_simple_extrude_polygon(self.mesh, self.options, self.at(**where),
                                                                       [unreal.Vector2D(x, y) for x, y in outline], height=thickness)

    def save(self, name):
        path = "{}/Meshes/{}".format(ROOT, name)

        # edges sharper than the default angle stay hard, the rest are smoothed
        unreal.GeometryScript_Normals.compute_split_normals(self.mesh, unreal.GeometryScriptSplitNormalsOptions(), unreal.GeometryScriptCalculateNormalsOptions())

        asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            self.mesh, path, unreal.GeometryScriptCreateNewStaticMeshAssetOptions())

        if asset is None:
            raise RuntimeError("Could not create {}".format(path))

        # a box round it is enough for it to lie on the floor when it is dropped
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).add_simple_collisions(asset, unreal.ScriptCollisionShapeType.BOX)

        save(asset)

        return asset


def mesh(name, builder):
    """Returns the static mesh of the given name, building it first if it is not there yet."""
    path = "{}/Meshes/{}".format(ROOT, name)

    if eal.does_asset_exist(path):
        log("kept {}".format(path))
        return load(path)

    shape = Shape()
    builder(shape)

    return shape.save(name)


def shape_rebar_club(shape):
    """A length of reinforcing bar: ribbed all the way up, with tape wound round the end it is held by."""
    shape.cylinder(1.5, 92.0, location=(0.0, 0.0, -14.0))

    # the ribs, and the two seams that run the length of the bar
    for i in range(18):
        shape.ring(1.75, 0.38, major_steps=10, minor_steps=4, location=(0.0, 0.0, 10.0 + (i * 3.9)), pitch=12.0)

    for side in (-1.0, 1.0):
        shape.box(0.5, 0.5, 68.0, location=(side * 1.5, 0.0, 9.0))

    shape.cylinder(2.3, 21.0, steps=10, location=(0.0, 0.0, -13.0))


def shape_machete(shape):
    """A leaf spring ground to an edge: a long plate that widens toward a clipped point, with a wrapped tang."""
    outline = [(-1.3, -13.0), (1.3, -13.0), (1.3, 0.0), (3.2, 2.5), (4.4, 38.0), (3.4, 54.0), (0.2, 61.0),
               (-2.4, 58.0), (-2.6, 30.0), (-2.4, 2.5), (-1.3, 0.0)]

    # made lying flat, then stood on end
    shape.plate(outline, 0.6, location=(0.0, 0.3, 0.0), roll=90.0)

    # the wrap, and the guard where blade meets tang
    shape.box(3.4, 2.2, 12.5, location=(0.0, 0.0, -13.0))
    shape.box(6.5, 1.6, 1.4, location=(0.4, 0.0, -0.4))


def shape_tow_chain(shape):
    """Two metres of chain: oval links, each turned a quarter round from the last, and a hook on the far end."""
    for i in range(26):
        shape.ring(2.6, 0.75, major_steps=10, minor_steps=5, location=(0.0, 0.0, 4.0 + (i * 7.5)), pitch=90.0, yaw=90.0 * (i % 2), scale=(1.8, 1.0, 1.0))

    # the hook, as a heavier half ring with a shank
    shape.cylinder(1.3, 9.0, steps=8, location=(0.0, 0.0, 196.0))
    shape.ring(4.2, 1.3, major_steps=10, minor_steps=5, location=(0.0, 0.0, 208.0), pitch=90.0)


def shape_stop_sign(shape):
    """A stop sign with a strap bolted to the back: eight sides, a raised face inside the border, four bolts."""
    import math

    def octagon(radius):
        return [(radius * math.cos(math.radians(22.5 + (45.0 * i))), radius * math.sin(math.radians(22.5 + (45.0 * i)))) for i in range(8)]

    # made lying flat, then stood up to face along X, which is the way the arm it is strapped to faces
    shape.plate(octagon(31.0), 1.0, location=(0.0, 0.0, 0.0), pitch=-90.0)
    shape.plate(octagon(27.5), 1.6, location=(0.3, 0.0, 0.0), pitch=-90.0)

    for y, z in ((-14.0, -14.0), (14.0, -14.0), (-14.0, 14.0), (14.0, 14.0)):
        shape.cylinder(1.1, 3.4, steps=6, location=(-1.2, y, z), pitch=-90.0)

    # the strap the forearm goes through
    shape.box(2.0, 26.0, 4.0, location=(-3.6, 0.0, -2.0))


def shape_collar(shape):
    """A welded collar: a ring of bar stock with a lump where the ends were joined."""
    shape.ring(8.2, 1.3, major_steps=16, minor_steps=6)
    shape.box(3.2, 4.2, 3.6, location=(8.2, 0.0, -1.8))


def build_meshes():
    """The art pass on what the fighters carry and wear: shapes of their own in place of the engine's cubes and cylinders."""
    return {
        "club": mesh("SM_Gut_RebarClub", shape_rebar_club),
        "machete": mesh("SM_Gut_Machete", shape_machete),
        "chain": mesh("SM_Gut_TowChain", shape_tow_chain),
        "shield": mesh("SM_Gut_StopSign", shape_stop_sign),
        "collar": mesh("SM_Gut_Collar", shape_collar),
    }


# --- input -------------------------------------------------------------------

def build_input():
    folder = ROOT + "/Input"

    def action(name):
        ia, created = create(name, folder + "/Actions", unreal.InputAction, None)
        if created:
            ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            save(ia)
        return ia

    actions = {
        "light": action("IA_Gut_LightAttack"),
        "heavy": action("IA_Gut_HeavyAttack"),
        "block": action("IA_Gut_Block"),
        "dodge": action("IA_Gut_Dodge"),
        "restart": action("IA_Gut_Restart"),
        "lock_on": action("IA_Gut_LockOn"),
    }

    # movement and camera stay on the template contexts; this one only adds the fight
    bindings = {
        "light": ["LeftMouseButton", "Gamepad_FaceButton_Left", "Gamepad_RightShoulder"],
        "heavy": ["F", "MiddleMouseButton", "Gamepad_FaceButton_Top", "Gamepad_RightTrigger"],
        "block": ["RightMouseButton", "Gamepad_LeftShoulder", "Gamepad_LeftTrigger"],
        "dodge": ["SpaceBar", "Gamepad_FaceButton_Right"],
        "restart": ["R", "Enter", "Gamepad_Special_Right"],
        "lock_on": ["Tab", "Gamepad_RightThumbstick"],
    }

    actions["context"] = context("IMC_Gut", folder, actions, bindings)

    # the rack has a context of its own, added only while the fighters stand at it,
    # so that its keys can double as fight keys without both happening at once
    rack_bindings = {
        "rack_previous": ["A", "Left", "Gamepad_DPad_Left", "Gamepad_LeftStick_Left"],
        "rack_next": ["D", "Right", "Gamepad_DPad_Right", "Gamepad_LeftStick_Right"],
        "rack_opponent": ["W", "Up", "Gamepad_DPad_Up"],
        "rack_down": ["S", "Down", "Gamepad_DPad_Down"],
        "rack_confirm": ["LeftMouseButton", "Enter", "SpaceBar", "Gamepad_FaceButton_Bottom"],
    }

    actions["rack_previous"] = action("IA_Gut_RackPrevious")
    actions["rack_next"] = action("IA_Gut_RackNext")
    actions["rack_opponent"] = action("IA_Gut_RackOpponent")
    actions["rack_down"] = action("IA_Gut_RackDown")
    actions["rack_confirm"] = action("IA_Gut_RackConfirm")

    actions["rack_context"] = context("IMC_Gut_Rack", folder, actions, rack_bindings)

    return actions


def context(name, folder, actions, bindings):
    """An input mapping context with the given keys bound to the given actions."""
    imc, created = create(name, folder, unreal.InputMappingContext, None)

    # bind every action that has no binding yet. On an existing context that is only the actions added since it was made.
    bound = set(m.action for m in imc.get_editor_property("default_key_mappings").mappings)
    changed = False

    for action_name, keys in bindings.items():
        if actions[action_name] not in bound:
            for key_name in keys:
                imc.map_key(actions[action_name], key(key_name))
            changed = True

    if changed:
        save(imc)

    return imc


# --- weapons -----------------------------------------------------------------

def attack(label, zone, anim, anim_start, anim_impact, **numbers):
    """One attack. Numbers not given keep the defaults of FGutAttackDef."""
    result = unreal.GutAttackDef()
    result.set_editor_property("label", label)
    result.set_editor_property("hit_zone", zone)
    result.set_editor_property("animation", load("{}/Unarmed/Attack/{}".format(MANNEQUIN_ANIMS, anim)))
    result.set_editor_property("anim_start_time", anim_start)
    result.set_editor_property("anim_impact_time", anim_impact)
    for name, value in numbers.items():
        result.set_editor_property(name, value)
    return result


def finisher(label, anim, anim_start, anim_impact, windup, recovery, sever=None):
    """One finisher. Sever is the bones the killing blow cuts through, for a finisher that does not leave the body whole."""
    result = unreal.GutFinisherDef()
    result.set_editor_property("sever_bones", sever or [])
    result.set_editor_property("label", label)
    result.set_editor_property("animation", load("{}/Unarmed/Attack/{}".format(MANNEQUIN_ANIMS, anim)))
    result.set_editor_property("anim_start_time", anim_start)
    result.set_editor_property("anim_impact_time", anim_impact)
    result.set_editor_property("windup", windup)
    result.set_editor_property("recovery", recovery)
    return result


def weapon(name, display_name, description, lights, heavy, clean, slow, shape=None, **properties):
    """
    One weapon. The shape is (mesh, material, grip location, scale): a basic shape held in the hand, long axis up.
    Any other properties of the weapon asset can be given by name.
    """
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GutWeaponData)

    asset, created = create(name, ROOT + "/Weapons", unreal.GutWeaponData, factory)

    if created:
        asset.set_editor_property("display_name", display_name)
        asset.set_editor_property("description", description)
        asset.set_editor_property("light_attacks", lights)
        asset.set_editor_property("heavy_attack", heavy)
        asset.set_editor_property("clean_finisher", clean)
        asset.set_editor_property("slow_finisher", slow)

        for property_name, value in properties.items():
            asset.set_editor_property(property_name, value)

        if shape:
            shape_mesh, material, location, scale = shape

            # a shape of the weapon's own, or the name of one of the engine's basic shapes
            asset.set_editor_property("mesh", load("/Engine/BasicShapes/" + shape_mesh) if isinstance(shape_mesh, str) else shape_mesh)
            asset.set_editor_property("material", material)
            asset.set_editor_property("grip_transform", unreal.Transform(
                location=unreal.Vector(*location),
                rotation=unreal.Rotator(0.0, 0.0, 0.0),
                scale=unreal.Vector(*scale)))

        save(asset)

    # weapons made before the rack have no description yet
    elif fill_empty(asset, {"description": description}):
        save(asset)

    return asset


def build_weapons(materials, meshes):
    """
    The first pass at each weapon's numbers. Once the assets exist they are the place to tune.

    Rule of thumb for chains: an attack's stun should outlast the time until the next attack of the chain lands,
    less the fastest windup in the game (0.22 s). Otherwise the fighter who was just hit can swing first.
    """
    zone = unreal.GutHitZone

    # bare hands. Every fighter has these, and falls back on them when disarmed.
    jab = dict(damage=10.0, stamina_cost=12.0, windup=0.22, active=0.1, recovery=0.26, reach=125.0, half_angle=55.0,
               lunge_distance=35.0, stun_time=0.45, knockback=30.0, block_stamina_damage=8.0)

    fists = weapon(
        "DA_Gut_Fists", "Bare hands", "What is left when the weapon is gone.",
        lights=[
            attack("Jab", zone.LEFT_ARM, "MM_Attack_01", 0.12, 0.467, **jab),
            attack("Cross", zone.RIGHT_ARM, "MM_Attack_02", 0.12, 0.467, **jab),
            attack("Kick", zone.LEFT_LEG, "MM_Attack_03", 0.05, 0.4,
                   damage=13.0, stamina_cost=15.0, windup=0.28, active=0.12, recovery=0.4, reach=140.0, half_angle=55.0,
                   lunge_distance=35.0, stun_time=0.45, knockback=80.0, block_stamina_damage=12.0),
        ],
        heavy=attack("Haymaker", zone.TORSO, "MM_ChargedAttack", 0.33, 1.161,
                     damage=28.0, stamina_cost=30.0, windup=0.65, active=0.14, recovery=0.55, reach=150.0, half_angle=60.0,
                     lunge_distance=90.0, stun_time=0.75, knockback=160.0, block_stamina_damage=45.0),
        clean=finisher("Neck Snap", "MM_Attack_02", 0.12, 0.467, windup=0.6, recovery=0.5),
        slow=finisher("Beatdown", "MM_ChargedAttack", 0.0, 1.161, windup=1.6, recovery=0.7))

    # --- the rack. Four weapons, each good at one thing. ---

    # the club: a length of rebar. The all-rounder: hits hardest, and its overhead is the guard breaker.
    swing = dict(damage=15.0, stamina_cost=14.0, windup=0.28, active=0.12, recovery=0.32, reach=160.0, half_angle=60.0,
                 lunge_distance=35.0, stun_time=0.55, knockback=45.0, block_stamina_damage=9.0)

    club = weapon(
        "DA_Gut_Club", "Rebar club", "The all-rounder. Hits hard, and its overhead breaks a guard.",
        lights=[
            attack("Swing", zone.LEFT_ARM, "MM_Attack_01", 0.12, 0.467, **swing),
            attack("Backswing", zone.RIGHT_ARM, "MM_Attack_02", 0.12, 0.467, **swing),
            attack("Leg Sweep", zone.RIGHT_LEG, "MM_Attack_03", 0.05, 0.4,
                   damage=13.0, stamina_cost=15.0, windup=0.32, active=0.12, recovery=0.45, reach=165.0, half_angle=60.0,
                   lunge_distance=35.0, stun_time=0.5, knockback=70.0, block_stamina_damage=10.0),
        ],
        heavy=attack("Overhead", zone.TORSO, "MM_ChargedAttack", 0.33, 1.161,
                     damage=34.0, stamina_cost=32.0, windup=0.72, active=0.14, recovery=0.6, reach=170.0, half_angle=55.0,
                     lunge_distance=80.0, stun_time=0.85, knockback=180.0, block_stamina_damage=50.0),
        clean=finisher("Skull Crack", "MM_Attack_01", 0.12, 0.467, windup=0.7, recovery=0.5),
        slow=finisher("Execution", "MM_ChargedAttack", 0.0, 1.161, windup=1.8, recovery=0.8),
        # held by the taped end, the rest pointing forward out of the fist
        shape=(meshes["club"], materials["rust"], (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)))

    # the machete: a sharpened leaf spring. Fast, short and light, and it takes arms:
    # cuts wear a limb down faster than blunt blows do, and its second cut and its heavy both land on the weapon arm.
    cut = dict(damage=11.0, stamina_cost=10.0, windup=0.22, active=0.1, recovery=0.24, reach=140.0, half_angle=55.0,
               lunge_distance=35.0, stun_time=0.42, knockback=25.0, block_stamina_damage=6.0, limb_damage_scale=1.4,
               damage_type=unreal.GutDamageType.BLADED)

    machete = weapon(
        "DA_Gut_Machete", "Leaf-spring machete", "Fast and short. Cuts arms, and an arm that goes takes its weapon with it.",
        lights=[
            attack("Cut", zone.LEFT_ARM, "MM_Attack_01", 0.12, 0.467, **cut),
            attack("Backcut", zone.RIGHT_ARM, "MM_Attack_02", 0.12, 0.467, **cut),
            attack("Thrust", zone.TORSO, "MM_Attack_03", 0.05, 0.4,
                   damage=13.0, stamina_cost=12.0, windup=0.26, active=0.1, recovery=0.34, reach=150.0, half_angle=40.0,
                   lunge_distance=45.0, stun_time=0.45, knockback=50.0, block_stamina_damage=8.0,
                   damage_type=unreal.GutDamageType.BLADED),
        ],
        heavy=attack("Hack", zone.RIGHT_ARM, "MM_ChargedAttack", 0.33, 1.161,
                     damage=26.0, stamina_cost=26.0, windup=0.55, active=0.12, recovery=0.5, reach=150.0, half_angle=55.0,
                     lunge_distance=80.0, stun_time=0.7, knockback=120.0, block_stamina_damage=36.0,
                     damage_type=unreal.GutDamageType.BLADED),
        clean=finisher("Throat Cut", "MM_Attack_02", 0.12, 0.467, windup=0.6, recovery=0.5),
        slow=finisher("Butchery", "MM_ChargedAttack", 0.0, 1.161, windup=1.9, recovery=0.8, sever=["upperarm_l", "upperarm_r", "head"]),
        shape=(meshes["machete"], materials["blade"], (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        block_cost_scale=1.15)

    # the chain: a tow chain. Long and slow. It hits from outside the reach of everything else and goes for the legs,
    # but every swing takes a long time to come round, and it is poor to guard with.
    # Its long stun is what the rule of thumb above asks of a swing this slow.
    lash = dict(damage=14.0, stamina_cost=15.0, windup=0.36, active=0.14, recovery=0.38, reach=270.0, half_angle=70.0,
                lunge_distance=20.0, stun_time=0.66, knockback=40.0, block_stamina_damage=12.0)

    chain = weapon(
        "DA_Gut_Chain", "Tow chain", "Long and slow. Hits from outside their reach and takes the legs. Poor to guard with.",
        lights=[
            attack("Lash", zone.LEFT_LEG, "MM_Attack_01", 0.12, 0.467, **lash),
            attack("Backlash", zone.RIGHT_LEG, "MM_Attack_02", 0.12, 0.467, **lash),
        ],
        heavy=attack("Whip Round", zone.LEFT_LEG, "MM_ChargedAttack", 0.33, 1.161,
                     damage=32.0, stamina_cost=34.0, windup=0.85, active=0.16, recovery=0.7, reach=290.0, half_angle=90.0,
                     lunge_distance=30.0, stun_time=0.8, knockback=150.0, block_stamina_damage=44.0, limb_damage_scale=0.9),
        clean=finisher("Neck Break", "MM_Attack_01", 0.12, 0.467, windup=0.8, recovery=0.5),
        slow=finisher("Strangle", "MM_ChargedAttack", 0.0, 1.161, windup=2.2, recovery=0.8),
        shape=(meshes["chain"], materials["rust"], (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        block_cost_scale=1.1)

    # the shield: a stop sign with a strap. Carried on the left arm, with the right fist free.
    # It hits least and guards best, and its charge covers the distance to a fighter with a longer weapon.
    shield = weapon(
        "DA_Gut_Shield", "Stop-sign shield", "Hits least, guards best. Guard held, it strikes with the shield. Its charge closes the distance.",
        lights=[
            attack("Jab", zone.TORSO, "MM_Attack_01", 0.12, 0.467,
                   damage=10.0, stamina_cost=10.0, windup=0.22, active=0.1, recovery=0.26, reach=125.0, half_angle=55.0,
                   lunge_distance=35.0, stun_time=0.45, knockback=30.0, block_stamina_damage=8.0),
            attack("Bash", zone.TORSO, "MM_Attack_02", 0.12, 0.467,
                   damage=13.0, stamina_cost=14.0, windup=0.3, active=0.12, recovery=0.36, reach=120.0, half_angle=60.0,
                   lunge_distance=45.0, stun_time=0.6, knockback=110.0, block_stamina_damage=11.0),
        ],
        heavy=attack("Charge", zone.TORSO, "MM_ChargedAttack", 0.33, 1.161,
                     damage=22.0, stamina_cost=28.0, windup=0.6, active=0.16, recovery=0.6, reach=130.0, half_angle=50.0,
                     lunge_distance=220.0, stun_time=0.9, knockback=260.0, block_stamina_damage=48.0),
        clean=finisher("Rim Strike", "MM_Attack_02", 0.12, 0.467, windup=0.7, recovery=0.5, sever=["head"]),
        slow=finisher("Crush", "MM_ChargedAttack", 0.0, 1.161, windup=1.8, recovery=0.8),
        shape=(meshes["shield"], materials["shield"], (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
        block_cost_scale=0.6, held_arm_damage_scale=0.2, off_hand=True, guarded_light_attack=1)

    return {"fists": fists, "club": club, "machete": machete, "chain": chain, "shield": shield, "rack": [club, machete, chain, shield]}


# --- the ladder --------------------------------------------------------------

# the Kennel Master does not come in alone. The line on the card that says so, and the health that goes with having a dog
# at heel (measured: see for-review/LOG.md, Nov 14). Tools/import_gut_beast.py writes the same two onto a ladder that already exists.
KENNEL_LINE = "The Kennel Master does not come in alone."
KENNEL_HEALTH = 95.0


def rung(name, billing, card, weapon, health, tint, purse, book_chance, tuning=None, spares_player=False, armor=None, dogs=0):
    """One fight on the ladder. Tuning is the fields of the opponent's tuning to change from their defaults, if any."""
    result = unreal.GutLadderRung()
    result.set_editor_property("opponent_name", name)
    result.set_editor_property("billing", billing)
    result.set_editor_property("card", card)
    result.set_editor_property("weapon", weapon)
    result.set_editor_property("health", health)
    result.set_editor_property("body_tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    result.set_editor_property("purse", purse)
    result.set_editor_property("book_chance", book_chance)
    result.set_editor_property("spares_player", spares_player)
    result.set_editor_property("armor", armor or [])

    # the pit boss's dogs that come in at this one's heel (Tools/import_gut_beast.sh brings the dog itself in)
    result.set_editor_property("dogs", dogs)

    if tuning:
        values = unreal.GutOpponentTuning()
        for field, value in tuning.items():
            values.set_editor_property(field, value)
        result.set_editor_property("override_tuning", True)
        result.set_editor_property("tuning", values)

    return result


def form(weapon, against, win_share):
    result = unreal.GutFormEntry()
    result.set_editor_property("weapon", weapon)
    result.set_editor_property("against", against)
    result.set_editor_property("win_share", win_share)
    return result


# --- armor ---------------------------------------------------------------------

def plate(bone, toward, length_share, thickness):
    result = unreal.GutArmorPlate()
    result.set_editor_property("bone", bone)
    result.set_editor_property("toward_bone", toward)
    result.set_editor_property("length_share", length_share)
    result.set_editor_property("thickness", thickness)
    return result


def build_armor(materials):
    """
    Armor for the ring, from the design document's two cheap materials: tire rubber, which is made against blunt blows,
    and road sign, which is made against blades. A piece of each for the body, the arms and the legs.
    Head and coat, the document's other two places, are not built.
    """
    slot = unreal.GutArmorSlot

    # each piece is boxes laid along the bones of the part it covers
    shapes = {
        slot.TORSO: [plate("spine_02", "neck_01", 0.9, 34.0)],
        slot.ARMS: [plate("upperarm_l", "lowerarm_l", 0.7, 13.0), plate("lowerarm_l", "hand_l", 0.8, 11.0),
                    plate("upperarm_r", "lowerarm_r", 0.7, 13.0), plate("lowerarm_r", "hand_r", 0.8, 11.0)],
        slot.LEGS: [plate("thigh_l", "calf_l", 0.75, 18.0), plate("calf_l", "foot_l", 0.8, 14.0),
                    plate("thigh_r", "calf_r", 0.75, 18.0), plate("calf_r", "foot_r", 0.8, 14.0)],
    }

    def piece(name, display_name, piece_slot, material, blunt, bladed, price):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.GutArmorData)

        asset, created = create(name, ROOT + "/Armor", unreal.GutArmorData, factory)

        if created:
            asset.set_editor_property("display_name", display_name)
            asset.set_editor_property("slot", piece_slot)
            asset.set_editor_property("blunt_scale", blunt)
            asset.set_editor_property("bladed_scale", bladed)
            asset.set_editor_property("price", price)
            asset.set_editor_property("plate_mesh", load("/Engine/BasicShapes/Cube"))
            asset.set_editor_property("plate_material", material)
            asset.set_editor_property("plates", shapes[piece_slot])
            save(asset)

        return asset

    rubber, sign = materials["tire_rubber"], materials["road_sign"]

    return {
        "rubber_torso": piece("DA_Gut_Armor_RubberVest", "Tire-rubber vest", slot.TORSO, rubber, 0.7, 0.95, 20.0),
        "rubber_arms": piece("DA_Gut_Armor_RubberSleeves", "Tire-rubber sleeves", slot.ARMS, rubber, 0.7, 0.95, 15.0),
        "rubber_legs": piece("DA_Gut_Armor_RubberGreaves", "Tire-rubber greaves", slot.LEGS, rubber, 0.7, 0.95, 15.0),
        "sign_torso": piece("DA_Gut_Armor_SignPlate", "Road-sign breastplate", slot.TORSO, sign, 0.95, 0.65, 30.0),
        "sign_arms": piece("DA_Gut_Armor_SignBracers", "Road-sign bracers", slot.ARMS, sign, 0.95, 0.65, 20.0),
        "sign_legs": piece("DA_Gut_Armor_SignGreaves", "Road-sign greaves", slot.LEGS, sign, 0.95, 0.65, 20.0),
    }


def sponsor(name, demand, pay, colors):
    result = unreal.GutSponsor()
    result.set_editor_property("sponsor_name", name)
    result.set_editor_property("demand", demand)
    result.set_editor_property("pay", pay)
    result.set_editor_property("colors", unreal.LinearColor(colors[0], colors[1], colors[2], 1.0))
    return result


# how an opponent fights, as changes to the opponent's ordinary tuning. Ordinary is what the opponent has had since milestone 2.
GREEN = dict(min_decision_interval=1.3, max_decision_interval=2.8, min_reaction_time=0.25, max_reaction_time=0.45,
             block_light_chance=0.35, block_heavy_chance=0.25, dodge_heavy_chance=0.15, counter_chance=0.5, punish_chance=0.3)
EAGER = dict(attack_chance=0.38, min_decision_interval=0.9, max_decision_interval=2.0)
SHARP = dict(min_reaction_time=0.18, max_reaction_time=0.3, block_light_chance=0.6, counter_chance=0.9, punish_chance=0.65, attack_chance=0.35)
DEADLY = dict(min_reaction_time=0.16, max_reaction_time=0.27, block_light_chance=0.65, block_heavy_chance=0.5, counter_chance=0.95,
              punish_chance=0.75, attack_chance=0.38, min_decision_interval=0.9, max_decision_interval=1.9)


def ladder_asset(name, rank_name, rungs, won_ending, next_ladder=None, **properties):
    """One rank of the ladder. Returns (asset, created). Any other properties of the ladder asset can be given by name."""
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GutLadderData)

    ladder, created = create(name, ROOT + "/Ladder", unreal.GutLadderData, factory)

    if created:
        ladder.set_editor_property("rank_name", rank_name)
        ladder.set_editor_property("rungs", rungs)
        ladder.set_editor_property("won_ending", won_ending)
        ladder.set_editor_property("next_ladder", next_ladder)
        ladder.set_editor_property("cutter_name", "Doc Stitch")

        for property_name, value in properties.items():
            ladder.set_editor_property(property_name, value)

    elif "sponsors" in properties:
        # who makes offers at a rank is kept up to date on a ladder that is already there: everything else on it may have been edited by hand
        ladder.set_editor_property("sponsors", properties["sponsors"])
        eal.save_loaded_asset(ladder, only_if_is_dirty=False)

    return ladder, created


def build_ladder(weapons, armor):
    """
    The ladder of the Gut, a rank to an asset: Meat, which is the arc that opens the game, then Dog, Butcher and Champion.
    The words are placeholders built on the design document. Once an asset exists it is the place to change
    its words, its opponents and its sums.

    They are made from the top down, since each names the rank above it.
    """
    club, machete, chain, shield = weapons["club"], weapons["machete"], weapons["chain"], weapons["shield"]

    # what the book knows about the weapons: the share of 100 fights the first won against the second
    # with the same opponent on both sides, as measured for milestone 5
    form_book = [
        form(club, machete, 0.57),
        form(club, chain, 0.31),
        form(club, shield, 0.71),
        form(machete, chain, 0.57),
        form(machete, shield, 0.49),
        form(chain, shield, 0.35),
    ]

    # after Meat nobody bills the player for room and board: it fights because it chooses to,
    # and the pit boss takes his debt out of its purses
    later = dict(form_book=form_book, bill_at_gate=False, debt_share_of_purse=0.5)

    # clearing Dog rank is what brings the sponsors and the armorers, so they are there from Butcher rank up.
    # The sponsors are the town's own businesses, named in the design document.
    perks = dict(
        armor_shop=[armor["rubber_torso"], armor["rubber_arms"], armor["rubber_legs"], armor["sign_torso"], armor["sign_arms"], armor["sign_legs"]],
        sponsors=[
            sponsor("The grill carts", unreal.GutSponsorDemand.KILL_SLOWLY, 15.0, (0.5, 0.2, 0.05)),
            sponsor("Tooth & Trigger", unreal.GutSponsorDemand.WEAR_COLORS, 10.0, (0.75, 0.6, 0.1)),
            # the Gut's own betting window, when the money is all on one fighter: it buys the other corner too, so the one
            # who goes down is left breathing. The fight is still there to be won afterwards.
            sponsor("The window", unreal.GutSponsorDemand.THROW_FIGHT, 40.0, (0.3, 0.3, 0.3)),
        ])

    rubber = [armor["rubber_torso"], armor["rubber_arms"], armor["rubber_legs"]]
    sign = [armor["sign_torso"], armor["sign_arms"], armor["sign_legs"]]

    # --- Champion: one fight, against whoever holds the cage ---

    champion, created = ladder_asset("DA_Gut_ChampionLadder", "Champion", [
        rung("The Champion", "Holds the cage. Nobody has asked for this fight in a year.",
             ["One fight. Win it and a piece of the Gut is yours.",
              "The whole deck is full. Ham Rask is taking bets himself."],
             club, 210.0, (0.3, 0.3, 0.32), purse=100.0, book_chance=0.2, tuning=DEADLY, armor=sign),
    ], [
        "The Champion is dead, and you are the Champion.",
        "Ham Rask signs over a share of the Gut.",
        "You own a piece of the pool you were sold into.",
    ], gate_cut_base=10.0, gate_cut_per_name=1.0, **later, **perks)

    if created:
        save(champion)

    # --- Butcher: five fights against fighters with names of their own ---

    butcher, created = ladder_asset("DA_Gut_ButcherLadder", "Butcher", [
        rung("Brakes", "Stops everything you throw, then walks through you.",
             ["You are a Butcher now. The crowd comes to see you in particular."],
             shield, 160.0, (0.5, 0.35, 0.05), purse=30.0, book_chance=0.35, tuning=SHARP, armor=[armor["rubber_torso"]]),
        rung("Saltlick", "Fast, and likes the work.",
             ["Four more."],
             machete, 135.0, (0.55, 0.5, 0.45), purse=30.0, book_chance=0.33, tuning=SHARP, armor=[armor["sign_arms"]]),
        rung("The Collector", "Fights for the Chainmen. The debt is always someone else's.",
             ["Three more."],
             chain, 135.0, (0.3, 0.05, 0.3), purse=35.0, book_chance=0.3, tuning=SHARP, armor=[armor["rubber_torso"], armor["rubber_legs"]]),
        rung("Drum", "Named for what he was sold for.",
             ["Two more."],
             club, 145.0, (0.1, 0.3, 0.12), purse=35.0, book_chance=0.28, tuning=SHARP, armor=[armor["sign_torso"], armor["sign_arms"]]),
        rung("The Butcher of Lane Four", "Has held this rank longer than you have had a tag.",
             ["The last one between you and the Champion."],
             shield, 165.0, (0.45, 0.02, 0.02), purse=45.0, book_chance=0.25, tuning=DEADLY, armor=rubber),
    ], [
        "Five Butchers dead. There is one name above yours now.",
        "From here you take a cut of the gate.",
    ], next_ladder=champion, **later, **perks)

    if created:
        save(butcher)

    # --- Dog: five fights, nothing asked of the player but to win them ---

    dog, created = ladder_asset("DA_Gut_DogLadder", "Dog", [
        rung("Gasket", "Hides behind a stop sign and waits for you to tire.",
             ["Nobody makes you come back. You came back.",
              "Ham Rask keeps half of every purse until his bill is paid."],
             shield, 125.0, (0.2, 0.25, 0.3), purse=20.0, book_chance=0.4),
        rung("Rustjaw", "Swings first and counts on it being enough.",
             ["Four more to make Butcher."],
             club, 130.0, (0.4, 0.15, 0.05), purse=20.0, book_chance=0.38, tuning=EAGER),
        rung("Tally", "Cuts a notch in the leaf spring for every arm.",
             ["Three more."],
             machete, 130.0, (0.35, 0.35, 0.1), purse=25.0, book_chance=0.35, tuning=SHARP),
        rung("Coldwater", "Never lets anyone inside the chain twice.",
             ["Two more."],
             chain, 135.0, (0.05, 0.3, 0.35), purse=25.0, book_chance=0.33, tuning=SHARP),
        rung("The Kennel Master", "Top Dog. Five have tried this month.",
             ["The last Dog.", KENNEL_LINE],
             club, KENNEL_HEALTH, (0.25, 0.25, 0.25), purse=30.0, book_chance=0.3, tuning=SHARP, dogs=1),
    ], [
        "Five Dogs dead. You are a Butcher.",
        "Sponsors will make you offers now, and the armorers will take your water.",
    ], next_ladder=butcher, **later)

    if created:
        save(dog)

    # --- Meat: the three fights that open the game ---

    meat, created = ladder_asset("DA_Gut_MeatLadder", "Meat", [
        # a fighter as new as the player: slow to decide, slow to react, and a poor guard
        rung("The newcomer", "Sold the same night as you. First time in the cage.",
             ["Caught in the Kiln. Collared, branded, and sold to the Gut for 40 litres.",
              "Three fights earn a tag. Ham Rask keeps the count."],
             club, 100.0, (0.45, 0.33, 0.08), purse=10.0, book_chance=0.5, tuning=GREEN),

        # the opponent as it has been tuned since milestone 2, with the chain
        rung("The Dog", "Five fights up the ladder and still breathing.",
             ["One down. Winners are paid in water.",
              "The dead go to Doc Stitch for parts, or to the grill carts."],
             chain, 125.0, (0.6, 0.08, 0.05), purse=15.0, book_chance=0.35),

        # the cellmate: quicker, surer and harder to put down than anyone before, and it will not finish the player
        rung("Sable", "Your cellmate. The one who kept you alive.",
             ["Two down. One fight from a tag.",
              "Ham Rask has matched you with your cellmate.",
              "One of you walks out. That is the rule."],
             machete, 145.0, (0.08, 0.2, 0.5), purse=25.0, book_chance=0.3, spares_player=True, tuning=SHARP),
    ], [
        "Sable is dead. Ham Rask stamps a tag and hangs it round your neck.",
        "Then he reads you the bill for room and board.",
        "You leave with a tag, no water, and a debt.",
    ], next_ladder=dog, form_book=form_book)

    if created:
        meat.set_editor_property("spared_ending", [
            "Sable would not finish you, and the crowd went home without its kill.",
            "You are both alive. Ham Rask adds what that cost him to your bill.",
            "You leave with a tag, no water, and a bigger debt.",
        ])

        save(meat)

    return meat


# --- blueprints --------------------------------------------------------------

def blueprint(name, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return create(name, ROOT + "/Blueprints", unreal.Blueprint, factory)


def finish_blueprint(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)


def setup_fighter(cdo, body_mesh, materials):
    """Placeholder body, animations and effects shared by every fighter."""
    mesh = cdo.get_editor_property("mesh")
    mesh.set_editor_property("skeletal_mesh_asset", load(body_mesh))
    # standing, walking and guarding are done in code (UGutAnimInstance); the clips come from Tools/import_gut_motion.sh
    mesh.set_editor_property("anim_class", unreal.GutAnimInstance.static_class())

    cdo.get_editor_property("guard_plate").set_material(0, materials["guard"])

    cdo.set_editor_property("dodge_animation", load(MANNEQUIN_ANIMS + "/Unarmed/Jump/MM_Dash"))
    cdo.set_editor_property("stun_animation", load(MANNEQUIN_ANIMS + "/Death/MM_Death_Front_01"))
    cdo.set_editor_property("hit_effect", load("/Game/Variant_Combat/VFX/NS_Damage"))


def arm_fighter(bp, weapons, extra=None):
    """Gives a fighter Blueprint, new or old, whatever it is still missing: fists, a starting weapon, a stagger animation."""
    cdo = unreal.get_default_object(bp.generated_class())

    values = {
        "fists": weapons["fists"],
        "starting_weapon": weapons["club"],
        "stagger_animation": load(MANNEQUIN_ANIMS + "/Death/MM_Death_Back_01"),
        "blood_material": load(ROOT + "/Materials/M_Gut_Blood"),
        "gib_material": load(ROOT + "/Materials/M_Gut_Flat"),
        "collar_mesh": load(ROOT + "/Meshes/SM_Gut_Collar"),
        "collar_material": load(ROOT + "/Materials/M_Gut_Rust"),
    }
    values.update(extra or {})

    if fill_empty(cdo, values):
        finish_blueprint(bp)


def build_blueprints(materials, actions, weapons, ladder):
    meshes = "/Game/Characters/Mannequins/Meshes"
    combat = "/Game/Variant_Combat/Blueprints"

    # the player
    player, created = blueprint("BP_GutPlayer", unreal.GutPlayerCharacter)

    if created:
        cdo = unreal.get_default_object(player.generated_class())

        setup_fighter(cdo, meshes + "/SKM_Quinn_Simple", materials)

        cdo.set_editor_property("move_action", load("/Game/Input/Actions/IA_Move"))
        cdo.set_editor_property("look_action", load("/Game/Input/Actions/IA_Look"))
        cdo.set_editor_property("mouse_look_action", load("/Game/Input/Actions/IA_MouseLook"))
        cdo.set_editor_property("light_attack_action", actions["light"])
        cdo.set_editor_property("heavy_attack_action", actions["heavy"])
        cdo.set_editor_property("block_action", actions["block"])
        cdo.set_editor_property("dodge_action", actions["dodge"])
        cdo.set_editor_property("hit_landed_shake", load(combat + "/BP_CameraShake_Hit_Enemy").generated_class())
        cdo.set_editor_property("hit_received_shake", load(combat + "/BP_CameraShake_Hit_Player").generated_class())

        finish_blueprint(player)

    arm_fighter(player, weapons, {"lock_on_action": actions["lock_on"]})

    # the dummy: walks in and swings on a timer. A punching bag, no longer placed in the cage.
    dummy, created = blueprint("BP_GutDummy", unreal.GutFighter)

    if created:
        cdo = unreal.get_default_object(dummy.generated_class())

        setup_fighter(cdo, meshes + "/SKM_Manny_Simple", materials)

        cdo.set_editor_property("display_name", "THE DUMMY")
        cdo.set_editor_property("ai_controller_class", unreal.GutDummyController)
        cdo.set_editor_property("apply_body_tint", True)

        finish_blueprint(dummy)

    arm_fighter(dummy, weapons)

    # the opponent's brain. A Blueprint of its own so its tuning can be changed in the editor.
    brain, created = blueprint("BP_GutOpponentController", unreal.GutOpponentController)

    if created:
        finish_blueprint(brain)

    # the opponent: the same fighter as the player, with a brain that approaches, attacks, blocks and retreats
    opponent, created = blueprint("BP_GutOpponent", unreal.GutFighter)

    if created:
        cdo = unreal.get_default_object(opponent.generated_class())

        setup_fighter(cdo, meshes + "/SKM_Manny_Simple", materials)

        cdo.set_editor_property("display_name", "THE OPPONENT")
        cdo.set_editor_property("ai_controller_class", brain.generated_class())
        cdo.set_editor_property("apply_body_tint", True)

        finish_blueprint(opponent)

    arm_fighter(opponent, weapons)

    # the player controller
    controller, created = blueprint("BP_GutPlayerController", unreal.GutPlayerController)

    if created:
        cdo = unreal.get_default_object(controller.generated_class())

        cdo.set_editor_property("default_mapping_contexts", [
            load("/Game/Input/IMC_Default"),
            load("/Game/Input/IMC_MouseLook"),
            actions["context"],
        ])
        cdo.set_editor_property("restart_action", actions["restart"])

        finish_blueprint(controller)

    rack_inputs = {
        "rack_mapping_context": actions["rack_context"],
        "rack_previous_action": actions["rack_previous"],
        "rack_next_action": actions["rack_next"],
        "rack_opponent_action": actions["rack_opponent"],
        "rack_down_action": actions["rack_down"],
        "rack_confirm_action": actions["rack_confirm"],
    }

    if fill_empty(unreal.get_default_object(controller.generated_class()), rack_inputs):
        finish_blueprint(controller)

    # the game mode
    game_mode, created = blueprint("BP_GutGameMode", unreal.GutGameMode)

    if created:
        cdo = unreal.get_default_object(game_mode.generated_class())

        cdo.set_editor_property("default_pawn_class", player.generated_class())
        cdo.set_editor_property("player_controller_class", controller.generated_class())

        finish_blueprint(game_mode)

    if fill_empty(unreal.get_default_object(game_mode.generated_class()), {"rack_weapons": weapons["rack"], "ladder": ladder}):
        finish_blueprint(game_mode)

    return {"player": player, "dummy": dummy, "opponent": opponent, "controller": controller, "game_mode": game_mode}


# --- level -------------------------------------------------------------------

def build_level(materials, blueprints):
    level_path = ROOT + "/Lvl_Gut"

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    def spawn(actor_class, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
        actor = actors.spawn_actor_from_class(
            actor_class,
            unreal.Vector(location[0], location[1], location[2]),
            unreal.Rotator(roll=rotation[0], pitch=rotation[1], yaw=rotation[2]))
        if actor is None:
            raise RuntimeError("Could not spawn {}".format(actor_class))
        if label:
            actor.set_actor_label(label)
        return actor

    if eal.does_asset_exist(level_path):
        log("kept {}".format(level_path))

        # the cage is the editor's startup level, so it is usually open already.
        # Loading it again would fail if recompiling a fighter Blueprint above has marked it as changed.
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

        if current is None or current.get_outermost().get_name() != level_path:
            if not levels.load_level(level_path):
                raise RuntimeError("Could not load {}".format(level_path))
    else:
        if not levels.new_level(level_path):
            raise RuntimeError("Could not create {}".format(level_path))

        # the cage itself, floor at height zero
        cage = spawn(unreal.GutCage, label="GutCage")
        cage.set_editor_property("floor_material", load("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"))
        cage.set_editor_property("wall_material", load("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark"))
        cage.set_editor_property("bar_material", materials["bars"])

        # the player starts at one end of the long side
        spawn(unreal.PlayerStart, location=(-450.0, 0.0, 92.0), label="PlayerStart")

        # daylight through the lid
        sun = spawn(unreal.DirectionalLight, location=(0.0, 0.0, 1200.0), rotation=(0.0, -58.0, -35.0), label="Sun")
        sun.get_editor_property("light_component").set_editor_property("intensity", 7.0)

        spawn(unreal.SkyAtmosphere, location=(0.0, 0.0, 1000.0), label="SkyAtmosphere")

        sky = spawn(unreal.SkyLight, location=(0.0, 0.0, 1100.0), label="SkyLight")
        sky.get_editor_property("light_component").set_editor_property("real_time_capture", True)

        spawn(unreal.ExponentialHeightFog, location=(0.0, 0.0, 900.0), label="Fog")

        # this level always runs a cage fight, whatever the project default is
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", blueprints["game_mode"].generated_class())

    # exactly one fighter stands opposite the player, and it is the opponent.
    # The milestone 1 dummy, if it is still there, gives up its spot.
    opponent_class = blueprints["opponent"].generated_class()
    dummy_class = blueprints["dummy"].generated_class()

    placed = actors.get_all_level_actors()

    if not any(actor.get_class() == opponent_class for actor in placed):
        location = unreal.Vector(450.0, 0.0, 92.0)
        rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)

        for actor in placed:
            if actor.get_class() == dummy_class:
                location = actor.get_actor_location()
                rotation = actor.get_actor_rotation()
                actors.destroy_actor(actor)
                log("removed the dummy from the cage")

        opponent = actors.spawn_actor_from_class(opponent_class, location, rotation)
        if opponent is None:
            raise RuntimeError("Could not place the opponent")
        opponent.set_actor_label("Opponent")
        log("placed the opponent in the cage")

    # one weapon rack, flat on the middle of the long wall to the player's right, facing into the cage
    racks = [actor for actor in placed if isinstance(actor, unreal.GutWeaponRack)]

    if not racks:
        racks = [spawn(unreal.GutWeaponRack, location=(0.0, 499.0, 165.0), rotation=(0.0, 0.0, -90.0), label="WeaponRack")]
        log("hung the weapon rack on the wall")

    for rack in racks:
        fill_empty(rack, {"board_material": materials["rack"]})

    # the hazards. The spikes come out of the middle of the long wall opposite the rack;
    # the generator stands against the end wall behind the player's start, its cable running along the floor on the rack side.
    hit_effect = load("/Game/Variant_Combat/VFX/NS_Damage")

    spike_walls = [actor for actor in placed if isinstance(actor, unreal.GutSpikeWall)]

    if not spike_walls:
        spike_walls = [spawn(unreal.GutSpikeWall, location=(0.0, -500.0, 0.0), rotation=(0.0, 0.0, 90.0), label="SpikeWall")]
        log("set spikes in the wall")

    for spike_wall in spike_walls:
        fill_empty(spike_wall, {
            "plate_material": materials["spike_plate"],
            "spike_material": materials["spikes"],
            "live_spike_material": materials["spikes_live"],
            "hit_effect": hit_effect,
        })

    cables = [actor for actor in placed if isinstance(actor, unreal.GutLiveCable)]

    if not cables:
        cables = [spawn(unreal.GutLiveCable, location=(-700.0, 250.0, 0.0), rotation=(0.0, 0.0, 0.0), label="LiveCable")]
        log("laid the live cable")

    for cable in cables:
        fill_empty(cable, {
            "generator_material": materials["generator"],
            "cable_material": materials["cable"],
            "live_cable_material": materials["cable_live"],
            "hit_effect": hit_effect,
        })

    # the crowd, standing on the deck round the cage
    crowds = [actor for actor in placed if isinstance(actor, unreal.GutCrowd)]

    if not crowds:
        crowds = [spawn(unreal.GutCrowd, label="Crowd")]
        log("let the crowd in")

    for crowd in crowds:
        fill_empty(crowd, {
            "spectator_mesh": load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"),
            "idle_animation": load(MANNEQUIN_ANIMS + "/Unarmed/MM_Idle"),
            "cheer_animation": load(MANNEQUIN_ANIMS + "/Unarmed/Jump/MM_Jump"),
        })

    dress_level(materials, placed, spawn)

    # a surface that has been deleted to be made again leaves the cage bare
    for actor in placed:
        if isinstance(actor, unreal.GutCage):
            fill_empty(actor, {"floor_material": materials["pool_floor"], "wall_material": materials["pool_wall"], "bar_material": materials["rust"]})

    if not levels.save_current_level():
        raise RuntimeError("Could not save {}".format(level_path))

    log("saved {}".format(level_path))


def dress_level(materials, placed, spawn):
    """
    The art pass on the level: the pool's own surfaces in place of the prototype grid, rust on the steel,
    and night, with the cage lit by four floodlights on the deck.

    It is done once. If the floodlights are already there it does nothing, so that lighting changed by hand afterwards is kept.
    """
    if any(actor.get_actor_label().startswith("Floodlight") for actor in placed):
        return

    for actor in placed:
        if isinstance(actor, unreal.GutCage):
            actor.set_editor_property("floor_material", materials["pool_floor"])
            actor.set_editor_property("wall_material", materials["pool_wall"])
            actor.set_editor_property("bar_material", materials["rust"])

        elif isinstance(actor, unreal.GutSpikeWall):
            actor.set_editor_property("spike_material", materials["rust"])

        elif isinstance(actor, unreal.GutLiveCable):
            actor.set_editor_property("live_cable_material", materials["cable_hot"])

        elif isinstance(actor, unreal.DirectionalLight):
            # the sun becomes a low moon
            light = actor.get_editor_property("light_component")
            light.set_editor_property("intensity", 2.0)
            light.set_editor_property("light_color", unreal.Color(r=150, g=175, b=255, a=255))

        elif isinstance(actor, unreal.SkyLight):
            actor.get_editor_property("light_component").set_editor_property("intensity", 5.0)

        elif isinstance(actor, unreal.ExponentialHeightFog):
            # enough in the air for the floodlights to show as beams
            fog = actor.get_editor_property("component")
            fog.set_editor_property("fog_density", 0.02)
            fog.set_editor_property("enable_volumetric_fog", True)

    # four floodlights on poles at the corners of the deck, two warm and two cold, pointed down into the cage
    deck = 320.0
    top = 780.0

    for index, (x, y) in enumerate(((-800.0, -600.0), (800.0, -600.0), (-800.0, 600.0), (800.0, 600.0))):
        name = "Floodlight_{}".format(index + 1)

        position = unreal.Vector(x, y, top)
        aim = unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(x * 0.2, y * 0.2, 0.0))

        lamp = spawn(unreal.SpotLight, location=(x, y, top), label=name)
        lamp.set_actor_rotation(aim, False)

        light = lamp.get_editor_property("light_component")
        light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        light.set_editor_property("intensity", 24000.0)
        light.set_editor_property("attenuation_radius", 3200.0)
        light.set_editor_property("inner_cone_angle", 26.0)
        light.set_editor_property("outer_cone_angle", 48.0)
        light.set_editor_property("volumetric_scattering_intensity", 1.0)
        light.set_editor_property("light_color", unreal.Color(r=255, g=214, b=160, a=255) if index % 3 == 0 else unreal.Color(r=190, g=215, b=255, a=255))

        # the pole it stands on, and the lamp head, so the light comes from something
        pole = spawn(unreal.StaticMeshActor, location=(x, y, (deck + top) * 0.5), label=name + "_Pole")
        pole.static_mesh_component.set_static_mesh(load("/Engine/BasicShapes/Cylinder"))
        pole.static_mesh_component.set_material(0, materials["rust"])
        pole.set_actor_scale3d(unreal.Vector(0.14, 0.14, (top - deck) / 100.0))

        head = spawn(unreal.StaticMeshActor, location=(x, y, top), label=name + "_Head")
        head.static_mesh_component.set_static_mesh(load("/Engine/BasicShapes/Cube"))
        head.static_mesh_component.set_material(0, materials["lamp"])
        head.set_actor_rotation(aim, False)
        head.set_actor_scale3d(unreal.Vector(0.25, 0.7, 0.45))

    # pin the camera's exposure to what the floodlit floor needs. Left to itself it brightens the night
    # until it looks like an overcast day. The two numbers are EV100.
    volume = spawn(unreal.PostProcessVolume, label="NightExposure")
    volume.set_editor_property("unbound", True)

    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 7.6)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 8.4)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.55)
    volume.set_editor_property("settings", settings)

    log("dressed the cage for the night")


def main():
    materials = build_materials()
    meshes = build_meshes()
    actions = build_input()
    weapons = build_weapons(materials, meshes)
    armor = build_armor(materials)
    ladder = build_ladder(weapons, armor)
    blueprints = build_blueprints(materials, actions, weapons, ladder)
    build_level(materials, blueprints)
    log("DONE")


try:
    main()
except Exception:
    unreal.log_error("[build_gut_content] FAILED\n{}".format(traceback.format_exc()))
