#!/bin/sh
# v3 client smoke test: drive bnbot_v3 against the Python BOT-class
# mock in `tests/e2e/fake_bnbot_server.py`.  Asserts that bnbot's
# stdout contains the canned banner the mock sends.
#
# bnbot is a raw telnet-style byte pump that never exits on its own
# (it polls stdin in a tight loop when stdin = /dev/null), so we run
# it in the background and kill it after the banner has had time to
# round-trip.

set -eu

bnbot_bin="${1:-/src/build/v3/src/v3/tools/client/bnbot}"
port="${2:-6903}"
script_dir="$(cd "$(dirname "$0")/../tests/e2e" && pwd)"
mock="${script_dir}/fake_bnbot_server.py"

if [ ! -x "$bnbot_bin" ]; then
    printf 'bnbot binary not found at %s\n' "$bnbot_bin" >&2
    exit 1
fi
if [ ! -f "$mock" ]; then
    printf 'mock server not found at %s\n' "$mock" >&2
    exit 1
fi

work="$(mktemp -d)"
mock_pid=
bot_pid=
cleanup() {
    [ -n "$bot_pid"  ] && kill "$bot_pid"  2>/dev/null || true
    [ -n "$mock_pid" ] && kill "$mock_pid" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT

python3 "$mock" --port "$port" >"$work/mock.log" 2>&1 &
mock_pid=$!
sleep 0.3

"$bnbot_bin" 127.0.0.1 "$port" </dev/null \
    >"$work/client.log" 2>"$work/client.err" &
bot_pid=$!

# Give bnbot ~1.5s to print the banner the mock pushed at us, then
# kill it -- it would otherwise spin on stdin EOF forever.
sleep 1.5
kill "$bot_pid" 2>/dev/null || true
wait "$bot_pid" 2>/dev/null || true
bot_pid=

wait "$mock_pid" 2>/dev/null || true
mock_pid=

expected_substr='Welcome from fake bnbot'
if grep -q -F "$expected_substr" "$work/client.log"; then
    printf '[v3-e2e-bnbot] OK: banner rendered\n'
    exit 0
fi

printf 'expected substring not found in bnbot stdout.\n  want: %s\n  got:\n' \
    "$expected_substr" >&2
cat "$work/client.log" >&2
printf 'client.err:\n' >&2
cat "$work/client.err" >&2
printf 'mock.log:\n' >&2
cat "$work/mock.log" >&2
exit 1
