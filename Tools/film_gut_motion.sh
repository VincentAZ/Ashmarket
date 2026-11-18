#!/usr/bin/env bash
# Plays motion clips on the player's fighter, off screen, and saves each as for-review/motion/<clip>.mp4
# with a sheet of stills beside it, <clip>.jpg. Nothing opens on the screen.
#
#   Tools/film_gut_motion.sh <clip>[+<clip>...] [weapon]        for example: Tools/film_gut_motion.sh club_swing+club_overhead Club
#   GUT_ARGS="-GutMotionLoops=3 -GutMotionAlone -GutMotionFront" for loops, for no opponent in the picture, for a view from in front
#   GUT_MOTION_OUT=Saved/GutMotionSheets                        somewhere other than for-review/motion
#   GUT_MOTION_GALLERY=stances                                  also one still from the middle of every clip, side by side, as <name>.jpg
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FRAMES="$PROJECT_DIR/Saved/GutMotion"
OUT="$PROJECT_DIR/${GUT_MOTION_OUT:-for-review/motion}"

rm -rf "$FRAMES"
mkdir -p "$OUT"

ARGS="-GutMotion=$1"
[[ -n "${2:-}" ]] && ARGS="$ARGS -GutLookWeapon=$2"

GUT_ARGS="$ARGS ${GUT_ARGS:-}" GUT_TESTS=Ashmarket.Film.Motion "$PROJECT_DIR/Tools/run_gut_tests.sh" --render | tail -n 1
grep -F "[motion]" "$PROJECT_DIR/Saved/Logs/gut_tests.log" | sed -E "s/^.*\[motion\] //" || true

for folder in "$FRAMES"/*/; do
	clip="$(basename "$folder")"
	count="$(ls "$folder" | wc -l)"

	ffmpeg -y -loglevel error -framerate 30 -i "$folder/frame_%04d.png" -c:v libx264 -crf 20 -pix_fmt yuv420p "$OUT/$clip.mp4"

	# twenty stills at most, evenly through the clip, the middle of the picture only
	step=$(( (count + 19) / 20 ))
	ffmpeg -y -loglevel error -framerate 30 -i "$folder/frame_%04d.png" \
		-vf "select='not(mod(n\,$step))',crop=iw*0.46:ih:iw*0.27:0,scale=-1:420,tile=10x2" -frames:v 1 -q:v 3 "$OUT/$clip.jpg"

	echo "$clip: $count frames -> ${OUT#$PROJECT_DIR/}/$clip.mp4 and .jpg (a still every $step frames)"
done

if [[ -n "${GUT_MOTION_GALLERY:-}" ]]; then
	inputs=()
	filter=""
	n=0

	for folder in "$FRAMES"/*/; do
		clip="$(basename "$folder")"
		count="$(ls "$folder" | wc -l)"
		middle="$(printf "%s/frame_%04d.png" "$folder" $(( count / 2 )))"

		inputs+=(-i "$middle")
		filter+="[$n:v]crop=iw*0.46:ih:iw*0.27:0,scale=-1:640,drawtext=text='$clip':x=8:y=8:fontsize=26:fontcolor=white:box=1:boxcolor=black@0.6[v$n];"
		n=$(( n + 1 ))
	done

	for (( i = 0; i < n; i++ )); do filter+="[v$i]"; done

	ffmpeg -y -loglevel error "${inputs[@]}" -filter_complex "${filter}hstack=inputs=$n" -frames:v 1 -q:v 3 "$OUT/$GUT_MOTION_GALLERY.jpg"
	echo "gallery of $n -> ${OUT#$PROJECT_DIR/}/$GUT_MOTION_GALLERY.jpg"
fi
