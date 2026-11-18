#!/usr/bin/env bash
# Runs Tools/make_gut_motion.py inside the Kimodo install that lives in ~/Repos/DigitalHumanStudio.
# That project is only read and executed: no lock is taken there and Python is told not to write byte code.
set -euo pipefail

STUDIO="${GUT_KIMODO_HOME:-$HOME/Repos/DigitalHumanStudio}"
MODELS="$STUDIO/.runtime/kimodo/models"
HERE="$(cd "$(dirname "$0")" && pwd)"

if docker ps --format '{{.Names}}' 2>/dev/null | grep -q '^studio-'; then
    echo "A studio-* container is running in DigitalHumanStudio; not taking the graphics card from it." >&2
    exit 1
fi

export PYTHONDONTWRITEBYTECODE=1
export CHECKPOINT_DIR="$MODELS"
export TEXT_ENCODERS_DIR="$MODELS/text_encoder"
export TEXT_ENCODER_MODE=local
export LOCAL_CACHE=True
export HF_HUB_OFFLINE=1
export TRANSFORMERS_OFFLINE=1

cd "$STUDIO"
exec "$STUDIO/.runtime/kimodo/venv/bin/python" "$HERE/make_gut_motion.py" "$@"
