#!/usr/bin/env bash
# Plays many fights against the opponent brain and prints how they went (Ashmarket.Balance.*).
# Runs with no renderer on a fixed 60 Hz timestep and no frame limit, so a 75 second fight takes a couple of seconds.
#
#   Tools/run_gut_balance.sh                 40 fights of every matchup
#   Tools/run_gut_balance.sh 200             200 fights of every matchup
#   Tools/run_gut_balance.sh 200 Mirror      200 fights of one matchup (Mirror, Masher or Sharp)
#   Tools/run_gut_balance.sh 50 Ladder       50 runs at the Meat arc by a stand-in for the player, from the first fight each time
#   GUT_FULL_LADDER=1 Tools/run_gut_balance.sh 50 Ladder     the same, on up through Dog, Butcher and Champion
#   GUT_FULL_LADDER=1 GUT_RETRY=1 Tools/run_gut_balance.sh 5 Ladder     ...fighting a lost fight again, so every run reaches the top
#   Tools/run_gut_balance.sh 4 Window        4 nights at the betting window for each of five stand-in gamblers, three times each
#   Tools/run_gut_balance.sh 30 Stable       30 days in the yard for each of four stand-in owners, three times each
#   Tools/run_gut_balance.sh 40 "" --fights  also list every fight
#
# Set GUT_TUNE to try opponent tuning without touching the Blueprint, for example
#   GUT_TUNE=AttackChance=0.4,MaxDecisionInterval=1.8 Tools/run_gut_balance.sh 100 Mirror
# Set GUT_HEALTH to try a different amount of health for both fighters, for example GUT_HEALTH=120.
# Set GUT_WEAPONS to the player's weapon and the opponent's, for example GUT_WEAPONS=Machete,Shield. Without it both hold clubs.
# Set GUT_HAZARDS=0 to fight in a cage whose hazards never switch on, as fights were before milestone 5.
# Set GUT_ARGS for anything else the game reads from its command line: -GutDodge=380 (how far a dodge goes, cm),
# -GutMove=2048,8,2000 (push-off, grip and braking).
# Tools/run_gut_weapons.sh plays every pair of weapons against each other.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

FIGHTS="${1:-40}"
FILTER="Ashmarket.Balance.Opponent${2:+.$2}"
SHOW="(SUMMARY|FIGHT [0-9]+:)"

if [[ "${2:-}" == "Ladder" ]]; then
	FILTER="Ashmarket.Balance.Ladder"
fi

# the number is nights at the betting window, for each of five kinds of gambler, three runs each
if [[ "${2:-}" == "Window" ]]; then
	FILTER="Ashmarket.Balance.Window"
fi

# the number is days in the yard, for each of four kinds of owner, three runs each
if [[ "${2:-}" == "Stable" ]]; then
	FILTER="Ashmarket.Balance.Stable"
fi

if [[ "${3:-}" == "--fights" ]]; then
	SHOW=""
fi

# a log of its own for each matchup and choice of weapons, so several can run at the same time.
# Set GUT_TAG to run the same thing more than once at the same time.
LOG="$PROJECT_DIR/Saved/Logs/gut_balance${2:+_$2}${GUT_WEAPONS:+_${GUT_WEAPONS/,/_}}$([[ "${GUT_HAZARDS:-1}" == "0" ]] && echo _nohazards)${GUT_TAG:+_$GUT_TAG}.log"

# what the process itself printed, kept in case it dies without finishing its log
OUT="${LOG%.log}.out"

"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" /Game/Gut/Lvl_Gut -game -nullrhi \
	-benchmark -fps=60 -GutBalanceFights="$FIGHTS" ${GUT_TUNE:+-GutTune="$GUT_TUNE"} ${GUT_HEALTH:+-GutHealth="$GUT_HEALTH"} \
	${GUT_WEAPONS:+-GutWeapons="$GUT_WEAPONS"} $([[ "${GUT_HAZARDS:-1}" == "0" ]] && echo -GutNoHazards) $([[ "${GUT_FULL_LADDER:-0}" == "1" ]] && echo -GutFullLadder) $([[ "${GUT_RETRY:-0}" == "1" ]] && echo -GutRetry) \
	-unattended -nosplash -nosound -nopause -forcelogflush -GutNoSave ${GUT_ARGS:-} \
	-ExecCmds="Automation RunTests $FILTER; Quit" -testexit="Automation Test Queue Empty" \
	-abslog="$LOG" >"$OUT" 2>&1

EXIT_CODE=$?

grep -E "LogGut: Display: BALANCE .*$SHOW|LogAutomationController: (Error|Warning|Display: Test Completed)" "$LOG" | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]//; s/^LogGut: Display: //'

# the process exit code is the verdict: 0 only if every matchup met its targets. The engine exits abruptly once the tests
# are done and can lose the last lines of its log on the way out, so the log's closing line cannot be relied on.
if [[ $EXIT_CODE -eq 0 ]] && grep -q "Result={Success}" "$LOG" && ! grep -qE "Result=\{(Fail|Error)" "$LOG"; then
	echo "Balance targets met."
else
	echo "Balance targets NOT met, or the run failed (process exit code $EXIT_CODE). See $LOG and $OUT" >&2
	exit 1
fi
