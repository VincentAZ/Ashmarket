"""Assembles the Gut's fighters as MetaHumans: Epic's preset people, rigged and textured by Epic's service
(needs the editor signed in to an Epic account once), built into /Game/Gut/Art/MetaHumans, and put in the cage.
This script only picks, assembles and assigns; the look comes from MetaHuman's scanned data and Epic's own clothing.

Environment: GUT_METAHUMANS, the presets to build (space separated, "all" for the whole cast, "crowd" for the spectators). With none it only
puts the ones already built into the cage. GUT_RETAILOR=1 also goes over the clothes of the ones already built.
"""
import json
import os
import random
import zlib

import unreal

ROOT = "/Game/Gut/Art/MetaHumans"
PRESETS = "/MetaHumanCharacter/Optional/Presets/"
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"
GARMENT = "/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment"

EVERYONE = ["Ada", "Aera", "Aoi", "Asha", "Bo", "Bruce", "Cameron", "Celeste", "Dominic", "Etta", "Grace", "Isaiah",
            "Jelena", "Jorge", "Kelvin", "Lani", "Lorenzo", "Mateo", "Mikel", "Omari", "Orlando", "Sook-ja", "Sunita",
            "Trey", "Tuya", "Victor", "Vivian", "Walter", "Zuri"]

# who is who. The player is the first fighter that was shown; the opponents are drawn by lot from the rest,
# the same lot every run, so nobody's place on the ladder follows from what they look like.
PLAYER = "Bruce"
LADDERS = [("DA_Gut_MeatLadder", 3), ("DA_Gut_DogLadder", 5), ("DA_Gut_ButcherLadder", 5), ("DA_Gut_ChampionLadder", 1)]
OPPONENTS = random.Random("the Gut").sample([name for name in EVERYONE if name != PLAYER], sum(count for _, count in LADDERS))

# The lot drew seven women in fourteen. Vincent asked for three fighters in ten to be women (2026-11-15), so three of the
# seven stand down to the deck and a man comes off it in each one's place on the ladder. The four who stay are spread over
# the ranks: Sable, Tally, Saltlick and the Butcher of Lane Four. None of the men is one the street already uses.
STOOD_DOWN = {"Lani": "Dominic", "Sunita": "Kelvin", "Sook-ja": "Mikel"}
OPPONENTS = [STOOD_DOWN.get(name, name) for name in OPPONENTS]

# everyone the lot left out stands on the deck and watches: dressed, and built as Epic made them
CROWD = [name for name in EVERYONE if name != PLAYER and name not in OPPONENTS]

# more people for the deck than Epic made presets: each has one preset's body and hair, another's skin,
# and a face half-way between the two, set through MetaHuman's own face model as its blend tool does. Drawn by lot.
_lot = random.Random("the deck")
MIXES = {"Mix%02d" % (index + 1): tuple(_lot.sample(EVERYONE, 2)) for index in range(14)}
CROWD += list(MIXES)

# changes to a preset's own hair, where the preset's would not last a week in the cage (slot -> wardrobe item)
GROOMING = {
    "Bruce": {
        "Hair": GROOMS + "Hair/WI_Hair_S_BuzzCut",
        "Beard": GROOMS + "Beards/WI_Beard_S_Stubble",
        "Mustache": GROOMS + "Mustaches/WI_Mustache_S_Stubble",
    },
}

# New meat (Vincent, 2026-11-19: "we need a variety of fighters", and whoever dies is not to be seen again). The cage
# kills four a night and Epic made twenty-nine people, so these are made as the deck's mixes are, but as fighters: one
# preset's body, another's skin, a face somewhere between the two, and hair and a beard that are neither's. A man is
# mixed with a man and a woman with a woman, three in ten are women, and no pair is one the deck already has. Each is
# (body, skin and the other half of the face, how much of the face is the second's, hair, beard, mustache).
MEN = ["Aoi", "Bo", "Bruce", "Cameron", "Dominic", "Isaiah", "Jorge", "Kelvin", "Lorenzo", "Mateo", "Mikel", "Omari", "Orlando", "Trey", "Victor", "Walter"]
WOMEN = [name for name in EVERYONE if name not in MEN]
# Hair that is seen. Strand hair is off in this build, so a style is drawn only if Epic made cards for it, and four have
# none: 360Waves, BuzzCut, CoilBuzzCut and BaldingStubble leave a bare scalp. The first twenty were drawn from lists with
# those in (2026-11-20: seven came out shaven, four of them alike). Now the one buzz cut in each list is a head shaven on
# purpose, and every other style has cards.
MENS_HAIR = ["S_BuzzCut", "S_BrushCut", "S_AfroFade", "S_CurlyFade", "S_HairLoss", "M_Mohawk", "M_FauxMohawk", "S_Cornrows", "S_RecedeMessy",
             "S_Clean", "S_Messy", "M_TwistedBraids", "S_LowPonytail", "S_PulledBack", "S_Coil", "S_Casual", "S_SlickBack", "S_SweptUp"]
WOMENS_HAIR = ["S_BuzzCut", "S_Pixie", "S_Cornrows", "S_UpdoBraids", "S_UpdoBuns", "S_LowPonytail", "S_PulledBack", "M_Mohawk",
               "M_TwistedBraids", "M_BobMessy", "S_Updo"]
