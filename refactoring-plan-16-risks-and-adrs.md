# 16 · Risks, Trade-offs, ADRs

## 1. Risks register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | Boost.Fiber stack overhead × thousands of sessions exhausts memory. | Med | High | Profile-driven stack sizing (start at 32 KiB, pooled, segmented stacks where available); cap concurrent sessions per config; metric `pvpgn_fiber_pool_size` alerting. |
| R2 | Subtle behavioural drift between legacy and v3 handlers breaks live games. | High | High | Shadow-mode comparator in dev/canary; protocol replay tests as merge gate; phased flag flip with rollback within minutes. |
| R3 | SQL migrations corrupt existing databases. | Low | Critical | Migrations atomic + checksummed + tested on snapshots of real datasets; mandatory pre-migration backup tool; staging environment in CI. |
| R4 | Lua 5.4 transition breaks community scripts. | High | Med | Compat shim; documentation; one-major deprecation window; bundled converter. |
| R5 | Embedded WebUI introduces auth bypass / XSS in the admin surface. | Med | Critical | Strict CSP, JWT validation tests, fuzz on parser, OWASP ZAP in CI, dependency audit; default bind to `127.0.0.1`. |
| R6 | Refactor never finishes (typical for big rewrites). | Med | Critical | Strangler pattern with merge gates per phase; no parallel maintainers required; legacy code keeps compiling throughout. |
| R7 | Win32 SCM/service path regresses. | Med | Med | Win CI matrix; smoke tests on Server 2022; keep `win_service.cpp` thin and isolated. |
| R8 | Boost dependency footprint balloons binary size. | Med | Low | Static link only needed Boost libs; LTO; measure & cap in CI (artifact size budget). |
| R9 | Performance regression on the network hot-path (per-fiber overhead vs `fdwatch` loop). | Med | Med | Bench harness comparing v2/v3 throughput; investigate batched send via `boost::asio::scatter`. |
| R10 | Maintainer fatigue around CI flakes (sanitizers, fuzz). | Med | Med | Retry policy on flaky tests; quarantine label; dedicated nightly job for slow checks. |
| R11 | Scripting capability gate bypass. | Med | High | Sandbox enforced at both `sol3` registration *and* runtime check inside use-cases (defence-in-depth). |
| R12 | Public read-only widgets become a DDoS vector. | Med | Low | Caching headers; per-IP rate limit; option to disable. |
| R13 | Lost knowledge of legacy quirks (e.g. specific BNet packet edge cases). | High | Med | Replay corpus grows continuously; documented quirks in `docs/quirks.md`; every removed legacy file gets a "what it did" summary in commit. |

## 2. Trade-offs explicitly accepted

* **Compile time** will grow (Boost.Asio + Beast + Fiber are heavy
  templates). Mitigation: precompiled headers, unity builds in CI,
  `ccache`/`sccache` everywhere.
* **Binary size** grows from ~3 MiB to ~10–15 MiB (statically linked).
  Acceptable for a server daemon.
* **Some legacy admin tools (`bnproxy`, `bnpcap`) are dropped** rather
  than carried forward. Anyone needing them can keep the 2.x branch.
* **Single-process scaling cap** stays — no multi-master clustering.
  HA strategies are deferred.

## 3. Architecture Decision Records (ADR index)

Stored under `docs/adr/`, MADR format. Initial set:

* ADR-0001 — Adopt hexagonal/DDD layering.
* ADR-0002 — C++20, drop C++11 baseline.
* ADR-0003 — Boost.Asio + Boost.Fiber for concurrency.
* ADR-0004 — `spdlog` replaces `eventlog`.
* ADR-0005 — `toml++` for configuration; drop pugixml.
* ADR-0006 — `sol3` + Lua 5.4 for scripting.
* ADR-0007 — SQLite is the new default storage; file backend retained
  for parity.
* ADR-0008 — Schema migrations stored as versioned `.sql` files.
* ADR-0009 — Embedded WebUI via Boost.Beast; assets bundled into the
  binary.
* ADR-0010 — Strangler-fig migration with shadow-mode parity gates.
* ADR-0011 — Drop `bnproxy`/`bnpcap`; archive `client/bnbot` in favor
  of the WebUI live console.
