#!/usr/bin/env bash
# Brings the blood pictures in ArtSource/mixar/blood into Content/Gut/Art/Blood and makes what shows them
# by running import_gut_blood.py in an editor with no window.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_gut_blood.log"


"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_gut_blood.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_gut_blood\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[import_gut_blood\] DONE" "$LOG"; then
	echo "Blood imported."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
