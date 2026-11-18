#!/usr/bin/env bash
# Opens the project in the Unreal editor. The cage level loads on startup: press Play to fight.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}"

exec "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_DIR/Ashmarket.uproject" "$@"
