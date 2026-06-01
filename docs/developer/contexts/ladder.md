# Ladder Context

The **ladder** bounded context owns competitive rankings for all supported games. It consumes
game-result events from the gameplay context and maintains per-season, per-game-type leaderboards.

---

## Responsibilities

- Recording wins, losses, and disconnects for ladder-eligible games
- Computing and storing ladder rankings (points, <ORGANIZATION_e0e7e5e5>, experience)
- Managing ladder seasons (start, end, archival)
- Serving the ladder list to clients (`SID_GETLADDERDATA`)
- Enforcing ladder eligibility rules (minimum games played, no self-play)

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `LadderEntry` | `src/domain/ladder/` | Aggregate root — account ref, game type, season, stats |
| `LadderEntryId` | `src/domain/ladder/` | Strong typedef: account ID + game type + season ID |
| `LadderSeason` | `src/domain/ladder/` | Season metadata — start date, end date, game type |
| `LadderStats` | `src/domain/ladder/` | Value object — wins, losses, disconnects, rating |
| `LadderRank` | `src/domain/ladder/` | Value object — computed rank position |

## Port Interfaces (`src/domain/ladder/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `ILadderRepository` | Persist and retrieve `LadderEntry` aggregates and seasons |

## Key Use Cases (`src/application/ladder/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `RecordGameResultUseCase` | `GameEndedEvent` from gameplay context | Updates `LadderEntry` for each participant |
| `GetLadderUseCase` | Client sends `SID_GETLADDERDATA` | Returns ranked list for a game type and season |
| `StartSeasonUseCase` | Operator command / scheduled | Creates new `LadderSeason`; resets active rankings |
| `EndSeasonUseCase` | Operator command / scheduled | Archives current season; snapshots final rankings |
| `CheckEligibilityUseCase` | Before recording a result | Validates no self-play, minimum game count, etc. |

## Ladder Types

PvPGN supports multiple ladder types per game:

| Game | Ladder Types |
|------|-------------|
| StarCraft | `standard`, `<ORGANIZATION_e0e7e5e5>`, `<ORGANIZATION_e0e7e5e5>_team` |
| Diablo II | `standard`, `hardcore`, `expansion`, `expansion_hardcore` |
| Warcraft III | `solo`, `team_2v2`, `team_3v3`, `team_4v4`, `ffa` |

## Where to Add New Features

- **New game type ladder** → extend `LadderGameType` enum; add eligibility rules in
  `CheckEligibilityUseCase`
- **ELO/MMR rating system** → replace the points formula in `RecordGameResultUseCase`; the
  `LadderStats` value object is the only change point
- **Season scheduling** → add a `IScheduler` port (from `core/scheduler.hpp`); inject into
  `StartSeasonUseCase` and `EndSeasonUseCase`
- **Ladder webhooks** → subscribe to `LadderRankChangedEvent` in an infra adapter

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Gameplay Context](gameplay.md) — source of `GameEndedEvent`
- [Matchmaking Context](matchmaking.md) — anonymous games also feed the ladder
- [Social Context](social.md) — team ladder entries reference clan/team aggregates
