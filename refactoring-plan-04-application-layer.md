# 04 · Application Layer (Use Cases, Services, Sagas)

The application layer orchestrates the domain. It is where transactions
begin and end, where authorization decisions are checked, where domain
events are dispatched, and where adapters (protocol, web, scripting)
land their requests.

## 1. Style

* **Use-Case-Per-File**. Each public operation is a class with a single
  `execute()` method (a.k.a. "interactor"). One reason to change.
* Constructor-injected dependencies (repositories, event bus, clock,
  logger). No service-locator.
* All input is a typed DTO (`struct LoginRequest`); all output is a
  `tl::expected<LoginResponse, AppError>`. Never raw bools/ints.
* Use-cases are **fiber-friendly**: any blocking call inside is to a
  Boost.Fiber-aware abstraction (DB driver wrapper, async I/O). The
  caller can `co_await`/`yield` without blocking the OS thread.

## 2. Ports (interfaces in `application/ports/`)

```cpp
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;
    virtual tl::expected<Account, RepoError> findById(AccountId)         = 0;
    virtual tl::expected<Account, RepoError> findByName(const UserName&) = 0;
    virtual tl::expected<void,    RepoError> save(const Account&)        = 0;
    virtual tl::expected<void,    RepoError> remove(AccountId)           = 0;
    virtual std::size_t                       count() const              = 0;
    virtual void                              forEach(std::function<void(const Account&)>) = 0;
};

class IChannelRepository    { /* findByName, list, save, … */ };
class IGameRepository       { /* findById, list, save, removeIfFinalized */ };
class IClanRepository       { /* … */ };
class ILadderRepository     { /* loadFor(ClientTag), save, snapshot */ };
class IBanRepository        { /* matchAccount, matchIp, save, expire */ };

class ISessionRegistry {
public:
    virtual void  attach(SessionId, AccountId)   = 0;
    virtual void  detach(SessionId)              = 0;
    virtual std::optional<SessionId> sessionFor(AccountId) const = 0;
    virtual std::optional<AccountId> accountFor(SessionId) const = 0;
    virtual std::vector<SessionId>   list()      const = 0;
};

class IMessageRouter {
public:
    virtual void send(SessionId, OutgoingMessage)                    = 0;
    virtual void broadcastChannel(ChannelId, OutgoingMessage)        = 0;
    virtual void broadcastClan(ClanId, OutgoingMessage)              = 0;
    virtual void broadcastGlobal(OutgoingMessage)                    = 0;
};

class IEventBus {
public:
    virtual void publish(DomainEvent)                                = 0;
    virtual SubscriptionId subscribe(std::type_index,
                                     std::function<void(const DomainEvent&)>) = 0;
    virtual void unsubscribe(SubscriptionId)                         = 0;
};

class IScriptHost {
public:
    virtual void onEvent(const DomainEvent&)                         = 0;
    virtual tl::expected<bool, ScriptError>
            interceptCommand(SessionId, std::string_view cmd)        = 0;
    virtual void reload()                                            = 0;
};
```

Every adapter (`protocol/*`, `infrastructure/webapi`, scripting) talks
**only** to these ports.

## 3. Use-cases (representative list, ~60 total)

### 3.1 Auth
* `CreateAccount`
* `LoginUser`
* `LogoutUser`
* `ChangePassword`
* `LockAccount` / `UnlockAccount`

### 3.2 Chat
* `JoinChannel`, `LeaveChannel`
* `ListChannels(ClientTag)`
* `SendChannelMessage`, `SendWhisper`, `SendEmote`
* `KickFromChannel`, `BanFromChannel`, `SetChannelTopic`

### 3.3 Games
* `CreatePublicGame`, `CreatePrivateGame`
* `JoinGame`, `LeaveGame`
* `ReportGameResult` (W3, SC, D2 variants behind strategy)
* `ListPublicGames(ClientTag)`
* `EnqueueAnonGame(MatchmakingPolicy)`, `CancelAnonQueue`
* `StartTournament`, `RegisterForTournament`

### 3.4 Social
* `AddFriend`, `RemoveFriend`, `ListFriends`
* `CreateClan`, `DisbandClan`, `InviteToClan`, `KickFromClan`
* `PromoteClanMember`, `SetClanMotd`
* `CreateTeam`, `DisbandTeam`

### 3.5 Ladder
* `RecomputeLadder(ClientTag)` (background fiber task)
* `GetLadderPage(ClientTag, Range)`

### 3.6 Moderation (admin / commands)
* `BanAccount`, `UnbanAccount`
* `BanIp(Cidr, Duration, Reason)`
* `KickConnection`, `SilenceUser`, `WatchUser`
* `RotateLogs`, `ReloadConfig`, `SaveAll`, `ShutdownServer`

### 3.7 Realm (D2)
* `RegisterRealm`, `DeregisterRealm`
* `AssignCharacterToRealm`, `LoadCharacter`, `SaveCharacter`

