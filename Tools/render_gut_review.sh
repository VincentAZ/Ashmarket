#!/usr/bin/env bash
# Renders an in-engine 360 of one character into for-review/turntable: the real meshes, materials and hair,
# under one of MetaHuman's lighting environments.
#
#   Tools/render_gut_review.sh <blueprint path> <name> [light] [bottom] [top]
#   Tools/render_gut_review.sh /Game/Gut/Art/MetaHumans/FighterA/BP_FighterA FighterA_face Portrait 0.8 1
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"
EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"

export GUT_REVIEW_ACTOR="$1" GUT_REVIEW_NAME="$2" GUT_REVIEW_LIGHT="${3:-Studio}" GUT_REVIEW_BOTTOM="${4:-0}" GUT_REVIEW_TOP="${5:-1}"
NAME="$2"
FRAMES="$PROJECT_DIR/Saved/Review/$NAME"
OUT="$PROJECT_DIR/for-review/turntable"
SETUP_LOG="$PROJECT_DIR/Saved/Logs/render_gut_review_setup.log"
RENDER_LOG="$PROJECT_DIR/Saved/Logs/render_gut_review.log"

rm -rf "$FRAMES"
rm -f "$PROJECT_DIR/Content/Gut/Review/"*"_Review_$NAME."*
mkdir -p "$OUT"

"$EDITOR" "$PROJECT_DIR/Ashmarket.uproject" -ExecutePythonScript="$PROJECT_DIR/Tools/render_gut_review.py" \
	-unattended -nosplash -RenderOffscreen -nosound -abslog="$SETUP_LOG" >/dev/null 2>&1
grep -E "\[render_gut_review\] (subject|preset|DONE|FAILED)" "$SETUP_LOG" | sed -E 's/^\[[^]]*\]\[[ 0-9]*\]//'
grep -q "\[render_gut_review\] DONE" "$SETUP_LOG" || { echo "Review setup failed. See $SETUP_LOG" >&2; exit 1; }

# strand hair stalls the 5.9 preview when it renders off screen, so hair is drawn as cards unless GUT_REVIEW_CVARS says otherwise
timeout "${GUT_REVIEW_TIMEOUT:-400}" "$EDITOR" "$PROJECT_DIR/Ashmarket.uproject" "/Game/Gut/Review/Lvl_Review_$NAME" -game \
	-LevelSequence="/Game/Gut/Review/Seq_Review_$NAME" -MoviePipelineConfig="/Game/Gut/Review/MRQ_Review_$NAME" \
	-RenderOffscreen -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -GutNoSave -DPCVars="${GUT_REVIEW_CVARS:-r.HairStrands.Strands=0,r.HairStrands.Simulation=0}" -abslog="$RENDER_LOG" >/dev/null 2>&1

COUNT=$(ls "$FRAMES" 2>/dev/null | wc -l)
[[ $COUNT -gt 0 ]] || { echo "Nothing was rendered. See $RENDER_LOG" >&2; exit 1; }

ffmpeg -y -loglevel error -framerate 24 -i "$FRAMES/f_%04d.png" -c:v libx264 -crf 16 -pix_fmt yuv420p -movflags +faststart "$OUT/${NAME}_360.mp4"
ffmpeg -y -loglevel error -start_number 0 -i "$FRAMES/f_%04d.png" -vf "select='not(mod(n\,18))',scale=iw/2:-1,tile=4x2" -frames:v 1 "$OUT/${NAME}_8_angles.png"
echo "$COUNT frames -> $OUT/${NAME}_360.mp4 and ${NAME}_8_angles.png"