# whose body no new fighter is given: Grace's eyebrows are white and stay white under whatever hair is put on her
NOT_A_NEW_BODY = ["Grace"]
BEARDS = [None, None, None, "Beards/WI_Beard_S_Stubble", "Beards/WI_Beard_M_Stubble", "Beards/WI_Beard_S_Full", "Beards/WI_Beard_L_Full",
          "Beards/WI_Beard_L_Messy", "Beards/WI_Beard_M_MuttonChops", "Beards/WI_Beard_S_Uneven", "Beards/WI_Goatee_L_Scraggly",
          "Beards/WI_Goatee_M_Pointed", "Beards/WI_Goatee_S_ChinStrap", "Beards/WI_Beard_M_Curly"]
MUSTACHES = [None, None, None, "WI_Mustache_S_Stubble", "WI_Mustache_M_Stubble", "WI_Mustache_S_Horseshoe", "WI_Mustache_L_Handlebar",
             "WI_Mustache_M_Gunslinger", "WI_Mustache_L_Scraggly", "WI_Mustache_S_Full", "WI_Mustache_S_Uneven"]
NEW_MEAT_COUNT = 20


def _new_meat():
    lot = random.Random("new meat")
    taken = {frozenset(pair) for pair in MIXES.values()}
    people = {}
    # nobody's body twice and nobody's hair twice: each is dealt out like a pack of cards, the men's and the women's apart.
    # Beards and mustaches are dealt from two packs each, so no one of them is on more than two faces.
    bodies = {woman: lot.sample([name for name in names if name not in NOT_A_NEW_BODY], len(names) - sum(name in NOT_A_NEW_BODY for name in names))
              for woman, names in ((False, MEN), (True, WOMEN))}
    hair = {woman: lot.sample(styles, len(styles)) for woman, styles in ((False, MENS_HAIR), (True, WOMENS_HAIR))}
    beards, mustaches = lot.sample(BEARDS * 2, len(BEARDS) * 2), lot.sample(MUSTACHES * 2, len(MUSTACHES) * 2)
    for index in range(NEW_MEAT_COUNT):
        # three in ten are women: the third, sixth and tenth of every ten
        woman = index % 10 in (2, 5, 9)
        body = bodies[woman].pop()
        while True:
            pair = (body, lot.choice([name for name in (WOMEN if woman else MEN) if name != body]))
            if frozenset(pair) not in taken:
                taken.add(frozenset(pair))
                break
        people["Meat%02d" % (index + 1)] = pair + (round(lot.uniform(0.35, 0.65), 2), hair[woman].pop(),
                                                  None if woman else beards.pop(), None if woman else mustaches.pop())
    return people


NEW_MEAT = _new_meat()

# People of the town who are somebody (25 November): the Wardens' sergeant, the medic at the gate, and the three who keep
# the bars of Rotgut Row. Each is made as the deck's mixes are (one preset's body and hair, another's skin, a face between
# the two), so that nobody else in the town has their face: until now they wore the trial people's. Added after the deck
# and the new meat have been drawn, so that neither lot is changed by them, and they are not put on the deck.
TOWN = {"Rook": ("Walter", "Isaiah"), "Wynn": ("Celeste", "Etta"), "Mash": ("Tuya", "Zuri"), "Lottie": ("Grace", "Asha"), "Fen": ("Aoi", "Kelvin")}
MIXES.update(TOWN)

# a medic four days off the salt does not have Celeste's dyed bob: her hair is tied back
GROOMING["Wynn"] = {"Hair": GROOMS + "Hair/WI_Hair_S_LowPonytail"}

# People dressed out of what Vincent has from Fab (2026-11-19; unpacked into Content/Outfits by Tools/unpack_fab_outfits.py,
# and never in the repository). Each is (the preset it is, the wardrobe items it wears): one whole outfit, or pieces worn
# together. They keep the build the preset came with, and nothing of theirs is dyed.
WARDROBE = "/Game/Outfits/"


def _wi(folder, item=None):
    return WARDROBE + folder + "/" + (item or "WI_OA_" + folder)


# whole outfits, as their makers dressed them: none of these is dyed
SCAVENGER = _wi("Scavenger", "WI_Scavenger")
PA_07 = _wi("PA_07", "WI_PA_07")
SURVIVOR = _wi("SurvivorOutfit")
SOLDIER = _wi("Soldier_Outfit_v1", "WI_Soldier_Outfit_v1")
HELMET = _wi("Helmet_Outfit_v1", "WI_Helmet_Outfit_v1")
TECHWEAR = _wi("techwearOutfit")
WRAPPED = "/Game/outfit_40/WI_OA_outfit_40"
VEST = _wi("HS_Tactical_Vest_42", "WI_HS_Tactical_Vest_42")

