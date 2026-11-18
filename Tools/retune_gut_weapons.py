"""
Writes changed attack numbers onto weapons that already exist, and nothing else.

Tools/build_gut_content.py makes a weapon once and then leaves it alone, because Tools/import_gut_motion.py has since put
the weapon's clips on it. So a number changed after a measurement is written here as well as there: there for the record
and for a project built from nothing, here for the weapon as it stands. Only the numbers named below are touched.

Run it through Tools/retune_gut_weapons.sh, which starts the editor with no window.
"""

import traceback

import unreal

WEAPONS = "/Game/Gut/Weapons"

# weapon asset: numbers for every light attack, and numbers for the heavy one. The fields are FGutAttackDef's.
# Empty as it stands: nothing has been retuned this way yet. An entry looks like
#   "DA_Gut_Chain": {"light": {"recovery": 0.32}, "heavy": {"damage": 34.0}},
# Try numbers first without writing anything: GUT_ARGS="-GutWeaponTune=Chain:Light.Recovery=0.85" on the balance tools
# multiplies them in memory for that run only.
# "weapon" holds properties of the weapon itself, by the names the editor's Python gives them.
RETUNE = {
    # Nov 15, from Vincent's first play: with the guard held a shield strikes with the shield (its second light attack, the
    # bash); with the guard down it jabs and bashes as before.
    "DA_Gut_Shield": {"weapon": {"guarded_light_attack": 1,
                                 "description": "Hits least, guards best. Guard held, it strikes with the shield. Its charge closes the distance."}},
}

eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log("[retune_gut_weapons] " + text)


def main():
    for name, numbers in RETUNE.items():
        weapon = unreal.load_asset("{}/{}".format(WEAPONS, name))

        if not weapon:
            raise RuntimeError("There is no weapon called {}".format(name))

        # an entry read from an array of structs is a copy: each is written back
        lights = list(weapon.get_editor_property("light_attacks"))

        for index, attack in enumerate(lights):
            for field, value in numbers.get("light", {}).items():
                was = attack.get_editor_property(field)
                attack.set_editor_property(field, value)

                log("{} {}: {} {} -> {}".format(name, attack.get_editor_property("label"), field, round(was, 3), value))

            lights[index] = attack

        weapon.set_editor_property("light_attacks", lights)

        heavy = weapon.get_editor_property("heavy_attack")

        for field, value in numbers.get("heavy", {}).items():
            was = heavy.get_editor_property(field)
            heavy.set_editor_property(field, value)

            log("{} {}: {} {} -> {}".format(name, heavy.get_editor_property("label"), field, round(was, 3), value))

        weapon.set_editor_property("heavy_attack", heavy)

        for field, value in numbers.get("weapon", {}).items():
            was = weapon.get_editor_property(field)
            weapon.set_editor_property(field, value)

            log("{}: {} {} -> {}".format(name, field, was, value))

        if not eal.save_loaded_asset(weapon, only_if_is_dirty=False):
            raise RuntimeError("Could not save {}".format(name))

    log("DONE")


try:
    main()
except Exception:
    log("FAILED\n" + traceback.format_exc())
