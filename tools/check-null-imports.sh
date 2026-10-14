#!/usr/bin/env bash
# Accord - the null-import guard for the title (T2.2b decision, point 1).
#
# In a title some POSIX symbols link and then resolve to a null import at run
# time: calling one jumps to address zero and the console reports a crash. The
# first version of the title's daemon link died exactly that way through
# getaddrinfo, and the harness's own networking example documents the class
# (examples/update-check/console_curl.c). This reads the built title's undefined
# symbols and fails the build if one of them is on the list.
#
# The list holds confirmed entries only. A symbol earns its place with a source
# or a measurement, not with caution: poll, fcntl and strerror are imported by
# this title today and none of them has been shown to resolve to zero.
#
# Runs inside the builder container (see the app and ffpkg targets), against the
# pre-CONVERT ELF, because eboot.bin has no symbols left.
#
# usage: check-null-imports.sh [elf] [symbol ...]
#   the symbol list is an override, which is how the guard itself is tested.

set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

elf="${1:-third_party/ps5-native-app-boilerplate/build/llvm-pie.elf}"
if [ "$#" -gt 0 ]; then
    shift
fi
if [ "$#" -gt 0 ]; then
    forbidden=("$@")
else
    forbidden=(getaddrinfo)
fi

if [ ! -f "$elf" ]; then
    printf '%s\n' "==> [title] no $elf to check: build the title first" >&2
    exit 1
fi

nm=""
for candidate in llvm-nm /opt/ps5-payload-sdk/bin/llvm-nm nm; do
    if command -v "$candidate" >/dev/null 2>&1; then
        nm="$candidate"
        break
    fi
done
if [ -z "$nm" ]; then
    printf '%s\n' "==> [title] no symbol reader (llvm-nm or nm) in this container" >&2
    exit 1
fi

imports="$("$nm" -u "$elf" | awk '{print $NF}' | sort -u)"
bad=()
for symbol in "${forbidden[@]}"; do
    if grep -qx -- "$symbol" <<<"$imports"; then
        bad+=("$symbol")
    fi
done

if [ "${#bad[@]}" -gt 0 ]; then
    printf '%s\n' "==> [title] $(basename "$elf") imports a null import: ${bad[*]}" >&2
    printf '%s\n' "    a title that calls one of these jumps to address zero;" >&2
    printf '%s\n' "    use a direct call instead (see platform/ps5/daemon_link.cpp)" >&2
    exit 1
fi

printf '%s\n' "==> [title] no null imports among: ${forbidden[*]}"
