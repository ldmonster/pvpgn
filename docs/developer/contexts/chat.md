# Chat Context

The **chat** bounded context owns everything related to Battle.net channels, in-channel messaging,
and the help-file system. It is the primary real-time communication layer between connected clients.

---

## Responsibilities

- Creating, joining, and leaving channels (public, private, restricted)
- Broadcasting messages (talk, emote, whisper, server info, error) to channel members
- Enforcing channel operator privileges (kick, ban, unban, squelch)
- Serving help-file content in response to `/help` commands
- Maintaining the in-memory channel roster (who is in which channel)

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Channel` | `src/domain/chat/` | Aggregate root — holds member list, topic, flags |
| `ChannelMember` | `src/domain/chat/` | Value object — account ref + operator flag |
| `ChannelDefinition` | `src/domain/chat/ports.hpp` | Seed data for permanent channels |
| `HelpEntry` | `src/domain/chat/` | A single help-file record |

## Port Interfaces (`src/domain/chat/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IChannelRepository` | Persist and retrieve channel definitions across restarts |
| `IChannelStore` | In-memory runtime store of live `Channel` aggregates |
| `IMessageBroadcaster` | Deliver a `Message` to one or all members of a channel |
| `IHelpfileSource` | Load help-file entries by keyword |

These interfaces are implemented in `src/infra/` and injected at startup via the DI root in
`src/app/bnetd/`.

## Key Use Cases (`src/application/chat/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `JoinChannelUseCase` | Client sends `SID_JOINCHANNEL` | Validates access, adds member, broadcasts join event |
| `LeaveChannelUseCase` | Client disconnects or sends leave | Removes member, broadcasts part event, destroys empty channels |
| `SendMessageUseCase` | Client sends `SID_CHATCOMMAND` | Validates squelch/ban, routes to `IMessageBroadcaster` |
| `ChannelCommandUseCase` | `/kick`, `/ban`, `/op` etc. | Checks operator permission via `IPermissionChecker` |
| `HelpUseCase` | `/help <keyword>` | Queries `IHelpfileSource`, replies to requesting client |

## Where to Add New Features

- **New channel flag** → add to `Channel` aggregate in `src/domain/chat/`; update `IChannelRepository` schema
- **New chat command** → add a handler in `src/application/chat/`; register in `ICommandRegistry`
- **New message type** → extend the `MessageType` enum in `src/domain/chat/`; update `IMessageBroadcaster`
- **New help content** → add entries to the help-file data source; no code change needed

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md) — why ports live in `domain/`
- [ADR 0004 — Strangler Fig Pattern](../../adr/0004-strangler-fig-pattern.md) — migration from legacy `handle_bnet.cpp`

## See Also

- [Moderation Context](moderation.md) — ban/squelch enforcement
- [Connection Context](connection.md) — how messages reach the wire
- [Identity Context](identity.md) — account lookup for channel members
