"""
Gives the retargeter that carries the fighting skeleton's pose onto every MetaHuman body its last step: Keep Clear Of Own
Body (FGutClearOp, in the game's own module), which moves each arm, hand and held weapon out of the wearer's own trunk and
thighs as they are dressed. See Source/Ashmarket/Gut/GutClearOp.h for why.

Run headless, with the editor closed:
  UnrealEditor-Cmd Ashmarket.uproject -ExecutePythonScript=Tools/clear_gut_retarget.py -unattended -nosplash -RenderOffscreen -nosound
The retargeter as it was is kept in ArtSource/level_backups/RTG_Gut_MetaHuman_before_clear.uasset.
GUT_CLEAR_REMOVE=1 takes the step out again.
"""

import os

import unreal

PATH = "/Game/Gut/Art/MetaHumans/RTG_Gut_MetaHuman"
OP = "/Script/Ashmarket.GutClearOp"
NAME = "Keep Clear Of Own Body"


def log(*words):
    unreal.log("[clear_gut_retarget] " + " ".join(str(word) for word in words))


def main():
    asset = unreal.load_asset(PATH)

    if not asset:
        log("FAILED: no", PATH)
        return

    controller = unreal.IKRetargeterController.get_controller(asset)

    names = [str(controller.get_op_name(index)) for index in range(controller.get_num_retarget_ops())]
    log("steps before:", ", ".join(names))

    mine = [index for index in range(controller.get_num_retarget_ops()) if str(controller.get_op_type_by_name(controller.get_op_name(index))) == OP]

    if os.environ.get("GUT_CLEAR_REMOVE"):
        for index in reversed(mine):
            controller.remove_retarget_op(index)
    elif not mine:
        index = controller.add_retarget_op(OP)

        if index < 0:
            log("FAILED: the step could not be added")
            return

    names = [str(controller.get_op_name(index)) for index in range(controller.get_num_retarget_ops())]
    log("steps after:", ", ".join(names))

    unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False)
    log("saved")


main()
