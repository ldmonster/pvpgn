# E2E Tests

End-to-end tests drive a real `bnetd-v3` instance using the actual client tools
(`bnbot`, `bnchat`, `bnftp`, `bnstat`).

## Running

```bash
# Start bnetd-v3 first (e.g. via docker-compose)
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

## Rules

- **`nc -z` is forbidden** as a readiness probe — use the actual client tool
- Tests must be idempotent (can run multiple times without side effects)
- Tests must clean up any accounts/games they create
