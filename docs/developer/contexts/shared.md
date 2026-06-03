# Shared Context

The **shared** bounded context contains cross-cutting domain primitives that are used by multiple
other bounded contexts but do not belong to any single one. It is a deliberate "kernel" — kept
minimal to avoid becoming a dumping ground.

---

## Responsibilities

- Defining shared domain events published on the internal event bus
- Providing common value objects used across context boundaries
- Hosting the `core::EventBus` integration point
- Documenting the event contract between contexts

## What Belongs Here

Only add something to `shared` if **two or more** bounded contexts need it and it has no natural
home in either. If in doubt, keep it in the originating context and expose it via a port interface.

## Key Types

### Domain Events

Domain events are plain structs published via `core::EventBus`. Subscribers register at startup
in the DI root (`src/app/bnetd/`).

| Event | Published by | Consumed by |
|-------|-------------|-------------|
| `AccountCreatedEvent` | identity | moderation (audit), social (friend suggestions) |
| `AccountDeletedEvent` | identity | all contexts holding account refs |
| `ConnectionClosedEvent` | connection | identity (session cleanup), chat (leave channel) |
| `GameEndedEvent` | gameplay | ladder (record result), matchmaking (release queue slot) |
| `MatchFoundEvent` | matchmaking | gameplay (create game) |
| `BanIssuedEvent` | moderation | connection (kick session) |

### Shared Value Objects

| Type | Location | Description |
|------|----------|-------------|
| `AccountRef` | `src/domain/shared/` | Lightweight reference: `AccountId` + display name |
| `Timestamp` | `src/domain/shared/` | Alias for `std::chrono::system_clock::time_point` |
| `IpAddress` | `src/domain/shared/` | IPv4/IPv6 union with CIDR support |
| `GameTag` | `src/domain/shared/` | Four-byte game product tag (e.g., `STAR`, `W3XP`) |
| `CharacterClass` | `src/domain/shared/d2_character_class.hpp` | D2 character class enum; shared by `realm` + `ladder` (M3.1) |
| `Permission` | `src/domain/shared/permission.hpp` | Authorizable-capability enum; shared by `chat` + `moderation` |

### Cross-cutting ports

Authorization is cross-cutting, so its port lives in the kernel rather than
coupling one context to another's internals (see ADR 0012 context and
`scripts/check_domain_cross_context.sh`):

| Port | Location | Description |
|------|----------|-------------|
| `IPermissionChecker` | `src/domain/shared/permission.hpp` | `has_permission` / `has_command_group`; used by chat's command registry and moderation. `domain::moderation::IPermissionChecker` is a kept alias. |

## Event Bus Integration

Events are published synchronously on the calling thread. Subscribers must not block. If a
subscriber needs to do I/O, it should enqueue work on the scheduler.

```cpp
// Publishing (in a use case)
event_bus_.publish(GameEndedEvent{ .game_id = id, .result = result });

// Subscribing (in the DI root)
event_bus_.subscribe<GameEndedEvent>([&ladder](const GameEndedEvent& e) {
    ladder.record_result(e);
});
```

The `core::EventBus` implementation is in `src/core/include/core/event_bus.hpp`.

## Where to Add New Events

1. Define the event struct in `src/domain/shared/events.hpp`
2. Add a row to the table above
3. Publish from the originating use case
4. Subscribe in the DI root; implement the handler in the consuming context

## What Does NOT Belong Here

- Anything that only one context uses → keep it in that context
- Infrastructure concerns (DB connections, sockets) → `src/infra/`
- Core utilities (logging, metrics, result types) → `src/core/`

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Connection Context](connection.md) — publishes `ConnectionClosedEvent`
- [Identity Context](identity.md) — publishes `AccountCreatedEvent`
- [Gameplay Context](gameplay.md) — publishes `GameEndedEvent`
