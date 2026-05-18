#!/bin/sh
# v3 client smoke test: drive bnftp_v3 against a Python mock FILE-class
# server that performs the single-step BNFTP handshake and returns a
# fixed payload.  Exit non-zero if either side disagrees about the wire
# layout or if the downloaded bytes do not match.
#
# Usage: scripts/v3-e2e-bnftp-smoke.sh <path-to-bnftp> [<port>]
#
# Default port is 6900 to avoid colliding with a real bnetd on 6112.

set -eu

bnftp_bin="${1:-/src/build/v3/src/v3/tools/client/bnftp}"
port="${2:-6900}"
script_dir="$(cd "$(dirname "$0")/../tests/e2e" && pwd)"
mock="${script_dir}/fake_bnftp_server.py"

if [ ! -x "$bnftp_bin" ]; then
    printf 'bnftp binary not found at %s\n' "$bnftp_bin" >&2
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

# Give the mock a beat to bind.  We deliberately do NOT probe the
# port with `nc -z` -- that opens a TCP connection which the mock
# would consume as the "real" client.  A short sleep is enough.
sleep 0.3

cd "$work"
if ! "$bnftp_bin" -f hello.bin 127.0.0.1 "$port" >"$work/client.log" 2>&1; then
    printf 'bnftp exited non-zero. client log:\n' >&2
    cat "$work/client.log" >&2
    printf 'mock log:\n' >&2
    cat "$work/mock.log" >&2
    exit 1
fi

wait "$mock_pid" || true
mock_pid=

if [ ! -f "$work/hello.bin" ]; then
    printf 'bnftp did not write hello.bin\n' >&2
    cat "$work/client.log" >&2
    exit 1
fi

expected='hello from fake bnftp server'
got="$(head -c 64 "$work/hello.bin")"
case "$got" in
    "$expected"*)
        printf '[v3-e2e-bnftp] OK: payload matches (%d bytes)\n' \
            "$(wc -c <"$work/hello.bin")"
        exit 0
        ;;
    *)
        printf 'payload mismatch.\n  want: %s\n  got:  %s\n' \
            "$expected" "$got" >&2
        exit 1
        ;;
esac
