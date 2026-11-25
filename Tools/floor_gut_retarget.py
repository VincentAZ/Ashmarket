"""
Gives the retargeter that carries the fighting skeleton's pose onto every MetaHuman body a floor constraint on both legs:
a foot that is near the floor on the fighting skeleton is kept the same height above it on the body that is seen.

Without it the legs were copied joint by joint, and on any body whose legs are not the mannequin's the feet hung in the
air through part of every step (measured 21 November: the lower sole 4 cm off the floor on average at a walk, 10 at worst).

Run headless, with the editor closed:
  UnrealEditor-Cmd Ashmarket.uproject -ExecutePythonScript=Tools/floor_gut_retarget.py -unattended -nosplash -RenderOffscreen -nosound
The retargeter as it was is kept in ArtSource/level_backups/RTG_Gut_MetaHuman_before_floor.uasset.
"""

import unreal

PATH = "/Game/Gut/Art/MetaHumans/RTG_Gut_MetaHuman"


def log(*words):
    unreal.log("[floor_gut_retarget] " + " ".join(str(word) for word in words))


def main():
    asset = unreal.load_asset(PATH)

    if not asset:
        log("FAILED: no", PATH)
        return

    controller = unreal.IKRetargeterController.get_controller(asset)

    if controller.get_index_of_op_by_name("Floor Constraint") < 0:
        floor = controller.add_retarget_op("/Script/IKRig.IKRetargetFloorConstraintOp")
        controller.set_parent_op_by_name(controller.get_op_name(floor), "Run IK Rig")
        controller.run_op_initial_setup(floor)

    floor_controller = controller.get_op_controller(controller.get_index_of_op_by_name("Floor Constraint"))
    settings = floor_controller.get_settings()
    chains = settings.get_editor_property("chains_to_affect")

    for index in range(len(chains)):
        chain = chains[index]

        if str(chain.get_editor_property("target_chain_name")) in ("LeftLeg", "RightLeg"):
            chain.set_editor_property("enable_floor_constraint", True)
            chain.set_editor_property("maintain_height_offset", 1.0)
            chains[index] = chain

    settings.set_editor_property("chains_to_affect", chains)
    floor_controller.set_settings(settings)

    log("floor constraint on:", ", ".join("{} {}".format(chain.get_editor_property("target_chain_name"), chain.get_editor_property("enable_floor_constraint"))
                                          for chain in floor_controller.get_settings().get_editor_property("chains_to_affect")))

    # And each foot of the body that is seen goes where the fighting skeleton's foot is, not where legs of another length
    # happen to put it. Measured 21 November: with the fighting skeleton's planted feet held to the floor (1 cm of sliding in
    # a hundred of travel), the feet of the body carried from it still slid 28, because the legs were copied joint by joint.
    if controller.get_index_of_op_by_name("Blend to Source") < 0:
        blend = controller.add_retarget_op("/Script/IKRig.IKRetargetBlendToSourceOp")
        controller.set_parent_op_by_name(controller.get_op_name(blend), "Run IK Rig")
        controller.run_op_initial_setup(blend)

    blend_controller = controller.get_op_controller(controller.get_index_of_op_by_name("Blend to Source"))
    settings = blend_controller.get_settings()
    chains = settings.get_editor_property("chains")

    for index in range(len(chains)):
        chain = chains[index]
        legs = str(chain.get_editor_property("target_chain_name")) in ("LeftLeg", "RightLeg")
        chain.set_editor_property("blend_to_source", 1.0 if legs else 0.0)
        chain.set_editor_property("translation_alpha", 1.0)
        chain.set_editor_property("rotation_alpha", 0.0)
        chains[index] = chain

    settings.set_editor_property("chains", chains)
    settings.set_editor_property("alpha", 1.0)
    blend_controller.set_settings(settings)

    log("feet go to the source's feet:", ", ".join("{} {}".format(chain.get_editor_property("target_chain_name"), chain.get_editor_property("blend_to_source"))
                                                   for chain in blend_controller.get_settings().get_editor_property("chains")))
    # in that order: first to the source's foot, then held off the floor by as much as the body's own foot stands off it.
    # The other way round, the source's height would be put back over what the floor constraint had done.
    blend_at = controller.get_index_of_op_by_name("Blend to Source")
    floor_at = controller.get_index_of_op_by_name("Floor Constraint")

    if blend_at > floor_at:
        controller.move_retarget_op_in_stack(blend_at, floor_at)

    log("ops:", ", ".join(str(controller.get_op_name(index)) for index in range(controller.get_num_retarget_ops())))

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    log("DONE")


try:
    main()
except Exception:
    import traceback
    log("FAILED", traceback.format_exc())
