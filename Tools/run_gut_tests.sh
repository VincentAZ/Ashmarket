#!/usr/bin/env bash
# Runs the Gut automation tests (Ashmarket.Gut.*) in a game world and prints the results.
#
#   Tools/run_gut_tests.sh            no renderer, fastest
#   Tools/run_gut_tests.sh --render   renders off screen and saves screenshots to for-review/screenshots
#
# Lots at the Block are bought at the book's price here (-GutNoBidding), as the tests of buying were written:
# Ashmarket.Town.Bidding turns bidding on for itself.
# Set GUT_ARGS to pass more to the engine, for example GUT_ARGS=-GutNoBodies to fight as the placeholder skeletons.
# Set GUT_CVARS to set renderer variables for a rendered run, for example GUT_CVARS=r.Lumen.DiffuseIndirect.Allow=0.
# Set GUT_SOUND=1 to have sound in a rendered run (worked out with the game, not played aloud).
# Set GUT_RESX and GUT_RESY for a picture of another shape, for example 2560 and 720 for a 32:9 screen.
# Set GUT_TESTS to run only some of them, for example GUT_TESTS=Ashmarket.Gut.Hazard for the three hazard tests.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

RENDER_ARGS="-nullrhi"

if [[ "${1:-}" == "--render" ]]; then
	# every frame counts as a sixtieth of a second however long it took to draw, so a busy graphics card
	# slows the run down without changing what happens in it
	# strand hair stalls the off-screen renderer in this engine build and simulated hair flies apart in it,
	# so hair is drawn here as cards that do not move
	RENDER_ARGS="-RenderOffscreen -windowed -ResX=${GUT_RESX:-1600} -ResY=${GUT_RESY:-900} -DPCVars=r.HairStrands.Strands=0,r.HairStrands.Simulation=0${GUT_CVARS:+,$GUT_CVARS} -UseFixedTimeStep -FPS=60"
fi

# no sound, unless GUT_SOUND=1: then the sound is worked out frame by frame with the game, not played, so that a run
# slower than life still sounds as it would at full speed, and a film of it can be given its sound
SOUND_ARGS="-nosound"

if [[ -n "${GUT_SOUND:-}" ]]; then
	SOUND_ARGS="-DeterministicAudio"
fi

# GUT_LOG names another log, so that a rendered run and a plain one can go on at the same time
LOG="${GUT_LOG:-$PROJECT_DIR/Saved/Logs/gut_tests.log}"

# what the process itself printed, kept in case it dies without finishing its log
OUT="${LOG%.log}.out"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" /Game/Gut/Lvl_Gut -game $RENDER_ARGS \
	-unattended -nosplash $SOUND_ARGS -nopause -forcelogflush -GutNoSave -GutNoBidding -GutNoNeeds ${GUT_ARGS:-} \
	-ExecCmds="DisableAllScreenMessages,Automation RunTests ${GUT_TESTS:-Ashmarket.Gut+Ashmarket.Town}; Quit" -testexit="Automation Test Queue Empty" \
	-abslog="$LOG" >"$OUT" 2>&1

EXIT_CODE=$?

# show what happened in the cage and what the tests made of it
grep -E "LogGut:|LogAutomationController: (Error|Warning|Display: Test Completed)|Test (Started|Completed)|LogAutomationCommandLine: Display: .*(Found|Running)|TEST COMPLETE|EXIT CODE" "$LOG" | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]//'

# the process exit code is the verdict: 0 only if every test passed. The engine exits abruptly once the tests are done
# and can lose the last lines of its log on the way out, so the log's closing line cannot be relied on.
if [[ $EXIT_CODE -eq 0 ]] && grep -q "Result={Success}" "$LOG" && ! grep -qE "Result=\{(Fail|Error)" "$LOG"; then
	echo "Gut tests passed."
else
	echo "Gut tests FAILED (process exit code $EXIT_CODE). See $LOG and $OUT" >&2
	exit 1
fi