## 4. Sample use-case

```cpp
// application/auth/login_user.hpp
struct LoginRequest {
    UserName     name;
    BNHash       passwordCandidate;
    ClientTag    tag;
    IpAddress    ip;
    SessionId    session;
};
struct LoginResponse {
    AccountId  id;
    Locale     locale;
    bool       firstLoginToday;
};
enum class LoginError {
    UnknownUser, BadPassword, Banned, AlreadyLoggedIn, ServerFull, InternalError
};

class LoginUser {
public:
    LoginUser(IAccountRepository&, ISessionRegistry&, IBanRepository&,
              IEventBus&, IClock&, ILogger&);
    tl::expected<LoginResponse, LoginError> execute(LoginRequest);
private:
    /* injected refs */
};
```

```cpp
// application/auth/login_user.cpp
tl::expected<LoginResponse, LoginError>
LoginUser::execute(LoginRequest req)
{
    auto acc = accountRepo_.findByName(req.name);
    if (!acc)                            return tl::unexpected(LoginError::UnknownUser);
    if (banRepo_.matchAccount(acc->id())) return tl::unexpected(LoginError::Banned);
    if (banRepo_.matchIp(req.ip))         return tl::unexpected(LoginError::Banned);

    auto outcome = acc->login(req.passwordCandidate, req.ip, clock_.now());
    if (outcome != LoginOutcome::Ok)      return tl::unexpected(map(outcome));

    if (sessionReg_.sessionFor(acc->id())) {
        // Configurable policy: kick previous or refuse
        return tl::unexpected(LoginError::AlreadyLoggedIn);
    }

    sessionReg_.attach(req.session, acc->id());

    if (auto saved = accountRepo_.save(*acc); !saved)
        log_.warn("persist after login failed: {}", saved.error());

    for (auto& e : acc->drainEvents()) bus_.publish(std::move(e));

    return LoginResponse{ acc->id(), acc->locale(), …};
}
```

This replaces fragments scattered across
`handle_bnet.cpp`, `account.cpp`, `connection.cpp`, `ipban.cpp`,
`luainterface.cpp`. It is testable with three mock repositories and
zero infrastructure.

## 5. Command and chat-command handlers

The legacy `command.cpp` switch-statement of `/whisper`, `/whois`, `/ban`,
`/kick`, … is reorganised:

```
application/chat/commands/
├── whisper_command.cpp
├── whois_command.cpp
├── ignore_command.cpp
├── kick_command.cpp
├── ban_command.cpp
└── command_dispatcher.{hpp,cpp}   # /name → use-case
```

`CommandDispatcher` maps `std::string_view` ↔ `ICommand`; new commands
register in a `CommandRegistry` at composition time. This is the
**Open/Closed** seam: a plug-in can add a command without modifying
core source.

## 6. Sagas / long-running flows (Boost.Fiber)

Some flows span multiple events/messages: D2 character creation,
WC3 ladder recompute, tournament round resolution.  Each is a fiber
spawned by an application-layer "saga":

```cpp
class TournamentRoundSaga {
public:
    boost::fibers::fiber start(TournamentId id);
private:
    // …
};
```

The fiber `yields` while waiting on game-result events (via a
`fiber_channel<MatchReport>`) and produces commands back to the bus.
This is impossible in the current event-loop callback style without
storing state machines manually.

## 7. Cross-cutting concerns

* **Authorization** is a `IPermissionChecker` injected into every
  admin/moderation use-case.  Per-command groups (the legacy
  `command_groups` config) become typed `CommandGroup` enums; checks
  happen at the use-case entry, not buried inside helper functions.
* **Auditing**: every state-mutating use-case publishes a domain event
  consumed by `infrastructure/observability/audit_log.cpp`. The
  web-UI's "audit" page reads from this stream.
* **Idempotency**: repository writes accept an optional `IdempotencyKey`
  so HTTP API retries are safe.
* **Transactions**: each use-case opens a `UnitOfWork` from
  `IUnitOfWork` (no-op for in-memory/file repos; real for SQL).

## 8. Testability

A typical unit-test:

```cpp
TEST_CASE("LoginUser rejects banned IP")
{
    InMemoryAccountRepository accs;
    InMemoryBanRepository     bans; bans.banIp("10.0.0.0/8", "abuse", …);
    InMemorySessionRegistry   sess;
    NullEventBus              bus;
    FixedClock                clk{ Clock::TimePoint{} };
    NullLogger                log;

    auto acc = make_account("alice", "p@ss");
    accs.save(acc);

    LoginUser uc{accs, sess, bans, bus, clk, log};
    auto r = uc.execute({"alice", hash("p@ss"), "STAR"_tag,
                         IpAddress::parse("10.1.2.3").value(), SessionId{1}});

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::Banned);
}
```

All dependencies are in-memory; the entire `application` library can
be exercised in well under a second.
