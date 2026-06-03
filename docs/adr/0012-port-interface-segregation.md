# ADR 0012: Port Interface Segregation (ISP)

**Date**: 2026-06-03
**Status**: Accepted
**Deciders**: PvPGN Core Team

## Context

Milestone 3 ([plans/05-application-layer.md](../../plans/05-application-layer.md),
[plans/14-migration-roadmap.md](../../plans/14-migration-roadmap.md) §M3) calls
for auditing the domain/application **ports** for the Interface Segregation
Principle: "no client should be forced to depend on methods it does not use."

An audit (method counts of every `I*` port in `src/domain` + `src/application`)
found the fat ports below. The two structural smells are **(a)** a
unit-of-work god-port and **(b)** repositories that bundle reads and writes,
forcing read-only consumers to depend on mutation methods.

| Port | Methods | Smell |
|------|--------:|-------|
| `IUnitOfWork` | 13 | 3 transaction methods + **10 repository accessors**; any consumer needing one repo depends on all ten contexts |
| `IIpBanRepository` | 8 | read (find/contains/for_each) + write (add/remove/clear) bundled |
| `IConnectionContext` | 7 | connection I/O + **game-lifecycle callbacks** (`on_game_*`) bundled |
| `IAccountRepository` | 6 | `find_by_id`/`find_by_name`/`forEach`/`size` (read) + `save`/`remove` (write) |
| `IChannelRepository` | 6 | read + write bundled |
| `IRealmRepository`, `IGameRepository`, `IClanRepository`, `ISessionRegistry`, `ICharacterSaveRepository` | 5 | read + write bundled |

Concrete read-only consumers that should *not* depend on writes already exist —
e.g. `application::auth::InMemoryPermissionChecker` and `application::social::
ListFriends` use only `IAccountRepository::find_by_id`; `application::ladder::
GetLadderEntry`/`GetLadderPage` only read.

## Decision

1. **`IUnitOfWork` is transaction control only.** Keep `begin`/`commit`/
   `rollback`. Consumers that need a repository take **that repository's port by
   constructor injection**, not the whole UoW. The UoW remains the place that
   hands out repositories bound to one transaction, but use-cases depend on the
   narrow repo interface they actually use.

2. **Split repositories into reader + writer ports** where a read-only consumer
   exists. Canonical shape:
   `IAccountReader` (`find_by_id`, `find_by_name`, `forEach`, `size`) +
   `IAccountWriter` (`save`, `remove`); the full `IAccountRepository` becomes
   `IAccountReader, IAccountWriter`. Read-only use-cases take `IAccountReader&`.
   Same pattern for `IChannelRepository`, `IIpBanRepository`, etc.

3. **Segregate capability bundles** such as `IConnectionContext`'s game-lifecycle
   callbacks (`on_game_created/joined/left`) into an `IGameLifecycleSink` that
   the InGame handlers depend on, leaving chat handlers free of it.

## Consequences

- **Positive:** read-only use-cases become impossible to misuse (no write
  surface); fakes in tests shrink to the methods under test; a use-case's
  constructor signature documents exactly what it touches.
- **Cost / sequencing:** each split is a multi-file change touching every
  implementer. The repository ports are implemented by **env-gated backends**
  (`infra/mysql`, `infra/postgres`, `infra/sqlite`, `infra/shadow`) that cannot
  be fully build-verified on the default local toolchain, so the splits are
  **scoped as separate, backend-by-backend follow-ups** rather than one blind
  sweep — each landing behind `check-all` with the relevant backend available.
- **Compatibility:** splits use the inherit-from-both idiom
  (`struct IAccountRepository : IAccountReader, IAccountWriter {}`) so existing
  `IAccountRepository&` consumers and implementers keep compiling; only the
  read-only consumers are narrowed, incrementally.
- This ADR records the audit and the target shape; the M3 Definition-of-Done
  "ports audited for ISP" is met by this decision, with the mechanical splits
  tracked as follow-up work.
