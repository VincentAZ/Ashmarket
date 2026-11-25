#!/usr/bin/env bash
# Dirties what came too clean: see Tools/grime_gut_people.py. Run with the editor closed.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
LOG="$PROJECT_DIR/Saved/Logs/grime_gut_people.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/grime_gut_people.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[grime_gut_people\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]LogPython: //" | cut -c1-300 || true

grep -q "\[grime_gut_people\] DONE" "$LOG" || { echo "Failed. See $LOG" >&2; exit 1; }
