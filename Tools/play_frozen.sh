#!/usr/bin/env bash
# Plays the frozen copy of the game (see Tools/freeze_play_copy.sh), which a build or an import in the project cannot
# reach. Takes everything Tools/play_gut.sh takes: GUT_MODE=ladder Tools/play_frozen.sh, and so on.
# Its log is ~/.cache/ashmarket-sim/play/Saved/Logs/Ashmarket.log.
set -euo pipefail

COPY_DIR="${GUT_PLAY_COPY:-$HOME/.cache/ashmarket-sim/play}"

if [[ ! -x "$COPY_DIR/Tools/play_gut.sh" ]]; then
	echo "There is no frozen copy yet: run Tools/freeze_play_copy.sh" >&2
	exit 1
fi

echo "Playing the copy frozen at $(head -1 "$COPY_DIR/FROZEN_AT" 2>/dev/null || echo '?')"
exec "$COPY_DIR/Tools/play_gut.sh" "$@"
