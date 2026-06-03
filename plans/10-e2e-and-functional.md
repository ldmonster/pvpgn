# 10 — End-to-End & Functional Coverage

E2E is where we prove a *real client journey* works against a *real server
process* — the ultimate regression net for a protocol emulator, and the thing
that lets us refactor aggressively without fear of changing a byte a shipped
game client depends on. All of it runs locally; none of it requires CI.

## 1. Principle

> If a real Battle.net/D2/IRC client can do it, there is an automated scenario
> that does it against a locally-spawned PvPGN and asserts the observable
> result.

Today there are smoke scripts (`scripts/v3-e2e-*.sh`,
`scripts/dev/v3-*-smoke.sh`). The plan turns these into a **complete, asserted,
repeatable scenario suite**.

## 2. The harness

- `services/combined` boots all three daemons in one process for e2e, against a
  throwaway SQLite DB and an ephemeral port allocation.
- A **scripted fake client** library (`tests/e2e/support/`) speaks the real wire
  protocols — it is the protocol codec from `src/protocol` reused as a client
  (DRY: the same codecs encode/decode in both directions), driving sequences and
  asserting received bytes/decoded messages.
- Each scenario: start server → connect fake client(s) → perform a journey →
  assert observable outcomes (responses, DB state, events) → clean shutdown.
- Determinism: inject the fake clock/rng into the server build used for e2e so
  timestamps and tokens are reproducible; scenarios assert on stable fields.

## 3. The journey catalogue (acceptance scenarios)

At minimum, one asserted scenario each:

| Journey | Asserts |
|---------|---------|
| **Account create + login (NLS/SRP)** | account persisted, session established, correct reply |
| **Bad login** | typed rejection, no session, lockout after N attempts |
| **Password change + re-login** | old fails, new works, `hash_version` upgraded |
| **Channel join / leave / list** | membership, broadcasts to other clients |
| **Chat message + whisper** | delivery to right recipients only |
| **Friends / clan** | social state changes propagate |
| **Game create / join / start / end** | game lifecycle + result recorded |
| **Anongame matchmaking** | queue → pairing → game |
| **Ladder query after games** | rankings reflect results |
| **Moderation (ban/mute/kick)** | restriction enforced on next action |
| **D2 realm: char create + login + save** | d2cs↔d2dbs↔d2gs round-trip |
| **File transfer (bnftp)** | requested file delivered intact |
| **Stats query (bnstat)** | correct profile returned |
| **IRC client login + channel** | IRC bridge interop |
| **Telnet/WOL paths** | basic interop smoke |
| **Multi-client interaction** | two fake clients see each other's actions |

These subsume the existing `bnbot/bnchat/bnftp/bnstat` smoke scripts and the
`v3-compose-smoke`/`v3-smoke-runtime` checks.

## 4. Golden-output discipline

- Where a journey produces a stable byte/message sequence, store it as a golden
  file and diff against it. A refactor that changes the bytes fails loudly and
  the diff is reviewed — this is the protocol-compatibility safety net.

## 5. Performance journeys

- A few scenarios run under the microbench harness (already present) to pin
  latency/throughput and idle-connection memory footprint, with a local
  regression gate (`check-bench-regression.py`) comparing to
  `tests/bench/baselines`.

## 6. Tasks for this plan

1. Build the reusable **fake-client** library on top of `src/protocol` codecs.
2. Stand up the `services/combined` e2e fixture (ephemeral DB + port).
3. Implement the journey catalogue in `tests/e2e/`, each as a CTest under label
   `e2e`, each asserted (not just "didn't crash").
4. Add golden-output diffing for stable journeys.
5. Retire the ad-hoc smoke `.sh` scripts once their journeys are covered by
   asserted scenarios.

## Definition of Done

- [ ] `ctest -L e2e` boots a real server and runs every journey in §3 with
      assertions, then shuts down cleanly, with no leaked ports/processes.
- [ ] The fake-client library reuses `src/protocol` codecs (no second codec
      implementation).
- [ ] Stable journeys are pinned by golden files; a byte change fails the suite.
- [ ] Legacy smoke `.sh` scripts are deleted in favour of asserted scenarios.
