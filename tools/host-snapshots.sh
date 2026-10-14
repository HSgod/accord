#!/usr/bin/env bash
# Accord - render every screen on the host (Mesa surfaceless EGL) to PNG.
#
# Builds the platform-neutral part of the app for the PC and runs
# platform/host/snapshot_main.cpp. The console platform layer
# (ui/kit/platform/) is left out; the kit's host shim stands in for it. Runs
# inside the builder container, which carries Mesa:
#
#   scripts/dev.sh bash tools/host-snapshots.sh [theme id]
#   scripts/dev.sh bash tools/host-snapshots.sh --link-only   # build, no PNGs
#
# The source list is the Makefile's (`make snapshot-sources`). The script used to
# keep its own copy of that list, the two drifted apart, and the link failed on
# core/zlib_stream.cpp's inflateEnd while every green target stayed green. One
# list now, and `make test` links this binary without rendering it.
#
# Output: build/snapshots/*.png (one per screen).

set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cxx="${HOST_CXX:-clang++}"
cc="${HOST_CC:-clang}"
# Optional sanitizer for the whole build, e.g. HOST_SNAPSHOT_SANITIZE=address:
# the runner is the one host build that exercises the frame composition.
sanitize="${HOST_SNAPSHOT_SANITIZE:-}"
build="$root/build/host-snapshots"
output="$root/build/snapshots"
theme="acrylic"
link_only=0

for argument in "$@"; do
    case "$argument" in
    --link-only)
        link_only=1
        ;;
    *)
        theme="$argument"
        ;;
    esac
done

rm -rf -- "$build"
mkdir -p -- "$build/obj" "$output"

cd "$root"

sources=()
while IFS= read -r source; do
    if [ -n "$source" ]; then
        sources+=("$source")
    fi
done < <(make -s -C "$root" snapshot-sources)

if [ "${#sources[@]}" -eq 0 ]; then
    printf '%s\n' "==> [host-snapshots] make snapshot-sources printed nothing" >&2
    exit 1
fi

sanitize_flags=()
if [ -n "$sanitize" ]; then
    sanitize_flags=("-fsanitize=$sanitize" "-g")
fi

objects=()
for source in "${sources[@]}"; do
    object="$build/obj/${source//\//_}.o"
    case "$source" in
    *.c)
        # C: the pinned JSON parser and the inflate side of zlib.
        "$cc" -std=c11 -O2 -w "${sanitize_flags[@]}" -I"$root" -c "$source" -o "$object"
        ;;
    ui/kit/* | platform/host/*)
        # The vendored kit and the host shim are third-party: keep our warnings,
        # not theirs.
        "$cxx" -std=c++20 -O2 -w "${sanitize_flags[@]}" -DGL_GLEXT_PROTOTYPES=1 -I"$root" -c "$source" -o "$object"
        ;;
    *)
        "$cxx" -std=c++20 -O2 -Wall -Wextra -Wpedantic "${sanitize_flags[@]}" \
            -DGL_GLEXT_PROTOTYPES=1 -I"$root" -c "$source" -o "$object"
        ;;
    esac
    objects+=("$object")
done

"$cxx" "${objects[@]}" "${sanitize_flags[@]}" -lEGL -lGL -lm -o "$build/hui_snapshots"

if [ "$link_only" -eq 1 ]; then
    printf '%s\n' "==> [host-snapshots] linked $build/hui_snapshots (no rendering)"
    exit 0
fi

EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "$build/hui_snapshots" "$root/third_party/ps5-homebrew-ui/assets" "$output" "$theme"

printf '%s\n' "==> [host-snapshots] $(find "$output" -name '*.png' | wc -l | tr -d ' ') PNGs in build/snapshots"