# Epic's own plain pieces, worn together: a top, something on the legs, something on the feet
HOODIE, SWEATER = _wi("Hoodie"), _wi("Sweater")
TEE, TEE_3Q, TEE_LONG, TEE_CUFF, TEE_PUSH = (_wi("OA_" + name, "WI_OA_" + name) for name in ("Crewneckt", "Tshirt3QSlv", "TshirtLngSlv", "TshirtLngCufSlv", "TshirtPushCufSlv"))
TUCKED, TUCKED_3Q, TUCKED_LONG, TUCKED_CUFF, TUCKED_PUSH = (_wi("OA_" + name, "WI_OA_" + name) for name in ("CrewnecktTk", "TshirtTk3QSlv", "TshirtTkLngSlv", "TshirtTkLngCufSlv", "TshirtTkPushCufSlv"))
CROP_3Q, CROP_LONG, CROP_CUFF, CROP_PUSH = (_wi("OA_" + name, "WI_OA_" + name) for name in ("TCrop3QSlv", "TCropLngSlv", "TCropLngCufSlv", "TCropPushCufSlv"))
CARGO, JEANS, JEANS_SLIM, JEANS_ANKLE, JEANS_CUFFED, SHORTS = _wi("Cargopants"), _wi("Jeans"), _wi("Jeans_slm"), _wi("Jeansakn"), _wi("Jeansmcf"), _wi("Shorts")
LEGGINGS_SHORT, LEGGINGS = _wi("Yogaakn"), _wi("Yogaful")
BOOTS, CHELSEA, FLATS, FLIPFLOPS, HIGHTOPS, LOAFERS = _wi("Boots"), _wi("Chelseaboots"), _wi("Flats"), _wi("Flipflops"), _wi("Hightop"), _wi("Loafers")

# Everybody in the town and on the deck (Vincent, 2026-11-18: "a wholesale upgrade on ALL characters"; the people were 4 in 10).
# Each is dressed for what they are in the town's lists: a Warden in a vest, a lot off the Block in what it was sold in
# and barefoot, a trader in what a trader would have. Each is (the preset it is, or None for one of the mixes; what it wears).
DRESSED = {
    # Pool Street
    "Zuri": ("Zuri", [SWEATER, JEANS, BOOTS]),                 # Chalk, at the slate
    "Omari": ("Omari", [PA_07]),                               # Hasp, who hands out the weapons
    "Jorge": ("Jorge", [SURVIVOR]),                            # the yard hand
    "Lorenzo": ("Lorenzo", [VEST, TEE, CARGO, BOOTS]),         # a Warden
    # Block Square
    "Vivian": ("Vivian", [TUCKED_LONG, JEANS_SLIM, CHELSEA]),  # Margin, the clerk
    "Lani": ("Lani", [TECHWEAR]),                              # Mother Ledger
    "Orlando": ("Orlando", [SWEATER, CARGO, BOOTS]),           # Caller Strand
    "Bo": ("Bo", [VEST, TEE_LONG, JEANS, BOOTS]),              # a Warden
    "Mix05": (None, [SHORTS]),                                 # the lots, in what they were sold in
    "Asha": ("Asha", [TEE, SHORTS]),
    "Mix09": (None, [TEE, SHORTS]),
    "Aera": ("Aera", [CROP_3Q, LEGGINGS_SHORT]),
    "Mix06": (None, [TEE_3Q, SHORTS]),
    "Sunita": ("Sunita", [TEE_LONG, LEGGINGS]),
    # Main Street
    "Ada": ("Ada", [TEE_PUSH, CARGO, BOOTS]),                  # Needle, and Thread
    "Mix02": (None, [SURVIVOR]),                               # Brass
    "Mix04": (None, [SOLDIER]),                                # the Gate Warden
    "Mix07": (None, [SWEATER, JEANS_ANKLE, FLATS]),            # the stallholder
    "Sook-ja": ("Sook-ja", [SCAVENGER]),                       # the scrap seller
    "Jelena": ("Jelena", [TEE_CUFF, LEGGINGS, FLIPFLOPS]),     # the tea seller
    "Mix14": (None, [PA_07]),                                  # the dealer
    "Mix03": (None, [WRAPPED]),                                # a carrier
    "Mix08": (None, [HOODIE, CARGO, BOOTS]),                   # a carrier
    "Mix13": (None, [VEST, TEE_PUSH, CARGO, BOOTS]),           # a Warden
    "Mix11": (None, [VEST, TEE_3Q, JEANS_CUFFED, BOOTS]),      # a Warden
    "Mix10": (None, [SCAVENGER]),                              # somebody
    "Mix12": (None, [HOODIE, JEANS_SLIM, HIGHTOPS]),           # somebody
    "Mix01": (None, [WRAPPED]),                                # on the deck only

    # people of the town who are somebody (TOWN, above)
    "Rook": (None, [SOLDIER, HELMET]),                         # Sergeant Rook, at the tally desk
    "Wynn": (None, [HOODIE, CARGO, BOOTS]),                    # Calla Wynn, a medic off the salt
    "Mash": (None, [SWEATER, JEANS, BOOTS]),                   # who distils, and keeps Detention
    "Lottie": (None, [SURVIVOR]),                              # Big Lottie, who keeps The Sump
    "Fen": (None, [HOODIE, JEANS_SLIM, HIGHTOPS]),             # the fence, at Tarp Nine

    # the first people the wardrobe was tried on, kept for the pictures of them
    "Try01": ("Vivian", [SCAVENGER]),
    "Try02": ("Etta", [HOODIE, CARGO, BOOTS]),
    "Try03": ("Orlando", [SURVIVOR]),
    "Try04": ("Jorge", [PA_07]),
    "Try05": ("Lorenzo", [SOLDIER, HELMET]),
    "Try06": ("Bo", [VEST, TEE, CARGO, BOOTS]),
    "Try07": ("Lani", [TECHWEAR]),
    "Try08": ("Sunita", [WRAPPED]),
    "Try09": ("Dominic", [CARGO, BOOTS]),
    "Try10": ("Tuya", [CROP_PUSH, LEGGINGS_SHORT]),
    "Try11": ("Asha", [TEE, SHORTS]),
}

