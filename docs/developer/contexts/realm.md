# Realm Context

The **realm** bounded context owns Diablo II realm management — the registry of game servers
(d2gs instances) that host actual Diablo II games, and the mapping of characters to realms.

---

## Responsibilities

- Registering and deregistering Diablo II game servers (d2gs)
- Serving the realm list to Diablo II clients
- Tracking which characters belong to which realm
- Health-checking registered game servers
- Coordinating with d2cs when a character enters or leaves a game

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Realm` | `src/domain/realm/` | Aggregate root — name, description, game server list |
| `RealmId` | `src/domain/realm/` | Strong typedef (`uint32_t`) |
| `GameServer` | `src/domain/realm/` | Entity — IP, port, capacity, current load |
| `GameServerId` | `src/domain/realm/` | Strong typedef (`uint32_t`) |
| `RealmCharacterRef` | `src/domain/realm/` | Value object — account ID + character name + realm ID |

## Port Interfaces (`src/domain/realm/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IRealmRepository` | Persist and retrieve `Realm` aggregates and `GameServer` registrations |

## Key Use Cases (`src/application/realm/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `RegisterGameServerUseCase` | d2gs connects and authenticates | Adds `GameServer` to the realm; starts health checks |
| `DeregisterGameServerUseCase` | d2gs disconnects | Removes `GameServer`; migrates in-progress games if possible |
| `ListRealmsUseCase` | Diablo II client requests realm list | Returns all active realms with load info |
| `SelectRealmUseCase` | Client selects a realm | Returns the realm token needed to connect to d2cs |
| `HealthCheckUseCase` | Periodic scheduler tick | Pings each `GameServer`; marks unhealthy ones |

## Realm Configuration

Realms are configured in `bnetd.toml` under `[[realm]]` sections:

```toml
[[realm]]
name = "USEast"
description = "US East Realm"
host = "d2cs.example.com"
port = 6112
```

Each realm entry maps to a `Realm` aggregate seeded at startup.

## Where to Add New Features

- **Dynamic realm registration** → allow d2cs instances to self-register via an API; implement
  `IRealmRegistrationGateway` port
- **Load balancing** → extend `SelectRealmUseCase` to pick the least-loaded realm
- **Realm metrics** → emit `realm.game_servers.active` and `realm.characters.online` via
  `core::IMetricsRegistry`
- **Cross-realm character transfer** → new use case coordinating with d2cs and d2dbs contexts

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)
- [ADR 0004 — Strangler Fig Pattern](../../adr/0004-strangler-fig-pattern.md)

## See Also

- [D2CS Context](d2cs.md) — character server that uses realm membership
- [D2DBS Context](d2dbs.md) — character save files are scoped per realm
