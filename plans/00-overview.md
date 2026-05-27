# PvPGN Refactoring Master Plan — Overview

**Status:** Draft, 2026-05-27
**Owner:** core maintainers
**Scope:** Whole repository, multi-release roadmap

## 1. Why this plan exists

The project is mid-flight on a strangler-fig migration from the legacy
C-style `src/bnetd`, `src/d2cs`, `src/d2dbs` trees into a layered
`src/v3/` tree (`core`, `domain`, `application`, `infra`, `integration`,
`protocol`, `runtime`, `scripting`, `services`, `app`). Rounds R201–R209
relocated ~15 k LOC out of `bnetd_legacy`. The migration works but the
end state, layering invariants, public API surface, plugin contract and
test/release pipeline are not yet codified. This document set is the
codification.

## 2. Guiding principles (binding for all sub-plans)

- **DDD** — model the business (Battle.net realm/clan/ladder/chat/game)
  in `src/v3/domain` as pure C++ types with no I/O, no globals, no
  singletons, no logging. Use cases live in `src/v3/application` and
  orchestrate domain objects through ports.
- **Hexagonal / ports & adapters** — `application` defines abstract
  ports; `infra` provides adapters (sqlite, mysql, file, lua, asio,
  fmt-log). `application` MUST NOT depend on `infra` (already enforced
  for new code; see `/memories` v3 layering rule).
- **YAGNI** — no speculative abstractions. A port is added only when a
  second adapter or a fake-for-tests is needed *now*.
- **KISS** — prefer flat module structure, free functions over
  classes-with-one-method, `std::expected`-style `core::Result` over
  exceptions for expected failure modes.
- **DRY** — one canonical implementation per concern (one packet
  framing, one config loader, one logging facade). Legacy duplicates
  must be deleted, not parallel-maintained.
- **Expandable** — public extension surface is the plugin ABI
  (`plugins/`), the Lua script API (`lua/`), the TOML config schema
  and the IPC/event bus. These are SemVer-governed.
- **Testable** — every new file ships with a Catch2 unit test. The
  test pyramid (unit / functional / e2e / fuzz) is mandatory.
- **Modern** — target C++20 (concepts, ranges, `<format>`, `<span>`,
  designated initialisers, `consteval`). Drop bespoke utilities that
  the standard library now provides.

## 3. End-state architecture (target)

```
+-------------------------------------------------------------+
|  app/  (thin main()s: bnetd, d2cs, d2dbs, single-binary)    |
+-----------------------------+-------------------------------+
                              |
+-----------------------------v-------------------------------+
|  services/  (composition root: builds Application from cfg) |
+-----------------------------+-------------------------------+
                              |
+-----------------------------v-------------------------------+
|  application/  (use cases, ports, command/query handlers)   |
+--+----------------------+-----------------+-----------------+
   |                      |                 |
+--v---+        +---------v-------+   +-----v-----+
|domain|        | protocol (codecs|   | scripting |
|      |        | bnet/irc/telnet)|   | (lua API) |
+------+        +-----------------+   +-----------+
                          ^                  ^
+-------------------------+------------------+----------------+
|  infra/  (adapters: sqlite, mysql, asio, file, fmt-log,     |
|          metrics, tracing, sandbox, lua-vm, crypto)         |
+-------------------------------------------------------------+
                              |
+-----------------------------v-------------------------------+
|  integration/  (legacy_bnetd, legacy_d2cs, legacy_d2dbs --  |
|  shrinking; bnet/irc/telnet/wol -- v3-native)               |
+-------------------------------------------------------------+

  core/  (header-only utilities: Result, Bytes, Clock, EventBus,
         Logging, Strong typedefs, Endian, Hexdump, Format)
```

Invariants (CI-enforced — see `12-build-tooling-ci.md`):

1. `core` depends on nothing else.
2. `domain` depends only on `core`.
3. `application` depends on `core`, `domain`, `protocol`, `scripting`
   (interfaces only). NEVER on `infra`.
4. `infra` may depend on `core`, `domain`, `application` (to implement
   port interfaces).
5. `integration_legacy_*` are the only libraries allowed to include
   legacy headers via `setup_before.h` / `setup_after.h` brackets.
6. `app/*` and `services/*` are the only places `main()` symbols live.

## 4. Sub-plans (read in order)

| # | File | Theme |
|---|------|-------|
| 01 | [01-modern-cpp-baseline.md](01-modern-cpp-baseline.md) | C++20, stdlib adoption, header hygiene |
| 02 | [02-finish-strangler-fig.md](02-finish-strangler-fig.md) | Finish legacy → v3 relocation |
| 03 | [03-domain-purification.md](03-domain-purification.md) | Pure domain, value objects, invariants |
| 04 | [04-application-use-cases.md](04-application-use-cases.md) | Use-case handlers, CQRS-lite |
| 05 | [05-ports-and-adapters.md](05-ports-and-adapters.md) | Hexagonal port discipline |
| 06 | [06-protocol-and-codecs.md](06-protocol-and-codecs.md) | Safe packet codecs, fuzz targets |
| 07 | [07-persistence-and-migrations.md](07-persistence-and-migrations.md) | Repositories, schema migrations |
| 08 | [08-error-handling-and-logging.md](08-error-handling-and-logging.md) | Result/StatusCode, structured logs |
| 09 | [09-config-and-secrets.md](09-config-and-secrets.md) | TOML, env overrides, secrets handling |
| 10 | [10-observability.md](10-observability.md) | Metrics, tracing, healthchecks |
| 11 | [11-testing-strategy.md](11-testing-strategy.md) | Pyramid, fuzz, property-based |
| 12 | [12-build-tooling-ci.md](12-build-tooling-ci.md) | Presets, sanitizers, lint gates |
| 13 | [13-plugin-and-scripting.md](13-plugin-and-scripting.md) | Plugin ABI + Lua API SemVer |
| 14 | [14-legacy-retirement.md](14-legacy-retirement.md) | Deletion roadmap for `src/bnetd|d2cs|d2dbs` |
| 15 | [15-release-and-versioning.md](15-release-and-versioning.md) | SemVer, deprecation policy |
| 16 | [16-execution-roadmap.md](16-execution-roadmap.md) | Round-by-round sequencing |

## 5. Definitions

- **Round** — one self-contained commit/PR with a checklist file in
  `plans/r###-checklist.md` (current convention, e.g. R209). Each round
  must land GREEN: build + ctest + v3-compose smoke + sanitizer subset.
- **Strangler-fig op** — `extern "C" int pvpgn_v3_<op>_try(...)` symbol
  exposed by `integration_legacy_bnetd_linked`, called from a guarded
  legacy site, falling back to legacy on non-zero return. Pattern is
  fixed; see `/memories/windows-tooling.md`.
- **GREEN** — the literal pass criterion: `ctest -C Release` zero
  failures AND `scripts/dev/v3-compose-smoke.sh` reports
  `R197 compose smoke GREEN`.

## 6. Non-goals

- Rewriting the on-wire BNet 1.x protocol — wire-compat with Diablo II
  / WC3 / Starcraft retail clients is sacred.
- Switching the build system away from CMake.
- Adopting C++23 features that are not yet shipped by MSVC 19.38 (the
  pinned floor — see `01-modern-cpp-baseline.md`).
- Replacing TOML with YAML / JSON / dhall / etc.
