#!/usr/bin/env bash
# Everything a review needs, from the build as it stands, in one go and with nothing on the screen:
# every test with no window, every test rendered (which redraws for-review/screenshots), every film in for-review
# that shows a fight, the ten finishers, and the short film cut from them. About three quarters of an hour.
#
#   Tools/make_gut_review.sh                 all of it
#   Tools/make_gut_review.sh films           only the films and the short film
#   Tools/make_gut_review.sh tests           only the two test runs
#
# The films are made from two frozen copies of the build on the disk (Tools/freeze_gut_build.sh), one film at a time in
# each, while the tests run in the project itself: three copies of the game drawing at once, about 30 GB of memory.
# It will not start with less than 45 GB free. What each part came to is printed at the end and kept in
# Saved/Logs/gut_review.txt. A film made again is a different fight: look at it before describing it.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WHAT="${1:-all}"
REPORT="$PROJECT_DIR/Saved/Logs/gut_review.txt"

FREE="$(free -g | awk '/^Mem:/ { print $7 }')"

if (( FREE < 45 )); then
	echo "Only $FREE GB of memory is free; this wants 45. Not started." >&2
	exit 1
fi

if pgrep -f "Ashmarket.uprojec[t]" >/dev/null; then
	echo "A copy of the game is already running. Not started." >&2
	exit 1
fi

mkdir -p "$PROJECT_DIR/Saved/Logs"
: > "$REPORT"

say() { echo "$*" | tee -a "$REPORT"; }

tests() {
	local kept="$PROJECT_DIR/Saved/Logs/gut_review_tests"

	"$PROJECT_DIR/Tools/run_gut_tests.sh" 2>&1 | grep -E "Test Completed|Gut tests" > "${kept}_no_window.txt"
	say "tests, no window: $(grep -c 'Result={Success}' "${kept}_no_window.txt") passed, $(grep -c 'Result={Fail' "${kept}_no_window.txt") failed ($(date +%H:%M))"
	grep 'Result={Fail' "${kept}_no_window.txt" | tee -a "$REPORT"

	"$PROJECT_DIR/Tools/run_gut_tests.sh" --render 2>&1 | grep -E "Test Completed|Gut tests" > "${kept}_rendered.txt"
	say "tests, rendered: $(grep -c 'Result={Success}' "${kept}_rendered.txt") passed, $(grep -c 'Result={Fail' "${kept}_rendered.txt") failed ($(date +%H:%M))"
	grep 'Result={Fail' "${kept}_rendered.txt" | tee -a "$REPORT"
}

# one lane of films, from its own frozen copy: lane <copy> <film> ...
lane() {
	local copy="$1"; shift
	local sim

	sim="$("$PROJECT_DIR/Tools/freeze_gut_build.sh" "$copy")" || return 1

	(
		source "$PROJECT_DIR/Tools/freeze_gut_build.sh" --env
		cd "$sim" || exit 1

		film() { # name, switches, then the weapons
			local name="$1" switches="$2"; shift 2

			GUT_ARGS="$switches" "$sim/Tools/film_gut_fight.sh" "$name" "$@" >/dev/null 2>&1

			if [[ -s "for-review/$name.mp4" ]]; then
				cp -f "for-review/$name.mp4" "$PROJECT_DIR/for-review/"
				say "film $name: $(ffprobe -v error -show_entries format=duration -of csv=p=0 "for-review/$name.mp4" | cut -d. -f1) s. $(grep -ah 'Fight decided\|is spared' Saved/Logs/gut_tests.log | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]LogGut: //' | head -n 1) $(grep -ah 'is dead, killed by' Saved/Logs/gut_tests.log | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]LogGut: //' | tail -n 1)"
			else
				say "film $name: NOT MADE"
			fi

			rm -rf Saved/GutFilm "for-review/$name.mp4"
		}

		for which in "$@"; do
			case "$which" in
				chain) film fight_chain_v_club_last "" Chain Club ;;
				shield) film fight_shield_v_machete_last "" Shield Machete ;;
				fists) film fight_fists_v_fists_last "" None None ;;
				spared) film fight_spared "-GutSpare -GutFilmPlayerHealth=45" Machete Machete ;;
				two) film fight_two_on_two "-GutFilmSpecial=2" Club ;;
				three) GUT_FILM_SECONDS=210 film fight_one_against_three "-GutFilmSpecial=3" Machete ;;
				dogs) film fight_against_the_dogs "-GutFilmSpecial=4" Club ;;
				lost) film fight_lost_to_the_dogs "-GutFilmSpecial=4 -GutBeast=160,2.2" Club ;;
				heel) film fight_with_a_dog_at_heel "-GutFilmSpecial=5 -GutRungDogs=1" Machete Club ;;
				hazards) GUT_FILM_SECONDS=120 film fight_everything_the_pit_boss_has "-GutRoundSeconds=12 -GutFilmHealth=330" Club Machete ;;
				finishers)
					"$sim/Tools/film_gut_finishers.sh" >/dev/null 2>&1
					cp -f for-review/finishers/* "$PROJECT_DIR/for-review/finishers/" 2>/dev/null
					say "finishers: $(ls for-review/finishers/*.mp4 2>/dev/null | wc -l) of 10"
					rm -rf Saved/GutMotion for-review/finishers/*
					;;
			esac
		done
	)

	rm -rf "$sim"
}

say "review made from the build of $(date -r "$PROJECT_DIR/Binaries/Linux/libUnrealEditor-Ashmarket.so" '+%b %d %H:%M'), begun $(date '+%b %d %H:%M')"

if [[ "$WHAT" == "all" || "$WHAT" == "tests" ]]; then
	tests &
fi

if [[ "$WHAT" == "all" || "$WHAT" == "films" ]]; then
	lane review_a chain shield fists spared two three &
	lane review_b dogs lost heel hazards finishers &
fi

wait

if [[ "$WHAT" == "all" || "$WHAT" == "films" ]]; then
	say "short film: $(python3 "$PROJECT_DIR/Tools/make_gut_reel.py" 2>&1 | tail -n 1)"
fi

say "done $(date +%H:%M)"
