# 05 — Application Layer (Use-Cases)

The application layer is thin orchestration: it receives an intent (from a
protocol handler or tool), loads aggregates through ports, calls domain
methods, persists results, and publishes events. It contains **no business
rules** (those are in the domain) and **no I/O mechanics** (those are in infra).

## 1. Use-case per file (SRP)

The established pattern under `src/application/<context>/src/` is one use-case
per file, each a small callable object with a single `execute`/`operator()`:

```
application/auth/src/
  create_account.cpp        login_user.cpp        change_password.cpp
  login_user_nls.cpp        logout_user.cpp       account_lock.cpp
  list_sessions.cpp         lookup_account.cpp     password_upgrade.cpp
  permission_checker.cpp    login_classifier.cpp   password_rotation_observer.cpp
```

This is the model for **every** context (`chat`, `game`, `ladder`, `social`,
`realm`, `moderation`, `profile`, `connection`, `tournament`, …). One file →
one reason to change → one paired test.

## 2. Use-case shape

A use-case:
1. takes a **command/query struct** (validated, primitive-free input);
2. holds its dependencies as **constructor-injected ports** (no globals, no
   singletons, no `infra` includes);
3. returns a typed `std::expected<Result, Error>` — never logs-and-swallows;
4. is **deterministic** given its ports (time, ids, randomness are ports too).

```cpp
// application/auth/include/application/auth/create_account.hpp
class CreateAccount {
public:
  CreateAccount(AccountRepository&, PasswordHasher&, Clock&, EventPublisher&);
  std::expected<AccountSnapshot, CreateAccountError> execute(CreateAccountCommand);
};
```

Notice every collaborator is a **port** defined in `domain`/`application`, not a
concrete `infra` type. That is the whole game: the use-case is unit-testable
with fakes and the composition root supplies real adapters.

## 3. The no-infra rule

`application` MUST NOT `#include` `infra`. This is the single most important
enforced boundary (`v3_layering_check.sh`). Consequences:

- Application code names `AccountRepository`, not `SqliteAccountRepository`.
- Transaction control is expressed via a `UnitOfWork`/transaction port, not by
  calling a SQL `BEGIN`.
- Logging/metrics from a use-case go through an injected observability port, not
  `spdlog::` directly.

## 4. Transactions & consistency

- A use-case that mutates more than one aggregate runs inside a `UnitOfWork`
  port boundary; the adapter maps it to a backend transaction.
- Cross-aggregate / cross-context consistency is **eventual**, via domain events
  handled by other use-cases — not by reaching across contexts synchronously.
- Idempotency: commands that a client may retry (login, join) are written to be
  safely repeatable.

## 5. CQRS-lite

Separate **commands** (mutate, return minimal result) from **queries** (read,
return projections). Queries may use a dedicated read port that is allowed to be
a thin, denormalized projection for performance — but still a port, still
testable. Don't over-engineer this into full event-sourcing (YAGNI); the split
is organizational, not infrastructural.

## 6. Tasks for this plan

1. **Inventory every protocol handler** in `protocol/` and ensure each maps to a
   named application use-case rather than doing work inline. Handlers become
   *adapters*: parse → build command → call use-case → encode response.
2. **Extract any rule still living in a handler or in `integration/`** into a
   use-case (or deeper, into the domain).
3. **Define the port set per context** and audit for Interface Segregation:
   split read/write, split capability-specific ports where consumers differ.
4. **Replace any singleton/global access** in application code with constructor
   injection from the composition root.
5. **Add the `UnitOfWork`/transaction port** where multi-aggregate writes exist.

## 7. Testability target

- Every use-case has a unit test that drives it with in-memory fakes for all
  ports and asserts both success and each typed error path.
- Use-case tests **do not link `infra`**; they link `core`, `domain`,
  `application`, and the in-memory fakes from `infra/inmemory` (the one infra
  module tests may use, by design).
- `check-unit-pairing.sh` reports no unpaired use-case file.

## Definition of Done

- [ ] No file under `src/application/` includes `infra/` (layering check green
      with empty allow-list for application).
- [ ] Every protocol handler delegates to a named use-case; no business logic in
      `protocol/` or `integration/`.
- [ ] Every use-case returns a typed result and has success + error-path unit
      tests with fakes.
- [ ] Ports audited for ISP; no god-port remains where consumers differ.
