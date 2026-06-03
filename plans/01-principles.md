# 01 — Principles, Made Concrete

Generic principles are useless until they name files. This section binds
SOLID/DDD/DRY/KISS/YAGNI to PvPGN's actual layers and gives each a *local,
checkable* rule.

## SOLID

### S — Single Responsibility
- **Rule:** one use-case per file in `src/application/<context>/src/*.cpp`, one
  reason to change per class. `application/auth/` already models this
  (`login_user`, `create_account`, `change_password`, `logout_user`, …). Extend
  it everywhere.
- **Smell:** a `*_manager`/`*_handler` class with unrelated public methods, or a
  `.cpp` over ~400 lines mixing parsing + rules + persistence.
- **Check:** `check-unit-pairing.sh` (every use-case file has a sibling test);
  manual review flags multi-purpose classes.

### O — Open/Closed
- **Rule:** new storage backends, protocol revisions, auth schemes, and
  scripting hooks are added by implementing a port / registering an adapter —
  **not** by editing a `switch` in core logic.
- **Pattern:** the consolidated `IDbDriver` (one driver, many backends) and the
  plugin C ABI are the templates. Protocol dispatch uses a registry keyed by
  message id, not a growing conditional.
- **Check:** adding a backend touches only `infra/<backend>/` + one composition
  root; `git diff --stat` of such a change stays inside those directories.

### L — Liskov Substitution
- **Rule:** every adapter is fully substitutable for its port. Contract tests
  (see [09-testing-strategy.md](09-testing-strategy.md)) run the *same* test
  body against every adapter (`sqlite`, `mysql`, `postgres`, `inmemory`).
- **Smell:** an adapter that throws "not supported" for a port method, or whose
  behaviour differs by capability — split the port instead.

### I — Interface Segregation
- **Rule:** ports are small and role-specific. Prefer `AccountReader` +
  `AccountWriter` over a god `AccountStore` when consumers differ.
- **Check:** no port interface has methods that *no single consumer* uses
  together; review at port-definition time.

### D — Dependency Inversion
- **Rule:** the dependency arrow points inward. `application` depends on
  `domain` ports; `infra` implements them. **`application` must never include
  `infra`.**
- **Check:** `scripts/v3_layering_check.sh` enforces this mechanically; the
  allow-list only shrinks.

## DDD (Domain-Driven Design)

- **Bounded contexts** are the directories under `src/domain/`: `identity`,
  `chat`, `connection`, `gameplay`, `ladder`, `matchmaking`, `moderation`,
  `realm`, `social`, `d2cs`, `d2dbs`, plus `shared` for the published kernel.
- **Ubiquitous language:** names in code match the Battle.net domain (account,
  channel, realm, ladder, clan, anongame, tournament). No `Manager`, `Util`, or
  `Helper` in domain names.
- **Aggregates & invariants** live in the domain and protect themselves;
  callers cannot put an aggregate into an invalid state.
- **Domain events** (`domain/identity/events.hpp` pattern) are the integration
  seam between contexts — contexts react to events, they do not reach into each
  other's internals.
- **Ports** (`domain/<ctx>/ports.hpp`) and **errors**
  (`domain/<ctx>/errors.hpp`) are part of the context's published surface.
- **Anti-corruption:** legacy on-disk formats and wire structs are translated at
  the `infra`/`protocol` boundary, never leaked inward.
- See [04-domain-layer.md](04-domain-layer.md) for the full model.

## DRY

- **Rule:** one home for each concept. Cross-cutting primitives live in `core/`
  (strings, time, net, error, encoding, config) and are reused, not re-pasted.
- **The big DRY win** — collapsing per-daemon duplication of `bnetd`/`d2cs`/
  `d2dbs` and per-backend repositories — is mostly done; finish it
  ([02-current-state.md](02-current-state.md)).
- **Tripwire:** `jscpd`/manual duplication review on `src/` flags copy-paste
  blocks > 30 lines; any duplication must be justified or hoisted.
- **Caveat:** DRY is about *knowledge*, not coincidental similarity. Two
  protocol codecs that happen to look alike but evolve independently stay
  separate (this is also KISS).

## KISS

- **Rule:** the simplest construct that satisfies the test wins. No template
  metaprogramming, no inheritance trees deeper than 1, no premature plugin
  points.
- **Replace cleverness with the standard library:** hand-rolled `hashtable`,
  `xstring`, `fdwatch`, and bespoke parsers are replaced by `std::`
  containers/strings and a single vetted async/runtime layer.
- **Check:** functions stay short and single-exit-ish; cyclomatic complexity
  reviewed at PR time; no new bespoke container types.

## YAGNI

- **Rule:** do not build for imagined futures. One backend? No driver
  abstraction yet. One protocol revision? No version-negotiation framework yet.
- **Concretely for this plan:** abstractions are introduced only at the moment a
  *second real consumer* lands. Speculative ports, config knobs, and
  "extensibility hooks" are deleted.
- **Check:** every new interface in a PR names ≥ 2 implementers or consumers; if
  it names one, inline it.

## Precedence when principles collide

`KISS/YAGNI` > `DRY`. If de-duplicating couples two things that should evolve
independently, leave the duplication and write a comment explaining why. The
goal is *changeability*, not abstraction for its own sake.

## Definition of Done

- [ ] Each principle above has a named, runnable check or a named review step.
- [ ] The precedence rule is documented in `CONTRIBUTING`/`docs/` so reviewers
      apply it consistently.
