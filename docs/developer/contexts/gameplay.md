# Gameplay Context

The **gameplay** bounded context owns the lifecycle of Battle.net game lobbies — from creation
through to completion. It tracks which games exist, who has joined them, and what their current
state is.

---

## Responsibilities

- Creating and destroying game lobbies (public, private, tournament)
- Tracking game membership (who is in which game, as host or player)
- Enforcing game capacity limits and password protection
- Reporting game state to the ladder context on game completion
- Serving the game list to clients browsing the lobby

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Game` | `src/domain/gameplay/` | Aggregate root — name, type, state, member list |
| `GameId` | `src/domain/gameplay/` | Strong typedef (`uint32_t`) |
| `GameMember` | `src/domain/gameplay/` | Value object — account ref + join time + team slot |
| `GameType` | `src/domain/gameplay/` | Enum: `melee`, `ffa`, `ladder`, `custom`, `tournament` |
| `GameState` | `src/domain/gameplay/` | Enum: `open`, `started`, `closed`, `expired` |

## Port Interfaces (`src/domain/gameplay/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IGameRepository` | Persist game records for reporting and reconnect |
| `IGameEventPublisher` | Publish `GameStartedEvent`, `GameEndedEvent` to the event bus |

## Key Use Cases (`src/application/gameplay/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `CreateGameUseCase` | Client sends `SID_STARTADVEX3` | Validates name, creates `Game` aggregate, publishes event |
| `JoinGameUseCase` | Client sends `SID_GETADVLISTEX` + join | Checks password, capacity; adds `GameMember` |
| `LeaveGameUseCase` | Client disconnects or sends leave | Removes member; destroys game if empty |
| `ListGamesUseCase` | Client requests game list | Filters by type/name; returns paginated list |
| `ReportGameResultUseCase` | Game host reports result | Validates result; fires `GameEndedEvent` for ladder |

## Game List Filtering

The game list is filtered server-side before being sent to the client. Filters include:
- Game type (melee, FFA, etc.)
- Name prefix search
- Maximum ping (client-reported)
- Expansion flag (e.g., Brood War only)

## Where to Add New Features

- **New game type** → extend `GameType` enum; add validation in `CreateGameUseCase`
- **Tournament bracket integration** → subscribe to `GameEndedEvent` in a new tournament service
- **Game replay storage** → add `IReplayStore` port; call from `ReportGameResultUseCase`
- **Anti-cheat hooks** → add `IAntiCheatGateway` port; call from `JoinGameUseCase`

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Ladder Context](ladder.md) — consumes `GameEndedEvent` to update rankings
- [Matchmaking Context](matchmaking.md) — automated game creation for anonymous games
- [Chat Context](chat.md) — game channel created alongside each game lobby
