#!/usr/bin/env bash
# Makes a frozen copy of the built game for simulations and films to run from while the real project is rebuilt or is
# busy with something else: its own binaries, settings and tools; the real project's content and cache through links;
# its own Saved and its own for-review.
#
#   Tools/freeze_gut_build.sh <name>          prints the folder of the copy: ~/.cache/ashmarket-sim/<name>
#   source Tools/freeze_gut_build.sh --env    only sets what a copy needs to find the engine's plugins
#
# The copy goes on the DISK. /tmp on this machine is held in memory: copies kept there, with the film frames they wrote,
# ran the machine out of memory twice on Nov 14, 2026. Delete a copy when its work is done.
#
# A copy shares the real project's Content: do not save an asset into Content with a property or a class the copy's
# older build does not know while the copy is running. New assets are safe.

# the game module finds the engine's plugins by a path relative to where it was built; a copy elsewhere is told where they are
GUT_ENGINE="${UE_ROOT:-$HOME/UnrealEngine/UE_5.9}/Engine"
export LD_LIBRARY_PATH="$GUT_ENGINE/Binaries/Linux:$GUT_ENGINE/Plugins/EnhancedInput/Binaries/Linux:$GUT_ENGINE/Plugins/Runtime/StateTree/Binaries/Linux:$GUT_ENGINE/Plugins/Runtime/GameplayStateTree/Binaries/Linux:$GUT_ENGINE/Plugins/FX/Niagara/Binaries/Linux:$GUT_ENGINE/Plugins/Animation/IKRig/Binaries/Linux:$GUT_ENGINE/Plugins/Animation/PerformanceCaptureCore/Binaries/Linux${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [[ "${1:-}" == "--env" ]]; then
	return 0 2>/dev/null || exit 0
fi

set -euo pipefail

REAL="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM="$HOME/.cache/ashmarket-sim/${1:?give the copy a name}"

mkdir -p "$SIM/Saved/Logs" "$SIM/for-review/finishers"
rm -rf "$SIM/Binaries" "$SIM/Config" "$SIM/Tools"

cp -a "$REAL/Binaries" "$SIM/Binaries"
cp -a "$REAL/Config" "$SIM/Config"
cp -a "$REAL/Tools" "$SIM/Tools"
cp -a "$REAL/Ashmarket.uproject" "$SIM/Ashmarket.uproject"

[[ -e "$SIM/Content" ]] || ln -s "$REAL/Content" "$SIM/Content"
[[ -e "$SIM/DerivedDataCache" ]] || ln -s "$REAL/DerivedDataCache" "$SIM/DerivedDataCache"

echo "$SIM"
