#!/usr/bin/env bash
# Assembles fighters as MetaHumans in an editor with no window, and puts the ones that are built into the cage.
# Pass the names of Epic's presets to build (each is rebuilt from scratch), "all" for the whole cast in build_gut_metahuman.py, or "crowd" for the spectators.
# With no names it only puts the built ones in the cage. A fighter takes about 3 minutes.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
LOG="$PROJECT_DIR/Saved/Logs/build_gut_metahuman.log"

for PRESET in "$@"; do
	NAME="${PRESET//-/}"
	[ "$NAME" = all ] || [ "$NAME" = crowd ] || rm -rf "$PROJECT_DIR/Content/Gut/Art/MetaHumans/$NAME" "$PROJECT_DIR/Content/Gut/Art/MetaHumans/Characters/$NAME.uasset"
done

GUT_METAHUMANS="$*" "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/build_gut_metahuman.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[build_gut_metahuman\]" "$LOG" | grep -v "\] asset " | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]//' | cut -c1-1500

if grep -q "\[build_gut_metahuman\] DONE" "$LOG" && ! grep -q "\[build_gut_metahuman\] FAILED" "$LOG"; then
	echo "MetaHumans built."
else
	echo "MetaHuman build failed. See $LOG" >&2
	exit 1
fi
