#!/usr/bin/env bash
# Makes the same few lines several ways to choose between: see Tools/audition_town_voices.py.
# The tool itself reads the key from .env.local. It is never printed and never written anywhere else.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="${ASHMARKET_VOICES_VENV:-$HOME/.cache/ashmarket-sim/voices-venv}"

if [[ ! -x "$VENV/bin/python" ]]; then
	python3 -m venv "$VENV"
fi

# whatever the tools need that the environment has not got yet
"$VENV/bin/python" -c "import elevenlabs, dotenv" 2>/dev/null || "$VENV/bin/pip" install -q -r "$PROJECT_DIR/Tools/requirements.txt"

if [[ ! -f "$PROJECT_DIR/.env.local" ]]; then
	echo "There is no .env.local with an ElevenLabs key in it: run npx -y @elevenlabs/cli@latest onboard init" >&2
	exit 1
fi

exec "$VENV/bin/python" -B "$PROJECT_DIR/Tools/audition_town_voices.py" "$@"
