#!/usr/bin/env bash
# Stands a piece of the town round the Gut (containers, a bus, the broken freeway: models made in Mixar, ArtSource/mixar/town_*)
# by running place_gut_town.py in an editor with no window. GUT_TOWN=remove takes it away again.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/place_gut_town.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/place_gut_town.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[place_gut_town\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[place_gut_town\] DONE" "$LOG"; then
	echo "The town stands."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
