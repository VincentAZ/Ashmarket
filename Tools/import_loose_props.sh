#!/usr/bin/env bash
# Brings the small scanned props that lie about the town into the project, by running import_loose_props.py in an editor with no window.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_loose_props.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_loose_props.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_loose_props\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

grep -q "\[import_loose_props\] DONE" "$LOG" || { echo "Import failed. See $LOG" >&2; exit 1; }
