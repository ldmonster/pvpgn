#!/bin/sh
# =============================================================================
# PvPGN Docker entrypoint
# =============================================================================
# - Seeds /etc/pvpgn and /var/pvpgn from the pristine copy baked into the
#   image (/usr/local/share/pvpgn) when the mounted named volumes are empty.
# - Writes bnetd's log to /var/pvpgn/bnetd.log and tails that file to the
#   container's stdout so `docker logs` works regardless of the uid bnetd
#   runs under (avoids the /dev/stdout EACCES trap when dropping privs).
# - Drops privileges to the pvpgn user (uid 1001) via su-exec, then execs
#   bnetd in the foreground so it stays attached to the container.
# =============================================================================
set -eu

PVPGN_UID=1001
PVPGN_GID=1001

PRISTINE_ETC=/usr/local/share/pvpgn/etc
PRISTINE_VAR=/usr/local/share/pvpgn/var

RUNTIME_ETC=/etc/pvpgn
RUNTIME_VAR=/var/pvpgn

CONF_FILE="${RUNTIME_ETC}/bnetd.toml"

log() { printf '[entrypoint] %s\n' "$*" >&2; }

seed_dir() {
    src=$1
    dst=$2
    if [ ! -d "$src" ]; then
        log "WARNING: pristine source $src is missing; skipping seed"
        return 0
    fi
    mkdir -p "$dst"
    if [ -z "$(ls -A "$dst" 2>/dev/null || true)" ]; then
        log "Seeding $dst from $src"
        cp -a "$src/." "$dst/"
    fi
}

seed_dir "$PRISTINE_ETC" "$RUNTIME_ETC"
seed_dir "$PRISTINE_VAR" "$RUNTIME_VAR"

mkdir -p \
    "$RUNTIME_VAR/users" \
    "$RUNTIME_VAR/clans" \
    "$RUNTIME_VAR/teams" \
    "$RUNTIME_VAR/files" \
    "$RUNTIME_VAR/reports" \
    "$RUNTIME_VAR/chatlogs" \
    "$RUNTIME_VAR/i18n"

if [ ! -f "$CONF_FILE" ]; then
    log "ERROR: $CONF_FILE not found after seeding. Aborting."
    exit 1
fi

# Resolve the ${SYSCONFDIR} / ${LOCALSTATEDIR} placeholders that
# configure_file(@ONLY) in conf/CMakeLists.txt leaves literal, pointing them at
# the runtime locations. Idempotent: a re-run finds no placeholders left.
# Mirrors scripts/dev/v3-daemon-entry.sh.
for f in "$RUNTIME_ETC"/bnetd.toml "$RUNTIME_ETC"/d2cs.toml "$RUNTIME_ETC"/d2dbs.toml; do
    [ -f "$f" ] && sed -i \
        -e 's#${SYSCONFDIR}#'"$RUNTIME_ETC"'#g' \
        -e 's#${LOCALSTATEDIR}#'"$RUNTIME_VAR"'#g' \
        "$f"
done

# v3 bnetd logs to stdout/stderr (spdlog console sink), so `docker logs` works
# directly — no legacy .conf logfile patching or log-file tailing needed.
if [ "$(id -u)" = "0" ]; then
    chown -R "${PVPGN_UID}:${PVPGN_GID}" "$RUNTIME_ETC" "$RUNTIME_VAR" || true
    exec su-exec "${PVPGN_UID}:${PVPGN_GID}" "$@"
fi

exec "$@"
