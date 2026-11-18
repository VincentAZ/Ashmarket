#!/usr/bin/env bash
# Writes the attack numbers listed in retune_gut_weapons.py onto the weapons that already exist,
# by running it in an editor with no window. Nothing else about a weapon is touched.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/retune_gut_weapons.log"


"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/retune_gut_weapons.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[retune_gut_weapons\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[retune_gut_weapons\] DONE" "$LOG"; then
	echo "Weapons retuned."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
