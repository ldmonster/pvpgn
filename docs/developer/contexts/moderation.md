# Moderation Context

The **moderation** bounded context owns all enforcement actions against accounts and IP addresses:
bans, squelches, operator permissions, and the audit trail of who did what to whom.

---

## Responsibilities

- Account bans (temporary and permanent)
- IP address bans (CIDR ranges supported)
- Squelch (mute) management per channel
- Permission checking (operator, admin, moderator roles)
- Audit logging of all moderation actions

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `AccountBan` | `src/domain/moderation/` | Aggregate root — account ref, reason, expiry, issuer |
| `IpBan` | `src/domain/moderation/` | Aggregate root — CIDR range, reason, expiry, issuer |
| `AuditEntry` | `src/domain/moderation/` | Immutable record of a moderation action |
| `Permission` | `src/domain/moderation/` | Enum: `operator`, `admin`, `moderator`, `bot` |
| `AuditAction` | `src/domain/moderation/` | Enum: `ban`, `unban`, `kick`, `squelch`, `promote`, etc. |

## Port Interfaces (`src/domain/moderation/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IAccountBanRepository` | Persist and query `AccountBan` aggregates |
| `IIpBanRepository` | Persist and query `IpBan` aggregates (with CIDR lookup) |
| `IAuditLog` | Append `AuditEntry` records; query by account or time range |
| `IPermissionChecker` | Check whether an account holds a given `Permission` |

## Key Use Cases (`src/application/moderation/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `BanAccountUseCase` | `/ban` command | Creates `AccountBan`; writes `AuditEntry`; kicks active session |
| `UnbanAccountUseCase` | `/unban` command | Removes or expires `AccountBan`; writes `AuditEntry` |
| `BanIpUseCase` | `/ipban` command | Creates `IpBan` for the given CIDR; writes `AuditEntry` |
| `CheckBanUseCase` | On every login | Checks both account and IP bans; returns ban reason if found |
| `SquelchUseCase` | `/squelch` command | Adds squelch entry; prevents messages reaching the target |
| `PromoteUseCase` | `/operator` command | Grants `Permission`; writes `AuditEntry` |
| `QueryAuditLogUseCase` | Admin UI / `/audit` | Returns paginated `AuditEntry` list |

## Permission Model

Permissions are hierarchical:

```
admin > moderator > operator > (none)
```

`IPermissionChecker` is injected into every use case that requires elevated access. The chat
context's `ChannelCommandUseCase` calls it before executing `/kick`, `/ban`, etc.

## Where to Add New Features

- **New permission level** → extend `Permission` enum; update `IPermissionChecker` implementations
- **Temporary squelch** → add expiry field to the squelch record; check in `CheckBanUseCase`
- **Webhook on ban** → subscribe to `AccountBannedEvent` in an infra adapter
- **GDPR account deletion** → add `DeleteAccountDataUseCase`; purge `AuditEntry` records older
  than the retention window

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Identity Context](identity.md) — `CheckBanUseCase` is called after authentication
- [Chat Context](chat.md) — channel commands delegate permission checks here