* ADR-0012 — Single composition root per binary; no service locator.
* ADR-0013 — All persistence uses repository + snapshot pattern.
* ADR-0014 — Domain layer forbids `std::chrono::system_clock::now()`
  direct usage; injected `IClock` only.
* ADR-0015 — Prometheus is the metrics format of record.
* ADR-0016 — Replace `t_storage` function-pointer "vtable" with
  per-aggregate polymorphic repositories.
* ADR-0017 — JWT (HS256 default, RS256 optional) for WebAPI auth.
* ADR-0018 — Capability-based plugin sandbox.
* ADR-0019 — `git` as the source of truth for `bnetd.toml` schema;
  validation in CI.
* ADR-0020 — Mutual TLS between bnetd↔d2cs↔d2dbs.

Each ADR records: context, decision, alternatives considered,
consequences, status (proposed/accepted/superseded).

## 4. What "done" looks like

The refactor is **complete** when:

1. `src/legacy/` is empty.
2. `bnetd`, `d2cs`, `d2dbs` are tiny `main.cpp` files over
   `runtime/`.
3. Unit/integration/protocol-replay test coverage:
   * `domain` ≥ 80 % line coverage,
   * `application` ≥ 75 %,
   * `protocol` ≥ 70 % + fuzz corpus with no crashes for 24 h.
4. CI matrix is green on all OS/compiler/sanitizer combinations.
5. Default install (`apt install pvpgn` / `docker run pvpgn`) brings
   up a working server with WebUI on `127.0.0.1:8086`, SQLite
   backend, metrics on `:9100`, no manual config required.
6. A user from the old version can `pvpgn upgrade --from 2.x` and
   migrate flat-file accounts into SQLite in one step.
7. The official Lua scripts continue to function unchanged.
8. New plug-ins can be written in either Lua or C++ against a
   versioned API.

## 5. Estimated relative effort (T-shirt sizing)

| Phase | Size |
|---|---|
| 0 Pre-work                     | S |
| 1 Stabilise & abstract         | M |
| 2 Network spine (Asio/Fiber)   | L |
| 3 Domain extraction            | XL |
| 4 Protocol decoupling          | XL |
| 5 Persistence overhaul         | L |
| 6 WebUI + observability        | L |
| 7 Scripting & plug-ins         | M |
| 8 d2cs/d2dbs alignment         | M |
| 9 Clean-up & 4.0 prep          | S |

Each phase is independently shippable; users can adopt any released
intermediate version.

---

> End of plan. See:
> [refactoring-plan-00-overview.md](refactoring-plan-00-overview.md) ·
> [refactoring-plan-01-current-architecture.md](refactoring-plan-01-current-architecture.md) ·
> [refactoring-plan-02-target-architecture.md](refactoring-plan-02-target-architecture.md) ·
> [refactoring-plan-03-domain-model.md](refactoring-plan-03-domain-model.md) ·
> [refactoring-plan-04-application-layer.md](refactoring-plan-04-application-layer.md) ·
> [refactoring-plan-05-infrastructure.md](refactoring-plan-05-infrastructure.md) ·
> [refactoring-plan-06-networking-and-boost-fiber.md](refactoring-plan-06-networking-and-boost-fiber.md) ·
> [refactoring-plan-07-protocols.md](refactoring-plan-07-protocols.md) ·
> [refactoring-plan-08-bnetd-refactor.md](refactoring-plan-08-bnetd-refactor.md) ·
> [refactoring-plan-09-d2cs-d2dbs-refactor.md](refactoring-plan-09-d2cs-d2dbs-refactor.md) ·
> [refactoring-plan-10-scripting-and-plugins.md](refactoring-plan-10-scripting-and-plugins.md) ·
> [refactoring-plan-11-testing-strategy.md](refactoring-plan-11-testing-strategy.md) ·
> [refactoring-plan-12-build-tooling-ci.md](refactoring-plan-12-build-tooling-ci.md) ·
> [refactoring-plan-13-webui.md](refactoring-plan-13-webui.md) ·
> [refactoring-plan-14-observability.md](refactoring-plan-14-observability.md) ·
> [refactoring-plan-15-migration-roadmap.md](refactoring-plan-15-migration-roadmap.md) ·
> [refactoring-plan-16-risks-and-adrs.md](refactoring-plan-16-risks-and-adrs.md)
