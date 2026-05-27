#!/bin/sh
# scripts/dev/v3-smoke-runtime.sh
#
# R196.e: runtime smoke test for the legacy bnetd + d2cs binaries
# under WITH_BNETD=ON + WITH_D2CS=ON + PVPGN_BUILD_V3=ON. Run from
# inside the v3-smoke Docker stage; expects:
#   - $BUILD_DIR (default: /src/build/v3-smoke) holds bnetd/d2cs
#     binaries plus the configure-time-substituted conf/*.toml.
#   - `ss` from iproute2 is on PATH.
#
# Steps:
#   1. Stage a runtime tree at /tmp/pvpgn-{conf,state}.
#   2. Substitute the two leftover cmake template placeholders
#      `${SYSCONFDIR}` and `${LOCALSTATEDIR}` (left literal by
#      `configure_file(@ONLY)` in conf/CMakeLists.txt) using sed.
#   3. Run each daemon in --debug mode in the background for a few
#      seconds, then:
#        (a) assert it is still alive,
#        (b) check whether its expected listen port (6112 / 6200)
#            is bound -- warn rather than fail (port may not bind
#            in --debug for some configurations).
#
# Exit non-zero iff a daemon dies within the alive window.

set -e

BUILD_DIR="${BUILD_DIR:-/src/build/v3-smoke}"
CONF_DIR="${CONF_DIR:-/tmp/pvpgn-conf}"
STATE_DIR="${STATE_DIR:-/tmp/pvpgn-state}"
ALIVE_SECS="${ALIVE_SECS:-6}"

BNETD="$BUILD_DIR/src/bnetd/bnetd"
D2CS="$BUILD_DIR/src/d2cs/d2cs"

test -x "$BNETD" || { echo "FAIL: bnetd not built at $BNETD"; exit 1; }
test -x "$D2CS"  || { echo "FAIL: d2cs not built at $D2CS";  exit 1; }

mkdir -p "$CONF_DIR" "$STATE_DIR"
# Conf templates live in $BUILD_DIR/conf/ via configure_file(@ONLY).
# Copy the leaf files only (skip subdirs CMakeFiles/ and i18n/).
for f in "$BUILD_DIR"/conf/*.toml "$BUILD_DIR"/conf/*.conf "$BUILD_DIR"/conf/*.json "$BUILD_DIR"/conf/*.txt "$BUILD_DIR"/conf/*.plain; do
    [ -f "$f" ] && cp "$f" "$CONF_DIR/" || true
done

# Substitute the two template placeholders left by configure_file(@ONLY).
# Use a literal dollar-brace; quotes inside this script are sh-level and
# protected from the docker buildkit Dockerfile variable expansion that
# ate the inline form.
for f in "$CONF_DIR"/*.toml "$CONF_DIR"/*.conf "$CONF_DIR"/*.json; do
    [ -f "$f" ] && sed -i \
        -e 's#${SYSCONFDIR}#'"$CONF_DIR"'#g' \
        -e 's#${LOCALSTATEDIR}#'"$STATE_DIR"'#g' \
        "$f" || true
done

# R196.f: prefs_v3::effective_user() / effective_group() now return
# nullptr (not "") for the missing-key / empty-string case, and
# give_up_root_privileges() treats nullptr as "skip". With the shipped
# bnetd.toml.in commenting out both fields, the daemon now stays root
# inside the container -- which is fine, the container itself runs as
# root. No sed injection required.

# Pre-create the few state-tree leaves the daemons expect.
mkdir -p "$STATE_DIR/users" "$STATE_DIR/clans" "$STATE_DIR/teams" \
         "$STATE_DIR/files" "$STATE_DIR/lua" "$STATE_DIR/reports" \
         "$STATE_DIR/chanlogs" "$STATE_DIR/userlogs" \
         "$STATE_DIR/bnmail" "$STATE_DIR/ladders" "$STATE_DIR/status" \
         "$STATE_DIR/charsave" "$STATE_DIR/charinfo" \
         "$STATE_DIR/bak.charsave" "$STATE_DIR/bak.charinfo"

# bnetd's support_check_files() refuses to start without the support MPQ
# / BNI files shipped under repo `files/`. Copy them into the state-tree
# `files/` subdir so the check passes.
if [ -d /src/files ]; then
    cp -r /src/files/. "$STATE_DIR/files/" 2>/dev/null || true
fi

# ---- Pre-flight: verify substitution succeeded ------------------------
unresolved="$(grep -c '\${' "$CONF_DIR"/bnetd.toml || true)"
if [ "$unresolved" != "0" ] && [ -n "$unresolved" ]; then
    echo "FAIL: bnetd.toml still has $unresolved unresolved \${VAR} placeholders after sed"
    grep -n '\${' "$CONF_DIR"/bnetd.toml | head -10
    exit 1
fi

# ---- Helper: smoke a single daemon ------------------------------------
smoke_daemon() {
    name="$1"
    bin="$2"
    conf="$3"
    port="$4"

    echo "==> starting $name --debug -c $conf"
    "$bin" --debug -c "$conf" > "/tmp/${name}.log" 2>&1 &
    pid=$!
    sleep "$ALIVE_SECS"

    if ! kill -0 "$pid" 2>/dev/null; then
        echo "FAIL: $name died within ${ALIVE_SECS}s"
        echo "--- $name log tail ---"
        tail -50 "/tmp/${name}.log"
        return 1
    fi

    echo "PASS: $name alive after ${ALIVE_SECS}s (pid $pid)"
    echo "--- listen ports ---"
    ss -tln | tee "/tmp/${name}.ports"
    if ss -tln | awk '{print $4}' | grep -qE ":${port}\$"; then
        echo "PASS: $name listening on :${port}"
    else
        echo "WARN: $name alive but not bound to :${port}"
        echo "--- $name log tail ---"
        tail -50 "/tmp/${name}.log"
    fi

    kill -TERM "$pid" 2>/dev/null || true
    sleep 1
    kill -KILL "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    echo "==> $name smoke complete"
    return 0
}

rc=0
smoke_daemon bnetd "$BNETD" "$CONF_DIR/bnetd.toml" 6112 || rc=1
# d2cs default listen port is 6113 (per its toml `servaddrs` default,
# matched against the legacy code path); the historical "6200" was the
# d2gs port, not d2cs.
smoke_daemon d2cs  "$D2CS"  "$CONF_DIR/d2cs.toml"  6113 || rc=1

exit "$rc"
