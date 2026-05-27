# 03 — Domain Purification

**Goal:** `src/v3/domain/**` contains **only** value objects, entities,
aggregates, domain services and domain events. **No** I/O, **no**
globals, **no** logging, **no** `iostream`, **no** `time(NULL)`,
**no** `errno`, **no** dependency on `application/`, `infra/`,
`protocol/`, or any third-party library outside `core/`.

## 1. Audit (round R230)

Run a grep audit and fix every violation:

```
grep -rE '#include <(iostream|fstream|cstdio|chrono>$|ctime)' src/v3/domain/
grep -rE 'std::cout|std::cerr|printf|fprintf|fopen|fread|fwrite' src/v3/domain/
grep -rE 'PVPGN_LOG|core::log::' src/v3/domain/
grep -rE 'time\(NULL\)|std::time|::time\(' src/v3/domain/
grep -rE 'static[[:space:]]+[A-Za-z_:<>]+[[:space:]]+g_' src/v3/domain/
```

Acceptable in `domain/`:

- `<string>`, `<string_view>`, `<vector>`, `<array>`, `<optional>`,
  `<variant>`, `<unordered_map>`, `<chrono>` (types only, no `system_clock`).
- `pvpgn::core::*` (Result, StrongTypedef, Bytes, BnetTime).

Forbidden in `domain/`:

- `std::chrono::system_clock::now()` — use an injected
  `core::Clock` port reference instead.
- Random number sources — inject `core::RandomSource`.
- File or socket I/O.

## 2. Value objects (sweep R231–R235)

Replace primitive obsession with strong typedefs (`core::StrongTypedef<T,
Tag>`):

| Concept | Today | Target |
|---------|-------|--------|
| Account id | `unsigned int` | `domain::identity::AccountId` |
| Connection id | `t_uint32` | `domain::connection::ConnectionId` |
| Channel name | `char[64]` | `domain::chat::ChannelName` (validated ctor) |
| Username | raw string | `domain::identity::Username` |
| Realm name | raw string | `domain::realm::RealmName` |
| Clan tag | `t_clantag` (`uint32`) | `domain::social::ClanTag` |
| Game name | raw string | `domain::gameplay::GameName` |
| IP address | `unsigned long` host order | `domain::net::Ipv4Address` (std::array<uint8,4>) |
| BnetTime | `core::BnetTime` already exists | keep |

Each typedef:

- Has a `parse(std::string_view) -> Result<T,StatusCode>` factory.
- Has a `to_string() / to_bnet_wire()` projection.
- Has its own unit test in `tests/unit/domain/<bc>/<type>_test.cpp`.

## 3. Entities & aggregates

Per bounded context (folders already exist):

- **identity**: `Account` aggregate. Invariants: username unique
  case-insensitively per realm, password-hash always set, banned-state
  consistent.
- **chat**: `Channel` aggregate. Invariants: operator list ⊆
  member list, no duplicate members, max-capacity respected.
- **gameplay**: `Game` aggregate. Invariants: host present until
  end, no double-join, result reportable only after start.
- **social**: `Clan`, `FriendList`, `Team`.
- **realm**: `Realm`, `RealmCharacter`.
- **ladder**: `LadderEntry`, `LadderTable` (immutable snapshot).
- **moderation**: `IpBan`, `Mute`, `Warning`.
- **matchmaking**: `MatchmakingQueue`, `MatchRequest`.
- **connection**: `Connection` (the protocol-agnostic part —
  authenticated session, tag, version, locale).
- **d2cs**, **d2dbs**: realm and character-store aggregates.

Methods on entities **return** new state or `Result<NewState,
StatusCode>` for invariant-breaking attempts; they don't throw.

## 4. Domain events

Define a single header `domain/shared/Events.h` with a `std::variant`
of every domain event:

```cpp
using DomainEvent = std::variant<
  identity::AccountCreated,
  identity::AccountLoggedIn,
  identity::AccountBanned,
  chat::ChannelJoined,
  chat::ChannelLeft,
  chat::MessageSent,
  gameplay::GameCreated,
  gameplay::GameJoined,
  gameplay::GameEnded,
  social::FriendAdded,
  social::ClanInvited,
  ...
>;
```

Events are **value types**, copyable, `noexcept`-movable. They flow
through `core::EventBus` (already exists). No subscribers in
`domain/`; subscribers live in `application/` or `infra/`.

## 5. Domain services

A domain service is a stateless function or function-object on
`domain::` that needs two or more entities to make a decision but
doesn't belong inside any single entity (e.g.,
`domain::ladder::compute_rating_delta(LadderEntry, LadderEntry,
GameResult) -> RatingDelta`). Keep these as free functions in
namespace-scope; YAGNI says no `IDomainService` interface.

## 6. Acceptance

- CI rule (script `scripts/ci/check_domain_purity.sh`) greps the
  forbidden tokens above on every PR; non-zero → red.
- `tests/unit/domain/**` covers ≥ 90 % line and ≥ 80 % branch.
- Mutation testing (`mull` or `pitest`-equivalent) optional but
  recommended for the critical aggregates (Account, Game, Channel).
