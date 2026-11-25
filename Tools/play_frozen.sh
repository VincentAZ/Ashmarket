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

# The game's own module finds the engine's plugins (IKRig, since 23 November, for the step that keeps arms out of bodies)
# by a path written into it that is counted from where the PROJECT stands. From the copy that path leads nowhere, and the
# copy would not start: "The game module 'Ashmarket' could not be loaded" (25 November). The same folders are named here
# outright, read off the module itself, so that whatever plugin it comes to need later is found too.
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
MODULE="$COPY_DIR/Binaries/Linux/libUnrealEditor-Ashmarket.so"
FOUND=""

if command -v readelf >/dev/null && [[ -f "$MODULE" ]]; then
	while IFS= read -r WHERE; do
		[[ -d "$UE_ROOT/Engine/$WHERE" ]] && FOUND="$FOUND:$UE_ROOT/Engine/$WHERE"
	done < <(readelf -d "$MODULE" | grep -E "RPATH|RUNPATH" | tr ':[]' '\n\n\n' | sed -n 's#.*/Engine/\(Plugins/.*\)$#\1#p')
fi

if [[ -z "$FOUND" ]]; then
	FOUND=":$UE_ROOT/Engine/Plugins/Animation/IKRig/Binaries/Linux"
fi

export LD_LIBRARY_PATH="${FOUND#:}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "Playing the copy frozen at $(head -1 "$COPY_DIR/FROZEN_AT" 2>/dev/null || echo '?')"
exec "$COPY_DIR/Tools/play_gut.sh" "$@"
