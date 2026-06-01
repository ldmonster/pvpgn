# Runbook: Diagnose Slow Login

**Symptom:** Players report that logging in takes more than 2–3 seconds, or the login screen
hangs before the character list appears.

---

## Quick Checklist

- [ ] Check `bnetd` log for `auth.srp` timing warnings
- [ ] Check database query latency (`persistence.query_ms` metric)
- [ ] Check argon2id parameters (`[auth].argon2id_time_cost`, `memory_cost`)
- [ ] Check network round-trip time between client and server
- [ ] Check DNS resolution time for the server hostname

---

## Step 1 — Enable Timing Logs

Set the log level to `debug` temporarily:

```toml
# bnetd.toml
[log]
levels = "debug,info,warning,error"
```

Restart `bnetd`. Each login attempt will now emit lines like:

```
[DEBUG] auth.srp  step=1 account=alice elapsed_ms=12
[DEBUG] auth.srp  step=2 account=alice elapsed_ms=340
[DEBUG] auth.srp  step=3 account=alice elapsed_ms=8
[DEBUG] persistence.account_lookup account=alice elapsed_ms=4
```

A healthy login completes in < 100 ms total. If `step=2` (password verification) is slow,
see **Step 3**.

---

## Step 2 — Check Database Query Latency

If `persistence.account_lookup` is slow (> 50 ms), the database is the bottleneck.

**SQLite:**
```bash
# Check for missing index on accounts table
sqlite3 /var/lib/pvpgn/pvpgn.db "EXPLAIN QUERY PLAN SELECT * FROM accounts WHERE username = 'alice';"
# Should show "SEARCH accounts USING INDEX" — if it shows "SCAN", add the index:
sqlite3 /var/lib/pvpgn/pvpgn.db "CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts(username);"
```

**MySQL / PostgreSQL:**
```sql
-- Check slow query log or run EXPLAIN
EXPLAIN SELECT * FROM accounts WHERE username = 'alice';
-- Ensure the index exists:
SHOW INDEX FROM accounts;  -- MySQL
\d accounts                -- PostgreSQL
```

---

## Step 3 — Check argon2id Parameters

If `auth.srp step=2` is slow, the argon2id hash verification is taking too long. This is
intentional — argon2id is designed to be slow — but the parameters may be misconfigured for
your hardware.

Check current parameters:

```toml
# bnetd.toml
[auth]
argon2id_time_cost   = 3      # iterations (default: 3)
argon2id_memory_cost = 65536  # KiB (default: 64 MiB)
argon2id_parallelism = 1      # threads (default: 1)
```

Benchmark on your hardware:

```bash
# Time a single argon2id hash with current params
python3 -c "
import argon2, time
ph = argon2.PasswordHasher(time_cost=3, memory_cost=65536, parallelism=1)
t = time.perf_counter()
ph.hash('benchmark_password')
print(f'argon2id: {(time.perf_counter()-t)*1000:.1f} ms')
"
```

Target: < 200 ms per hash on the server CPU. If it exceeds this, reduce `argon2id_time_cost`
or `argon2id_memory_cost`. See the
[Rotate argon2id Params runbook](rotate-argon2id-params.md) for safe parameter rotation.

---

## Step 4 — Check SRP Handshake Timing

The SRP-6a handshake requires two round trips. Each round trip adds one network RTT.

```bash
# Measure RTT from a client machine
ping -c 10 <server-hostname>
# Or use hping3 for TCP RTT on port 6112
hping3 -S -p 6112 -c 5 <server-hostname>
```

If RTT > 50 ms, the network path is the bottleneck. Consider:
- Moving the server geographically closer to the player base
- Using a CDN or Anycast IP for the login endpoint
- Enabling TCP_NODELAY (already set by default in PvPGN v3)

---

## Step 5 — Check for Lock Contention

Under high concurrency, the session registry or account repository may be contended.

```bash
# Check for lock wait metrics (if OTel is configured)
curl -s http://localhost:9090/metrics | grep pvpgn_lock_wait_ms
```

If `pvpgn_lock_wait_ms_p99 > 100`, consider:
- Sharding the session registry by account ID prefix
- Switching to a connection-pooled database backend

---

## Escalation

If none of the above resolves the issue, collect a flamegraph:

```bash
# Requires perf + flamegraph tools
perf record -g -p $(pgrep bnetd) -- sleep 30
perf script | stackcollapse-perf.pl | flamegraph.pl > login-flamegraph.svg
```

Attach the SVG to the bug report along with the debug log excerpt.
