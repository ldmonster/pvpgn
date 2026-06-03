# 06 — Infrastructure & Adapters

`infra/` is where the program is finally allowed to touch the outside world:
databases, sockets, crypto libraries, the filesystem, the clock, logging,
metrics, the Lua VM, and plugins. Everything here is an **adapter implementing a
port** defined further inward. Nothing here contains business rules.

## 1. Persistence (the biggest adapter family)

Target: **one** driver abstraction, **many** backends.

- `core`/`domain` define repository and `UnitOfWork` ports.
- `infra/persistence` holds backend-agnostic repository logic over a single
  `IDbDriver` interface.
- `infra/{sqlite,mysql,postgres}` each implement `IDbDriver` only — they hold
  connection handling and SQL dialect, nothing else.
- `infra/inmemory` implements the same repository ports for tests (and is the
  *only* infra module that test code in `domain`/`application` may link).
- `infra/migrations` owns schema versioning, applied uniformly across backends.

This replaces the historical N-parallel-repository design (already largely done
for SQLite). **Open/Closed:** adding Postgres support is a new `infra/postgres`
target + a registration line in a composition root, touching nothing inward.

### Persistence tasks
1. Finish deleting the old per-backend MySQL/Postgres repository code in favour
   of `IDbDriver` (env-gated: needs those backends locally).
2. Make all three backends pass the **same** repository contract test suite
   (LSP); see [09-testing-strategy.md](09-testing-strategy.md).
3. Keep SQL/dialect strings in one place per backend; no SQL anywhere outside
   `infra/{backend}`.

## 2. Networking & async I/O

- Replace the hand-rolled `fdwatch` with **one** vetted async runtime under
  `infra/net` (and the `runtime/` event loop), behind a transport port.
- Protocol handlers never see raw fds; they see decoded frames and a `Responder`
  port. Connection lifecycle is a domain state machine (`domain/connection`)
  driven by transport events.
- Backpressure, timeouts, and idle-connection reaping are adapter concerns with
  a measurable memory-footprint regression gate (already piloted) run locally.

## 3. Crypto

- `core/crypto` defines algorithm-agnostic interfaces (hasher, SRP, RNG).
- `infra/crypto` implements them with a **vetted library** (argon2id for
  password-at-rest; SRP for the Battle.net NLS login).
- A `hash_version` field + an **upgrade-on-login** policy rehashes old
  credentials transparently (Open/Closed: new algorithm = new version, no data
  migration outage).
- The dead legacy crypto chain is deleted (done). No `std::rand` anywhere; RNG
  is a port backed by a CSPRNG.
- Env-gated: argon2id wiring + SRP capture tests need libsodium + real clients.

## 4. Configuration

- One config model in `core/config`; one parser in `infra/config` (TOML), with a
  thin `infra/legacy_config` shim for old `.conf` files that translates → the
  new model at the boundary (anti-corruption) and is slated for deletion once
  migration tooling (`app/pvpgn-migrate`, `tools/conf_converter`) is the only
  supported path.
- Config is **immutable after load** and injected; no module reads global config
  state. Reference docs are generated from the schema
  (`gen-config-docs.sh`) and kept in sync by `check-config-reference-sync.sh`.

## 5. Logging, metrics, tracing

- Defined as ports in `application`/`core`; implemented in
  `infra/{log,metrics,tracing}`.
- Domain has **zero** logging. Application logs through a port. Infra adapters
  may log operationally.
- Tracing: `[observability].sample_ratio` is wired; OTLP/HTTP export is an
  adapter (env-gated: needs libcurl + a collector). Trace context propagates
  across the three daemons via protocol headers at the integration seam.

## 6. Scripting & plugins

- `infra/lua` hosts the Lua VM behind a scripting port; scripts cannot reach
  domain internals except through the published script API.
- `infra/plugin` loads native plugins through a **semver'd pure-C ABI**
  (`include/pvpgn/plugin`). The ABI purity gate
  (`check-plugin-abi-purity.sh`) forbids C++/domain/application symbols from
  leaking across the boundary — keeping plugins decoupled and the core free to
  change (Open/Closed for third parties).

## 7. The adapter contract

Every adapter must:
- implement exactly one port (or a small cohesive set), nothing more;
- contain no branching on business rules (that's a smell of leaked domain
  logic — push it inward);
- be substitutable for its port under the contract tests (LSP);
- be wired only at a composition root, never self-register via a global.

## Definition of Done

- [ ] All persistence goes through `IDbDriver`; no per-backend repository
      hierarchy remains; the three backends pass one shared contract suite.
- [ ] No raw fd handling outside `infra/net`/`runtime`; one async runtime.
- [ ] No `std::rand`; all randomness via the RNG port; password-at-rest is
      argon2id with `hash_version` upgrade-on-login.
- [ ] No module reads global config; config is injected and immutable.
- [ ] Plugin ABI purity gate green; no domain/application symbols cross the C
      ABI.