# and of those, whoever fights: lean and trained as the cage's own are, whatever they have on
DRESSED_FIGHTERS = {"Try09", "Try10"}

# what the new meat fights in. A man is stripped to the waist, as the cage's own are: something on the legs, and boots
# or bare feet. A woman has a top as well. Drawn by lot, the same lot every run.
_cloth = random.Random("what the new meat has on")
for _name, _made in NEW_MEAT.items():
    _woman = _made[0] in WOMEN
    _worn = [_cloth.choice([CROP_PUSH, CROP_3Q, CROP_LONG, TEE])] if _woman else []
    _worn.append(_cloth.choice([LEGGINGS_SHORT, SHORTS, CARGO, LEGGINGS] if _woman else [CARGO, JEANS, JEANS_CUFFED, SHORTS, JEANS_ANKLE, CARGO]))
    _feet = _cloth.choice([None, None, BOOTS] if _woman else [BOOTS, None, None, HIGHTOPS])
    if _feet:
        _worn.append(_feet)
    DRESSED[_name] = (None, _worn)
    DRESSED_FIGHTERS.add(_name)
for _name, (_body, _skin, _share, _hair, _beard, _mustache) in NEW_MEAT.items():
    GROOMING[_name] = {"Hair": GROOMS + "Hair/WI_Hair_" + _hair}
    if _beard:
        GROOMING[_name]["Beard"] = GROOMS + _beard
    if _mustache:
        GROOMING[_name]["Mustache"] = GROOMS + "Mustaches/" + _mustache

# cloth colours for the shorts (and the women's shirts): worn, dark, nobody's team
CLOTH = [(0.016, 0.016, 0.018), (0.05, 0.045, 0.04), (0.07, 0.02, 0.018), (0.03, 0.04, 0.03), (0.02, 0.028, 0.045), (0.09, 0.075, 0.055)]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(*parts):
    unreal.log("[build_gut_metahuman] " + " ".join(str(part) for part in parts))


def asset_name(preset):
    return preset.replace("-", "")


def lot(preset, salt):
    """A number from 0 to 1 that is always the same for the same fighter."""
    return (zlib.crc32((preset + salt).encode()) % 10000) / 10000.0


def is_man(preset):
    """Whether somebody has a man's body: by whose body it is. MetaHuman's own "Masculine/Feminine" measurement is not to be
    trusted for this: it read Asha as a man (2026-11-20), and the game takes a fighter's voice and strength from the answer."""
    body = MIXES.get(preset, (preset,))[0]
    if preset in DRESSED and DRESSED[preset][0]:
        body = DRESSED[preset][0]
    if preset in NEW_MEAT:
        body = NEW_MEAT[preset][0]
    return body in MEN


def note_bodies():
    """Writes down who is a man and who a woman, by the name each is built under, for Tools/import_gut_sound.py."""
    everyone = [PLAYER] + OPPONENTS + CROWD + list(NEW_MEAT) + list(DRESSED)
    said = {asset_name(preset): "man" if is_man(preset) else "woman" for preset in everyone}
    path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "gut_bodies.json")
    with open(path, "w") as out:
        json.dump(said, out, indent=1, sort_keys=True)
        out.write("\n")


def shorts_cut_away():
    """The map of the skin MetaHuman cuts away under the shorts alone. Epic's own map (kept beside this one in ArtSource/metahuman)
    cuts it away under shirt and shorts both; this is that picture with every part of the body above the waistband made white,
    which is to say kept. Worked out from the body's own unwrapping and the height of the shorts' waistband; it has no look."""
    path = ROOT + "/T_Gut_ShortsBodyMask"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    source = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "ArtSource", "metahuman", "body_hidden_face_map_shorts_only.png")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("destination_name", "T_Gut_ShortsBodyMask")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    # read the way Epic's own is read
    epic = unreal.load_asset(GARMENT).get_pipeline().get_editor_property("editor_pipeline").get_editor_property("body_hidden_face_map_texture").get_editor_property("texture")
    for setting in ("srgb", "compression_settings", "lod_group", "mip_gen_settings", "never_stream", "filter"):
        texture.set_editor_property(setting, epic.get_editor_property(setting))
    eal.save_loaded_asset(texture, only_if_is_dirty=False)
    log("map of the skin under the shorts:", path)
    return texture


def bare_chested_garment():
    """Epic's garment as worn without its shirt. MetaHuman cuts away the skin a garment covers, and Epic's garment
    covers the chest: this copy of it cuts the skin away under the shorts only, so the chest is still there when the
    shirt is not drawn. (Until Nov 15, 2026 it cut nothing away, and in a wide stance the skin of the thigh came
    through the cloth of the shorts.)"""
    path = ROOT + "/WI_Gut_Shorts"
    item = unreal.load_asset(path) if eal.does_asset_exist(path) else eal.duplicate_asset(GARMENT, path)
    editor_pipeline = item.get_pipeline().get_editor_property("editor_pipeline")
    cut_away = editor_pipeline.get_editor_property("body_hidden_face_map_texture")
    wanted = shorts_cut_away()
    if cut_away.get_editor_property("texture") != wanted:
        cut_away.set_editor_property("texture", wanted)
        editor_pipeline.set_editor_property("body_hidden_face_map_texture", cut_away)
        eal.save_loaded_asset(item, only_if_is_dirty=False)
    return path


