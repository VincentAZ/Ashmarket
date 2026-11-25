#!/usr/bin/env bash
# Every body there is, stood every way the game stands people, measured for clothes under skin and photographed whole
# from in front and from behind (Ashmarket.Fit.Cuts -GutCutsWhole). Ten bodies a run: seventy do not fit in memory.
#
#   Tools/sweep_gut_cuts.sh            all of them: Saved/cuts_all.txt, Saved/cuts_summary.txt, Saved/GutWholeSheets/<body>.png
#   Tools/sweep_gut_cuts.sh Grace Asha only those
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

if [[ $# -gt 0 ]]; then
	BODIES=("$@")
else
	mapfile -t BODIES < <(for DIR in Content/Gut/Art/MetaHumans/*/; do NAME="$(basename "$DIR")"; [[ -f "$DIR/BP_$NAME.uasset" ]] && echo "$NAME"; done)
fi

# GUT_SWEEP_MORE=1 adds to what an earlier run measured and photographed, and does not begin again
if [[ -z "${GUT_SWEEP_MORE:-}" ]]; then
	rm -rf for-review/screenshots/whole Saved/GutWholeSheets
	: > Saved/cuts_all.txt
fi

for ((i = 0; i < ${#BODIES[@]}; i += 10)); do
	BATCH="$(IFS=+; echo "${BODIES[*]:i:10}")"

	GUT_RESX=600 GUT_RESY=1000 GUT_TESTS=Ashmarket.Fit.Cuts GUT_ARGS="-GutFitBodies=$BATCH -GutCutsWhole -GutShots=whole ${GUT_SWEEP_ARGS:-}" GUT_LOG="$PROJECT_DIR/Saved/Logs/${GUT_SWEEP_LOG:-gut_cuts}.log" \
		Tools/run_gut_tests.sh --render >/dev/null 2>&1 || true

	grep "\[cuts\] " "Saved/Logs/${GUT_SWEEP_LOG:-gut_cuts}.log" | sed 's/.*\[cuts\] //' >> Saved/cuts_all.txt
	echo "measured $((i + 10 < ${#BODIES[@]} ? i + 10 : ${#BODIES[@]})) of ${#BODIES[@]}"
done

# GUT_SWEEP_LOG names the log of a run that goes on beside another (two halves of the bodies at once); such a run leaves
# the summing up to whoever started it
if [[ -z "${GUT_SWEEP_LOG:-}" ]]; then
	python3 Tools/list_gut_cuts.py > Saved/cuts_summary.txt || true
	Tools/sheet_gut_whole.sh
fi
