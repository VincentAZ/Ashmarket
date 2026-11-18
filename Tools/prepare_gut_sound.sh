#!/usr/bin/env bash
# Runs Tools/prepare_gut_sound.py with a Python that has numpy: the one in the Kimodo install in ~/Repos/DigitalHumanStudio,
# which is only read and executed (no byte code is written there).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PYTHON="${GUT_NUMPY_PYTHON:-$HOME/Repos/DigitalHumanStudio/.runtime/kimodo/venv/bin/python}"

exec "$PYTHON" -B "$HERE/prepare_gut_sound.py" "$@"
