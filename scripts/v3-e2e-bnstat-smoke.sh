#!/bin/sh
# v3 client smoke test: drive bnstat_v3 against the Python BNet mock
# in stats mode (handshake, then one CLIENT_STATSREQ /
# SERVER_STATSREPLY exchange).  Asserts that bnstat prints one of the
# canned values to stdout.
#
# Usage: scripts/v3-e2e-bnstat-smoke.sh <path-to-bnstat> [<port>]

set -eu

bnstat_bin="${1:-/src/build/v3/src/v3/tools/client/bnstat}"
port="${2:-6902}"
script_dir="$(cd "$(dirname "$0")/../tests/e2e" && pwd)"
mock="${script_dir}/fake_bnet_server.py"

if [ ! -x "$bnstat_bin" ]; then
    printf 'bnstat binary not found at %s\n' "$bnstat_bin" >&2
    exit 1
fi
if [ ! -f "$mock" ]; then
    printf 'mock server not found at %s\n' "$mock" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"; [ -n "${mock_pid:-}" ] && kill "$mock_pid" 2>/dev/null || true' EXIT

python3 "$mock" --port "$port" --mode stats >"$work/mock.log" 2>&1 &
mock_pid=$!

sleep 0.3

# bnstat takes positional [host [port]] and -p PLAYER for the
# account name.  Use `-c D2DV` so the handshake skips the CDKEY2
# stage (the mock doesn't model it) and we don't send the
# DRTL-style UNKNOWN_1B prelude.
set +e
"$bnstat_bin" -c D2DV -p smoketest 127.0.0.1 "$port" \
        >"$work/client.log" 2>"$work/client.err" </dev/null
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
    printf 'bnstat exited %d. client.log:\n' "$rc" >&2
    cat "$work/client.log" >&2
    printf 'client.err:\n' >&2
    cat "$work/client.err" >&2
    printf 'mock.log:\n' >&2
    cat "$work/mock.log" >&2
    exit "$rc"
fi

wait "$mock_pid" || true
mock_pid=

# The mock cycles ["male","42","moon","fake account"] over key_count.
# bnstat with `-c CHAT` will print at least the first four profile
# keys with those values, e.g. `profile\location          = moon`.
expected_substr='moon'
if grep -q -F "$expected_substr" "$work/client.log"; then
    printf '[v3-e2e-bnstat] OK: STATSREPLY rendered\n'
    exit 0
fi

printf 'expected substring not found in bnstat stdout.\n  want: %s\n  got:\n' \
    "$expected_substr" >&2
cat "$work/client.log" >&2
printf 'mock log:\n' >&2
cat "$work/mock.log" >&2
exit 1
