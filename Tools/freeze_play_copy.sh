#!/usr/bin/env bash
# Makes (or brings up to date) a copy of the game as it is built now, to play while the project itself is being worked on.
#
#   Tools/freeze_play_copy.sh        copy what is built now to ~/.cache/ashmarket-sim/play
#   Tools/play_frozen.sh [...]       play that copy; takes what Tools/play_gut.sh takes
#
# The copy has its own binaries, content and logs, so a build, an import or a rebuilt place here cannot reach a game
# running there. The run kept on disk (Saved/SaveGames) is the project's own: a fighter is the same fighter in both.
# Refuses while a game is running from the copy.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COPY_DIR="${GUT_PLAY_COPY:-$HOME/.cache/ashmarket-sim/play}"

for PROC in /proc/[0-9]*; do
	if [[ "$(readlink "$PROC/exe" 2>/dev/null)" == */UnrealEditor* ]] && tr '\0' ' ' <"$PROC/cmdline" 2>/dev/null | grep -qF "$COPY_DIR/"; then
		echo "A game is running from the copy (process ${PROC#/proc/}). Not touched." >&2
		exit 2
	fi
done

mkdir -p "$COPY_DIR/Saved"

for PART in Binaries Config Content DerivedDataCache Intermediate Tools; do
	rsync -a --delete "$PROJECT_DIR/$PART/" "$COPY_DIR/$PART/"
done

rsync -a "$PROJECT_DIR/Ashmarket.uproject" "$COPY_DIR/"
rsync -a --delete "$PROJECT_DIR/Saved/Config/" "$COPY_DIR/Saved/Config/"

# one run on disk for both
mkdir -p "$PROJECT_DIR/Saved/SaveGames"
ln -sfn "$PROJECT_DIR/Saved/SaveGames" "$COPY_DIR/Saved/SaveGames"

date '+%Y-%m-%d %H:%M %Z' >"$COPY_DIR/FROZEN_AT"
git -C "$PROJECT_DIR" rev-parse --short HEAD >>"$COPY_DIR/FROZEN_AT" 2>/dev/null || true
echo "Frozen at $(head -1 "$COPY_DIR/FROZEN_AT") in $COPY_DIR"
