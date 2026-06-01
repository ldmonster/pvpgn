# Identity Context

The **identity** bounded context owns account management and authentication. It is the first
context touched on every client connection and the source of truth for who a user is.

---

## Responsibilities

- Account creation, lookup, and deletion
- Password hashing and verification (argon2id for new accounts; transparent rehash on login)
- SRP-6a handshake for Battle.net protocol authentication
- Session token issuance and validation
- Session registry (which `ConnectionId` maps to which account)

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Account` | `src/domain/identity/` | Aggregate root — username, hash, profile fields, flags |
| `AccountId` | `src/domain/identity/` | Strong typedef (`uint64_t`) |
| `PasswordHash` | `src/domain/identity/` | Value object — algorithm tag + encoded hash string |
| `SessionToken` | `src/domain/identity/` | Short-lived opaque token (HMAC-SHA256) |
| `Session` | `src/domain/identity/` | Maps `SessionToken` → `AccountId` + `ConnectionId` |

## Port Interfaces (`src/domain/identity/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IAccountRepository` | Persist and retrieve `Account` aggregates |
| `IPasswordHasher` | Hash a plaintext password; verify a hash; detect rehash need |
| `ISessionRegistry` | Store and look up active `Session` objects |
| `ISessionTokenIssuer` | Issue and validate `SessionToken` values |

## Key Use Cases (`src/application/identity/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `RegisterAccountUseCase` | Client sends `SID_AUTH_ACCOUNTCREATE` | Validates username, hashes password with argon2id, persists |
| `AuthenticateUseCase` | Client completes SRP handshake | Verifies credentials; issues `SessionToken`; registers session |
| `LogoutUseCase` | Connection closed | Revokes session token; removes from registry |
| `ChangePasswordUseCase` | Client sends `SID_AUTH_ACCOUNTCHANGE` | Re-hashes with current argon2id params |
| `LookupAccountUseCase` | Any context needing account data | Read-only; returns `Account` by ID or username |

## Password Hashing Strategy

PvPGN uses a two-layer approach:

1. **Legacy hash** — the original `pvpgn_hash` (SHA-1 based) stored in `account.hash_version = 0`
2. **argon2id** — stored in `account.hash_version = 1` (Plan 08)

On successful login, if `account.hash_version < current_version`, the account is transparently
rehashed and saved. This means the migration is zero-downtime and incremental.

See the [Rotate argon2id Params runbook](../../operator/runbooks/rotate-argon2id-params.md) for
operational guidance.

## Where to Add New Features

- **OAuth / external identity provider** → add `IExternalIdentityGateway` port; call from a new
  `FederatedLoginUseCase`
- **Two-factor authentication** → add `IOTP` port; inject into `AuthenticateUseCase`
- **Account profile fields** → extend the `Account` aggregate; add migration in `infra/migrations/`
- **Login rate limiting** → add `ILoginRateLimiter` port; inject into `AuthenticateUseCase`

## Related ADRs

- [ADR 0001 — TOML Configuration](../../adr/0001-use-toml-for-configuration.md) — `[auth]` section
- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Moderation Context](moderation.md) — ban checks happen after authentication
- [Connection Context](connection.md) — session registry ties accounts to connections
- [Rotate argon2id Params runbook](../../operator/runbooks/rotate-argon2id-params.md)
