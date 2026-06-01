# Matchmaking Context

The **matchmaking** bounded context owns the anonymous game system used by Warcraft III and
Diablo II. It pairs players who want to play without creating a named lobby, compresses the
game data for transmission, and coordinates with the gameplay context to create the actual game.

---

## Responsibilities

- Maintaining the anonymous game queue per game type and map
- Pairing players based on skill level and wait time
- Compressing and decompressing anonymous game data (`IAnonGameCompressor`)
- Coordinating with the gameplay context to create matched games
- Reporting match results back to the ladder context

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `MatchRequest` | `src/domain/matchmaking/` | Aggregate root — account ref, game type, skill bracket, enqueue time |
| `MatchRequestId` | `src/domain/matchmaking/` | Strong typedef (`uint64_t`) |
| `Match` | `src/domain/matchmaking/` | Value object — set of paired `MatchRequest` IDs |
| `SkillBracket` | `src/domain/matchmaking/` | Value object — min/max rating range for pairing |

## Port Interfaces (`src/domain/matchmaking/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IAnonGameCompressor` | Compress/decompress the binary anonymous game data blob sent over the wire |

## Key Use Cases (`src/application/matchmaking/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `EnqueueMatchRequestUseCase` | Client sends `SID_WARCRAFTGENERAL` (anongame join) | Validates eligibility; adds to queue |
| `DequeueMatchRequestUseCase` | Client cancels or disconnects | Removes from queue |
| `RunMatchmakerUseCase` | Periodic scheduler tick | Pairs compatible requests; fires `MatchFoundEvent` |
| `CreateMatchedGameUseCase` | `MatchFoundEvent` | Calls gameplay context to create the game; notifies clients |

## Matchmaking Algorithm

The current algorithm is a simple bracket-based FIFO:

1. Group requests by `(game_type, map, skill_bracket)`
2. Within each group, pair the two oldest requests
3. If a request has waited > `[matchmaking].max_wait_seconds`, widen its bracket by one tier

To replace the algorithm, implement a new `RunMatchmakerUseCase` — the port interfaces and
surrounding use cases do not need to change.

## Where to Add New Features

- **MMR-based matchmaking** → update `SkillBracket` computation in `EnqueueMatchRequestUseCase`;
  read MMR from the ladder context via a new `ILadderQuery` port
- **Cross-game-type queues** → extend `MatchRequest` with a list of acceptable game types
- **Match history** → add `IMatchHistoryRepository` port; record each `Match` on creation
- **Spectator slots** → extend `Match` value object; update `CreateMatchedGameUseCase`

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Gameplay Context](gameplay.md) — creates the actual game lobby after a match is found
- [Ladder Context](ladder.md) — provides skill ratings used for bracket computation
