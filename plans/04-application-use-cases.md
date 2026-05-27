# 04 — Application Layer: Use Cases & CQRS-lite

**Goal:** Every user-visible behaviour is implemented as a named **use
case** in `src/v3/application/<bounded-context>/<use_case>.{h,cpp}`.
Use cases orchestrate domain objects through **ports**, return
`core::Result<Output, StatusCode>`, never throw, never log directly
(they emit events instead).

## 1. Shape of a use case

```cpp
// application/identity/LoginAccount.h
namespace pvpgn::application::identity {

struct LoginAccountInput {
  domain::identity::Username       username;
  std::string                      password_plaintext;  // zeroised after use
  domain::net::Ipv4Address         peer_ip;
  std::string                      client_version_tag;
};

struct LoginAccountOutput {
  domain::identity::AccountId      account_id;
  domain::identity::SessionToken   session_token;
  core::BnetTime                   logged_in_at;
};

class LoginAccount final {
public:
  LoginAccount(AccountRepository& repo,
               PasswordHasher&    hasher,
               SessionTokenIssuer& tokens,
               core::Clock&       clock,
               core::EventBus&    events,
               IpBanService&      bans);

  [[nodiscard]] core::Result<LoginAccountOutput, core::StatusCode>
      operator()(LoginAccountInput in);

private:
  // refs to injected ports — no ownership
};

}  // namespace
```

Conventions:

- One use case per file. Filename = class name.
- Class is `final`. Constructor takes only port references.
  Composition root (`services/`) owns the lifetimes.
- `operator()` is the single public entry. Async use cases return
  a `core::Future<Output>` (already in `core/scheduler.h`?) — if not,
  defer to roadmap and stay sync-only for now (YAGNI).
- No `throw`. Invariant violations → `StatusCode`.
- Side effects allowed: persistence (via repo port), event publication
  (via `EventBus` port). Logging is **not** a side effect a use case
  does directly — `infra/log` subscribes to events.

## 2. Catalogue (target list — extend over time)

### identity
- `RegisterAccount`, `LoginAccount`, `LogoutAccount`,
  `ChangePassword`, `BanAccount`, `UnbanAccount`,
  `LookupAccountByName`, `ListSessions`.

### chat
- `JoinChannel`, `LeaveChannel`, `SendChannelMessage`,
  `SendWhisper`, `KickFromChannel`, `OpFromChannel`,
  `SetChannelTopic`, `ListChannels`.

### gameplay
- `CreateGame`, `JoinGame`, `LeaveGame`, `ReportGameResult`,
  `ListGames`, `CancelGame`.

### social
- `AddFriend`, `RemoveFriend`, `CreateClan`, `JoinClan`,
  `LeaveClan`, `InviteToClan`, `PromoteClanMember`,
  `CreateTeam`, `DisbandTeam`.

### realm
- `RegisterRealm`, `UnregisterRealm`, `HeartbeatRealm`,
  `RealmAuth` (s2s).

### ladder
- `RecomputeLadder`, `GetLadderEntry`, `GetLadderPage`.

### moderation
- `BanIp`, `UnbanIp`, `MuteAccount`, `IssueWarning`,
  `ListBans`.

### email_management, profile, tournament, ads, init, ports — already
have folders; backfill use-case files.

## 3. CQRS-lite split

- **Commands** = use cases above. They mutate state and may emit events.
- **Queries** = lightweight `application/<bc>/queries/*.h`. They take
  read-only repository projections and return DTOs. No events, no
  mutation. Typically free functions, not classes.
- No separate command/query bus. The "bus" is just calling the use
  case directly from a protocol adapter (`integration/bnet/*`). YAGNI
  forbids a Mediator/IServiceProvider until plugins demand it (then
  see `13-plugin-and-scripting.md`).

## 4. Ports referenced by use cases

Defined in `application/<bc>/ports/*.h`. List the canonical ones (see
also `05-ports-and-adapters.md`):

- `AccountRepository`, `SessionStore`, `PasswordHasher`,
  `SessionTokenIssuer`, `IpBanService`, `ChannelRegistry`,
  `MessageBroadcaster`, `GameDirectory`, `ClanRepository`,
  `FriendListStore`, `LadderRepository`, `RealmDirectory`,
  `MailStore`, `NewsStore`, `IconProvider`, `HelpfileSource`,
  `RandomSource`, `Clock`, `EventBus`, `MetricSink`, `AuditSink`.

## 5. Validation

- Input DTOs validate themselves in the use case's first 5 lines
  (`username.parse(...)` etc.). Garbage input ⇒
  `StatusCode::InvalidArgument`.
- Cross-aggregate invariants are enforced by re-reading state under
  a unit-of-work boundary (`AccountRepository::transactional<F>`).
  Optimistic concurrency via version field on the aggregate.

## 6. Test strategy (per use case)

`tests/unit/application/<bc>/<use_case>_test.cpp` with at minimum:

- happy path,
- one auth/permission failure,
- one invariant failure,
- one repository-failure (port returns `Result::error`),
- one duplicate/idempotency case where applicable.

Use in-memory fakes from `infra/inmemory/*` (already exists as a
folder; backfill fakes for every port).

## 7. Concrete tasks

- [ ] R233: define `application/<bc>/ports/*.h` headers for every
      bounded context (move existing ones into the canonical place).
- [ ] R234–R245: one bounded context per round; backfill use-case
      classes by extracting logic from `integration_legacy_bnetd_linked`.
- [ ] R246: wire the bnet protocol handlers to call use cases instead
      of legacy functions; flip `PVPGN_V3_BNETD_INTEGRATION` to
      mandatory.
