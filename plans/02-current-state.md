# 02 — Current State (Honest Inventory)

This refactor is not greenfield. A large amount of the target is already in
place. This section records what is done, what is partial, and what is debt, so
the roadmap ([14-migration-roadmap.md](14-migration-roadmap.md)) only spends
effort where it is needed.

## 1. Layout that already exists

```
src/
  core/         9.5k LOC  strings,time,net,error,encoding,config,util,version  (no v3 deps)
  domain/       7.3k LOC  identity,chat,connection,gameplay,ladder,matchmaking,
                          moderation,realm,social,d2cs,d2dbs,shared
  application/ 14.2k LOC  use-case-per-file slices (auth,chat,game,ladder,
                          social,realm,moderation,profile,connection,persistence,…)
  protocol/    22.6k LOC  bnet,d2cs,d2dbs,d2gs,d2save,irc,telnet,wol,udp,file,common
  infra/       29.6k LOC  sqlite,mysql,postgres,persistence,net,crypto,config,log,
                          metrics,tracing,lua,plugin,session,migrations,…
  integration/  0.8k LOC  bnet,irc,telnet,wol  (legacy strangler, now thin)
  services/     0.7k LOC  bnetd,d2cs,d2dbs,combined  (composition roots)
  app/          9.7k LOC  bnetd,d2cs,d2dbs,pvpgn-config,pvpgn-migrate  (entrypoints)
  runtime/      3.2k LOC  process/event-loop runtime
  scripting/    0.8k LOC  plugin host glue
  tools/        6.6k LOC  bniutils,bnpass,bntrackd,conf_converter,client
  common/       0.4k LOC  setup_before.h / setup_after.h only  (was a huge grab-bag)
```

The **layering rule** (`core → domain → application/protocol → infra →
integration → services/app`) is enforced by `scripts/v3_layering_check.sh`, and
**domain purity** by `scripts/check_domain_purity.sh`.

## 2. What is effectively done

- **Hexagonal skeleton** with enforced layering and domain purity.
- **Strangler-fig mostly complete.** `src/common/` is reduced from a grab-bag to
  two setup headers (437 LOC). `src/integration/` legacy bridges are thin
  (814 LOC) and shrinking.
- **Use-case-per-file application layer** — strong SRP baseline.
- **DDD-shaped domain slices** — each context ships `account/aggregate`, `ports`,
  `events`, `errors`, `snapshot` headers.
- **Persistence consolidated** onto a single `IDbDriver` (per-backend SQLite
  repos deleted), instead of N parallel repository hierarchies.
- **Crypto foundation** in `core/crypto` + `infra/crypto` (argon2id policy, SRP
  vectors) replacing the dead legacy crypto chain.
- **Testing infrastructure present:** `tests/{unit,functional,integration,e2e,
  fuzz,bench,abi}`, 333 test files, a large green unit suite, sanitizer presets
  (`v3-asan/ubsan/tsan`), coverage preset, mutation pilot, microbench harness.
- **Local tooling rich:** `scripts/dev/` already has layering, purity, coverage,
  unit-pairing, ABI purity, bench-regression, config/doc generation, and
  legacy-linkage checks.
- **Docs:** `mkdocs.yml` + `docs/` with ADRs; generated config/plugin/Lua docs.

## 3. What is partial / env-gated

These are blocked locally by missing backends/runtimes (GCC 13, no `sqlite3.h`,
no libsodium, Lua OFF, no Docker). They are *designed* but not *runnable* here:

- **Multi-backend persistence parity.** SQLite repos deleted; **MySQL/Postgres
  deletion of the old path + a 3-driver contract-test matrix** still need real
  backends or testcontainers.
- **Crypto at-rest.** argon2id hashing policy and `hash_version` upgrade-on-login
  exist by reference; **wiring + SRP captures need libsodium and real clients.**
- **Observability export.** `[observability].sample_ratio` is wired; **OTLP/HTTP
  exporter + cross-service trace headers need libcurl + a collector.**
- **Plugin/Lua runtime.** ABI header + semver + purity gates done; **loader +
  shipped-plugin migration need the Lua runtime built.**
- **C++23 literal spellings.** `std::flat_map`/`std::print` shimmed behind
  fallbacks because libstdc++ 13 lacks them; flip to literal once the toolchain
  floor rises.

## 4. Remaining structural debt (the real work of this plan)

1. **Layering/purity allow-lists are non-empty.** Each entry is a known inward
   dependency to retire. Target: empty lists.
2. **`integration/` legacy bridges still exist** for `bnet/irc/telnet/wol` and
   `app/d2cs|d2dbs/.../legacy_*_bridges`, plus `infra/legacy_config`. These are
   the last strangler residue.
3. **Protocol layer is the largest and least-tested surface** (22.6k LOC). Codec
   coverage, fuzzing, and golden vectors are uneven across families.
4. **E2E coverage is smoke-level.** `scripts/v3-e2e-*.sh` exist but do not yet
   cover every client journey as repeatable, asserted scenarios.
5. **Ports may be coarse in places.** Audit for Interface-Segregation: split
   reader/writer and capability-specific ports where consumers differ.
6. **Tooling/`scripts/` sprawl.** Many one-shot migration scripts
   (`plan02_*`, `migrate_bridges_*`, `split_*`) are now dead; prune them
   (`check-scripts-orphans.sh` should drive this).
7. **Two parallel trackers** (`refactoring-progress*.md`, `planstwo/`) — collapse
   reporting into one tracker for this plan.

## 5. Honest framing

The architecture is sound; this plan is about **finishing**, **emptying the
allow-lists**, **raising the test floor (especially protocol + e2e)**, and
**making every invariant locally enforced** — not about redesigning what works.

## Definition of Done

- [ ] This inventory is reconciled against `git ls-files` and the layering/purity
      reports at plan start, and any drift corrected.
- [ ] A single tracker file is chosen; older trackers are archived under `docs/`.
