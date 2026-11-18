#!/usr/bin/env bash
# Films one fight between two fighters with the opponent's brain, off screen, and saves it to for-review/<name>.mp4.
# Nothing opens on the screen. The film is 30 frames a second of game time however long the machine takes to draw it.
#
#   Tools/film_gut_fight.sh [name] [player's weapon] [opponent's weapon]      for example: Tools/film_gut_fight.sh fight_chain Chain Shield
#   GUT_FILM_SECONDS=40 Tools/film_gut_fight.sh                               stop after that many seconds if nobody is dead
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:-fight}"
FRAMES="$PROJECT_DIR/Saved/GutFilm"

rm -rf "$FRAMES"

ARGS="-GutFilmSeconds=${GUT_FILM_SECONDS:-120}"
[[ -n "${2:-}" ]] && ARGS="$ARGS -GutLookWeapon=$2"
[[ -n "${3:-}" ]] && ARGS="$ARGS -GutLookEnemyWeapon=$3"

# with sound unless GUT_FILM_SILENT=1: the game works its sound out frame by frame and the film is given it
if [[ -z "${GUT_FILM_SILENT:-}" ]]; then
	ARGS="$ARGS -GutFilmSound"
	export GUT_SOUND=1
fi

GUT_ARGS="$ARGS ${GUT_ARGS:-}" GUT_TESTS=Ashmarket.Film.Fight "$PROJECT_DIR/Tools/run_gut_tests.sh" --render | tail -n 1

if [[ -f "$FRAMES/sound.wav" ]]; then
	ffmpeg -y -loglevel error -framerate 30 -i "$FRAMES/frame_%05d.png" -i "$FRAMES/sound.wav" -c:v libx264 -crf 20 -pix_fmt yuv420p -c:a aac -b:a 192k -shortest "$PROJECT_DIR/for-review/$NAME.mp4"
	echo "$(ls "$FRAMES" | grep -c png) frames with sound -> for-review/$NAME.mp4"
else
	ffmpeg -y -loglevel error -framerate 30 -i "$FRAMES/frame_%05d.png" -c:v libx264 -crf 20 -pix_fmt yuv420p "$PROJECT_DIR/for-review/$NAME.mp4"
	echo "$(ls "$FRAMES" | grep -c png) frames, no sound -> for-review/$NAME.mp4"
fi
