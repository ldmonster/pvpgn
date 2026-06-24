# Bug Hunt: In-game Mail + Telnet/Bot Admin Interface

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Scope: in-game `/mail` (send/read/del, quota, mailbox) and telnet/bot login +
admin command interface.

---

## SUMMARY TABLE

| # | Area | Severity | Class | One-line |
|---|------|----------|-------|----------|
| 1 | Mail command | High | NOT-IMPLEMENTED | `/mail` send/read/del, quota, mailbox-full, self-mail logic absent; only a port + in-memory fake exist |
| 2 | Telnet auth | Critical | BUG | Admin-console FSM has NO login: no username/password prompt, dispatches every line as `AccountId{0}` |
| 3 | Telnet wiring | Critical | NOT-IMPLEMENTED / BUG | `infra::session::TelnetSessionFactory` never instantiates the FSM, discards all inbound bytes (TODOs) |
| 4 | Telnet (alt) session | High | BUG | `integration::telnet::TelnetSession` "authenticates" any username/password with zero validation |
| 5 | Telnet IAC handling | Medium | BUG | Inbound IAC bytes skipped without consuming the following command/option byte → option bytes leak into the line buffer |
| 6 | Mail subsystem (domain) | Low | INTENTIONAL | `IMailStore` fake has no quota cap (matches "fake/CI" intent) but documents the missing enforcement |

---

## 1. MAIL — NOT-IMPLEMENTED (command layer)

**Severity:** High (feature parity) | **Class:** NOT-IMPLEMENTED

### Original
`src/bnetd/mail.cpp` provides the full subsystem:
- `handle_mail_command` (mail.cpp:252) — gates on `prefs_get_mail_support()`,
  parses `send|s`, `read|r`, `delete|del` subcommands.
- `mail_func_send` (mail.cpp:323) — looks up receiver, **enforces quota**:
  ```cpp
  Mailbox mbox(account_get_uid(recv));
  if (get_mail_quota(recv) <= mbox.size()) {
      message_send_text(... "Receiver has reached his mail quota. ...");
      return;
  }
  ```
- `get_mail_quota` (mail.cpp:308) — per-account `BNET\auth\mailquota` strattr,
  clamped `1..MAX_MAIL_QUOTA`, falling back to `prefs_get_mail_quota()`.
- `mail_func_read` / `mail_func_delete` (mail.cpp:357/418) — summary listing,
  index read, `delete <idx>` and `delete all`, numeric-index validation.
- `Mailbox` (mail.cpp:91-250) — filesystem mailbox under `prefs_get_maildir()`,
  `deliver`, `read`, `readAll`, `erase`, `clear`, `size`, `empty`.

### v3
- Port only: `src/domain/social/include/domain/social/ports.hpp:117-141`
  (`struct MailMessage`, `class IMailStore { send/inbox/delete_message }`).
- In-memory fake: `src/infra/inmemory/include/infra/inmemory/in_memory_mail_store.hpp`.
- `/mail` appears **only** as a name→canonical entry in the router table:
  `src/application/admin_commands/src/router.cpp:143  {"/mail","/mail"}`.
- No handler implements the send/read/delete subcommands, no quota check, no
  mailbox-full behavior, no self-mail handling, no `prefs_get_mail_support`
  gate. Confirmed: the only references to mail in `src/` are the port, the
  in-memory fake, and that one router-table line (grep for
  `MAIL_FUNC|mail_func|handle_mail|"/mail"|mail send|mail read` yields nothing
  else).

### Divergence
The entire `/mail` user-facing command + quota enforcement is missing in v3.
The `IMailStore` port has a `send` with no quota argument and the fake never
caps inbox size, so even once wired there is no place quota is enforced.

### Proposed fix
Implement a `/mail` application handler over `IMailStore` that mirrors the
original: parse `send|read|delete`, enforce per-account quota before `send`
(quota source = account attr w/ prefs fallback, clamp 1..MAX), reject when the
recipient inbox is full, and gate on a mail-support pref. Add a quota parameter
(or a pre-check) at the `IMailStore::send` boundary so enforcement is testable.

