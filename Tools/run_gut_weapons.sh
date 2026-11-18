#!/usr/bin/env bash
# Plays every pair of rack weapons against each other, the opponent brain on both sides, and prints how the fights went
# and how they were fought (Ashmarket.Balance.Weapons.Pair). Each pair runs in a process of its own, all at the same time,
# with no renderer on a fixed 60 Hz timestep and no frame limit.
#
#   Tools/run_gut_weapons.sh        40 fights of every pair
#   Tools/run_gut_weapons.sh 100    100 fights of every pair
#
# Set GUT_TUNE or GUT_HEALTH to try other opponent tuning or health, as with Tools/run_gut_balance.sh.
# Set GUT_HAZARDS=0 to fight in a cage whose hazards never switch on.
# Set GUT_WEAPON_LIST to the weapons to pair up, if the rack changes. Each is any part of a weapon asset's name.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

FIGHTS="${1:-40}"

read -r -a WEAPONS <<< "${GUT_WEAPON_LIST:-Club Machete Chain Shield}"

LOG_DIR="$PROJECT_DIR/Saved/Logs/gut_weapons"

rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"

# every pair once, each weapon against itself included. With the same brain on both sides, A against B is B against A.
PIDS=()
PAIRS=()

for ((i = 0; i < ${#WEAPONS[@]}; i++)); do
	for ((j = i; j < ${#WEAPONS[@]}; j++)); do
		PAIR="${WEAPONS[i]},${WEAPONS[j]}"
		# numbered, so the table lists the pairs in the order the weapons were given
		NAME="$(printf '%02d' ${#PIDS[@]})_${WEAPONS[i]}_${WEAPONS[j]}"

		"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/Ashmarket.uproject" /Game/Gut/Lvl_Gut -game -nullrhi \
			-benchmark -fps=60 -GutBalanceFights="$FIGHTS" -GutWeapons="$PAIR" \
			${GUT_TUNE:+-GutTune="$GUT_TUNE"} ${GUT_HEALTH:+-GutHealth="$GUT_HEALTH"} $([[ "${GUT_HAZARDS:-1}" == "0" ]] && echo -GutNoHazards) \
			-unattended -nosplash -nosound -nopause -forcelogflush -GutNoSave ${GUT_ARGS:-} \
			-ExecCmds="Automation RunTests Ashmarket.Balance.Weapons.Pair; Quit" -testexit="Automation Test Queue Empty" \
			-abslog="$LOG_DIR/$NAME.log" >"$LOG_DIR/$NAME.out" 2>&1 &

		PIDS+=($!)
		PAIRS+=("$NAME")
	done
done

# the process exit code is the verdict for each pair, as in Tools/run_gut_balance.sh
FAILED=0

for ((k = 0; k < ${#PIDS[@]}; k++)); do
	if ! wait "${PIDS[k]}"; then
		echo "${PAIRS[k]}: the run failed or a fight never ended. See $LOG_DIR/${PAIRS[k]}.log" >&2
		FAILED=1
	fi
done

python3 "$PROJECT_DIR/Tools/gut_weapon_table.py" "$LOG_DIR"

exit $FAILED