def dress(character, preset, bare):
    collection = character.internal_collection
    instance = collection.default_instance
    worn = DRESSED[preset][1] if preset in DRESSED else [bare_chested_garment() if bare else GARMENT]
    choices = dict(GROOMING.get(preset, {}), Outfits=worn[0])
    for slot, choice in choices.items():
        key = collection.try_add_item_from_wardrobe_item(slot, unreal.load_asset(choice))
        instance.set_single_slot_selection(slot, key)
    # anything more is worn with the first: trousers and boots with a top
    for choice in worn[1:]:
        key = collection.try_add_item_from_wardrobe_item("Outfits", unreal.load_asset(choice))
        selection = unreal.MetaHumanPipelineSlotSelection()
        selection.set_editor_property("slot_name", "Outfits")
        selection.set_editor_property("selected_item", key)
        if not instance.try_add_slot_selection(selection):
            log(preset, "could not also put on", choice)
    if preset in DRESSED:
        log(preset, "wears", " + ".join(choice.split("/")[-1] for choice in worn), "; selections now:", len(instance.get_slot_selections()))


def shape(subsystem, character, preset):
    """Sets MetaHuman's fat and muscle sliders, as its editor does when one is dragged: lean and trained, each to a different degree.
    A spectator keeps the build the preset came with. Returns True for a man's body."""
    constraints = subsystem.get_body_constraints(character)
    man = is_man(preset)
    if preset in CROWD or (preset in DRESSED and preset not in DRESSED_FIGHTERS):
        return man
    sliders = {"Fat": -0.6 - 0.8 * lot(preset, "fat"), "Muscularity": (0.8 if man else 0.4) + 0.8 * lot(preset, "muscle")}
    for index, constraint in enumerate(constraints):
        if str(constraint.name) in sliders:
            constraint.is_active = True
            constraint.target_measurement = sliders[str(constraint.name)]
            # an entry read from the list is a copy: put the changed one back
            constraints[index] = constraint
    subsystem.set_body_constraints(character, constraints)
    subsystem.commit_body_state(character)
    log(preset, "man" if man else "woman", {key: round(value, 2) for key, value in sliders.items()},
        "height", [round(c.target_measurement) for c in subsystem.get_body_constraints(character) if str(c.name) == "Height"][0])
    return man


def not_drawn_material():
    """A material that draws nothing, for the part of a garment a fighter does not wear. It is a switch, not a look."""
    path = ROOT + "/M_Gut_NotDrawn"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    material = asset_tools.create_asset("M_Gut_NotDrawn", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("used_with_skeletal_mesh", True)
    nothing = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 0)
    nothing.set_editor_property("r", 0.0)
    unreal.MaterialEditingLibrary.connect_material_property(nothing, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.recompile_material(material)
    eal.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def tailor(preset, bare=None):
    """Epic's garment is a shirt and shorts in one piece. A man in the cage keeps the shorts only (bare); the cloth is dyed dark."""
    name = asset_name(preset)
    folder = "%s/%s/Clothing" % (ROOT, name)
    outfit = unreal.load_asset("%s/%s_Outfits" % (folder, name))
    colour = CLOTH[int(lot(preset, "cloth") * len(CLOTH)) % len(CLOTH)]
    not_drawn = not_drawn_material()
    blueprint = unreal.load_asset("%s/%s/BP_%s" % (ROOT, name, name))
    # which of the two it is was decided when the character was built, and is kept on its blueprint
    if bare is None:
        bare = eal.get_metadata_tag(blueprint, "GutBody") in ("man", "bare")
    eal.set_metadata_tag(blueprint, "GutBody", "bare" if bare else "covered")
    materials = outfit.materials
    for index, material in enumerate(materials):
        shirt = "Shirt" in str(material.material_slot_name)
        if shirt and bare:
            material.material_interface = not_drawn
            materials[index] = material
        elif isinstance(material.material_interface, unreal.MaterialInstanceConstant):
            instance = material.material_interface
            for parameter in ("diffuse_color_1", "diffuse_color_2"):
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, parameter, unreal.LinearColor(*colour, 1.0))
            unreal.MaterialEditingLibrary.update_material_instance(instance)
            eal.save_loaded_asset(instance, only_if_is_dirty=False)
    outfit.set_editor_property("materials", materials)
    eal.save_loaded_asset(outfit, only_if_is_dirty=False)

    # the character's blueprint names the garment's materials again on its own copy of the mesh
    subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(blueprint):
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(subobjects.k2_find_subobject_data_from_handle(handle))
        if isinstance(component, unreal.SkeletalMeshComponent) and component.get_skeletal_mesh_asset() == outfit:
            component.set_editor_property("override_materials", [material.material_interface for material in outfit.materials][:len(component.get_editor_property("override_materials"))])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    eal.save_loaded_asset(blueprint, only_if_is_dirty=False)
    log(preset, "shorts only" if bare else "shirt and shorts", "cloth", colour)


