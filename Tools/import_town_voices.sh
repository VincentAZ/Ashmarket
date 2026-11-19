#!/usr/bin/env bash
# Brings what the town's people say (ArtSource/voices, made by Tools/make_town_voices.sh) into Content/Town/Voices:
# each MP3 is first turned into a WAV the engine reads, one voice to a channel, then import_town_voices.py is run in an
# editor with no window. Tools/build_town_level.sh then gives each line and call its recording.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

LOG="$PROJECT_DIR/Saved/Logs/import_town_voices.log"

VOICES="$PROJECT_DIR/ArtSource/voices"

if ! compgen -G "$VOICES/*/*.mp3" >/dev/null; then
	echo "There are no recordings in $VOICES yet: run Tools/make_town_voices.sh" >&2
	exit 1
fi

for recording in "$VOICES"/*/*.mp3; do
	place="$(basename "$(dirname "$recording")")"

	[[ "$place" == "wav" ]] && continue

	mkdir -p "$VOICES/wav/$place"

	wav="$VOICES/wav/$place/$(basename "${recording%.mp3}").wav"

	# only what is new, or has been made again
	if [[ ! -f "$wav" || "$recording" -nt "$wav" ]]; then
		ffmpeg -v error -y -i "$recording" -ac 1 -ar 44100 -c:a pcm_s16le "$wav"
	fi
done

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" \
	-ExecutePythonScript="$PROJECT_DIR/Tools/import_town_voices.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$LOG" >/dev/null 2>&1 || true

grep -E "\[import_town_voices\]" "$LOG" | sed -E "s/^\[[^]]*\]\[[ 0-9]*\]//" | cut -c1-400 || true

if grep -q "\[import_town_voices\] DONE" "$LOG"; then
	echo "Voices imported."
else
	echo "Import failed. See $LOG" >&2
	exit 1
fi
