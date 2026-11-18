#!/usr/bin/env bash
# Compiles the game's C++ module for the editor. Run after changing anything under Source/, with the editor closed.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

exec "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" AshmarketEditor Linux Development -project="$PROJECT_DIR/Ashmarket.uproject" -waitmutex "$@"
