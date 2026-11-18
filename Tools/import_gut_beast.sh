#!/usr/bin/env bash
# Brings the pit boss's dog into the project (its model from ArtSource/mixar/pit_dog, its clips from ArtSource/beast)
# by running import_gut_beast.py in an editor with no window. GUT_BEAST_WIRE=1 also puts it to use in the cage.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_gut_beast.log"

# the rigs and retargeters are made afresh each time. Deleted from inside the editor they come back broken, so they go here.
find "$PROJECT_DIR/Content/Gut/Art/Beast" \( -name "IK_Gut_*.uasset" -o -name "RTG_Gut_*.uasset" \) -delete 2>/dev/null || true

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_gut_beast.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_gut_beast\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[import_gut_beast\] DONE" "$LOG"; then
	echo "Beast imported."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
