#!/usr/bin/env bash
# Brings the clips cut from the 100STYLE motion capture into the project: see Tools/import_style_motion.py.
# Run with the editor closed. STYLE_MOTION_REDO=1 makes the rig and the retargeter again and redoes every clip.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
LOG="$PROJECT_DIR/Saved/Logs/import_style_motion.log"

if [[ -n "${STYLE_MOTION_REDO:-}" ]]; then
	# deleted from inside the editor, the retargeter is left behind without its rig and every clip comes out in a T
	rm -f "$PROJECT_DIR/Content/Gut/Art/Motion/Style/RTG_Gut_Style.uasset" "$PROJECT_DIR/Content/Gut/Art/Motion/Style/IK_Gut_Style.uasset"

	# and a clip that a fighter or a place already uses cannot be deleted from inside the editor at all ("is in use"): the
	# old ones go here, and the new ones, made under the same names, are found by whatever used the old
	rm -f "$PROJECT_DIR"/Content/Gut/Art/Motion/style_*.uasset
fi

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_style_motion.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_style_motion\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]LogPython: //" | cut -c1-330 || true

grep -q "\[import_style_motion\] DONE" "$LOG" || { echo "Import failed. See $LOG" >&2; exit 1; }
