#!/bin/sh
# scripts/dev/v3-daemon-entry.sh
#
# R197: docker-compose entrypoint for the v3-compose-runtime stage.
# Stages /tmp/pvpgn-{conf,state}, performs the same template
# substitutions and state-tree prep as v3-smoke-runtime.sh, then
# execs the named daemon in foreground.
#
# Usage: v3-daemon-entry.sh <bnetd|d2cs>
#
# Env vars (with defaults):
#   BUILD_DIR  /src/build/v3-smoke   binary + conf source tree
#   CONF_DIR   /tmp/pvpgn-conf       staged conf (writable)
#   STATE_DIR  /tmp/pvpgn-state      runtime state tree
#   BNETD_HOST bnetd                 docker-compose hostname of bnetd
#   D2CS_HOST  d2cs                  docker-compose hostname of d2cs
#
# Cross-service plumbing applied here (vs. the single-host v3-smoke):
#   - bnetd's realm.conf: write a single line containing `D2CS` and
#     the d2cs hostname so bnetd's `realmlist_find_realm_by_ip()`
#     succeeds when d2cs's s2s connection arrives.
#   - d2cs.toml: substitute the placeholder `<bnetd-IP>` with the
#     bnetd hostname.

set -e

if [ -z "$1" ]; then
    echo "FAIL: v3-daemon-entry.sh requires daemon name (bnetd|d2cs)" 1>&2
    exit 1
fi
DAEMON="$1"

BUILD_DIR="${BUILD_DIR:-/src/build/v3-smoke}"
CONF_DIR="${CONF_DIR:-/tmp/pvpgn-conf}"
STATE_DIR="${STATE_DIR:-/tmp/pvpgn-state}"
BNETD_HOST="${BNETD_HOST:-bnetd}"
D2CS_HOST="${D2CS_HOST:-d2cs}"

BIN="$BUILD_DIR/src/$DAEMON/$DAEMON"
test -x "$BIN" || { echo "FAIL: $DAEMON binary not built at $BIN" 1>&2; exit 1; }

mkdir -p "$CONF_DIR" "$STATE_DIR"

# Idempotent: if /tmp/pvpgn-conf is already staged (compose restarts a
# container), skip the prep.
if [ ! -f "$CONF_DIR/.staged" ]; then
    for f in "$BUILD_DIR"/conf/*.toml "$BUILD_DIR"/conf/*.conf \
             "$BUILD_DIR"/conf/*.json "$BUILD_DIR"/conf/*.txt \
             "$BUILD_DIR"/conf/*.plain; do
        [ -f "$f" ] && cp "$f" "$CONF_DIR/" || true
    done

    # Substitute the two cmake template placeholders left literal by
    # configure_file(@ONLY).
    for f in "$CONF_DIR"/*.toml "$CONF_DIR"/*.conf "$CONF_DIR"/*.json; do
        [ -f "$f" ] && sed -i \
            -e 's#${SYSCONFDIR}#'"$CONF_DIR"'#g' \
            -e 's#${LOCALSTATEDIR}#'"$STATE_DIR"'#g' \
            "$f" || true
    done

    # d2cs.toml ships with `bnetdaddr = "<bnetd-IP>:6112"` -- swap in
    # the docker-compose hostname.
    if [ -f "$CONF_DIR/d2cs.toml" ]; then
        sed -i -e "s/<bnetd-IP>/$BNETD_HOST/g" "$CONF_DIR/d2cs.toml"
    fi

    # bnetd's realm.conf ships empty (only commented example).
    # Write a single realm line pointing at d2cs's container hostname,
    # so bnetd's `realmlist_find_realm_by_ip()` matches d2cs's
    # incoming s2s connection IP.
    if [ -f "$CONF_DIR/realm.conf" ]; then
        printf '"D2CS"\t"PvPGN Closed Realm"\t%s:6113\n' "$D2CS_HOST" \
            >> "$CONF_DIR/realm.conf"
    fi

    # Pre-create the state-tree leaves the daemons expect.
    mkdir -p "$STATE_DIR/users" "$STATE_DIR/clans" "$STATE_DIR/teams" \
             "$STATE_DIR/files" "$STATE_DIR/lua" "$STATE_DIR/reports" \
             "$STATE_DIR/chanlogs" "$STATE_DIR/userlogs" \
             "$STATE_DIR/bnmail" "$STATE_DIR/ladders" "$STATE_DIR/status" \
             "$STATE_DIR/charsave" "$STATE_DIR/charinfo" \
             "$STATE_DIR/bak.charsave" "$STATE_DIR/bak.charinfo"

    # Repo's `files/` -> state-tree's files/ subdir (MPQ/BNI support
    # files; bnetd's support_check_files() aborts otherwise).
    if [ -d /src/files ]; then
        cp -r /src/files/. "$STATE_DIR/files/" 2>/dev/null || true
    fi

    touch "$CONF_DIR/.staged"
fi

CONF_FILE="$CONF_DIR/$DAEMON.toml"
test -f "$CONF_FILE" || { echo "FAIL: missing $CONF_FILE" 1>&2; exit 1; }

echo "==> exec $DAEMON --debug -c $CONF_FILE"
exec "$BIN" --debug -c "$CONF_FILE"
