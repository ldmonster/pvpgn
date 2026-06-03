# 04 — Domain Layer (DDD)

The domain is the heart of the refactor: pure C++ encoding Battle.net rules,
with **no** I/O, logging, clocks, or global state. Everything else exists to
feed and serve it.

## 1. Bounded contexts

Each directory under `src/domain/` is a bounded context with its own ubiquitous
language and published surface:

| Context | Owns |
|---------|------|
| `identity` | accounts, credentials, attributes, account lifecycle |
| `connection` | client session state machine, connection lifecycle |
| `chat` | channels, messages, membership, moderation of speech |
| `social` | friends, clans, mutual presence |
| `gameplay` | game rooms, game lifecycle, results reporting |
| `matchmaking` | anongame/automatch queues and pairing |
| `ladder` | rankings, rating updates, ladder queries |
| `moderation` | bans, mutes, IP/account restrictions |
| `realm` | Diablo II realm registration and routing |
| `d2cs` / `d2dbs` | D2 character server / database server rules |
| `shared` | the **published kernel**: ids, value objects, common errors/events |

The `shared` kernel is deliberately tiny. Anything placed there is a contract
every context depends on, so additions require review.

## 2. Building blocks (the DDD vocabulary we use)

- **Value objects** — immutable, equality-by-value: `AccountName`,
  `ChannelName`, `Rating`, `SessionId`, `RealmName`. They validate on
  construction; an invalid value object cannot exist.
- **Entities & aggregates** — identity-bearing, mutable through methods that
  preserve invariants: `Account` (aggregate root of `identity`), `Channel`,
  `Game`, `LadderEntry`. Callers cannot mutate fields directly.
- **Domain services** — stateless operations that don't belong to one entity
  (e.g. rating computation, pairing rules).
- **Domain events** — `domain/<ctx>/events.hpp`: `AccountCreated`,
  `PasswordChanged`, `ChannelJoined`, `GameEnded`. The inter-context seam.
- **Ports** — `domain/<ctx>/ports.hpp`: the *outbound* interfaces the domain
  needs (e.g. an `AccountRepository` read/write port, a `Clock`, an
  `EventPublisher`). Defined here, implemented in `infra`.
- **Errors** — `domain/<ctx>/errors.hpp`: typed, exhaustive error enums returned
  via `std::expected`/result types — not exceptions for expected failures.

## 3. Invariants live in aggregates

Rules are enforced where the data lives, not in callers:

- An `Account` cannot be created with an invalid name or empty credential.
- A `Channel` enforces capacity, permission, and name rules on join/speak.
- A `Game` transitions only through legal lifecycle states.
- A `LadderEntry` rating update is monotonic per the rating rules.

The test of success: it is *impossible* to construct an invalid aggregate or to
drive one into an illegal state through its public API. Application code never
re-checks an invariant the aggregate already guarantees (DRY).

## 4. Purity rules (mechanically enforced)

`scripts/check_domain_purity.sh src/domain` forbids, in `domain/`:

- `#include` of `infra/…`, `<iostream>`, `<fstream>`, `<cstdio>`, `<ctime>`;
- `std::cout/cerr/cin`, `printf`-family;
- `system_clock::now()` (time is injected via a `Clock` port);
- `static … g_` global mutable state;
- `spdlog::` / any logging library.

If the domain needs the time, a random value, an id, or to emit an event, it
asks through a **port**; the composition root supplies the real thing and tests
supply a fake.

## 5. Modelling tasks for this plan

1. **Audit each context for an anemic model.** Where logic that protects an
   invariant lives in an application use-case instead of the aggregate, move it
   inward. (Common offenders: name validation, state-transition guards.)
2. **Promote primitive obsession to value objects.** Replace raw `std::string`
   account/channel names and raw ints for ratings/ids with validated value
   objects in `shared`/per-context.
3. **Make events the only cross-context link.** Remove any include of one
   context's headers from another; replace with an event + a handler in the
   application layer.
4. **Exhaustive typed errors.** Ensure every fallible domain operation returns a
   typed result; ban "return false / log and continue."
5. **Document the ubiquitous language** per context in `docs/domain/<ctx>.md`,
   generated/curated alongside the headers.

## 6. Testability target

- Every aggregate has a unit test asserting each invariant and each illegal
  transition is rejected.
- Every domain service has property-style tests over its rules (e.g. rating
  update bounds).
- Domain tests **link only `core` + the context under test** — if a domain test
  needs `infra`, the design is wrong.

## Definition of Done

- [ ] `check_domain_purity.sh src/domain` is green.
- [ ] No `domain/<a>` includes `domain/<b>` internals (cross-context coupling is
      events-only); a grep-based check confirms it.
- [ ] Each context has value objects for its identifiers (no raw-string ids in
      public domain APIs).
- [ ] Each aggregate and domain service has a paired unit test
      (`check-unit-pairing.sh` green for `domain/`).
- [ ] `docs/domain/<ctx>.md` exists for every context.
