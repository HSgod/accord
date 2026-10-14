#!/usr/bin/env bash
# Accord - the asset guard for the title (T2.2b decision, point 2).
#
# The title reads its fonts from /app0/assets/fonts, the harness copies the
# staged assets directory into the image, and that directory was empty: a console
# run drew panels and no words at all. The host snapshots never noticed, because
# they read the kit's own directory instead of the staged one, so the check
# belongs here, before the slow build rather than after it.
#
# usage: check-title-assets.sh <staging dir>

set -euo pipefail

staging="${1:?usage: check-title-assets.sh <staging dir>}"
fonts="$staging/assets/fonts"

required=(
    inter-regular.huifont
    inter-semibold.huifont
    montserrat-medium.huifont
    dejavu-sans-mono.huifont
    press-start-2p.huifont
    patrick-hand.huifont
)

missing=()
for name in "${required[@]}"; do
    if [ ! -f "$fonts/$name" ]; then
        missing+=("$name")
    fi
done

if [ "${#missing[@]}" -gt 0 ]; then
    printf '%s\n' "==> [title] staged assets are incomplete: missing ${missing[*]}" >&2
    printf '%s\n' "    expected them in $fonts" >&2
    printf '%s\n' "    the kit's fonts are copied by tools/stage-ps5-sources.sh" >&2
    exit 1
fi

printf '%s\n' "==> [title] staged fonts: ${#required[@]} of ${#required[@]}"
