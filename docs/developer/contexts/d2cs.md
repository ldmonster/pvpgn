# D2CS Context — Diablo II Character Server

The **d2cs** bounded context owns the Diablo II Character Server protocol. It handles character
selection, character creation, and the handshake between the Battle.net lobby and the Diablo II
game server (d2gs) that allows a character to enter a game.

---

## Responsibilities

- Authenticating Diablo II clients connecting to the character server port (TCP 6112 / 4000)
- Serving the character list for a given account
- Validating character data before a client enters a game
- Coordinating the three-way handshake: `bnetd ↔ d2cs ↔ d2gs`
- Enforcing realm membership (a character belongs to exactly one realm)

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `D2Character` | `src/domain/d2cs/` | Aggregate root — character name, class, level, flags |
| `D2CharacterId` | `src/domain/d2cs/` | Strong typedef combining account ID + character slot |
| `D2GameToken` | `src/domain/d2cs/` | Short-lived token authorising entry into a specific game |
| `D2RealmRef` | `src/domain/d2cs/` | Reference to the owning realm (cross-context link) |

## Port Interfaces (`src/domain/d2cs/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `ID2CharacterRepository` | Persist and retrieve `D2Character` aggregates |
| `ID2GameTokenIssuer` | Issue and validate short-lived game-entry tokens |
| `ID2RealmGateway` | Notify the realm context when a character enters or leaves a game |

## Key Use Cases (`src/application/d2cs/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `ListCharactersUseCase` | Client requests character list | Returns all characters for the authenticated account |
| `CreateCharacterUseCase` | Client creates a new character | Validates name uniqueness, class, and realm capacity |
| `DeleteCharacterUseCase` | Client deletes a character | Soft-deletes; character data retained for configurable grace period |
| `EnterGameUseCase` | Client selects a character and game | Issues `D2GameToken`; notifies d2gs via `ID2RealmGateway` |
| `LeaveGameUseCase` | Client disconnects from game | Revokes token; updates character state |

## Where to Add New Features

- **New character class** → extend the `D2CharacterClass` enum in `src/domain/d2cs/`; update
  `CreateCharacterUseCase` validation
- **Character transfer between realms** → new use case in `src/application/d2cs/`; requires
  coordination with the realm context via `ID2RealmGateway`
- **Character audit log** → inject `IAuditLog` (from moderation context) into the relevant use cases
- **Ladder integration** → `EnterGameUseCase` should emit a domain event consumed by the ladder context

## Migration Note

The legacy implementation lives in `src/integration/legacy_d2cs/`. Plan 04 (d2cs/d2dbs Strangler)
will move all logic into `src/application/d2cs/` and delete the legacy directory. Until then, the
use cases in `src/application/d2cs/` are stubs that delegate to the legacy code via `extern "C"`
bridge symbols.

## Related ADRs

- [ADR 0004 — Strangler Fig Pattern](../../adr/0004-strangler-fig-pattern.md) — migration strategy

## See Also

- [D2DBS Context](d2dbs.md) — character database storage
- [Realm Context](realm.md) — realm membership and game server registry
- [Identity Context](identity.md) — account authentication
