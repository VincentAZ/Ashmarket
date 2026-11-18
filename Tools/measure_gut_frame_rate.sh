#!/usr/bin/env bash
# Measures the frame rate of the cage off screen, in real time, at 1920 x 1080: the camera walk of Ashmarket.Look with two fighters
# in the pool and a crowd of the size given on the deck. Nothing opens on the screen. Only worth reading with the graphics card free.
#
#   Tools/measure_gut_frame_rate.sh [crowd size ...]        for example: Tools/measure_gut_frame_rate.sh 14 42
#   GUT_SOUND=1 Tools/measure_gut_frame_rate.sh 42          with the sound worked out as well (not played aloud)
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

SOUND_ARGS="-nosound"
[[ -n "${GUT_SOUND:-}" ]] && SOUND_ARGS="-NonRealtimeAudio"

for CROWD in "${@:-14 42}"; do
	for SIZE in $CROWD; do
		LOG="$PROJECT_DIR/Saved/Logs/gut_frame_rate_$SIZE.log"

		"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" /Game/Gut/Lvl_Gut -game \
			-RenderOffscreen -windowed -ResX=1920 -ResY=1080 -DPCVars=r.HairStrands.Strands=0,r.HairStrands.Simulation=0 \
			-unattended -nosplash $SOUND_ARGS -nopause -forcelogflush -GutNoSave -GutFrameRate -GutLookLinger=22 -GutCrowd=$SIZE ${GUT_ARGS:-} \
			-ExecCmds="DisableAllScreenMessages,Automation RunTests Ashmarket.Look; Quit" -testexit="Automation Test Queue Empty" \
			-abslog="$LOG" >/dev/null 2>&1

		echo "crowd of $SIZE: $(grep -E "Frame rate" "$LOG" | sed -E 's/^.*Frame rate/Frame rate/' | tr '\n' ' ')"
	done
done
