# Social Context

The **social** bounded context owns the social graph between players: clans, friend lists, teams,
and in-game mail. It is the persistence layer for all player-to-player relationships.

---

## Responsibilities

- Clan creation, membership management, and dissolution
- Friend list management (add, remove, block)
- Warcraft III team registration and management
- In-game mail (compose, deliver, read, delete)
- Serving friend/clan status updates to connected clients

## Key Aggregates and Entities

| Type | Location | Description |
|------|----------|-------------|
| `Clan` | `src/domain/social/` | Aggregate root — tag, name, member list, rank structure |
| `ClanId` | `src/domain/social/` | Strong typedef (`uint32_t`) |
| `ClanMember` | `src/domain/social/` | Entity — account ref + clan rank |
| `ClanRank` | `src/domain/social/` | Enum: `chieftain`, `shaman`, `grunt`, `peon` |
| `FriendList` | `src/domain/social/` | Aggregate root — account ref + list of `FriendEntry` |
| `FriendEntry` | `src/domain/social/` | Value object — friend account ref + mutual flag |
| `Team` | `src/domain/social/` | Aggregate root — Warcraft III team (2v2/3v3/4v4) |
| `TeamId` | `src/domain/social/` | Strong typedef (`uint32_t`) |
| `MailMessage` | `src/domain/social/` | Aggregate root — sender, recipient, subject, body, read flag |
| `MailMessageId` | `src/domain/social/` | Strong typedef (`uint64_t`) |

## Port Interfaces (`src/domain/social/ports.hpp`)

| Interface | Purpose |
|-----------|---------|
| `IClanRepository` | Persist and retrieve `Clan` aggregates |
| `IFriendListRepository` | Persist and retrieve `FriendList` aggregates |
| `ITeamRepository` | Persist and retrieve `Team` aggregates |
| `IMailStore` | Store, retrieve, and delete `MailMessage` aggregates |

## Key Use Cases (`src/application/social/`)

| Use Case | Trigger | Description |
|----------|---------|-------------|
| `CreateClanUseCase` | Client sends `SID_CLANMAKECHIEFTAIN` | Validates tag uniqueness; creates `Clan` |
| `InviteToClanUseCase` | Chieftain invites a player | Sends invitation mail; awaits acceptance |
| `AcceptClanInviteUseCase` | Invited player accepts | Adds `ClanMember`; notifies clan |
| `LeaveClanUseCase` | Member leaves or is kicked | Removes member; dissolves clan if empty |
| `AddFriendUseCase` | Client sends `SID_FRIENDSADD` | Adds `FriendEntry`; notifies friend if online |
| `RemoveFriendUseCase` | Client sends `SID_FRIENDSREMOVE` | Removes entry from both sides |
| `SendMailUseCase` | Client sends `/mail` command | Creates `MailMessage`; delivers to recipient's mailbox |
| `ReadMailUseCase` | Client requests mail | Returns unread messages; marks as read |
| `RegisterTeamUseCase` | Warcraft III team registration | Creates `Team`; links to ladder |

## Clan Rank Hierarchy

```
chieftain (1) → shaman (2) → grunt (3) → peon (4)
```

Only the chieftain can dissolve the clan or promote/demote members. Shamans can invite and kick
grunts and peons.

## Where to Add New Features

- **Clan wars** → add `ClanWar` aggregate; new use cases for challenge/accept/result
- **Friend status broadcast** → subscribe to `ConnectionClosedEvent` in `social`; push status
  update to all friends via `IConnectionEgress`
- **Mail attachments** → extend `MailMessage` with an `attachments` field; add size limit
- **Block list** → add `BlockEntry` to `FriendList`; check in `SendMailUseCase`

## Related ADRs

- [ADR 0002 — Hexagonal Architecture](../../adr/0002-hexagonal-architecture.md)

## See Also

- [Ladder Context](ladder.md) — team ladder entries reference `Team` aggregates
- [Chat Context](chat.md) — clan channel created alongside each clan
- [Identity Context](identity.md) — account refs used throughout
