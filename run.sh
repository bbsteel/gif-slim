#!/bin/bash
cd "$(dirname "$0")"
export DISPLAY="${DISPLAY:-:0}"

BIN="$(dirname "$0")/gif-slim"
PIDFILE="$(dirname "$0")/.gif-slim.pid"

if [ -f "$PIDFILE" ]; then
    old=$(cat "$PIDFILE")
    if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
        if grep -q gif-slim /proc/$old/cmdline 2>/dev/null; then
            kill "$old" 2>/dev/null
            for i in $(seq 1 10); do
                kill -0 "$old" 2>/dev/null || break
                sleep 0.1
            done
            kill -9 "$old" 2>/dev/null
        fi
    fi
    rm -f "$PIDFILE"
fi

"$BIN" "$@" &
echo $! > "$PIDFILE"
wait
rm -f "$PIDFILE"
