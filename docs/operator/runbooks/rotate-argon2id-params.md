# Runbook: Rotate argon2id Parameters

This runbook explains how to safely change the argon2id password-hashing parameters used by
PvPGN, and how to force all accounts to be re-hashed with the new parameters.

---

## Background

PvPGN stores passwords as argon2id hashes. Each account record includes an `account.hash_version`
field that tracks which parameter set was used:

| `hash_version` | Algorithm | Notes |
|---------------|-----------|-------|
| `0` | Legacy `pvpgn_hash` (SHA-1 based) | Accounts created before Plan 08 |
| `1` | argon2id with params from `bnetd.toml` | Current default |

**Transparent rehashing:** When a player logs in successfully, PvPGN checks whether
`account.hash_version < current_version`. If so, it re-hashes the password with the current
parameters and updates the record. This means the migration is zero-downtime and incremental —
accounts are upgraded on their next login.

---

## When to Rotate Parameters

Rotate argon2id parameters when:

- Hardware has been upgraded (more RAM or CPU available → increase cost)
- A security advisory recommends higher parameters
- You are migrating from the legacy `pvpgn_hash` (version 0) to argon2id (version 1)

---

## Step 1 — Choose New Parameters

Benchmark on your server hardware to find parameters that take ~200–500 ms per hash:

```bash
# Install argon2 CLI tool
apt-get install argon2   # Debian/Ubuntu
brew install argon2      # macOS

# Benchmark: time a hash with candidate parameters
time echo -n "benchmark_password" | argon2 "randomsalt" -id -t 3 -m 16 -p 1
# -t = time_cost (iterations)
# -m = memory_cost (2^m KiB, so -m 16 = 64 MiB)
# -p = parallelism

# Target: real time between 0.2s and 0.5s
```

Recommended starting point for a modern server (2024):

```toml
[auth]
argon2id_time_cost   = 3      # iterations
argon2id_memory_cost = 65536  # KiB (64 MiB)
argon2id_parallelism = 1
```

For high-traffic servers where login latency matters more than brute-force resistance:

```toml
[auth]
argon2id_time_cost   = 2
argon2id_memory_cost = 32768  # 32 MiB
argon2id_parallelism = 1
```

---

## Step 2 — Update `bnetd.toml`

Edit `bnetd.toml` with the new parameters:

```toml
[auth]
argon2id_time_cost   = 4      # increased from 3
argon2id_memory_cost = 131072 # increased from 65536 (128 MiB)
argon2id_parallelism = 1
```

The `hash_version` field in the database does **not** need to be changed — PvPGN uses the
presence of the `$argon2id$` prefix in the stored hash to detect the algorithm, and always
re-hashes with the current parameters on login.

---

## Step 3 — Reload the Server

```bash
# Reload config without restarting (if supported)
systemctl reload bnetd

# Or do a rolling restart (see rolling-upgrade.md)
systemctl restart bnetd
```

From this point, every successful login will transparently re-hash the account's password with
the new parameters.

---

## Step 4 — Force Re-hash of All Accounts (Optional)

If you want all accounts to use the new parameters immediately (rather than waiting for each
player to log in), use the `bnetd` admin tool:

```bash
# Dry run — shows how many accounts would be re-hashed
bnetd --rehash-accounts --dry-run

# Perform the re-hash (requires the server to be stopped or in maintenance mode)
systemctl stop bnetd
bnetd --rehash-accounts
systemctl start bnetd
```

> **Warning:** `--rehash-accounts` reads every account's stored hash and re-hashes it using
> the current parameters. This requires the plaintext password — which PvPGN does **not** store.
> Therefore, `--rehash-accounts` can only upgrade accounts that still have a legacy hash
> (version 0) by re-hashing the stored legacy hash value as the "password". For argon2id
> accounts (version 1), re-hashing only happens on the next successful login.

---

## Step 5 — Monitor the Migration

Watch the `pvpgn_auth_hash_algo` metric to track progress:

```bash
# Prometheus query: fraction of logins still using legacy hash
curl -s http://localhost:9090/metrics | grep pvpgn_auth_hash_algo
# pvpgn_auth_hash_algo{algo="pvpgn_hash"} 42
# pvpgn_auth_hash_algo{algo="argon2id"}   1337
```

The `pvpgn_auth_hash_algo{algo="pvpgn_hash"}` counter should trend toward zero as players log in.

---

## Rollback Procedure

argon2id parameter changes are **forward-only** — once an account is re-hashed with new
parameters, the old parameters are no longer needed. There is no rollback for individual accounts.

However, if the new parameters cause login timeouts (too slow), you can reduce them:

1. Edit `bnetd.toml` to lower `argon2id_time_cost` or `argon2id_memory_cost`
2. Reload the server: `systemctl reload bnetd`
3. Accounts will be re-hashed with the lower parameters on next login

The stored hash always includes the parameters used (in the `$argon2id$v=19$m=...,t=...,p=...$`
prefix), so PvPGN can always verify a hash regardless of the current configured parameters.

---

## Security Considerations

- Never reduce parameters below OWASP minimums: `t=1, m=47104` (46 MiB), `p=1`
- The `argon2id_parallelism` parameter should match the number of CPU cores available for
  hashing; setting it higher than available cores does not improve security
- Monitor `pvpgn_auth_srp_duration_ms` — if p99 exceeds 1000 ms, reduce parameters

## See Also

- [Identity Context developer guide](../../developer/contexts/identity.md) — how transparent rehashing works in code
- [Diagnose Slow Login runbook](diagnose-slow-login.md) — if parameter changes cause login latency
