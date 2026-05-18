#!/bin/sh
# v3 client smoke test: drive bnchat_v3 through a full BNet handshake
# against the Python mock in `tests/e2e/fake_bnet_server.py`, then
# assert that the SERVER_MESSAGE it sent us was rendered to bnchat's
# stdout.
#
# Usage: scripts/v3-e2e-bnchat-smoke.sh <path-to-bnchat> [<port>]

set -eu

bnchat_bin="${1:-/src/build/v3/src/v3/tools/client/bnchat}"
port="${2:-6901}"
script_dir="$(cd "$(dirname "$0")/../tests/e2e" && pwd)"
mock="${script_dir}/fake_bnet_server.py"

if [ ! -x "$bnchat_bin" ]; then
    printf 'bnchat binary not found at %s\n' "$bnchat_bin" >&2
    exit 1
fi
if [ ! -f "$mock" ]; then
    printf 'mock server not found at %s\n' "$mock" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"; [ -n "${mock_pid:-}" ] && kill "$mock_pid" 2>/dev/null || true' EXIT

python3 "$mock" --port "$port" >"$work/mock.log" 2>&1 &
mock_pid=$!

# Short sleep is enough; do NOT probe with `nc -z` (that opens a TCP
# connection the mock would consume as the real client).
sleep 0.3

# Stdin: keep the FD alive long enough for the mock's SERVER_MESSAGE
# to be rendered, then feed `/quit` for a clean shutdown.  Without
# this, bnchat exits the moment it sees stdin EOF and we race the
# server message.
( sleep 1; printf '/quit\n' ) | \
    "$bnchat_bin" -u smoketest -p hunter2 -c CHAT --channel=test \
        127.0.0.1 "$port" >"$work/client.log" 2>"$work/client.err" || {
    rc=$?
    printf 'bnchat exited %d. client.log:\n' "$rc" >&2
    cat "$work/client.log" >&2
    printf 'client.err:\n' >&2
    cat "$work/client.err" >&2
    printf 'mock.log:\n' >&2
    cat "$work/mock.log" >&2
    exit "$rc"
}

wait "$mock_pid" || true
mock_pid=

expected_substr='hello from fake bnetd'
if grep -q -F "$expected_substr" "$work/client.log"; then
    printf '[v3-e2e-bnchat] OK: SERVER_MESSAGE rendered\n'
    exit 0
fi

printf 'expected substring not found in bnchat stdout.\n  want: %s\n  got:\n' \
    "$expected_substr" >&2
cat "$work/client.log" >&2
printf 'mock log:\n' >&2
cat "$work/mock.log" >&2
exit 1
