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

# Every voice is brought to one loudness, however loud it was made: a voice designed for somebody comes out some
# six decibels above the account's own. LEVEL is the mean, in decibels below full, and no peak is let above -1.
LEVEL=-23

if [[ "$(cat "$VOICES/wav/.level" 2>/dev/null || true)" != "$LEVEL" ]]; then
	rm -rf "$VOICES/wav"
fi

mkdir -p "$VOICES/wav"
echo "$LEVEL" > "$VOICES/wav/.level"

for recording in "$VOICES"/*/*.mp3; do
	place="$(basename "$(dirname "$recording")")"

	# wav is ours, and designed holds each made voice's first hearing, which nobody says in the game
	[[ "$place" == "wav" || "$place" == "designed" ]] && continue

	mkdir -p "$VOICES/wav/$place"

	wav="$VOICES/wav/$place/$(basename "${recording%.mp3}").wav"

	# only what is new, or has been made again
	if [[ ! -f "$wav" || "$recording" -nt "$wav" ]]; then
		read -r mean peak < <(ffmpeg -nostdin -i "$recording" -af volumedetect -f null - 2>&1 | awk '/mean_volume/ {m = $5} /max_volume/ {p = $5} END {print m, p}')
		gain="$(awk -v m="$mean" -v p="$peak" -v t="$LEVEL" 'BEGIN {g = t - m; if (p + g > -1) g = -1 - p; printf "%.1f", g}')"

		ffmpeg -nostdin -v error -y -i "$recording" -af "volume=${gain}dB" -ac 1 -ar 44100 -c:a pcm_s16le "$wav"
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
