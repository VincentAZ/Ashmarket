#!/usr/bin/env bash
# Brings the surfaces the screens are drawn on (ArtSource/mixar/ui) into Content/Gut/Art/UI
# by running import_gut_ui.py in an editor with no window.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_gut_ui.log"


"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_gut_ui.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_gut_ui\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[import_gut_ui\] DONE" "$LOG"; then
	echo "Screens imported."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
