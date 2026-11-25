#!/usr/bin/env bash
# Starts the cage fight as a standalone game window, without opening the editor.
#
#   Tools/play_gut.sh                              Pool Street, outside the cage: walk it, talk, and go in to fight, to own or to bet
#   GUT_MODE=square Tools/play_gut.sh              Block Square on a sale night, which is otherwise a walk west along Pool Street and down the lane
#   GUT_MODE=main Tools/play_gut.sh                Main Street, which is otherwise a walk south out of Block Square
#   GUT_MODE=row Tools/play_gut.sh                 Rotgut Row, which is otherwise a walk west off the end of Pool Street
#   GUT_RANK=butcher GUT_MODE=main Tools/play_gut.sh   the same as a Butcher holding 60 L, to buy armor at Hide & Plate: a fighter made up
#                                                  for the sitting, with nothing read from or written to the run kept on disk
#   GUT_MODE=card Tools/play_gut.sh                the street as the card it was before it was a place, with the ways in listed on it
#   GUT_MODE=ladder Tools/play_gut.sh              straight in as a fighter: the Meat arc, or wherever you had got to (runs are kept on disk)
#   GUT_NEW=1 GUT_MODE=ladder Tools/play_gut.sh    forget the fighter's run on disk and start from the first fight
#   GUT_MODE=sandbox Tools/play_gut.sh             single fights with no ladder; the opponent draws a weapon at random
#   GUT_MODE=window Tools/play_gut.sh              come to the cage to bet: watch the night's fights from the wall, with water on them
#   GUT_MODE=stable Tools/play_gut.sh              come to the cage as an owner: buy or sign fighters, train them, send them in
#   GUT_WEAPONS=Chain,Shield Tools/play_gut.sh     start at the rack with the chain drawn for you and, in a single fight, the shield for the opponent
#   GUT_HAZARDS=0 Tools/play_gut.sh                the spikes and the cable never switch on
#
# Either way the rack lets you change both weapons before the fight. Anything else on the command line goes to the engine.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

MAP="/Game/Gut/Lvl_Gut"

# the level takes the two weapons as options. Each is any part of a weapon asset's name.
if [[ -n "${GUT_WEAPONS:-}" ]]; then
	MAP="$MAP?PlayerWeapon=${GUT_WEAPONS%%,*}?EnemyWeapon=${GUT_WEAPONS##*,}"
fi

# with nothing asked for, the street outside. Naming weapons or a new run is asking for the ladder, as it always was.
if [[ -z "${GUT_MODE:-}" && -z "${GUT_WEAPONS:-}" && "${GUT_NEW:-0}" != "1" && -f "$PROJECT_DIR/Content/Town/Lvl_PoolStreet.umap" ]]; then
	# the street is a place of its own now, with its own level
	MAP="/Game/Town/Lvl_PoolStreet"
elif [[ "${GUT_MODE:-}" == "square" && -f "$PROJECT_DIR/Content/Town/Lvl_BlockSquare.umap" ]]; then
	MAP="/Game/Town/Lvl_BlockSquare"
elif [[ "${GUT_MODE:-}" == "main" && -f "$PROJECT_DIR/Content/Town/Lvl_MainStreet.umap" ]]; then
	MAP="/Game/Town/Lvl_MainStreet"
elif [[ "${GUT_MODE:-}" == "row" && -f "$PROJECT_DIR/Content/Town/Lvl_RotgutRow.umap" ]]; then
	MAP="/Game/Town/Lvl_RotgutRow"
elif [[ "${GUT_MODE:-}" == "card" || ( -z "${GUT_MODE:-}" && -z "${GUT_WEAPONS:-}" && "${GUT_NEW:-0}" != "1" ) ]]; then
	MAP="$MAP?Door=1"
elif [[ "${GUT_MODE:-}" == "window" ]]; then
	MAP="$MAP?Window=1"
elif [[ "${GUT_MODE:-}" == "stable" ]]; then
	MAP="$MAP?Stable=1"
elif [[ "${GUT_MODE:-}" == "sandbox" ]]; then
	MAP="$MAP?Sandbox=1"
fi

if [[ "${GUT_NEW:-0}" == "1" ]]; then
	MAP="$MAP?NewRun=1"
fi

if [[ "${GUT_HAZARDS:-1}" == "0" ]]; then
	MAP="$MAP?Hazards=0"
fi

# a fighter of a given rank, made up for this sitting: the run on disk is neither read nor written
RANK_ARGS=()

case "${GUT_RANK:-}" in
	"") ;;
	meat) RANK_ARGS=(-GutNoSave -GutRank=0 -GutWater="${GUT_WATER:-60}") ;;
	dog) RANK_ARGS=(-GutNoSave -GutRank=1 -GutWater="${GUT_WATER:-60}") ;;
	butcher) RANK_ARGS=(-GutNoSave -GutRank=2 -GutWater="${GUT_WATER:-60}") ;;
	champion) RANK_ARGS=(-GutNoSave -GutRank=3 -GutWater="${GUT_WATER:-60}") ;;
	*) echo "GUT_RANK is meat, dog, butcher or champion" >&2; exit 1 ;;
esac

exec "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_DIR/Ashmarket.uproject" "$MAP" -game -windowed -ResX=1920 -ResY=1080 "${RANK_ARGS[@]}" "$@"
