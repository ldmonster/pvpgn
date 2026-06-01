# 00 — Overview (Wave Two)

## Where we are after Wave One

- v3 binaries (`bnetd`, `pvpgn-migrate`, `pvpgn-config`) are the supported
  entry points. The `bnetd-v3` name has been retired.
- `src/{core,domain,application,infra,integration,protocol,app,services,runtime,scripting}`
  is the canonical layout. Layering is enforced in CI.
- Config is TOML-first. Lua lives under `scripts/lua/` and feature Lua
  has been migrated into `plugins/`.
- Observability surface (`/healthz`, `/readyz`, `/metrics`) ships;
  metrics registry and structured logger are in `core/`.
- Plugin ABI conformance tests, Lua API v2 conformance tests, and TOML
  schema versioning are wired into CI.
- MSVC `/WX` is clean across the whole tree.

## What is still parallel / legacy

- `src/integration/legacy_bnetd/` — still ≈ 170 translation units, gated
  by per-feature `pvpgn_v3_*` bridge symbols.
- `src/integration/legacy_d2cs/` and `src/integration/legacy_d2dbs/` —
  not yet stranglered.
- `src/common/` — 80+ files of pre-C++20 utilities (`addr`, `conf`,
  `eventlog`, `fdwatch`, `hashtable`, `list`, `network`, `packet`,
  `tag`, `util*`, `xstring`, custom hashes). v3 sources no longer call
  most of these, but they are kept alive by the legacy tree.
- `application/ports/` exists despite Plan 07 saying it should not —
  duplicated ports for `realm_repository`, `permission_checker`,
  `command_registry`. Needs collapse into per-context ports.
- Infra adapters (`infra/sqlite`, `infra/mysql`, `infra/postgres`) carry
  three near-identical repository implementations per aggregate.
- `fdwatch` (epoll / kqueue / poll / select) is still the I/O loop;
  there is no executor abstraction.
- Custom password hashing (`bnethash`, `wolhash`) and a hand-rolled SRP
  implementation (`bnetsrp3`) live in `src/common/`.

## What "done" looks like at the end of Wave Two

1. `src/integration/legacy_*` is empty or holds only ABI shims that
   forward to v3.
2. `src/common/` contains only protocol-fixed wire constants
   (`bnet_protocol/`, `*_protocol.h`); everything else moved into
   `core/` or `infra/` or deleted.
3. One repository implementation per aggregate, with the storage
   backend chosen at composition time.
4. The I/O loop is `std::execution` / `asio` (one choice, picked in
   plan 06); no platform `#ifdef` outside `infra/net/`.
5. Crypto uses libsodium for password hashing (argon2id) and a vetted
   SRP-6a implementation; the wire layout stays bit-compatible with
   shipped clients.
6. CI gates: layering, MSVC `/WX`, `mkdocs --strict`, unit-coverage
   threshold, ASan + UBSan + TSan matrix, fuzz smoke, plugin ABI
   conformance, performance regression budget.
7. `bnetd --version` ships with a deprecation policy and a
   documented rolling-upgrade path.

## Non-goals (still)

- Rewriting wire protocols.
- Replacing Lua.
- New game support.
- Rewriting in another language.

## Sequencing principle

Wave two is **bottom-up**: kill the legacy floor (`common`, legacy
integration trees) before lifting the ceiling (C++23, async runtime).
Each plan file declares its prerequisites explicitly so work can be
parallelized where the graph allows.