### NOTE — original self-mail behavior
The original does **not** block self-mail; `mail_func_send` only checks the
receiver exists and is under quota. A v3 reimplementation should match (no
self-mail block) unless intentionally changing behavior.

---

## 2. TELNET — NO LOGIN, COMMANDS RUN AS GUEST (admin FSM)

**Severity:** Critical | **Class:** BUG

### Original
`src/bnetd/handle_telnet.cpp` implements a real login FSM:
- `conn_state_connected` → set username, prompt `"\r\nPassword: "`
  (handle_telnet.cpp:82-119).
- `conn_state_bot_username` → capture username, prompt for password (121-142).
- `conn_state_bot_password` (144-323) — looks up account, **verifies the
  password hash**:
  ```cpp
  if (bnet_hash(&trypasshash1, std::strlen(testpass), testpass) < 0) ...
  if (hash_eq(trypasshash1, oldpasshash1) != 1) { ... "Login failed." ... }
  ```
  Then enforces **bot-access gate** and **account-lock gate**:
  ```cpp
  if (account_get_auth_botlogin(account) != 1) { ... "Account has no bot access." }
  else if (account_get_auth_lock(account) == 1) { ... locked ... }
  ```
  Only after all checks: `conn_login(c, account, loggeduser)`.
- Subsequent commands run in `conn_state_loggedin` via `handle_command(c, ...)`
  (handle_telnet.cpp:336-337), and `handle_command` enforces per-command group
  permissions against the **logged-in account**
  (`src/bnetd/command.cpp:599  command_get_group(...) & account_get_command_groups(conn_get_account(c))`).

### v3
`src/protocol/telnet/src/admin_fsm.cpp`:
- `on_connected` sends only `"=== pvpgn admin console ===\r\n> "` — **no
  username prompt, no password prompt** (admin_fsm.cpp:27-35).
- `on_line` immediately calls `execute_command(trimmed)` with no auth state
  (admin_fsm.cpp:37-63).
- `execute_command` dispatches with a hardcoded guest id:
  ```cpp
  // No login yet -- dispatch as guest account 0. A real login flow
  // would replace this AccountId with the authenticated caller.
  domain::AccountId caller{0};
  auto result = commands_->dispatch(caller, line, *permissions_);
  ```
  (admin_fsm.cpp:78-81).
- Unit test confirms this is the intended current behavior: `on_line("hello")`
  dispatches with no login step
  (`tests/unit/protocol/telnet/admin_fsm_test.cpp:92-103`).

### Divergence
The telnet admin console has **no authentication flow at all** and never
associates a real account with the session. Every command runs as
`AccountId{0}`.

**Mitigating fact (why "Critical" not "instant RCE"):**
`InMemoryPermissionChecker::has_permission` fails closed — `find_by_id(0)`
returns not-found → `false`
(`src/application/auth/src/permission_checker.cpp:60-91`). So with that checker,
guest 0 is *denied* every permissioned command rather than *granted* them. The
result is a console that can authenticate nobody and run nothing privileged —
broken, but not an escalation **for that checker**.

The danger: any `IPermissionChecker` that treats an unknown/zero account
permissively, or any command with no permission requirement, becomes runnable
by an unauthenticated TCP client. The admin telnet port is implicitly trusted
in the original (real password + bot-access + group checks); v3 removes all of
that. Classify as a real auth bug: **telnet lets unauthenticated users reach
the command dispatcher.**

### Proposed fix
Implement the login FSM in the telnet adapter: prompt Username → Password,
verify the password via the auth use case (hash compare), enforce a
bot/telnet-access flag and account-lock, then dispatch commands with the
**authenticated** `AccountId`. Until that exists, the FSM should refuse all
command dispatch (deny by default) rather than dispatch as account 0.

---

