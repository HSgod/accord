#!/usr/bin/env bash
# Accord - capture the console's klog without going quiet (T2.2b decision, point 3).
#
# One `nc` with no deadline is what went silent for twelve minutes during T2.2b:
# the output file looked alive, nothing new arrived, and a diagnosis ran on a
# dead capture. Every connection here has a deadline, the loop reconnects, and a
# capture that receives no data for the stall window says so in the file and on
# stderr. Use this for every console session.
#
# usage: klog-capture.sh [host] [port] [output]
#   defaults: 192.168.1.17, 3232, /tmp/klog-<date>-<time>.klog
#
# Ctrl-C stops it; the file stays where it is.

set -euo pipefail

host="${1:-192.168.1.17}"
port="${2:-3232}"
out="${3:-/tmp/klog-$(date +%Y%m%d-%H%M%S).klog}"
connection_timeout=20
stall_seconds=30

printf '%s\n' "==> [klog] $host:$port -> $out"
printf '%s\n' "    deadline ${connection_timeout}s per connection, stall warning ${stall_seconds}s"
: > "$out"

data_bytes=0
last_data_at=$(date +%s)

while true; do
    printf '\n=== connect %s ===\n' "$(date +%H:%M:%S)" >> "$out"

    chunk="$(mktemp)"
    nc -w "$connection_timeout" "$host" "$port" > "$chunk" 2>/dev/null || true
    received="$(wc -c < "$chunk" | tr -d ' ')"
    if [ "$received" -gt 0 ]; then
        cat "$chunk" >> "$out"
        data_bytes=$((data_bytes + received))
        last_data_at=$(date +%s)
    fi
    rm -f "$chunk"

    now=$(date +%s)
    if [ $((now - last_data_at)) -ge "$stall_seconds" ]; then
        printf '\n=== [klog] %s: no data for %ss (total %s B), still reconnecting ===\n' \
            "$(date +%H:%M:%S)" "$stall_seconds" "$data_bytes" | tee -a "$out" >&2
        last_data_at=$now
    fi

    sleep 1
done
