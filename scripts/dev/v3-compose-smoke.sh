#!/bin/sh
# scripts/dev/v3-compose-smoke.sh
#
# R197: host-side orchestrator for docker-compose.v3.yml.
# Assumes the compose stack is already up (or will bring it up).
#
# Steps:
#   1. (optional) `docker compose up -d` if not already running.
#   2. Wait for both services to be listening on their primary ports.
#   3. Tail d2cs and bnetd logs to assert s2s success:
#        - d2cs prints "authed by bnetd"
#        - bnetd prints "d2cs <ip> authed"
#   4. Run bnchat from inside the bnetd container (loopback to itself)
#      with --create-account, verify it reaches "joining channel".
#   5. Print final PASS/FAIL summary.
#
# Env:
#   COMPOSE_FILE   docker-compose.v3.yml
#   STARTUP_GRACE  20 (seconds to wait for daemons to come up)
#   S2S_GRACE      20 (seconds beyond STARTUP_GRACE to wait for s2s auth)
#   KEEP_UP        if set, do not `docker compose down` at the end

set -eu

COMPOSE_FILE="${COMPOSE_FILE:-docker-compose.v3.yml}"
STARTUP_GRACE="${STARTUP_GRACE:-20}"
S2S_GRACE="${S2S_GRACE:-20}"

DC() { docker compose -f "$COMPOSE_FILE" "$@"; }

rc=0
fail() { echo "FAIL: $*"; rc=1; }
pass() { echo "PASS: $*"; }

# ---------------------------------------------------------------- 1
echo "==> bringing compose stack up"
DC up -d
echo "==> waiting ${STARTUP_GRACE}s for daemons to settle"
sleep "$STARTUP_GRACE"

# ---------------------------------------------------------------- 2
echo "==> checking listen ports"
if DC exec -T bnetd ss -tln | awk '{print $4}' | grep -qE ':6112$'; then
    pass "bnetd listening on :6112"
else
    fail "bnetd not bound to :6112"
    DC logs --tail=30 bnetd || true
fi
if DC exec -T d2cs ss -tln | awk '{print $4}' | grep -qE ':6113$'; then
    pass "d2cs listening on :6113"
else
    fail "d2cs not bound to :6113"
    DC logs --tail=30 d2cs || true
fi

# ---------------------------------------------------------------- 3
echo "==> waiting up to ${S2S_GRACE}s for s2s auth"
s2s_ok=0
i=0
while [ "$i" -lt "$S2S_GRACE" ]; do
    if DC logs d2cs 2>&1 | grep -q "authed by bnetd"; then
        s2s_ok=1
        break
    fi
    sleep 1
    i=$((i + 1))
done
if [ "$s2s_ok" = "1" ]; then
    pass "d2cs --> bnetd s2s authed"
else
    fail "d2cs never logged 'authed by bnetd' within ${S2S_GRACE}s"
    echo "--- d2cs logs ---"
    DC logs --tail=50 d2cs || true
    echo "--- bnetd logs ---"
    DC logs --tail=50 bnetd || true
fi

# Cross-check from the bnetd side too -- best-effort, do not fail the
# smoke if only one side logged (bnetd may log "d2cs ... authed" earlier
# and it could roll out of `--tail`).
if DC logs bnetd 2>&1 | grep -q "d2cs .* authed"; then
    pass "bnetd logged d2cs auth"
else
    echo "note: bnetd did not log 'd2cs ... authed' (non-fatal)"
fi

# ---------------------------------------------------------------- 4
echo "==> attempting bnchat client handshake (create + login + join)"
# Run bnchat inside the bnetd container, connecting to localhost:6112
# (its own service). Feed an empty stdin (no `/bin/sh -c` wrapper -- Git
# Bash's MSYS path translation on Windows rewrites the leading "/bin/sh"
# argument). bnchat exits on stdin EOF.
BNCHAT='/src/build/v3-smoke/src/v3/tools/client/bnchat'
bnchat_log=$(DC exec -T bnetd "$BNCHAT" \
        -u smoke_test -p smokepw --create-account -c CHAT \
        --channel=Public-Chat 127.0.0.1 6112 </dev/null 2>&1 | head -40 || true)
echo "$bnchat_log"

if echo "$bnchat_log" | grep -q 'handshake ok'; then
    pass "bnchat BNet handshake"
else
    fail "bnchat BNet handshake never reached 'handshake ok'"
fi
if echo "$bnchat_log" | grep -q 'login ok as'; then
    pass "bnchat LOGINREQ1 -> SERVER_LOGINREPLY1 success"
else
    fail "bnchat login never succeeded"
fi
if echo "$bnchat_log" | grep -q 'joining channel'; then
    pass "bnchat reached CLIENT_JOINCHANNEL"
else
    fail "bnchat never reached channel join"
fi

# ---------------------------------------------------------------- 5
echo "==================================================================="
if [ "$rc" -eq 0 ]; then
    echo "==> R197 compose smoke GREEN"
else
    echo "==> R197 compose smoke FAILED"
fi
echo "==================================================================="

if [ -z "${KEEP_UP:-}" ]; then
    DC down -v >/dev/null 2>&1 || true
else
    echo "(KEEP_UP set -- compose stack left running for inspection)"
fi

exit "$rc"