## 3. TELNET — FSM NEVER WIRED, INBOUND BYTES DISCARDED

**Severity:** Critical | **Class:** NOT-IMPLEMENTED / BUG

### v3
`src/infra/session/include/infra/session/telnet_session_factory.hpp:37-65`:
```cpp
// TODO: Create ISessionContext wrapper for tcp_session
// For now, we instantiate the FSM but don't wire it
// auto fsm = std::make_shared<protocol::telnet::TelnetAdminFsm>(...);
tcp_session->set_on_bytes([session_id](core::ByteView bytes) {
    // TODO: Feed bytes to TelnetAdminFsm::on_line
    (void)bytes;
});
```
The factory accepts a connection, discards all inbound bytes, and never
constructs or feeds the `TelnetAdminFsm`. The codec (`try_parse_line`) is never
invoked, so there is no line framing in the wired path.

### Original
The dispatch path is fully connected: `server.cpp` reads raw telnet packets,
NUL-terminates the line (`server.cpp:599-674`), and calls
`handle_telnet_packet` which drives login + `handle_command`.

### Divergence
The "real" infra telnet path is a stub. Combined with #2, the only *functional*
telnet code in v3 is either the un-wired `TelnetAdminFsm` (no auth) or the
separate `integration::telnet::TelnetSession` (see #4).

### Proposed fix
Build the `ISessionContext`/`ITelnetSessionContext` wrapper over `TcpSession`,
construct the FSM per connection, run a streaming framer (`codec::try_parse_line`)
over `on_bytes`, and feed complete lines to `on_line` / the login FSM.

---

## 4. TELNET (alt impl) — LOGIN ACCEPTS ANY CREDENTIALS

**Severity:** High | **Class:** BUG

### v3
`src/integration/telnet/src/telnet_session.cpp` is a second, parallel telnet
implementation with its own login state machine:
```cpp
void TelnetSession::handle_login_line(std::string_view line) {
    if (state_ == State::connected) {            // username
        username_ = std::string(line);
        state_ = State::authenticating;
        send("Password: ");
    } else if (state_ == State::authenticating) { // password
        // Second line is password (in real implementation, validate it)
        state_ = State::authenticated;
        send("\r\nAuthenticated. Type 'help' for commands.\r\n");
        ...
```
(telnet_session.cpp:71-83). The password line is **never validated** — any
username + any password transitions to `authenticated`. There is no account
lookup, no hash check, no bot-access/lock gate.

After "auth", `handle_command_line` only knows `help`/`quit`/`channels` and is
not connected to the real command registry (telnet_session.cpp:85-104), so it
can't run admin commands — but the **login itself is a no-op auth**, which is
exactly the class of bug to flag (unauthenticated user is told "Authenticated").

### Original
Password hash is verified and bot-access/lock enforced before login
(handle_telnet.cpp:203-307, see #2).

### Divergence
Two competing telnet implementations exist (`protocol/telnet` FSM and
`integration/telnet` session); neither performs real authentication. This one at
least has the login *shape* but stubs out the actual check.

### Proposed fix
Wire `handle_login_line` to the auth use case: look up the account, verify the
password hash, enforce telnet/bot-access + lock, and only then set
`State::authenticated`. Decide on a single telnet implementation and delete the
other to avoid divergent auth paths.

---

## 5. TELNET — IAC byte skipped without consuming option byte

**Severity:** Medium | **Class:** BUG

### v3
`src/integration/telnet/src/telnet_session.cpp:21-29`:
```cpp
for (auto b : data) {
    const uint8_t byte = static_cast<uint8_t>(b);
    if (byte == IAC) {
        // Skip telnet negotiation for now
        continue;   // <-- only skips the 0xFF; next byte(s) NOT consumed
    }
    ...
```
A telnet negotiation is `IAC <command> [<option>]` (e.g. `FF FB 01`). This loop
drops only the `0xFF` and then processes the following command/option bytes as
ordinary input. `WILL`(0xFB)/`DO`(0xFD)/etc. are >= 127 so they're filtered by
the printable check, but option bytes like `ECHO`(0x01) or `SGA`(0x03) are < 32
and are also dropped by the printable filter — so in practice most bytes are
silently dropped, but any option byte in the printable ASCII range
(e.g. arbitrary subnegotiation data) would leak into `line_buffer_`. There is no
state machine to consume the fixed 2-3 byte IAC sequence, and `IAC SB ... IAC SE`
subnegotiation is not handled at all.

### Original
The original's `handle_telnet_packet` likewise does no full IAC negotiation; it
relies on line-mode clients and only special-cases a leading `\004`
(handle_telnet.cpp:89-90). So "no IAC state machine" matches the original's
minimalism — but the original does not advertise options either. v3's
`TelnetSession` constructor **sends** `IAC WILL ECHO / WILL SGA / DO SGA`
(telnet_session.cpp:11-16) and thereby invites IAC replies it cannot correctly
parse.

### Divergence
v3 actively negotiates options (sends IAC WILL/DO) but then mis-frames the
client's IAC responses. Either negotiate properly (consume `IAC + command [+
option]`, handle `SB..SE`) or don't advertise options (match the original and
strip IAC sequences correctly).

### Proposed fix
Add a small IAC consumer: on `IAC`, read the next byte as command; if it is
`WILL/WONT/DO/DONT` consume one more option byte; if `SB`, consume until
`IAC SE`. Or drop the negotiation entirely to match the original line-mode
behavior.

---

## 6. MAIL — in-memory store has no quota cap (informational)

**Severity:** Low | **Class:** INTENTIONAL (documented as fake)

`src/infra/inmemory/include/infra/inmemory/in_memory_mail_store.hpp:23-28`:
`send` unconditionally `push_back`s with no size limit. The file header says it's
a fake "for tests and development/CI." This is acceptable for a fake, but note
that the **quota concept does not exist anywhere in the v3 mail port** (no
quota field on `MailMessage`, no quota arg on `send`), so even the production
store would have nowhere to enforce the original's mailbox-full behavior. Tie
this to fix #1.

---

## WHAT MATCHES

- **Telnet line framing intent:** v3 `codec::try_parse_line`
  (`src/protocol/telnet/src/codec.cpp:8-18`) handles `\r\n` and bare `\n` and
  strips the trailing `\r`, matching the original's NUL-terminate-the-line +
  CRLF handling (`server.cpp` line read). (Note: not wired — see #3.)
- **No full IAC negotiation by design:** Both original and v3 admin/console
  paths assume line-mode clients and do not implement option negotiation as a
  core feature (the original only special-cases `\004`). v3's *extra* IAC
  advertise+mis-parse in the integration session is the divergence (#5).
- **quit/exit handling** in the admin FSM (admin_fsm.cpp:50-54) is a reasonable
  console convenience; the original telnet had no explicit quit verb but relied
  on connection close — not a bug, just an addition.
- **Mail data model shape:** v3 `MailMessage {from,to,subject,body,timestamp}`
  is a superset of the original's stored `sender + message + timestamp`
  (mail.cpp:64-89) — fine once a handler exists.

---

## OVERALL ASSESSMENT

- **Mail:** essentially NOT-IMPLEMENTED at the command/feature level. Only a
  port + in-memory fake exist; no `/mail` subcommands, no quota enforcement, no
  mailbox-full / support-gate behavior.
- **Telnet:** the security-critical login + per-command authorization that the
  original performs (password hash verify, bot-access gate, account lock,
  command-group permission check against the logged-in account) is **absent**.
  The `protocol/telnet` FSM dispatches as guest account 0 with no auth and is
  not even wired; the `integration/telnet` session fakes authentication
  (accepts any credentials). The only thing preventing privilege escalation
  today is that `InMemoryPermissionChecker` fails closed on unknown accounts —
  not a deliberate telnet auth gate. This is the highest-priority finding.
