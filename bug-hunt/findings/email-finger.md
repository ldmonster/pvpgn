# Bug-hunt: Email handling, /finger profile, account-flags display

Subsystem: SID_SETEMAIL (0x59) / SID_GETPASSWORD (0x5A) / SID_CHANGEEMAIL
(0x5B) and the `/finger` text profile + account-flags display.

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

## TL;DR

- The **storage key matches** (`BNET\acct\email`) — the prior "different
  namespace" worry is **disproven** for the v3 domain layer.
- The v3 email pure-functions (`dispatch_email_change`,
  `dispatch_password_recovery`) are **never wired into the live FSM**; the
  three protocol handlers are no-op stubs. So SETEMAIL/CHANGEEMAIL/GETPASSWORD
  are **NOT-IMPLEMENTED** at runtime even though the parsing + validation code
  exists.
- `/finger` text output is **NOT-IMPLEMENTED** in v3: only an alias-table
  entry exists; there is no handler body and nothing consumes the router.
- No takeover/auth-leak bug found in the *implemented* code. The password
  recovery dispatcher is actually **more** enumeration-safe than the original.

---

## Finding 1 — SETEMAIL/CHANGEEMAIL/GETPASSWORD dispatchers not wired (dead code)

- Severity: **High (functional regression)** — no security risk, pure loss of
  feature.
- Classification: **NOT-IMPLEMENTED** (parse path live, business logic dead).
- Original ref: `src/bnetd/handle_bnet.cpp:5423` `_client_setemailreply`,
  `:5449` `_client_changeemailreq`, `:5494` `_client_getpasswordreq` — all
  registered in the packet dispatch table (`:279`, `:215`, etc.) and actually
  mutate `account_set_email(...)`.
- v3 ref:
  - `src/protocol/bnet/src/codec.cpp:262,272,277` — packets **are** decoded
    (`decode_setemail_reply` / `decode_get_password_request` /
    `decode_change_email_request`).
  - `src/protocol/bnet/src/fsm/fsm_misc.cpp:71-89` — the three handlers
    `on(SetEmailReply&)`, `on(GetPasswordRequest&)`, `on(ChangeEmailRequest&)`
    each just `return core::ok();` and **discard** the decoded payload.
  - `src/application/email_management/src/email_change.cpp` —
    `dispatch_email_change` / `dispatch_password_recovery` are fully
    implemented but `grep` shows they are referenced **only inside their own
    module** (no FSM/use-case caller).
- Divergence: In the original, a SETEMAILREPLY persists the address; in v3 the
  decoded `SetEmailReply.email` is dropped on the floor — the account email is
  never stored from the wire. Same for CHANGEEMAIL and GETPASSWORD.
- Proposed fix: In `fsm_misc.cpp`, call `dispatch_email_change` /
  `dispatch_password_recovery`, then persist via the identity attribute-map
  (`AttributeMap::set_email`, key already correct) inside a UoW transaction.
  Until wired, document these as unimplemented so they aren't assumed working.

## MATCH — email storage key is correct

- Original: `account_wrap.cpp:2620/2631` get/set `"BNET\\acct\\email"`.
- v3: `src/domain/identity/src/attribute_map.cpp:18` `get("BNET\\acct\\email")`
  and `:66` `set("BNET\\acct\\email", ...)`.
- The migration reader also uses the same field
  (`src/app/pvpgn-migrate/migrate_accounts.cpp:305`).
- Conclusion: **no key-namespace divergence.** Prior finding does not hold for
  the v3 domain layer.

## MATCH — packet opcode constants are correct

- Original `bnet_protocol.h`: SETEMAILREPLY `0x59`, GETPASSWORDREQ `0x5a`,
  CHANGEEMAILREQ `0x5b`.
- v3 `messages_common.hpp:55-57`: `kSidSetEmail=0x59`,
  `kSidGetPassword=0x5A`, `kSidChangeEmail=0x5B`. Correct.

## Finding 2 — Stale/contradictory opcode comments

- Severity: **Trivial (cosmetic)**.
- Classification: **BUG (doc only)**.
- v3 ref: `src/protocol/bnet/src/fsm/fsm_misc.cpp:18-19` says
  `GetPasswordRequest — SID_GETPASSWORDREQ (0x06)` and
  `ChangeEmailRequest — SID_CHANGEEMAILREQ (0x5A)`. Both wrong: GETPASSWORD is
  `0x5A`, CHANGEEMAIL is `0x5B` (the constants in `messages_common.hpp` are
  right; only the comment lies). `messages_misc.hpp:191` also mislabels
  GETPASSWORD as `0x5A` in its section banner while the struct comment at
  `:202` correctly calls CHANGEEMAIL `0x5B`. 0x06 is GAMELISTREQ_GREED.
