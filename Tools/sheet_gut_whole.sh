#!/usr/bin/env bash
# One sheet a body from the whole-figure pictures of Ashmarket.Fit.Cuts -GutCutsWhole: every stance, from in front and
# from behind, side by side.   Tools/sheet_gut_whole.sh [pictures folder] [sheets folder]
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FROM="${1:-$PROJECT_DIR/for-review/screenshots/whole}"
TO="${2:-$PROJECT_DIR/Saved/GutWholeSheets}"

mkdir -p "$TO"

for WHO in $(ls "$FROM" | sed -n 's/^whole_\(.*\)_[a-z_]*_[ab]\.png$/\1/p' | sed 's/_\(stand\|stand_f\|shift\|crossed\|hip\|hips\|clasp\|watch\|tied\|walk\|jog\|guard\|club\|machete\).*$//' | sort -u); do
	INPUTS=()
	COUNT=0

	for PICTURE in "$FROM"/whole_"${WHO}"_*.png; do
		INPUTS+=(-i "$PICTURE")
		COUNT=$((COUNT + 1))
	done

	COLUMNS=$(( (COUNT + 1) / 2 ))

	FILTER=""
	for ((i = 0; i < COUNT; i++)); do FILTER+="[$i]crop=iw*0.62:ih*0.9:iw*0.19:ih*0.05,scale=186:-1[v$i];"; done
	for ((i = 0; i < COUNT; i++)); do FILTER+="[v$i]"; done
	FILTER+="xstack=grid=${COLUMNS}x2"

	ffmpeg -y -loglevel error "${INPUTS[@]}" -filter_complex "$FILTER" -frames:v 1 "$TO/$WHO.png" || echo "no sheet for $WHO"
done

ls "$TO" | wc -l
