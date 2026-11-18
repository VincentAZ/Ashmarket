#!/usr/bin/env bash
# Painted wall markings come out of Mixar already cut out, but where the paint has chipped the picture shows white:
# on the pool that has to be the tile underneath. This makes the near-white inside each cut-out see-through as well.
#
#   Tools/key_marking.sh <name> [<name> ...]     reads ArtSource/mixar/markings/<name>.png, writes .../keyed/<name>.png
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ArtSource/mixar/markings"
mkdir -p "$HERE/keyed"

for NAME in "$@"; do
	ffmpeg -y -loglevel error -i "$HERE/$NAME.png" \
		-vf "format=rgba,geq=r='r(X,Y)':g='g(X,Y)':b='b(X,Y)':a='alpha(X,Y)*clip((225-min(r(X,Y),min(g(X,Y),b(X,Y))))/60,0,1)'" \
		"$HERE/keyed/$NAME.png"
	echo "keyed $NAME"
done
