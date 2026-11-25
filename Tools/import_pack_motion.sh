#!/usr/bin/env bash
# Carries clips from the bought motion capture packs over to the fighting skeleton: see Tools/import_pack_motion.py.
# Run with the editor closed. PACK_MOTION_REDO=1 makes the rigs and the retargeters again and redoes every clip;
# PACK_MOTION_CLIPS=a,b redoes just those.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
LOG="$PROJECT_DIR/Saved/Logs/import_pack_motion.log"

if [[ -n "${PACK_MOTION_REDO:-}" ]]; then
	# deleted from inside the editor, a retargeter is left behind without its rig and every clip comes out in a T
	rm -f "$PROJECT_DIR"/Content/Gut/Art/Motion/Packs/RTG_Pack_*.uasset "$PROJECT_DIR"/Content/Gut/Art/Motion/Packs/IK_Pack_*.uasset
fi

# a clip that a fighter or a place already uses cannot be deleted from inside the editor ("is in use"): the old ones go here
python3 -I - "$PROJECT_DIR" <<'PY'
import json, os, sys
project = sys.argv[1]
listed = json.load(open(os.path.join(project, "Tools", "gut_pack_clips.json")))
redo = bool(os.environ.get("PACK_MOTION_REDO"))
only = [name for name in os.environ.get("PACK_MOTION_CLIPS", "").split(",") if name]
for name in listed["clips"]:
    if redo or name in only:
        path = os.path.join(project, "Content", "Gut", "Art", "Motion", name + ".uasset")
        if os.path.exists(path):
            os.remove(path)
PY

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_pack_motion.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_pack_motion\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]LogPython: //" | cut -c1-900 || true

grep -q "\[import_pack_motion\] DONE" "$LOG" || { echo "Import failed. See $LOG" >&2; exit 1; }
