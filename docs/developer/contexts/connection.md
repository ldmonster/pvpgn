# Connection Context

The **connection** bounded context owns the lifecycle of individual TCP sessions — from the moment
a socket is accepted until it is closed. It is the entry point for all inbound protocol traffic and
the exit point for all outbound messages.

---

## Responsibilities

- Accepting and tracking TCP connections per protocol family (bnet, IRC, WOL, telnet)
- Routing inbound packets to the correct protocol handler / use case
- Delivering outbound messages to the correct socket (egress)
- Enforcing per-connection rate limits and idle timeouts
- Maintaining the connection registry (who is connected, from which IP)

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Connection` | `src/domain/connection/` | Aggregate root — socket handle, protocol state, account ref |
| `ConnectionId` | `src/domain/connection/` | Strong typedef (`core::StrongTypedef<uint64_t>`) |
| `ProtocolFamily` | `src/domain/connection/` | Enum: `bnet`, `irc`, `wol`, `telnet`, `webui` |
| `RateLimit` | `src/domain/connection/` | Token-bucket value object |

## Port Interfaces (`src/domain/connection/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IConnectionEgress` | Write a serialised packet buffer to a specific `ConnectionId` |
| `IConnectionHandler` | Callback invoked by the I/O layer when a packet arrives |
| `IMessageRouter` | Dispatch a decoded `Message` to the appropriate use case |

The I/O layer (`src/infra/net/`) implements `IConnectionEgress` and calls `IConnectionHandler`
on each received packet. The application layer implements `IMessageRouter`.

## Key Use Cases (`src/application/connection/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `AcceptConnectionUseCase` | New TCP accept | Allocates `Connection`, starts protocol negotiation |
| `CloseConnectionUseCase` | Socket EOF / timeout | Cleans up session state, fires `ConnectionClosedEvent` |
| `RoutePacketUseCase` | Packet received | Decodes header, dispatches to protocol-specific handler |
| `EnforceRateLimitUseCase` | Every packet | Checks token bucket; drops or delays if exceeded |

## Where to Add New Features

- **New protocol family** → implement `IConnectionHandler` for the new protocol; register in the
  I/O acceptor factory in `src/infra/net/`
- **New egress message type** → add a serialiser in `src/protocol/<family>/`; call via `IConnectionEgress`
- **Connection-level metrics** → emit via `core::IMetricsRegistry` inside `AcceptConnectionUseCase`
  and `CloseConnectionUseCase`
- **Idle timeout tuning** → adjust `[net.timeouts]` section in `bnetd.toml`; values are injected
  into `Connection` at construction time

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)
- [ADR 0004 — Strangler Fig Pattern](../../adr/0004-strangler-fig-pattern.md)

## See Also

- [Chat Context](chat.md) — uses `IMessageBroadcaster` which calls `IConnectionEgress`
- [Identity Context](identity.md) — authentication happens early in `AcceptConnectionUseCase`
