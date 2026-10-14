#!/usr/bin/env bash
# Accord - stage this project's PS5 sources into the boilerplate harness.
#
# The boilerplate builds from a single source directory, while our code lives in
# core/, platform/ps5/ and ui/. Those are copied into its gitignored
# .local/ tree, mirroring the layout so that root-relative includes such as
# "core/log.hpp" keep working.
#
# platform/host/ is deliberately NOT staged: it is a host substitute and must
# never reach the console. For the same reason ui/kit/host/ stays behind: it
# defines the kit's sys:: seam for the PC (monotonic_us, log, park, quit) and
# would collide with ui/kit/platform/ps5/system.cpp, which defines the same
# symbols for the console.
#
# platform/ps5/dave/ belongs to the payload build alone. It includes
# <dave/dave.h> from the cross-built prefix under build/, and it needs
# exceptions and RTTI, while the harness compiles the title with
# -fno-exceptions -fno-rtti. Staging it broke the title build with
# "dave/dave.h file not found". The payload reads its sources straight from
# this tree, so nothing here depends on that directory.

set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
staging="$root/third_party/ps5-native-app-boilerplate/.local/accord"

rm -rf -- "$staging"
mkdir -p -- "$staging/src" "$staging/sce_sys" "$staging/assets"

cp -a -- "$root/core" "$staging/src/core"
mkdir -p -- "$staging/src/platform"
cp -a -- "$root/platform/ps5" "$staging/src/platform/ps5"
# T2.2b: the title-to-daemon link probe is a test build's business. Without
# LINK_TEST=1 its module is not staged, so an ordinary build carries none of
# the socket code either.
if [ "${LINK_TEST:-0}" != "1" ]; then
    rm -f -- "$staging/src/platform/ps5/daemon_link.hpp" \
        "$staging/src/platform/ps5/daemon_link.cpp"
fi
cp -a -- "$root/ui" "$staging/src/ui"
rm -rf -- "$staging/src/platform/ps5/dave" "$staging/src/ui/kit/host"

cp -a -- "$root/sce_sys/param.json" "$staging/sce_sys/param.json"

# Every word the title draws comes from the kit's precompiled fonts, which the
# title reads from /app0/assets/fonts. The kit keeps them in its own tree, so the
# image has to carry them or a console run shows panels and no text at all -- the
# host snapshots never noticed, because they read the kit's directory directly.
kit_assets="$root/third_party/ps5-homebrew-ui/assets"
if [ ! -d "$kit_assets" ]; then
    printf '%s\n' "==> [stage] missing $kit_assets (the kit's fonts and cues)" >&2
    printf '%s\n' "    check out third_party/ps5-homebrew-ui first" >&2
    exit 1
fi
mkdir -p -- "$staging/assets"
cp -a -- "$kit_assets/." "$staging/assets/"

# core/json.cpp reads the pinned parser as "third_party/yyjson/yyjson.h", which
# is the path the host tests and the daemon resolve from the repository root.
# Staging the same pair under src/ keeps that spelling working here too, and the
# harness compiles the staged .c along with our own.
json_src="$root/third_party/yyjson"
[[ -f $json_src/yyjson.c && -f $json_src/yyjson.h ]] || {
    printf 'missing %s: run tools/fetch-yyjson.sh\n' "$json_src" >&2
    exit 2
}
mkdir -p -- "$staging/src/third_party/yyjson"
cp -a -- "$json_src/yyjson.c" "$json_src/yyjson.h" "$staging/src/third_party/yyjson/"

# core/zlib_stream.cpp reads the pinned decompressor the same way, as
# "third_party/zlib/zlib.h". All of the headers come along -- zlib's own .c files
# include their siblings -- but of the sources only the inflate side: the title
# never compresses anything. The host tests link the compressor as well, and only
# to build the fixtures they read back.
zlib_src="$root/third_party/zlib"
[[ -f $zlib_src/zlib.h ]] || {
    printf 'missing %s: run tools/fetch-zlib.sh\n' "$zlib_src" >&2
    exit 2
}
mkdir -p -- "$staging/src/third_party/zlib"
cp -a -- "$zlib_src"/*.h "$zlib_src"/LICENSE "$staging/src/third_party/zlib/"
for name in adler32 crc32 inffast inflate inftrees zutil; do
    cp -a -- "$zlib_src/$name.c" "$staging/src/third_party/zlib/"
done

# The kit draws its text with SDF atlases it loads from the application's own
# assets, so the six .huifont files (with their licences) are staged from the
# vendored kit into assets/fonts, which the harness copies into the .ffpkg as
# /app0/assets. They are third-party binaries: staged, not committed here.
kit_assets="$root/third_party/ps5-homebrew-ui/assets/fonts"
[[ -d $kit_assets ]] || {
    printf 'missing %s: run git submodule update --init third_party/ps5-homebrew-ui\n' \
        "$kit_assets" >&2
    exit 2
}
cp -a -- "$kit_assets" "$staging/assets/fonts"

printf '%s\n' '==> [stage] core/ platform/ps5/ ui/ + sce_sys/param.json + assets/fonts + third_party/yyjson + third_party/zlib -> boilerplate/.local/accord'
