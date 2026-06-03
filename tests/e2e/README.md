# E2E Tests

End-to-end tests drive a real `bnetd` instance. Two complementary styles:

- **client-tool smokes** (`*-smoke.sh`): the real client tools (`bnbot`,
  `bnchat`, `bnftp`, `bnstat`) drive a Python **mock** server.
- **wire-client journey** (`modern_login_journey_test.py`): a stdlib-only
  Python **client** spawns a real `bnetd` (inmemory backend, ephemeral port)
  and drives the modern SID auth handshake. This is the only test exercising
  real bnetd's wire dispatch / session-send / teardown paths end to end — it
  found the dispatch UAF, the un-sent-reply bug, and the on_close crash.
  Self-contained (no docker); registered as `e2e.modern_login_journey`.

## Running

```bash
# Start bnetd first (e.g. via docker-compose)
docker-compose -f docker-compose.v3.yml up -d bnetd

# Run e2e tests
cmake --preset v3-dev -DPVPGN_V3_E2E_TESTS=ON
cmake --build --preset v3-dev
ctest --preset v3-dev -L e2e
```

## Scripts

| Script | What it tests |
|--------|---------------|
| `scripts/v3-e2e-bnbot-smoke.sh` | Bot protocol login + channel join |
| `scripts/v3-e2e-bnchat-smoke.sh` | Chat protocol login + message |
| `scripts/v3-e2e-bnftp-smoke.sh` | File transfer protocol |
| `scripts/v3-e2e-bnstat-smoke.sh` | Stats query |
| `modern_login_journey_test.py` | Modern SID login (accept + reject) vs real bnetd |

The journey test needs no running server — it spawns its own `bnetd`. The
`*-smoke.sh` tests assume a server is already up (see Running, above).

## Rules

- **`nc -z` is forbidden** as a readiness probe — use the actual client tool,
  or (for the journey test) wait for bnetd's `listening on …` log line
- Tests must be idempotent (can run multiple times without side effects)
- Tests must clean up any accounts/games they create
