#!/usr/bin/env bash
# Makes (or brings up to date) a copy of the game as it is built now, to play while the project itself is being worked on.
#
#   Tools/freeze_play_copy.sh        copy what is built now to ~/.cache/ashmarket-sim/play, or to play-b if play is being played
#   Tools/play_frozen.sh [...]       play that copy; takes what Tools/play_gut.sh takes
#
# The copy has its own binaries, content and logs, so a build, an import or a rebuilt place here cannot reach a game
# running there. The run kept on disk (Saved/SaveGames) is the project's own: a fighter is the same fighter in both.
# A copy somebody is playing from is never touched: the other of the two is made instead.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE="${GUT_PLAY_BASE:-$HOME/.cache/ashmarket-sim}"

# There are two copies, play and play-b, so that a new one can be frozen while somebody is playing the last: the one
# nobody is playing from is brought up to date, and Tools/play_frozen.sh plays whichever was frozen last. (He was
# playing when the first piece of Act 2 was ready, 25 November, and one copy could not be touched.)
running() {
	local PROC

	for PROC in /proc/[0-9]*; do
		if [[ "$(readlink "$PROC/exe" 2>/dev/null)" == */UnrealEditor* ]] && tr '\0' ' ' <"$PROC/cmdline" 2>/dev/null | grep -qF "$1/Ashmarket.uproject"; then
			return 0
		fi
	done

	return 1
}

COPY_DIR="${GUT_PLAY_COPY:-}"

if [[ -z "$COPY_DIR" ]]; then
	A="$BASE/play"
	B="$BASE/play-b"

	if running "$A" && running "$B"; then
		echo "Games are running from both copies. Neither is touched." >&2
		exit 2
	elif running "$A"; then
		COPY_DIR="$B"
	elif running "$B"; then
		COPY_DIR="$A"
	elif [[ -f "$B/FROZEN_AT" && -f "$A/FROZEN_AT" && "$B/FROZEN_AT" -ot "$A/FROZEN_AT" ]]; then
		# nobody is playing: the older of the two
		COPY_DIR="$B"
	else
		COPY_DIR="$A"
	fi
elif running "$COPY_DIR"; then
	echo "A game is running from $COPY_DIR. Not touched." >&2
	exit 2
fi

# what has not changed since the other copy was made is shared with it on disk, not copied again: the content is 50 GB
OTHER="$BASE/play"
[[ "$COPY_DIR" == "$OTHER" ]] && OTHER="$BASE/play-b"

mkdir -p "$COPY_DIR/Saved"

for PART in Binaries Config Content DerivedDataCache Intermediate Tools; do
	LINK=()

	if [[ "$PART" == "Content" && -d "$OTHER/$PART" ]]; then
		LINK=(--link-dest="$OTHER/$PART")
	fi

	rsync -a --delete "${LINK[@]}" "$PROJECT_DIR/$PART/" "$COPY_DIR/$PART/"
done

rsync -a "$PROJECT_DIR/Ashmarket.uproject" "$COPY_DIR/"
rsync -a --delete "$PROJECT_DIR/Saved/Config/" "$COPY_DIR/Saved/Config/"

# one run on disk for both
mkdir -p "$PROJECT_DIR/Saved/SaveGames"
ln -sfn "$PROJECT_DIR/Saved/SaveGames" "$COPY_DIR/Saved/SaveGames"

date '+%Y-%m-%d %H:%M %Z' >"$COPY_DIR/FROZEN_AT"
git -C "$PROJECT_DIR" rev-parse --short HEAD >>"$COPY_DIR/FROZEN_AT" 2>/dev/null || true
echo "Frozen at $(head -1 "$COPY_DIR/FROZEN_AT") in $COPY_DIR"
