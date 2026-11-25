#!/usr/bin/env bash
# Makes the faces that go with the town's spoken lines, by running make_town_faces.py in an editor with no window.
# See the top of that file for what it does and its options.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/make_town_faces.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/make_town_faces.py" -EnablePlugins=MetaHuman \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[make_town_faces\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[make_town_faces\] DONE" "$LOG"; then
	echo "Faces made."
else
	echo "Failed. See $LOG" >&2
	grep -E "Error|Traceback|AttributeError" "$LOG" | tail -12 >&2
	exit 1
fi