# what Epic's own plain pieces are dyed for this town: they come clean and bright, and nobody here has soap. Tops and
# trousers out of one list, boots and shoes out of another; the bought outfits come dirty and are left as they are.
DENIM = [(0.02, 0.028, 0.045), (0.035, 0.035, 0.04), (0.012, 0.012, 0.014), (0.03, 0.036, 0.05)]
# The techwear outfit is Epic's too and comes brightest of all: yellow and white, on Mother Ledger (seen 2026-11-20).
PLAIN = ("Hoodie", "Sweater", "Crewneckt", "Tshirt", "TCrop", "Cargopants", "Jeans", "Shorts", "Yoga", "Boots", "Chelseaboots", "Flats", "Flipflops", "Hightop", "Loafers",
         "techwearOutfit")
DRAB = [(0.09, 0.075, 0.055), (0.05, 0.045, 0.04), (0.07, 0.022, 0.018), (0.03, 0.04, 0.03), (0.02, 0.028, 0.045), (0.11, 0.10, 0.085),
        (0.06, 0.05, 0.03), (0.035, 0.035, 0.04), (0.075, 0.06, 0.05), (0.045, 0.03, 0.02)]
LEATHER = [(0.02, 0.015, 0.012), (0.035, 0.025, 0.018), (0.012, 0.012, 0.013), (0.05, 0.035, 0.022)]
# left as Epic set them: turned right up, the wear goes white and bleaches trousers to rags of no colour (tried 2026-11-19)
WORN = {}


def weather(preset):
    """Dyes down whatever of Epic's plain pieces somebody has on, each piece its own worn colour, and wears the cloth."""
    name = asset_name(preset)
    folder = "%s/%s/Clothing" % (ROOT, name)
    library = unreal.MaterialEditingLibrary
    for path in sorted(eal.list_assets(folder, recursive=False)):
        instance = unreal.load_asset(path.split(".")[0])
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            continue
        piece = instance.get_name()
        # only Epic's own: what was bought as a whole outfit keeps its maker's colours
        if not piece.startswith(tuple("MI_WI_OA_" + plain for plain in PLAIN)):
            continue
        shoes = "_shs_" in piece or piece.endswith("_Shoes")
        colours = LEATHER if shoes else (DENIM if "Jeans" in piece else DRAB)
        colour = colours[int(lot(preset, piece) * len(colours)) % len(colours)]
        second = colours[int(lot(preset, piece + " trim") * len(colours)) % len(colours)]
        vectors = {str(parameter) for parameter in library.get_vector_parameter_names(instance)}
        for parameter, value in (("diffuse_color_1", colour), ("B_diffuse_color_1", colour), ("diffuse_color_2", second), ("C_color", second)):
            if parameter in vectors:
                library.set_material_instance_vector_parameter_value(instance, parameter, unreal.LinearColor(*value, 1.0))
        scalars = {str(parameter) for parameter in library.get_scalar_parameter_names(instance)}
        for parameter, value in WORN.items():
            if parameter in scalars:
                library.set_material_instance_scalar_parameter_value(instance, parameter, value)
        if os.environ.get("GUT_CLOTH_LOG"):
            parent = instance.parent
            log(preset, piece, "wear", {parameter: round(library.get_material_instance_scalar_parameter_value(instance, parameter), 2) for parameter in sorted(scalars) if "Wear" in parameter or "pilling" in parameter or "Overlay" in parameter or "txMult" in parameter},
                "parent says", {parameter: round(library.get_material_default_scalar_parameter_value(parent, parameter), 2) for parameter in ("WearMaskStrength", "B_WearMaskStrength") if parent and isinstance(parent, unreal.Material)})
        library.update_material_instance(instance)
        eal.save_loaded_asset(instance, only_if_is_dirty=False)
        log(preset, piece, "dyed", colour)


def other_parent(subsystem, preset):
    """The face and the skin of the second preset a mixed person is made from."""
    path = "%s/Characters/_parent" % ROOT
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    parent = eal.duplicate_asset(PRESETS + preset, path)
    subsystem.try_add_object_to_edit(parent)
    try:
        return list(subsystem.get_face_model_coefficients(parent)), parent.skin_settings.copy()
    finally:
        subsystem.remove_object_to_edit(parent)
        eal.delete_asset(path)


