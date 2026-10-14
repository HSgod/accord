#!/usr/bin/env bash
# Builds the T2.3c path-A probe payload into dist/accord-probe.elf.
#
# Run inside the builder container (see the probe target in the Makefile).
#
# Much smaller than the daemon's payload build on purpose. The probe calls one
# function the official Discord client is known to use, opens one audio port and
# plays the tone of T2.3a; it needs no JSON, no zlib, no TLS and no DAVE, so the
# only library on top of the SDK's own sysroot, crt, libc++ and libkernel is
# SceAudioOut -- the same stub the daemon links, which is where both the chat
# function and the classic output calls live.
#
# The tone comes from daemon/src/tone.cpp, compiled here without any change: the
# siren the probe plays is the siren the daemon plays, so the two are comparable
# by ear.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

sdk="${PS5_PAYLOAD_SDK:?PS5_PAYLOAD_SDK is unset: run this inside the builder container}"
cc="$sdk/bin/prospero-clang++"

out="dist/accord-probe.elf"
mkdir -p dist

"$cc" -std=c++20 -O2 -Wall -Wextra -I. \
    -o "$out" tools/audio-probe/main.cpp daemon/src/tone.cpp \
    -lSceAudioOut

printf '%s\n' "==> [probe] $out ($(wc -c <"$out") bytes)"
