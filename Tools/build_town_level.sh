#!/usr/bin/env bash
# Builds a walkable place of the town from its list (Tools/town_pool_street.json): the level, what stands in it,
# its lights, and who is there and what they say. Runs build_town_level.py in an editor with no window.
# Safe to run again after changing the list. GUT_TOWN_LIST names another list.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/build_town_level.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/build_town_level.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[build_town_level\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-600 || true

if grep -q "\[build_town_level\] OPEN EDGES" "$LOG"; then
	echo "Town level built, but a walker would be stopped by nothing in the places listed above." >&2
	exit 1
elif grep -q "\[build_town_level\] DONE" "$LOG"; then
	echo "Town level built."
else
	echo "Build failed. See $LOG" >&2
	exit 1
fi