def build(preset):
    name = asset_name(preset)
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    path = "%s/Characters/%s" % (ROOT, name)
    first, second = MIXES.get(preset, (preset, None))
    if preset in DRESSED and DRESSED[preset][0]:
        first = DRESSED[preset][0]
    share = 0.5
    if preset in NEW_MEAT:
        first, second, share = NEW_MEAT[preset][:3]
    other_face, other_skin = other_parent(subsystem, second) if second else (None, None)
    character = eal.load_asset(path) if eal.does_asset_exist(path) else eal.duplicate_asset(PRESETS + first, path)
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("cannot open %s for editing" % preset)
    try:
        if second:
            own_face = list(subsystem.get_face_model_coefficients(character))
            subsystem.set_face_model_coefficients(character, [a + (b - a) * share for a, b in zip(own_face, other_face)])
            subsystem.commit_face_state(character)
            log(preset, "is", first, "with the skin of", second, "and a face between the two,", share, "of it the second's")
        man = shape(subsystem, character, preset)
        bare = man and preset not in CROWD and preset not in DRESSED
        dress(character, preset, bare)
        skin = other_skin if second else character.skin_settings
        properties = skin.skin
        properties.show_top_underwear = False
        skin.skin = properties
        subsystem.commit_skin_settings(character, skin)

        # Epic's service now and then drops the connection in the middle of a request (Meat17 and Vivian, 2026-11-19 and 20),
        # and asking again is all it takes
        for attempt in range(3):
            rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            rig.blocking, rig.report_progress = True, False
            rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
            subsystem.request_auto_rigging(character, rig)

            textures = unreal.MetaHumanCharacterTextureRequestParams()
            textures.blocking, textures.report_progress = True, False
            subsystem.request_texture_sources(character, textures)

            if subsystem.can_build_meta_human(character=character):
                break
            log(preset, "was not rigged or textured at try", attempt + 1)

        if not subsystem.can_build_meta_human(character=character):
            raise RuntimeError("%s cannot be built (not rigged or textured: is the editor signed in to Epic?)" % preset)
        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        params.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        params.absolute_build_path = ROOT
        params.common_folder_path = ROOT + "/Common"
        params.enable_wardrobe_item_validation = False
        try:
            subsystem.build_meta_human(character=character, params=params)
        except RuntimeError as error:
            # the 5.9 preview raises on a face control rig link while copying the shared assets; the build carries on
            log(preset, "build reported:", error)
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    if preset in DRESSED:
        weather(preset)
    else:
        tailor(preset, bare)
    # a man or a woman, written on the character itself: a shirt no longer says which (Tools/import_gut_sound.py reads it)
    blueprint = unreal.load_asset("%s/%s/BP_%s" % (ROOT, name, name))
    eal.set_metadata_tag(blueprint, "GutSex", "man" if man else "woman")
    eal.save_loaded_asset(blueprint, only_if_is_dirty=False)
    log(preset, "built")


def ik_rig(name, folder, mesh):
    """An IK rig for a body, set up the way the editor's own automatic set-up does it."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    rig = asset_tools.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("%s does not fit %s" % (name, mesh.get_name()))
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError("no retarget chains could be found for %s" % mesh.get_name())
    controller.apply_auto_fbik()
    log(name, "chains:", ", ".join(str(chain.get_editor_property("chain_name")) for chain in controller.get_retarget_chains()))
    eal.save_loaded_asset(rig, only_if_is_dirty=False)
    return rig


def retargeter(body):
    """Carries the pose of the skeleton that does the fighting over to a MetaHuman body. One serves them all: they share a skeleton."""
    path = ROOT + "/RTG_Gut_MetaHuman"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    manny = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    asset = asset_tools.create_asset("RTG_Gut_MetaHuman", ROOT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(asset)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_rig("IK_Gut_Placeholder", "/Game/Gut/Art", manny))
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_rig("IK_Gut_MetaHuman", ROOT, body))
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, manny)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, body)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    eal.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


# what the fighters' faces do while they fight: two of the face performances Epic captured and ships with MetaHuman, looped.
# The player wears the first and whoever is across the cage the second, so the two faces never move as one.
FACES = ("/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Expression_Loops/AngerB/mhc_mh002_fmn_f_facialloop_angry_m_s001",
         "/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Expression_Loops/AngerA/mhc_mh002_fmn_f_facialloop_angry_f_s001")

# how far each joint of a hand turns to close it round a handle: finger, then the turn in degrees at each of its three joints.
# The thumb closes across the fingers, so it turns about another axis as well.
GRIP = {"index": (58.0, 72.0, 48.0), "middle": (62.0, 76.0, 50.0), "ring": (66.0, 78.0, 50.0), "pinky": (70.0, 78.0, 48.0)}
# how far closed the hand that holds nothing is, as a share of a full grip: a loose, ready hand, not a flat one
FREE_HAND = 0.4

GRIP_THUMB = (unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))


def grip_poses(asset):
    """Adds to the retargeter four more poses for the body to start from: the right hand closed with the left half closed
    (a weapon in the right hand, the left free to guard and grab), the left hand closed, both closed (fists), and both
    half closed (somebody whose wrists are tied behind them).
    The game picks one for what the fighter holds, and the fingers stay that way whatever the fight does with the arm."""
    controller = unreal.IKRetargeterController.get_controller(asset)
    target = unreal.RetargetSourceOrTarget.TARGET
    resting = controller.get_current_retarget_pose_name(target)
    for pose, closing in (("GripRight", {"r": 1.0, "l": FREE_HAND}), ("GripLeft", {"l": 1.0}), ("GripBoth", {"r": 1.0, "l": 1.0}), ("GripLoose", {"r": 0.5, "l": 0.5})):
        controller.remove_retarget_pose(pose, target)
        controller.duplicate_retarget_pose(resting, pose, target)
        controller.set_current_retarget_pose(pose, target)
        for side, share in closing.items():
            for finger, turns in GRIP.items():
                for joint, turn in enumerate(turns):
                    bone = "%s_%02d_%s" % (finger, joint + 1, side)
                    closed = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-turn * share).quaternion()
                    controller.set_rotation_offset_for_retarget_pose_bone(bone, controller.get_rotation_offset_for_retarget_pose_bone(bone, target) * closed, target)
            for joint, turn in enumerate(GRIP_THUMB):
                bone = "thumb_%02d_%s" % (joint + 1, side)
                controller.set_rotation_offset_for_retarget_pose_bone(bone, controller.get_rotation_offset_for_retarget_pose_bone(bone, target) * turn.quaternion(), target)
    controller.set_current_retarget_pose(resting, target)
    eal.save_loaded_asset(asset, only_if_is_dirty=False)
    log("grip poses set on", asset.get_name(), "beside", resting)


def character_class(preset):
    """The built character's blueprint class, or None if it has not been built yet."""
    name = asset_name(preset)
    path = "%s/%s/BP_%s" % (ROOT, name, name)
    return unreal.load_asset(path).generated_class() if eal.does_asset_exist(path) else None


