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

CONF_FILE="${RUNTIME_ETC}/bnetd.conf"
LOG_FILE="${RUNTIME_VAR}/bnetd.log"

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

# Point bnetd's logfile at a real on-disk file we can tail to stdout.
if grep -qE '^[[:space:]]*logfile[[:space:]]*=' "$CONF_FILE"; then
    sed -i "s|^[[:space:]]*logfile[[:space:]]*=.*|logfile = \"${LOG_FILE}\"|" "$CONF_FILE"
else
    printf '\nlogfile = "%s"\n' "$LOG_FILE" >> "$CONF_FILE"
fi

if ! grep -qE '^[[:space:]]*loglevels[[:space:]]*=' "$CONF_FILE"; then
    printf 'loglevels = fatal,error,warn,info\n' >> "$CONF_FILE"
fi

# Pre-create the log file with the right owner so bnetd can append to it.
: > "$LOG_FILE"

if [ "$(id -u)" = "0" ]; then
    chown -R "${PVPGN_UID}:${PVPGN_GID}" "$RUNTIME_ETC" "$RUNTIME_VAR" || true
fi

# Stream the log file to the container's stdout. -F follows across
# truncate/rotate. Runs as a background child of this entrypoint.
tail -n 0 -F "$LOG_FILE" 2>/dev/null &

if [ "$(id -u)" = "0" ]; then
    exec su-exec "${PVPGN_UID}:${PVPGN_GID}" "$@"
fi

exec "$@"