- Fix: correct the comments to 0x59/0x5A/0x5B.

## Finding 3 — v3 adds email format validation the original lacked

- Severity: **Low** — behavioral divergence, not a security issue. May reject
  inputs the original silently accepted (relevant only once Finding 1 is fixed).
- Classification: **INTENTIONAL** (documented as a deliberate, "intentionally
  permissive" tightening).
- Original ref: `account_wrap.cpp:2623-2632` `account_set_email` does **zero**
  format validation — it stores any non-NULL string. `_client_setemailreply`
  (`handle_bnet.cpp:5428`) only length-bounds via `MAX_EMAIL_STR`.
- v3 ref: `email_change.cpp:37-45` `looks_like_email` requires exactly one
  `'@'`, non-empty local part, and a domain containing a `'.'`; rejection ->
  `kNewInvalid`.
- Divergence: addresses like `foo` or `foo@bar` (no dot) that the original
  would store are rejected by v3. Practically harmless and arguably an
  improvement, but it is a behavior change to flag.
- Proposed fix: none required if intentional; ensure the rejection maps to a
  benign client outcome and is documented.

## MATCH — "set only when unset" + "echo current to change" semantics

- Original `_client_setemailreply` (`handle_bnet.cpp:5436`) refuses if an email
  is already set (you cannot silently overwrite via the set path). v3
  `dispatch_email_change` set-when-unset branch (`email_change.cpp:53-71`)
  honors this via `allow_when_unset` + the empty-`stored_email` guard, and even
  treats a stray `current_email_claim` on an unset address as
  `kCurrentMismatch`.
- Original `_client_changeemailreq` (`handle_bnet.cpp:5481`) requires
  `strcasecmp(email, oldaddr)==0` before replacing. v3 replace branch
  (`email_change.cpp:75-85`) uses `iequal(stored_email, current_email_claim)`
  (case-insensitive) -> `kCurrentMismatch` otherwise. Matches.

## Finding 4 — CHANGEEMAIL/GETPASSWORD target an arbitrary account by name (inherited)

- Severity: **Low/Informational** (pre-existing original design; v3 dispatcher
  preserves it but is unreachable per Finding 1).
- Classification: **INTENTIONAL / inherited** (not introduced by v3).
- Original ref: `_client_changeemailreq` and `_client_getpasswordreq` resolve
  the target via `accountlist_find_account(username)` from the **packet body**
  (`handle_bnet.cpp:5473`, `:5512`), i.e. not necessarily the connected
  account. The protection is "you must know the current email." Note: this
  means SETEMAIL is "your own account only" (uses `conn_get_account`), but
  CHANGEEMAIL/GETPASSWORD are username-addressed and pre-auth-capable.
- v3 ref: `EmailChangeRequest.username` / `PasswordRecoveryRequest.username`
  are passed in by the (not-yet-existing) caller; the dispatcher trusts the
  caller to resolve `stored_email`. The dispatcher itself enforces only the
  email-echo check.
- Divergence: none yet — but when wiring Finding 1, the caller MUST resolve the
  account from the packet-supplied username (matching original) and not assume
  the session account, or behavior will differ. Flag for the implementer.
- Proposed fix: when wiring, mirror original `accountlist_find_account(username)`
  for change/getpassword and `conn_get_account(c)` for setemail.

## MATCH (improved) — Password recovery is enumeration-safe in v3

- Severity: positive note (v3 hardens slightly).
- Original `_client_getpasswordreq` distinguishes "no such account"
  (`handle_bnet.cpp:5513`), "no email set" (`:5517`), and "email mismatch"
  (`:5521`) — only in logs, since no reply is sent. It also has a `TODO`
  (`:5524`) and **never actually mails anything** — recovery is a no-op in the
  original too.
- v3 `dispatch_password_recovery` (`email_change.cpp:88-112`) collapses
  no-email and mismatch into a single `kRejected` ("indistinguishable failure
  modes ... cannot be used to enumerate accounts"), and gates on
  `feature_enabled` -> `kDisabled`. No takeover path: it never returns a token
  unless `iequal(stored_email, claimed_email)` holds, and even then
  `token_to_deliver` is left for the caller to fill (dispatcher never reads
  account secrets).
- Conclusion: **no leak / no takeover** in the implemented dispatcher. Both the
  original and v3 lack actual mail delivery, so end-to-end recovery is
  unimplemented in both.

## Finding 5 — `/finger` text profile output not implemented

- Severity: **Medium (functional regression)** — user-visible command missing.
- Classification: **NOT-IMPLEMENTED**.
- Original ref: `src/bnetd/command.cpp:3391` `_handle_finger_command` renders a
  multi-line profile to the requester:
  1. `Login: {name} {uid} Sex: {sex}` (Sex omitted if empty) — `:3421-3430`
  2. `Created: {strftime %a %b %d %H:%M %Y}` (from `account_get_ll_ctime`) — `:3432`
  3. `Clan: {name}  Rank: {Chieftain|Shaman|Grunt|Peon}` if in a clan — `:3436-3466`
  4. `Location: {loc|unknown} Age: {age}` (Age omitted if empty) — `:3470-3478`
  5. `Client: {title}    Ver: {ver}   Country: {country}` if online — `:3482-3486`
  6. `Last login {time} from {ip}` / `On since {time} from {ip}` — `:3504-3510`
     (IP shown as `unknown` unless requester has `/admin-addr`) — `:3489-3493`
  7. **Admin-only block** (requester needs `/admin-addr`):
     `Operator: , Admin: , Locked: , Muted:` (Yes/No) — `:3520-3525`,
     `Email: {email}` — `:3527`, `Last login Owner: {owner}` — `:3530`
  8. `Idle {hh:mm:ss}` if online — `:3537`
  9. the account description, line by line, then a blank line — `:3541-3545`
- v3 ref: `src/application/admin_commands/src/router.cpp:29` maps
  `/finger -> /finger`, and `router.hpp:77` documents it, but:
  - There is **no `_handle_finger_command` body anywhere in v3** (no legacy
    `src/bnetd/command.cpp`; `grep "Login: {} {} Sex"`/`handle_finger_command`
    across `src` -> 0 hits).
  - **Nothing consumes `route()`** — `grep RouteAction::Handled` across `src`
    outside `router.cpp` -> 0 hits. So even the alias mapping is inert.
  - The v3 `application/profile` module (`profile_reply.cpp`) is the **binary
    WAR3 anongame profile** (SID_W3PROFILE ladder/race/AT-team blob), a
    completely different feature from the `/finger` text command.
- Divergence: the entire `/finger` profile + the admin-only account-flags
  display (Operator/Admin/Locked/Muted, Email, Last-login Owner) is absent.
- Proposed fix: port `_handle_finger_command` into an application use-case that
  reads the identity attribute-map (name/uid/sex/ctime/loc/age/desc/email),
  clan membership + rank, live connection (client/ver/country/idle/ip), and the
  auth flags, gating the IP + admin block on the `/admin-addr` command group.
  Then have a chat-command dispatcher actually invoke `route()`'s decision.

## Account-flags display (within /finger)

- Same status as Finding 5: the original prints auth flags
  (Operator/Admin/Locked/Muted) only in the admin-gated block
  (`command.cpp:3520`). v3 has the underlying flag getters in the domain layer
  but no `/finger` renderer, so this output is **NOT-IMPLEMENTED**. No incorrect
  flag mapping to report because there is no v3 code to compare.

---

## Summary table

| # | Area | Class | Severity |
|---|------|-------|----------|
| 1 | SETEMAIL/CHANGEEMAIL/GETPASSWORD handlers are no-op stubs; dispatchers dead | NOT-IMPLEMENTED | High |
| 2 | Wrong opcode comments (0x06/0x5A) in fsm_misc/messages_misc | BUG (doc) | Trivial |
| 3 | v3 adds email syntax validation original lacked | INTENTIONAL | Low |
| 4 | change/getpassword target packet-supplied username (wire carefully) | INTENTIONAL/inherited | Low |
| 5 | `/finger` text profile + admin flag block missing | NOT-IMPLEMENTED | Medium |
| M | Email storage key `BNET\acct\email` matches | MATCH | — |
| M | Opcodes 0x59/0x5A/0x5B match | MATCH | — |
| M | set-when-unset / echo-to-change semantics match | MATCH | — |
| M | Password recovery enumeration-safe (improved), no takeover | MATCH+ | — |
