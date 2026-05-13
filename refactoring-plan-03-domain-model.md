# 03 · Domain Model (DDD Bounded Contexts)

The current code conflates eight related but distinct subdomains.
Splitting them produces a smaller-vocabulary, testable domain layer.

## 1. Bounded contexts (modules under `src/domain/`)

| Context | Aggregate roots | Notes |
|---|---|---|
| `identity` | `Account` | Authentication, profile, attributes, locale. |
| `chat` | `Channel`, `Whisper` | Public/private rooms, message routing. |
| `social` | `FriendList`, `Clan`, `Team` | Social graph, ownership, invitations. |
| `gameplay` | `Game`, `MatchReport` | Hosted games, in-game lifecycle, results. |
| `matchmaking` | `AnonGameQueue`, `Tournament` | PG, ladder anonymous matching, brackets. |
| `ladder` | `LadderEntry`, `LadderSnapshot` | Per-clienttag rating tables. |
| `moderation` | `Ban`, `Mute`, `Watch`, `Quota` | IP/account/channel bans, message quotas. |
| `realm` | `Realm`, `Character`, `RealmServer` | Diablo II realm catalog. |

Cross-context references use **IDs** (`AccountId`, `ChannelId`,
`GameId`, `ClanId`) — never pointer aliases. Aggregates communicate
through **domain events** posted to `IEventBus`, not direct calls.

## 2. Core value objects (`domain/shared/`)

All immutable, hash-equipped, `constexpr`-friendly:

```cpp
struct AccountId  { std::uint32_t v; };          // strong typedef
struct ChannelId  { std::uint32_t v; };
struct GameId     { std::uint32_t v; };
struct ClanId     { std::uint32_t v; };
struct ClientTag  { std::array<char,4> v; };     // 'STAR','D2DV',…
class  UserName   { /* validated [a-zA-Z0-9_-]{2,15} */ };
class  Locale     { /* IETF tag, fallback "enUS" */ };
class  IpAddress  { /* v4+v6, parse/format */ };
class  BNHash     { std::array<std::uint32_t,5> words; };
class  WideString { /* UTF-16 LE for D2 names */ };
struct Latency    { std::chrono::milliseconds v; };
struct Money      { std::int64_t cents; };       // (future) economy/store
```

The `UserName`, `BNHash`, `ClientTag`, `Locale` constructors return
`tl::expected<UserName, NameError>` — invalid input never reaches the
aggregate.

## 3. Aggregate sketches

### 3.1 `identity::Account`

```cpp
class Account {
public:
    // Reconstitution from persistence
    static Account rehydrate(AccountSnapshot snapshot);

    // Factory
    static tl::expected<Account, AccountError>
        create(UserName, BNHash, Locale, Clock::TimePoint now);

    // Queries (pure)
    AccountId           id()            const noexcept;
    const UserName&     name()          const noexcept;
    bool                isLocked()      const noexcept;
    bool                isAdmin()       const noexcept;
    CommandGroupMask    commandGroups() const noexcept;

    // Commands → return events
    LoginOutcome        login(BNHash candidate, IpAddress, Clock::TimePoint);
    void                changePassword(BNHash newHash);
    void                grantCommandGroup(CommandGroup);
    void                applyBan(Ban);
    void                clearBan();

    // Pending domain events (drained by Application)
    std::vector<DomainEvent> drainEvents();

private:
    AccountId id_;
    UserName  name_;
    BNHash    passwordHash_;
    Locale    locale_;
    CommandGroupMask groups_;
    std::optional<Ban> ban_;
    AccountStats stats_;                  // wins, losses, last login, …
    AttributeMap attrs_;                  // unstructured legacy attrs
    std::vector<DomainEvent> events_;
};
```

* `attrs_` keeps the legacy stringly-typed bag (`"BNET\\acct\\…"`),
  but accessed only through typed wrappers in
  `infrastructure/persistence/legacy_attribute_map.hpp`. New features
  use typed fields on the aggregate.
* `Account` carries **no `connection*`, no friends list, no clanmember
  pointer** — those are separate aggregates referenced by ID.

### 3.2 `chat::Channel`

```cpp
class Channel {
public:
    static Channel create(ChannelId, ChannelName, ClientTag,
                          ChannelPolicy, Clock::TimePoint);

    JoinOutcome  admit(AccountId, ClientTag, Latency);
    void         leave(AccountId);
    void         kick(AccountId moderator, AccountId target, Reason);
    void         post(AccountId from, ChatMessage msg);     // emits ChannelMessageSent
    void         setTopic(AccountId moderator, Topic);

    bool         isFull()       const noexcept;
    bool         isModerated()  const noexcept;
    std::size_t  memberCount()  const noexcept;
private:
    // … members keyed by AccountId, ban list, topic, flags, etc.
};
```

* Replaces today's `t_channelmember` linked list (raw `t_connection*`)
  with an `AccountId`-keyed `flat_hash_map`.
