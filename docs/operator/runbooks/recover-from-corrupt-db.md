# Runbook: Recover from a Corrupt Database

**Symptom:** `bnetd` fails to start with a database error, or accounts/characters are missing
after an unclean shutdown.

---

## Quick Checklist

- [ ] Stop `bnetd` immediately — do not attempt writes to a suspect database
- [ ] Take a filesystem snapshot or copy of the database file before any repair
- [ ] Run the integrity check for your backend
- [ ] Restore from backup if integrity check fails
- [ ] Verify account data after recovery

---

## Step 1 — Stop the Server

```bash
systemctl stop bnetd
# Or if running in Docker:
docker stop pvpgn
```

Do **not** restart until the database is verified. Repeated writes to a corrupt database can
make recovery harder.

---

## Step 2 — Take a Safety Copy

Before touching anything:

```bash
# SQLite
cp /var/lib/pvpgn/pvpgn.db /var/lib/pvpgn/pvpgn.db.$(date +%Y%m%d-%H%M%S).bak

# MySQL — dump to SQL
mysqldump -u pvpgn -p pvpgn > /var/backups/pvpgn-$(date +%Y%m%d-%H%M%S).sql

# PostgreSQL
pg_dump -U pvpgn pvpgn > /var/backups/pvpgn-$(date +%Y%m%d-%H%M%S).sql
```

---

## Step 3 — Run the Integrity Check

### SQLite

```bash
sqlite3 /var/lib/pvpgn/pvpgn.db "PRAGMA integrity_check;"
```

Expected output: `ok`

If you see errors like `row N missing from index` or `invalid page`, proceed to **Step 4**.

Also run:
```bash
sqlite3 /var/lib/pvpgn/pvpgn.db "PRAGMA foreign_key_check;"
```

### MySQL

```bash
mysqlcheck -u pvpgn -p --all-databases
# Or for just the pvpgn database:
mysqlcheck -u pvpgn -p pvpgn
```

For InnoDB tables, also run:
```sql
CHECK TABLE accounts, characters, clans, friends, ladder_entries;
```

### PostgreSQL

```bash
# Check for corruption at the page level
vacuumdb -U pvpgn --analyze --verbose pvpgn
# Run pg_dump as a read test — if it fails, there is corruption
pg_dump -U pvpgn pvpgn > /dev/null && echo "OK" || echo "CORRUPT"
```

---

## Step 4 — Repair or Restore

### SQLite — Attempt Repair

```bash
# Export all recoverable data to SQL
sqlite3 /var/lib/pvpgn/pvpgn.db ".recover" > /tmp/pvpgn-recovered.sql

# Create a new database from the recovered SQL
sqlite3 /var/lib/pvpgn/pvpgn-new.db < /tmp/pvpgn-recovered.sql

# Verify the new database
sqlite3 /var/lib/pvpgn/pvpgn-new.db "PRAGMA integrity_check;"

# If OK, swap in the new database
mv /var/lib/pvpgn/pvpgn.db /var/lib/pvpgn/pvpgn.db.corrupt
mv /var/lib/pvpgn/pvpgn-new.db /var/lib/pvpgn/pvpgn.db
```

### MySQL — Repair InnoDB

InnoDB tables cannot be repaired in place. Restore from backup:

```bash
# Stop MySQL
systemctl stop mysql

# Restore from the most recent dump
mysql -u pvpgn -p pvpgn < /var/backups/pvpgn-<TIMESTAMP>.sql

# Start MySQL
systemctl start mysql
```

### PostgreSQL — Restore from Backup

```bash
dropdb -U postgres pvpgn
createdb -U postgres -O pvpgn pvpgn
psql -U pvpgn pvpgn < /var/backups/pvpgn-<TIMESTAMP>.sql
```

---

## Step 5 — Account Data Recovery

If specific accounts are missing after recovery, check the backup:

```bash
# SQLite — query the backup
sqlite3 /var/lib/pvpgn/pvpgn.db.bak "SELECT id, username, created_at FROM accounts WHERE username = 'alice';"

# Export a single account's data
sqlite3 /var/lib/pvpgn/pvpgn.db.bak ".dump accounts" | grep -A 5 "'alice'"
```

To manually re-insert a recovered account, use the `bnetd` admin CLI (if available) or insert
directly into the database using the schema from `src/infra/migrations/`.

---

## Step 6 — Verify and Restart

```bash
# Run the config check
bnetd --check-config

# Start the server
systemctl start bnetd

# Watch the log for errors
journalctl -u bnetd -f
```

---

## Prevention

- Enable WAL mode for SQLite (already the default in PvPGN v3):
  ```toml
  [storage]
  sqlite_journal_mode = "wal"
  ```
- Schedule nightly backups with `cron`:
  ```cron
  0 3 * * * sqlite3 /var/lib/pvpgn/pvpgn.db ".backup /var/backups/pvpgn-$(date +\%Y\%m\%d).db"
  ```
- Keep at least 7 days of backups
- Test restores quarterly
