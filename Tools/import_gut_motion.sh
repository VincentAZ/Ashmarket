#!/usr/bin/env bash
# Brings the motion clips in ArtSource/kimodo/fbx into Content/Gut/Art/Motion
# by running import_gut_motion.py in an editor with no window.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_gut_motion.log"

# GUT_MOTION_REDO=1 makes the rig and the retargeter again. They are removed here, before the editor has them open:
# deleted from inside it, the retargeter stays behind without its rig and every clip comes out standing in a T.
if [[ -n "${GUT_MOTION_REDO:-}" ]]; then
	rm -f "$PROJECT_DIR/Content/Gut/Art/Motion/Soma/RTG_Gut_Soma.uasset" "$PROJECT_DIR/Content/Gut/Art/Motion/Soma/IK_Gut_Soma.uasset"
fi

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_gut_motion.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_gut_motion\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[import_gut_motion\] DONE" "$LOG"; then
	echo "Motion imported."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