* `ChannelPolicy` captures all `channel_flags_*` bits with a single
  `enum class` and `<bitset>`.

### 3.3 `gameplay::Game`

```cpp
class Game {
public:
    static Game host(GameId, AccountId host, GameDescriptor,
                     Clock::TimePoint, MapInfo);
    JoinOutcome  join(AccountId);
    void         leave(AccountId);
    void         reportResult(AccountId reporter, ResultPayload);
    GameOutcome  finalize(Clock::TimePoint);

    // queries …
};
```

`Game` owns a deterministic FSM (`Open → InProgress → Reporting →
Finalized`). Result-reconciliation logic (currently spread across
`game.cpp`, `ladder.cpp`, `anongame_gameresult.cpp`) becomes pure
methods returning events `GameStarted`, `GameEnded`,
`LadderUpdateRequested`.

### 3.4 `ladder::LadderCalculator` (domain service)

```cpp
class LadderCalculator {
public:
    LadderUpdate compute(MatchReport, std::span<const LadderEntry>) const;
};
```

Stateless; today's `ladder_calc.cpp` is rewritten as one function with a
strategy parameter for W3 / SC / D2 algorithms. Unit-testable
exhaustively because no globals are touched.

### 3.5 `social::Clan`, `social::Team`, `social::FriendList`

Each is a small aggregate with explicit invariants (e.g. clan name
unique, max 4 ranks; team has 2–4 fixed members, etc.). They replace
`clan.cpp` (~3 kLOC) and `team.cpp` (~1 kLOC) which currently mutate
`t_account` directly.

### 3.6 `moderation::Ban`, `moderation::Quota`

```cpp
struct Ban {
    BanScope     scope;            // account, ip, ip-range
    std::string  reason;
    AccountId    issuer;
    Clock::TimePoint issuedAt;
    std::optional<Clock::TimePoint> expiresAt;
};
class IpBanList { … };             // CIDR-aware
class Quota     { /* msg/s, mute on excess */ };
```

### 3.7 `realm::Realm`, `realm::Character`

D2 realm registry + character indices, currently in
`bnetd/realm.cpp` + `d2cs/d2charfile.cpp`. Moved verbatim into a
clean aggregate; storage adapters in `infrastructure/persistence`.

## 4. Domain events

```cpp
namespace pvpgn::domain::events {
struct AccountCreated      { AccountId id; UserName name; };
struct UserLoggedIn        { AccountId id; IpAddress ip; ClientTag tag; };
struct UserLoggedOut       { AccountId id; };
struct ChannelJoined       { ChannelId ch; AccountId who; };
struct ChannelMessageSent  { ChannelId ch; AccountId from; ChatMessage msg; };
struct WhisperSent         { AccountId from, to; ChatMessage msg; };
struct GameCreated         { GameId id; AccountId host; };
struct GameEnded           { GameId id; MatchReport report; };
struct LadderUpdated       { ClientTag tag; std::vector<LadderEntry> delta; };
struct BanIssued           { Ban ban; };
// … ~25 events total
}
```

Events are dispatched through `IEventBus` (a Fiber-aware in-process
bus, see section 06) — subscribers include:

* The **Lua/sol3 script host** (`luaevent_*` is replaced).
* The **WebSocket hub** (live admin dashboard).
* The **metrics exporter** (counters per event type).
* The **integration test harness** (assert on events).

## 5. Persistence model (kept thin)

Each aggregate has a corresponding "snapshot" (POD) used by repos:

```cpp
struct AccountSnapshot {
    AccountId id;
    std::string name;
    std::array<std::uint8_t,20> passwordHash;
    std::string locale;
    std::uint64_t flags;
    AttributeBag legacyAttrs;
    // …
};
```

Repositories convert snapshot ↔ aggregate. This isolates schema
churn from the domain. See section 05.

## 6. Invariants enforced in the domain layer

Examples that today live scattered or only in handlers:

* `Account.name` is validated at construction; no part of the system
  can store an invalid name (compare today's runtime validation in
  `account_check_name`).
* `Channel.memberCount <= channel.maxMembers` — checked on `admit()`.
* `Game.players.size() <= map.maxSlots`.
* `Friend.target != self.id`.
* `Clan.members.size() <= 250`, ranks are an enum with a fixed mapping.
* A `Ban` for an `Account` automatically rejects future `login()`
  calls until expiry — no scattered `connlist_kill_*` calls.

## 7. What stays out of `domain/`

* Anything that knows about TCP sockets, `t_packet`, Lua, SQL,
  HTTP, filesystems, `std::cout`, or wall-clock through
  `std::chrono::system_clock::now()` (always inject `IClock`).
* Any free function with mutable static state.
* Win32-specific code.
* Logging — events ARE the log audit trail; trace logs are emitted by
  the application layer, not by aggregates.

Adhering to this rule is what allows `tests/unit/domain` to run in
milliseconds with no fixtures.
