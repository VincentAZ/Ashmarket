#!/usr/bin/env bash
# Builds Content/Gut (materials, input, Blueprints, level) by running build_gut_content.py in an editor with no window.
# Needs a working GPU: materials cannot be compiled without a renderer.
#
#   Tools/build_gut_content.sh            adds whatever is missing and leaves existing assets alone
#   Tools/build_gut_content.sh --force    deletes Content/Gut first and rebuilds all of it (hand edits are lost)
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

if [[ "${1:-}" == "--force" ]]; then
	rm -rf "$PROJECT_DIR/Content/Gut"
fi

LOG="$PROJECT_DIR/Saved/Logs/build_gut_content.log"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/build_gut_content.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[build_gut_content\]" "$LOG" || true

if grep -q "\[build_gut_content\] DONE" "$LOG"; then
	echo "Content/Gut built."
else
	echo "Build failed. See $LOG" >&2
	exit 1
fi
