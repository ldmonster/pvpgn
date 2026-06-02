# PvPGN Documentation

**PvPGN** (Player vs Player Gaming Network) is an open-source Battle.net emulator that allows
you to run your own server for classic Blizzard games — StarCraft, Diablo II, Warcraft III, and
more. This documentation covers everything from first-time setup to deep internals.

---

## Quick Navigation

| Section | What you'll find |
|---------|-----------------|
| [User Guide](#user-guide) | Playing on a PvPGN server — banners, MOTD, version check |
| [Operator Guide](#operator-guide) | Installing, configuring, and operating a server |
| [Developer Guide](#developer-guide) | Architecture, bounded contexts, extending PvPGN |
| [Reference](#reference) | Config keys, metrics, domain events |
| [ADRs](#architecture-decision-records) | Why we made the design choices we did |

---

## User Guide

Documentation for players and server administrators who interact with PvPGN through a game client.

- [Ad Banners](user/adbanners.md) — Creating SMK/MNG ad banners shown in the Battle.net lobby
- [MOTD](user/bnmotd.md) — Configuring the Message of the Day shown on login
- [Version Check](user/versioncheck.md) — How PvPGN validates game client versions
- [Observability](user/observability.md) — Metrics and health endpoints exposed to operators
- [Network Ports](user/ports.md) — Ports used by each supported game protocol

---

## Operator Guide

Everything needed to deploy, configure, and maintain a PvPGN server in production.

- [Building from Source](operator/building.md) — CMake build instructions for Linux, macOS, and Windows
- [TOML Migration](operator/toml-migration.md) — Migrating from the legacy `bnetd.conf` to `bnetd.toml`
- [TOML Schema Versioning](operator/toml-schema-versioning.md) — How schema versions work and how to upgrade
- [Metrics Reference](operator/metrics.md) — Every emitted metric with type, labels, and rationale

### Runbooks

Step-by-step procedures for common operational tasks:

- [Diagnose Slow Login](operator/runbooks/diagnose-slow-login.md) — Tracing login latency through logs and metrics
- [Recover from Corrupt DB](operator/runbooks/recover-from-corrupt-db.md) — Backup strategy and database repair
- [Rolling Upgrade](operator/runbooks/rolling-upgrade.md) — No-downtime upgrade procedure
- [Connect OTel Collector](operator/runbooks/connect-otel-collector.md) — Wiring up an OpenTelemetry collector
- [Rotate argon2id Params](operator/runbooks/rotate-argon2id-params.md) — Safely rotating password-hash parameters

---

## Developer Guide

Architecture documentation, bounded-context guides, and extension points for contributors.

- [Extending PvPGN](developer/extending-pvpgn.md) — Plugin system overview and capability model
- [Lua API v2](developer/lua-api-v2.md) — Scripting hooks available to Lua plugins
- [Plugin Versioning](developer/plugin-versioning-guide.md) — ABI stability guarantees and versioning policy
- [Sandbox Integration](developer/sandbox-integration-guide.md) — Running PvPGN in a sandboxed test environment
- [Single Binary Mode](developer/single-binary-mode.md) — Running bnetd + d2cs + d2dbs in one process
- [Config Reference](developer/config-reference.md) — All `bnetd.toml` keys (auto-generated)
- [Testing](developer/testing.md) — Test pyramid, coverage gates, and how to run the suite
- [Benchmarking](developer/benchmarking.md) — Microbench harness, baselines, and the regression gate
- [Release Process](developer/release-process.md) — SemVer policy, deprecation policy, and the release checklist

### Bounded Contexts

One guide per domain bounded context — what it owns, its port interfaces, and where to add features:

- [Chat](developer/contexts/chat.md) — Channels, messages, help files
- [Connection](developer/contexts/connection.md) — TCP sessions, message routing, egress
- [D2CS](developer/contexts/d2cs.md) — Diablo II character server
- [D2DBS](developer/contexts/d2dbs.md) — Diablo II database server
- [Gameplay](developer/contexts/gameplay.md) — Game lobbies and game lifecycle
- [Identity](developer/contexts/identity.md) — Accounts, sessions, authentication
- [Ladder](developer/contexts/ladder.md) — Ladder rankings and season management
- [Matchmaking](developer/contexts/matchmaking.md) — Anonymous game matchmaking (W3/D2)
- [Moderation](developer/contexts/moderation.md) — Bans, permissions, audit log
- [Realm](developer/contexts/realm.md) — Diablo II realm management
- [Shared](developer/contexts/shared.md) — Cross-cutting domain events and value objects
- [Social](developer/contexts/social.md) — Clans, friends, teams, mail

---

## Reference

Auto-generated and curated reference material:

- [Reference Overview](reference/index.md) — Index of all reference pages
- [Config Reference](developer/config-reference.md) — Every `bnetd.toml` key with type and default

---

## Architecture Decision Records

Records of significant design decisions and the reasoning behind them:

- [0001 — TOML Configuration](adr/0001-use-toml-for-configuration.md)
- [0002 — Hexagonal Architecture](adr/0002-hexagonal-architecture.md)
- [0003 — Lua Plugin API v2](adr/0003-lua-plugin-api-v2.md)
- [0004 — Strangler Fig Pattern](adr/0004-strangler-fig-pattern.md)
- [0005 — Catch2 Test Framework](adr/0005-catch2-test-framework.md)

---

## History

Background reading on legacy systems that have been superseded:

- [Alloc Survey](history/alloc-survey.md)
- [Compat Survey](history/compat-survey.md)
- [xalloc Migration](history/migration-xalloc-to-stl.md)
- [fdwatch](history/fdwatch.md)
- [Storage Backends](history/storage.md)
