# Runbook: Rolling Upgrade (No-Downtime)

This runbook describes how to upgrade PvPGN to a new version without disconnecting active players.
It applies to single-node and multi-node deployments.

---

## Prerequisites

- The new version is backwards-compatible with the current database schema (check the CHANGELOG)
- You have a recent database backup (see [Recover from Corrupt DB](recover-from-corrupt-db.md))
- The new binary passes `bnetd --check-config` against the current `bnetd.toml`

---

## Backwards-Compatibility Window

PvPGN guarantees a **one-version backwards-compatibility window** for:

- Database schema (migrations are additive; old columns are never dropped in the same release
  that adds new ones)
- Configuration keys (deprecated keys are accepted for one release cycle with a warning)
- Plugin ABI (see [Plugin Versioning Guide](../developer/plugin-versioning-guide.md))

This means you can run the old and new binary against the same database simultaneously during
the upgrade window.

---

## Step 1 — Pre-Flight Checks

```bash
# 1. Verify the new binary
bnetd-new --version
bnetd-new --check-config --config /etc/pvpgn/bnetd.toml

# 2. Run the database migration dry-run (if the release includes migrations)
bnetd-new --migrate --dry-run --config /etc/pvpgn/bnetd.toml

# 3. Check that the health probe responds on the current instance
curl -sf http://localhost:8080/healthz && echo "OK"
```

If any pre-flight check fails, **do not proceed**. Fix the issue first.

---

## Step 2 — Apply Database Migrations

Migrations are applied by the new binary before it starts accepting connections:

```bash
bnetd-new --migrate --config /etc/pvpgn/bnetd.toml
```

Migrations are idempotent and backwards-compatible. The old binary will continue to run against
the migrated schema without errors.

---

## Step 3 — Start the New Instance

### Systemd (single-node)

```bash
# Install the new binary alongside the old one
install -m 755 bnetd-new /usr/local/bin/bnetd-new

# Start the new instance on a secondary port to verify it starts cleanly
PVPGN_BNETD__NET__BNET_PORT=6113 bnetd-new --config /etc/pvpgn/bnetd.toml &
NEW_PID=$!

# Wait for the health probe
for i in $(seq 1 30); do
    curl -sf http://localhost:8081/healthz && break
    sleep 1
done

echo "New instance PID: ${NEW_PID}"
```

### Docker / Kubernetes

```bash
# Pull the new image
docker pull pvpgn/pvpgn:NEW_VERSION

# Start a new container alongside the old one
docker run -d --name pvpgn-new \
    -v /var/lib/pvpgn:/data \
    -v /etc/pvpgn:/config:ro \
    pvpgn/pvpgn:NEW_VERSION

# Wait for health
docker exec pvpgn-new curl -sf http://localhost:8080/healthz
```

---

## Step 4 — Health Probe Contract

The `/healthz` endpoint returns:

| Status | Meaning |
|--------|---------|
| `200 OK` `{"status":"ok"}` | Instance is ready to accept connections |
| `503 Service Unavailable` | Instance is starting up or draining |

During a rolling restart, the load balancer (if any) will stop routing new connections to an
instance that returns 503. Existing connections are not dropped.

---

## Step 5 — Drain and Replace the Old Instance

```bash
# Send SIGTERM to the old instance — it will stop accepting new connections
# and wait for active sessions to finish (up to [net.drain_timeout_seconds])
kill -TERM $(pgrep -f 'bnetd --config')

# Monitor active connections draining
watch -n 2 'ss -tnp | grep :6112 | wc -l'

# Once connections reach 0 (or drain_timeout expires), the old process exits
# Replace the binary
mv /usr/local/bin/bnetd-new /usr/local/bin/bnetd

# Reload systemd if using a unit file
systemctl daemon-reload
systemctl restart bnetd
```

---

## Step 6 — Verify

```bash
# Check the version
bnetd --version

# Check the health probe
curl -sf http://localhost:8080/healthz

# Watch the log for errors
journalctl -u bnetd -f --since "5 minutes ago"

# Verify player logins work
# (use a test account — see scripts/dev/login_testusers.sh)
bash scripts/dev/login_testusers.sh
```

---

## Rollback Procedure

If the new version has a critical bug:

```bash
# 1. Stop the new instance
systemctl stop bnetd

# 2. Restore the old binary
cp /usr/local/bin/bnetd.bak /usr/local/bin/bnetd

# 3. Roll back the database migration (if the release included one)
#    Only possible if the migration was reversible — check the migration file header
bnetd --migrate --rollback --config /etc/pvpgn/bnetd.toml

# 4. Start the old instance
systemctl start bnetd

# 5. Verify
curl -sf http://localhost:8080/healthz
```

!!! warning "Migration rollback"
    Not all migrations are reversible. If the migration added a NOT NULL column without a
    default, rolling back requires restoring from the pre-migration backup.
