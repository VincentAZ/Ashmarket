#!/usr/bin/env bash
# Wears the Gate Warden in: his armor dusty, scratched and dull, his face weathered, tired and grimed, from photographed
# dust, smudges and scratches (ambientCG, CC0) laid on his own pictures. Only him: the outfit as bought is not touched.
#
#   Tools/wear_gate_warden.sh              all three steps (write his pictures out, wear them, bring them in)
#   GATE_WARDEN_MUCH=1.4 Tools/wear_gate_warden.sh   more of it (0 is as bought)
#   GATE_WARDEN_CLEAN=1 Tools/wear_gate_warden.sh    as he was
# With the editor closed. Then rebuild nothing: his body is the same body.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
PYTHON="${GUT_NUMPY_PYTHON:-$HOME/Repos/DigitalHumanStudio/.runtime/kimodo/venv/bin/python}"

run() {
	"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" -ExecutePythonScript="$PROJECT_DIR/Tools/$1" \
		-unattended -nosplash -RenderOffscreen -nosound 2>&1 | grep -E "\[wear_gate_warden\]|Traceback|Error: " | sed -E 's/^.*\[wear_gate_warden\] //' | cut -c1-300 || true
}

if [[ -z "${GATE_WARDEN_CLEAN:-}" ]]; then
	[[ -f "$PROJECT_DIR/ArtSource/worn/gate_warden/src/pictures.json" ]] || run wear_gate_warden_export.py
	"$PYTHON" -B "$PROJECT_DIR/Tools/wear_gate_warden_paint.py" --much "${GATE_WARDEN_MUCH:-1.0}"
fi

run wear_gate_warden_apply.py
