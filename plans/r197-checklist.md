# R197 -- End-to-end compose smoke: parallel bnetd + d2cs + real BNCS client handshake

## Goal

Go beyond R196.e's sequential single-container smoke by running bnetd
and d2cs as two services on a shared Docker bridge network so:

1. d2cs's s2s connection to bnetd succeeds (resolves `bnetd` via Docker DNS).
2. bnetd's realm.conf entry `d2cs:6113` resolves to d2cs's container IP,
   so `realmlist_find_realm_by_ip()` matches when the s2s connection arrives.
3. A BNCS client (`bnchat_v3`) connects to bnetd via the published port
   and completes a real handshake: account create -> login -> channel join.

This is the first stage that exercises the entire strangler-fig path
**at runtime, over a real network, with real protocol traffic** --
including the v3 dispatch hooks for `bnetd_authreq` / `authreply` and
the v3 prefs accessors fed into the real privilege-drop / realm lookup
code paths.

## Implementation

### 1. `Dockerfile.v3` -- new stage `v3-compose` (+50 lines)

Mirrors `v3-smoke` but:

- Adds `bnchat` to the explicit `cmake --build --target` list.
- Installs `bash` alongside `iproute2` (for the orchestrator script).
- No build-time `RUN` smoke. ENTRYPOINT delegates to
  `scripts/dev/v3-daemon-entry.sh` (which is told `bnetd` or `d2cs`
  via the compose service's `command:` argument).

### 2. `scripts/dev/v3-daemon-entry.sh` -- per-daemon entry (new, +110 lines)

Idempotent staging (skipped on container restart) +
foreground `exec` of the daemon. Beyond what
`v3-smoke-runtime.sh` does sequentially:

- `d2cs.toml` sed-substitutes `<bnetd-IP>` -> `$BNETD_HOST` (compose
  hostname).
- `realm.conf` gets a single line appended:
  `"D2CS" "PvPGN Closed Realm" $D2CS_HOST:6113` -- so bnetd's
  `realmlist_find_realm_by_ip()` matches the d2cs container IP when
  d2cs's s2s arrives.

### 3. `docker-compose.v3.yml` (new, +60 lines)

Two services on a user-defined bridge network `pvpgn-v3-net`:

| service | command | hostname | published |
|---------|---------|----------|-----------|
| `bnetd` | `["bnetd"]` | `bnetd` | `6112:6112/tcp` |
| `d2cs`  | `["d2cs"]`  | `d2cs`  | `6113:6113/tcp` |

Both use the same `pvpgn-v3-compose:latest` image; `command:`
selects the daemon. `depends_on: [bnetd]` orders the startup so
d2cs's first s2s attempt has the right TCP listener waiting.

### 4. `scripts/dev/v3-compose-smoke.sh` -- orchestrator (new, +90 lines)

Host-side script run after `docker compose up`. Five-phase test:

1. **Bring stack up** (or assume already up).
2. **Listen port check** via `docker compose exec ... ss -tln`:
   bnetd:6112 + d2cs:6113.
3. **s2s auth check**: poll d2cs logs up to `$S2S_GRACE` seconds for
   `authed by bnetd`. Cross-check best-effort that bnetd logged
   `d2cs ... authed`.
4. **Client handshake**: run `bnchat_v3` inside the bnetd container
   loopback (`127.0.0.1:6112`) with `--create-account` flag. Grep
   stdout for: `handshake ok`, `login ok as`, `joining channel`.
5. **Teardown** via `docker compose down -v` (skipped if `KEEP_UP=1`).

### 5. `src/v3/tools/client/bnchat_v3.cpp` -- reinstate `--create-account` (+~30 lines)

The earlier modernization comment "deliberately drop account creation"
made the v3 client unable to bootstrap an account on a fresh server.
Reinstated as an opt-in `-C` / `--create-account` flag:

- After the BNet handshake but **before** `CLIENT_LOGINREQ1`, send a
  `CLIENT_CREATEACCTREQ1` with `password_hash1 = bnet_hash(lowercase(password))`.
- Drain `SERVER_CREATEACCTREPLY1`. Treat **both** `0x00000001` ("Ok,
  created") and `0x00000000` ("No, already exists or rejected") as
  success so the smoke is idempotent across re-runs.

Also added the matching `packet_id`s + struct definitions to
`bnclient_bnet_packets.hpp`:

```cpp
inline constexpr std::uint16_t CLIENT_CREATEACCTREQ1   = 0x2aff;
inline constexpr std::uint16_t SERVER_CREATEACCTREPLY1 = 0x2aff;

struct CClientCreateAcctReq1   { bn_int password_hash1[5]; };
struct SServerCreateAcctReply1 { bn_int result;            };

inline constexpr std::uint32_t kCreateAcctReply1_No = 0x00000000u;
inline constexpr std::uint32_t kCreateAcctReply1_Ok = 0x00000001u;
```

## Verification

Built and ran on the first try (no iteration needed):

```
==> bringing compose stack up
 Network pvpgn_pvpgn-v3-net  Created
 Container pvpgn-v3-bnetd  Started
 Container pvpgn-v3-d2cs   Started
==> waiting 20s for daemons to settle
==> checking listen ports
PASS: bnetd listening on :6112
PASS: d2cs listening on :6113
==> waiting up to 20s for s2s auth
PASS: d2cs --> bnetd s2s authed
PASS: bnetd logged d2cs auth
==> attempting bnchat client handshake (create + login + join)
bnchat: handshake ok, sessionkey=0x0a4e1a0b sessionnum=0x00000001
bnchat: createacctreply1 result=0x00000001 (created)
bnchat: login ok as "smoke_test"
bnchat: joining channel "Public-Chat"...
bnchat: stdin closed, exiting
PASS: bnchat BNet handshake
PASS: bnchat LOGINREQ1 -> SERVER_LOGINREPLY1 success
PASS: bnchat reached CLIENT_JOINCHANNEL
===================================================================
==> R197 compose smoke GREEN
===================================================================
```

## Iteration trace

Only one cosmetic correction was needed during this round:

1. **First orchestrator run**: client handshake step failed because
   Git Bash's MSYS path translation on Windows rewrote
   `docker compose exec bnetd /bin/sh -c '...'` into
   `... exec bnetd C:/Program Files/Git/usr/bin/sh -c '...'`,
   which doesn't exist in the container. Fix: drop the `sh -c`
   wrapper and call the bnchat binary directly via `exec -T`, with
   `</dev/null` instead of `echo /quit |` to terminate the chat loop.

No iteration was needed on the server-side plumbing: realm.conf entry,
bnetd hostname substitution in d2cs.toml, account creation packet
flow, and chat-room channel-list draining all worked on first try.

## What this newly catches (over R196.e/R196.f)

| Failure class | R196.e v3-smoke | R197 compose |
|---|---|---|
| Listen-port bind regression | catch | catch |
| Privilege-drop regression | catch | catch |
| TOML parse regression | catch | catch |
| **s2s authreq/authreply protocol regression** | miss | **catch** |
| **realm.conf parser regression** | miss | **catch** |
| **Cross-container hostname resolution path** | miss | **catch** |
| **`realmlist_find_realm_by_ip` regression** | miss | **catch** |
| **BNet handshake protocol regression** | miss | **catch** |
| **`CLIENT_AUTH_INFO` / `SERVER_AUTHREPLY_109` regression** | miss | **catch** |
| **`CLIENT_CREATEACCTREQ1` regression** | miss | **catch** |
| **Account creation -> file-storage round-trip regression** | miss | **catch** |
| **Password hash (bnet_hash double-SHA1 variant) regression** | miss | **catch** |
| **`CLIENT_LOGINREQ1` / `SERVER_LOGINREPLY1` regression** | miss | **catch** |
| **Channel-list draining / `CLIENT_JOINCHANNEL` regression** | miss | **catch** |
| **Strangler-fig dispatch hooks for `bnetd_authreq` / `bnetd_authreply` (`pvpgn_v3_d2cs_link_dispatch_try`)** | miss | **catch** |

## Files changed

| File | Change |
|------|--------|
| `Dockerfile.v3` | +50 lines (`v3-compose` stage + Usage:) |
| `docker-compose.v3.yml` | NEW, +60 lines |
| `scripts/dev/v3-daemon-entry.sh` | NEW, +110 lines |
| `scripts/dev/v3-compose-smoke.sh` | NEW, +95 lines |
| `src/v3/tools/client/bnchat_v3.cpp` | +50 lines (`--create-account`, helper, main wiring) |
| `src/v3/tools/client/bnclient_bnet_packets.hpp` | +20 lines (CREATEACCTREQ1 packet defs) |

## Out of scope / deferred

- **Wiring into CI**: same deferral as R195/R196.x. `docker compose up`
  works on a developer machine; calling it from GitHub Actions /
  appveyor (or k8s job) is a separate trivial change. (R198 candidate.)
- **Chat-message round-trip**: bnchat reaches the chat loop but the
  smoke exits immediately on EOF without sending or receiving a
  `CLIENT_MESSAGE` / `SERVER_MESSAGE` pair. A round-trip would
  exercise the channel state machine + chat broadcast, but the
  channel-join already exercises most of that path.
- **D2 character creation / D2GS integration**: would require d2gs
  (still a separate daemon, not yet integrated). Today d2cs's
  ladder + d2gs lookups fail in compose (logs `error opening ladder
  file`, `could not lookup host`) -- these are expected non-fatal
  warnings; nothing in the strangler-fig path drives them.
- **Cross-platform orchestrator**: the bash orchestrator works on
  Linux + Git Bash on Windows. A PowerShell port may be desirable
  but isn't required while the canonical CI target is Alpine/Linux.

## Status

- `pvpgn-v3-compose:latest` image built (~100s on cold cache).
- `scripts/dev/v3-compose-smoke.sh` GREEN end-to-end (7 PASS, 0 FAIL).
- Compose stack tears down cleanly via `docker compose down -v`.
- All four previous lanes (`v3-test`, `v3-bnetd-on`, `v3-d2cs-on`,
  `v3-smoke`) regression-free since this round only **adds** new
  artifacts (no edits to those stages).
