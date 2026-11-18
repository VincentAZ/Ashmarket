#!/usr/bin/env bash
# Stages finishers off screen, one after another, and saves each as for-review/finishers/<weapon>_<style>.mp4 with a sheet of
# stills beside it: from the opponent swaying in its stagger, through the finisher, to the body on the floor.
# Nothing opens on the screen.
#
#   Tools/film_gut_finishers.sh                              all ten: bare hands and the four weapons, clean and slow
#   Tools/film_gut_finishers.sh Chain:Slow+Machete:Clean     only these (a rack weapon or None, and Clean or Slow)
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FRAMES="$PROJECT_DIR/Saved/GutMotion"
OUT="$PROJECT_DIR/${GUT_FINISHER_OUT:-for-review/finishers}"

rm -rf "$FRAMES"
mkdir -p "$OUT"

ARGS=""
[[ -n "${1:-}" ]] && ARGS="-GutFinishers=$1"

GUT_ARGS="$ARGS ${GUT_ARGS:-}" GUT_TESTS=Ashmarket.Film.Finishers "$PROJECT_DIR/Tools/run_gut_tests.sh" --render | tail -n 1
grep -F "[finisher]" "$PROJECT_DIR/Saved/Logs/gut_tests.log" | sed -E "s/^.*\[finisher\] //" || true

for folder in "$FRAMES"/*/; do
	[[ -d "$folder" ]] || continue

	clip="$(basename "$folder")"
	count="$(ls "$folder" | wc -l)"

	ffmpeg -y -loglevel error -framerate 30 -i "$folder/frame_%04d.png" -c:v libx264 -crf 20 -pix_fmt yuv420p "$OUT/$clip.mp4"

	# twenty stills at most, evenly through it, the middle of the picture only
	step=$(( (count + 19) / 20 ))
	ffmpeg -y -loglevel error -framerate 30 -i "$folder/frame_%04d.png" \
		-vf "select='not(mod(n\,$step))',crop=iw*0.56:ih:iw*0.22:0,scale=-1:420,tile=10x2" -frames:v 1 -q:v 3 "$OUT/$clip.jpg"

	echo "$clip: $count frames -> ${OUT#$PROJECT_DIR/}/$clip.mp4 and .jpg (a still every $step frames)"
done