def wire():
    """Puts the built characters in the cage: the player, the cage's stock opponent, and each fight on the ladder."""
    player = character_class(PLAYER)
    if not player:
        log("nobody is in the cage yet:", PLAYER, "is not built")
        return
    name = asset_name(PLAYER)
    retarget = retargeter(unreal.load_asset("%s/%s/Body/SKM_%s_BodyMesh" % (ROOT, name, name)))
    grip_poses(retarget)

    stock = character_class(OPPONENTS[0]) or player
    for blueprint_name, body, face in (("BP_GutPlayer", player, FACES[0]), ("BP_GutOpponent", stock, FACES[1]), ("BP_GutDummy", stock, FACES[1])):
        blueprint = unreal.load_asset("/Game/Gut/Blueprints/" + blueprint_name)
        defaults = unreal.get_default_object(blueprint.generated_class())
        defaults.set_editor_property("body_class", body)
        defaults.set_editor_property("body_retargeter", retarget)
        defaults.set_editor_property("face_loop", unreal.load_asset(face))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        eal.save_loaded_asset(blueprint, only_if_is_dirty=False)

    # the people on the deck
    watching = [character_class(preset) for preset in CROWD if character_class(preset)]
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Gut/Lvl_Gut")
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.GutCrowd):
            actor.set_editor_property("spectator_classes", watching)
            actor.set_editor_property("spectator_retargeter", retarget)
            # a full deck of these people costs frames: 70 of them halve the frame rate again against 42,
            # and with 28 faces to go round 42 keeps the repeats down
            actor.set_editor_property("max_attendance", 42)
            actor.set_editor_property("attendance_per_name", 1.25)
            log("the crowd is", len(watching), "people:", " ".join(preset for preset in CROWD if character_class(preset)))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()

    cast = list(OPPONENTS)
    for ladder_name, count in LADDERS:
        ladder = unreal.load_asset("/Game/Gut/Ladder/" + ladder_name)
        rungs = ladder.get_editor_property("rungs")
        for index in range(count):
            preset = cast.pop(0)
            rung = rungs[index]
            rung.set_editor_property("body", character_class(preset))
            rungs[index] = rung
            log(ladder_name, index + 1, str(rung.get_editor_property("opponent_name")), "is", preset, "" if character_class(preset) else "(not built yet)")
        # the first ladder also keeps who the new meat is seen as: whoever comes into the cage after the dead
        if ladder_name == LADDERS[0][0]:
            fresh = [character_class(name) for name in NEW_MEAT if character_class(name)]
            ladder.set_editor_property("newcomer_bodies", fresh)
            log("new meat:", len(fresh), "of", len(NEW_MEAT), "built:", " ".join(name for name in NEW_MEAT if character_class(name)))
        ladder.set_editor_property("rungs", rungs)
        eal.save_loaded_asset(ladder, only_if_is_dirty=False)


def main():
    wanted = os.environ.get("GUT_METAHUMANS", "").split()
    if wanted == ["all"]:
        wanted = [PLAYER] + OPPONENTS
    elif wanted == ["crowd"]:
        wanted = list(CROWD)
    elif wanted == ["meat"]:
        # only the ones not built yet, so that a run that was stopped can be taken up again
        wanted = [name for name in NEW_MEAT if not character_class(name)]
    failed = []
    for preset in wanted:
        try:
            build(preset)
        except Exception as error:
            failed.append(preset)
            log(preset, "NOT BUILT:", repr(error))
    # GUT_REWEATHER dyes again whoever is dressed and built, without building anybody
    if os.environ.get("GUT_REWEATHER"):
        for preset in DRESSED:
            if character_class(preset) and preset not in wanted:
                weather(preset)
    if os.environ.get("GUT_RETAILOR"):
        for preset in [PLAYER] + OPPONENTS + CROWD + list(NEW_MEAT):
            if character_class(preset) and preset not in wanted:
                tailor(preset)
    note_bodies()
    # GUT_NO_WIRE builds people and leaves the cage as it is: for trying something on
    if not os.environ.get("GUT_NO_WIRE"):
        wire()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("cast:", PLAYER, "then", " ".join(OPPONENTS))
    if failed:
        log("FAILED", " ".join(failed))
    log("DONE")


try:
    main()
except Exception as error:
    import traceback
    log("FAILED", traceback.format_exc())
