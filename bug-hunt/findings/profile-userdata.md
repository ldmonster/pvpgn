# Bug-hunt: account profile + userdata wire ops

Subsystem: SID_READUSERDATA (0x26), SID_WRITEUSERDATA (0x27), SID_LOOKUPACCOUNT,
/finger, account creation (0x03/0x1A/0x3D/0x52), SID_CHANGEPASSWORD.

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Naming note: in the original PvPGN the "userdata" packets are named
`STATSREQ`/`STATSREPLY` (0x26ff) and `STATSUPDATE` (0x27ff) in
`src/common/bnet_protocol.h`; the handlers are `_client_statsreq` /
`_client_statsupdate` in `src/bnetd/handle_bnet.cpp`. These are the
READUSERDATA / WRITEUSERDATA operations.

---

## Executive summary

The v3 **codec** layer (decode/encode) for READUSERDATA / WRITEUSERDATA and the
**application** use-cases for CreateAccount / ChangePassword / LookupAccount are
present and mostly faithful. However the **protocol FSM handlers that connect
wire messages to behaviour are stubs**: READUSERDATA, WRITEUSERDATA,
CHANGEPASSWORD, CREATEACCOUNT(0x03), CREATEACCTREQ2(0x3D) and CREATEACCOUNT_W3
(0x52/CREATEACCOUNT2) all `return core::ok()` without doing anything. Only the
legacy OLS `CREATEACCTREQ1` (0x1A) is actually wired end-to-end.

So most of this subsystem is **NOT-IMPLEMENTED at the protocol boundary**. The
good news: because WRITEUSERDATA performs no write at all, the cross-account /
protected-key write vulnerability the task warned about is **not currently
exploitable** — but the security restriction (own-account + `profile\` prefix +
`changeprofile` auth) is also **absent**, so it must be added when the handler is
implemented or it will regress into a CRIT. Findings flagged accordingly.

A separate divergence exists in implemented code: the account-name validation
rules in `domain::UserName::parse` differ from the original `account_check_name`
(allowed symbol set, mandatory leading letter, non-configurable).

---

## FINDING 1 — WRITEUSERDATA handler is a no-op; own-account + profile-prefix restriction absent
Severity: HIGH (latent CRIT) | Classification: NOT-IMPLEMENTED (with security gap to preserve)

Original ref — `src/bnetd/handle_bnet.cpp:3479` `_client_statsupdate`:
- Ignores the name(s) in the packet; only ever mutates `conn_get_account(c)`
  (the caller's OWN account): line 3520 `if ((account = conn_get_account(c)))`.
- Requires `account_get_auth_changeprofile(account)` (line 3521) else refuse.
- Only allows keys with `strlen(key) >= 9 && strncasecmp(key,"profile\\",8)==0`
  (line 3529); anything else is logged as "suspicious" and skipped.

  ```cpp
  if (std::strlen(key) < 9 || strncasecmp(key, "profile\\", 8) != 0)
      eventlog(... "suspicious key" ...);
  else
      account_set_strattr(account, key, val);
  ```

v3 ref — `src/protocol/bnet/src/fsm/fsm_chat.cpp:368`:
```cpp
core::Status<> BnetFsm::on(const UserDataWriteRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn)
        return reject("bnet fsm: WRITEUSERDATA before login");
    return core::ok();   // <-- no write, no auth check, no prefix check
}
```
No use-case exists in `BnetUseCaseContext`
(`src/protocol/bnet/include/protocol/bnet/use_case_context.hpp`) for userdata
writes.

Divergence: WRITEUSERDATA does nothing. Profile updates from clients are
silently dropped. Because nothing is written, there is currently **no**
cross-account write bug. But the three protections (own-account-only,
`changeprofile` auth, `profile\` prefix whitelist) are not implemented anywhere,
so a naive future implementation that writes `req.names[i]` would be a CRIT
cross-account / protected-key overwrite.

Proposed fix: when implementing, (a) ignore `req.names` and target only the
session's own account, (b) gate on the `changeprofile` permission, (c) reject
keys that are not `profile\…` (length >= 9, case-insensitive prefix). Add a
regression test for "WRITEUSERDATA naming another account does not touch it".

---

## FINDING 2 — READUSERDATA handler is a no-op; never sends a reply
Severity: HIGH | Classification: NOT-IMPLEMENTED

Original ref — `src/bnetd/handle_bnet.cpp:1488` `_client_statsreq`:
- Builds `SERVER_STATSREPLY` echoing `name_count`, `key_count`, `requestid`
  (lines 1522-1524).
- For each name × each key (row-major), appends one value string
  (lines 1528-1538); empty key strings are skipped (`if (*key=='\0') continue;`).
- Access gating via `_attribute_req(reqacc, myacc, key)`
  (`handle_bnet.cpp:1470`): returns `""` for missing account/attr, and hides
  `BNET*` keys of OTHER accounts (`reqacc != myacc && !strncasecmp(key,"BNET",4)`).
- Unknown account falls back to the caller's own account (line 1530-1531).

v3 ref — `src/protocol/bnet/src/fsm/fsm_chat.cpp:361`:
```cpp
core::Status<> BnetFsm::on(const UserDataReadRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn)
        return reject("bnet fsm: READUSERDATA before login");
    return core::ok();   // <-- never builds/sends UserDataReadReply
}
```

Divergence: the client gets no `SERVER_STATSREPLY`. Profile reads (and the
client-side correlation by `request_id`) never complete. No use-case is wired.

Proposed fix: implement the handler to build a `UserDataReadReply` echoing
`request_id`, `name_count = names.size()`, `key_count = keys.size()`, with
`values.size() == name_count*key_count` in row-major order, `""` for missing
account/attribute, and the `BNET*`-other-account hiding rule from
`_attribute_req`. The empty-key skip in the original is a quirk — see Finding 6.

---

## FINDING 3 — CHANGEPASSWORD wire handler is a no-op; use-case implemented but unwired
Severity: HIGH | Classification: NOT-IMPLEMENTED (application layer done; protocol stub)

Original ref — `src/bnetd/handle_bnet.cpp:890` `_client_changepassreq`:
- Refuses if account is logged in or missing (line 916), if `changepass` auth
  disabled (920), or if session key mismatches (924).
- Old-password proof: double-hashes stored hash1 with (ticks, sessionkey) and
  compares to `oldpassword_hash2` (lines 941-966). On match sets new pass and
  replies `SERVER_CHANGEPASSACK_MESSAGE_SUCCESS`, else `_FAIL` and increments
  passfail count.
- If account has no stored pass (968) or stored hash is corrupt (944), it
  ACCEPTS the new pass (success).

v3 ref — `src/protocol/bnet/src/fsm/fsm_auth.cpp:254`:
```cpp
core::Status<> BnetFsm::on(const ChangePasswordRequest&) { return core::ok(); }
```
The actual logic exists and is good in
`src/application/auth/src/change_password.cpp` — including a
`ChangePasswordWithSessionHashRequest` overload that re-derives hash2 from stored
hash1 + (ticks, sessionkey) and compares constant-time (lines 55-93), which
mirrors the original double-hash check. It is simply never called from the FSM,
and no `PassChangeReply`/`SERVER_CHANGEPASSACK` is ever sent.

Sub-divergence to watch when wiring (BUG-in-waiting):
- The use-case returns `InvalidCurrentPassword` on hash mismatch. The original
  *accepts* the change when the account has NO stored password or a corrupt
  stored hash. The bridge must map "no stored pass" to success to preserve
  parity (`change_password.cpp` has no such branch).
- The original verifies the connection session key equals the packet session key
  *before* the password check. The session-hash use-case derives hash2 from the
  packet's `sessionkey` but does not independently assert it equals the
  connection's session key. Add that guard in the bridge.

Proposed fix: wire `on(ChangePasswordRequest)` to the ChangePassword use-case,
map results to `SERVER_CHANGEPASSACK_MESSAGE_SUCCESS/_FAIL`, send the reply, and
preserve the "no/ corrupt stored pass => success" and session-key-equality
behaviours.

---

## FINDING 4 — CREATEACCOUNT (0x03), CREATEACCTREQ2 (0x3D), CREATEACCOUNT_W3/2 (0x52) handlers are no-ops
Severity: MEDIUM | Classification: NOT-IMPLEMENTED

Original refs (`src/bnetd/handle_bnet.cpp`):
- `_client_createacctreq2` (line 836): checks `allow_new_accounts`, then
  `account_check_name` → `SERVER_CREATEACCTREPLY2_RESULT_INVALID` for bad
  symbols (867-871), then `accountlist_create_account` → `_RESULT_EXIST` on
  failure (874-877), else `_RESULT_OK`.
- `_client_createaccountw3` (line 675): `allow_new_accounts` → EXIST;
  `account_check_name` → `SERVER_CREATEACCOUNT_W3_RESULT_INVALID` (741-748);
  create failure → EXIST; success → OK.

v3 ref — `src/protocol/bnet/src/fsm/fsm_auth.cpp`:
```cpp
core::Status<> BnetFsm::on(const CreateAccount2Request&) { return core::ok(); } // 0x52, line 205
core::Status<> BnetFsm::on(const CreateAccountRequest&)  { return core::ok(); } // 0x3D, line 256
```
These accept silently and never create, validate, or reply.

POSITIVE / MATCHES: the v3 result-code constants are byte-correct vs original:
- `messages_realm.hpp:196-203` (`kCreateAccount2Result*` for 0x52) match
  `SERVER_CREATEACCOUNT_W3_RESULT_*` (`bnet_protocol.h:1800-1807`):
  OK=0, EXIST=4, EMPTY=7, INVALID=8, BANNED=9, SHORT=0xA, PUNCT=0xB, PUNCT2=0xC.
- `messages_legacy.hpp:220-228` (`kCreateAccountResult*` for 0x3D) match
  `SERVER_CREATEACCTREPLY2_RESULT_*` (`bnet_protocol.h:1861-1869`):
  OK=0, SHORT=1, INVALID=2, BANNED=3, EXIST=4, IN_PROGRESS=5, ALPHANUM=6,
  PUNCT=7, PUNCT2=8.
The constants are simply unused because the handlers are stubs.

Proposed fix: implement the 0x3D and 0x52 handlers via the CreateAccount
use-case, mapping `CreateAccountError` → the matching result code
(UsernameTaken→EXIST, UsernameInvalidChars→INVALID, TooShort→SHORT,
IpBanned→BANNED), and honour `allow_new_accounts`.

---

## FINDING 5 — CreateAccount use-case declares validation errors but never validates
Severity: MEDIUM | Classification: BUG

v3 ref — `src/application/auth/include/application/auth/create_account.hpp:39-48`
declares `CreateAccountError::{UsernameTooShort, UsernameTooLong,
UsernameInvalidChars, ServerFull}`, but `src/application/auth/src/create_account.cpp`
`execute()` never returns any of them — it only checks IP-ban, name-taken,
aggregate create, and persistence. Length/char validation happens *upstream* in
`domain::UserName::parse` at the FSM call site (`fsm_auth.cpp:229`,
`fsm_auth.cpp:99`), which collapses all UserName failures into a single
parse-error.

Divergence: the use-case cannot distinguish "too short" vs "invalid chars" vs
"too long", so a bridge calling the use-case directly cannot emit the
original's distinct `SHORT` vs `INVALID` result codes (original
`createacctreq2`/`w3` use `account_check_name` which returns a single -1, so the
original *also* collapses to INVALID for both — but the original always sorts
length problems under INVALID, never SHORT, for 0x3D/0x52; see note). Net effect:
result-code granularity present in the wire enum is unreachable through the
use-case.

Proposed fix: either validate UserName inside `CreateAccount::execute` and
return the specific `UsernameTooShort/TooLong/InvalidChars`, or have the bridge
inspect the `UserName::parse` failure and pick the right code. Remove the dead
`ServerFull` path or implement it.

Note: original `account_check_name` returns only -1 for any length/char failure,
so original 0x3D/0x52 in practice send INVALID for short names too (they never
send SHORT). Preserving exact parity means mapping all name failures to INVALID,
which makes the declared SHORT/TOOLONG enum members cosmetic. Flag as UNSURE
whether v3 intends stricter granularity than legacy.

---

## FINDING 6 — Account-name validation rules diverge from original `account_check_name`
Severity: MEDIUM | Classification: BUG

Original ref — `src/bnetd/account.cpp:712` `account_check_name`:
- Rejects only `/` and `\` hard-coded; allows ALL `isalnum`; allows any char in
  the **configurable** `prefs_get_account_allowed_symbols()` (default
  `"-_[]"` — see `src/common/setup_before.h:238` `PVPGN_DEFAULT_SYMB`).
- Length: `i >= MIN_USERNAME_LEN(2) && i < MAX_USERNAME_LEN(16)` i.e. 2..15 chars
  (`src/common/field_sizes.h:29-30`).
- Does NOT require the name to start with a letter.

v3 ref — `src/domain/shared/include/domain/shared/user_name.hpp:30-51`
`UserName::parse`:
```cpp
if (s.size() < 2 || s.size() > 15) ...                 // 2..15: MATCHES
if (!std::isalpha(first)) ... "must start with a letter"  // DIVERGES: original allows leading digit/symbol
for (char c : s)
    if (!(isalnum(u) || c=='_' || c=='-' || c=='.')) ...   // DIVERGES: hardcoded set
```

Divergences:
1. **Mandatory leading letter** — original allows names starting with a digit or
   allowed symbol (e.g. `[clan]name`, `42player`). v3 rejects them.
2. **Allowed symbol set is hard-coded `_ - .`** — original default is `-_[]`
   (square brackets, very common for clan tags). v3 **rejects `[` and `]`** and
   **accepts `.`** which the original default does not. Also v3 ignores the
   `account_allowed_symbols` config entirely, so admins cannot customise it.
3. The header docstring (`user_name.hpp:7-12`) cites `account_check_name` but
   then states a stricter rule than the code it references actually enforces.

Impact: accounts that were valid on the original server (e.g. `[CLAN]Bob`,
`1stPlace`) cannot be created or looked up in v3; conversely `a.b` is accepted in
v3 but not under the original default. This affects CREATEACCOUNT, LOOKUPACCOUNT
and every name-parsing path.

Proposed fix: drop the leading-letter requirement, change the hard-coded set to
match the default `-_[]`, and source the allowed-symbol set from configuration to
honour `account_allowed_symbols`. Reject `/` and `\` explicitly (currently they
fall through the not-in-set branch, which is fine, but make it intentional).

---

## FINDING 7 — LOOKUPACCOUNT / SID_LOOKUPACCOUNT not wired to protocol
Severity: LOW | Classification: NOT-IMPLEMENTED

v3 ref — `src/application/auth/src/lookup_account.cpp` implements
`LookupAccountByName::execute` (parse name, find account, report locked/online).
It is not referenced anywhere under `src/protocol/bnet/` (no FSM handler, not in
`BnetUseCaseContext`). The original PvPGN has no dedicated SID_LOOKUPACCOUNT
BNCS packet either (lookup is via NLS login / the `/finger` chat command), so
this is informational. The use-case also inherits the Finding-6 name-validation
divergence via `UserName::parse` (`lookup_account.cpp:12`), so valid legacy names
like `[CLAN]Bob` will fail lookup.

Proposed fix: when adding /finger or a lookup command, route through this
use-case and ensure the name rules match Finding 6.

---

## What MATCHES (positive parity)

- **READUSERDATA/WRITEUSERDATA codec field order & counts** are correct
  (`src/protocol/bnet/src/codec/codec_w3.cpp:184-354`):
  - Read request encodes `name_count, key_count, request_id, names…, keys…`
    (encode line 327-335) and decodes in the same order (184-198) — MATCHES
    original `t_client_statsreq` layout.
  - Read reply encodes `name_count, key_count, request_id, values…`
    (337-344) — `request_id` round-trips, counts echoed. MATCHES.
  - Write request encodes `name_count, key_count, names…, keys…, values…`
    (346-354) with `values.size() == names*keys` — MATCHES original
    `t_client_statsupdate` layout (no request_id on write, correct).
  - Defensive caps: per-list 256, cell-matrix 4096 (codec_w3.cpp:168, 207, 233).
    These are v3-only hardening; not in original but harmless/safe.
- **CreateAccount result-code constants** byte-for-byte match original for both
  0x3D and 0x52 (see Finding 4 positive note).
- **Username length bounds** 2..15 match original (Finding 6 item, length only).
- **ChangePassword double-hash old-password proof** logic is faithfully
  reproduced in the application use-case (`change_password.cpp:55-93`), just not
  wired.
- **CREATEACCTREQ1 (0x1A)** IS wired end-to-end (`fsm_auth.cpp:217-251`) and
  correctly maps any failure to the OK/NO-only result space of REPLY1
  (original `createacctreq1` likewise only has OK/NO) — MATCHES. It honours the
  "no use-case ⇒ refuse honestly" path rather than ACK-ing a phantom create.

---

## Severity roll-up

| # | Item | Severity | Class |
|---|------|----------|-------|
| 1 | WRITEUSERDATA no-op; own-account/profile-prefix guard absent | HIGH (latent CRIT) | NOT-IMPLEMENTED |
| 2 | READUSERDATA no-op; no reply | HIGH | NOT-IMPLEMENTED |
| 3 | CHANGEPASSWORD wire handler no-op (use-case exists) | HIGH | NOT-IMPLEMENTED |
| 4 | CREATEACCOUNT 0x03/0x3D/0x52 handlers no-op | MEDIUM | NOT-IMPLEMENTED |
| 5 | CreateAccount use-case declares but never returns validation errors | MEDIUM | BUG |
| 6 | Username validation rules diverge (leading letter, symbol set, non-configurable) | MEDIUM | BUG |
| 7 | LOOKUPACCOUNT not wired | LOW | NOT-IMPLEMENTED |

---

## RESOLVED (wave 45): READUSERDATA / WRITEUSERDATA implemented

The 0x26/0x27 handlers were stubs that returned ok() with no reply, so a client
reading its profile got nothing. Implemented against the original
_client_statsreq / _client_statsupdate:
- New IUserProfileStore (application/auth) + InMemoryUserProfileStore (run-loop
  scoped, per-account string attributes), wired into BnetUseCaseContext.
- on(UserDataWriteRequest): stores `profile\*` keys on the CALLER's own account
  (mirrors the original, which ignores non-profile keys + only mutates self).
- on(UserDataReadRequest): replies SID_READUSERDATA with name_count/key_count/
  request_id echoed + name-major/key-minor values (stored value or "" if unset;
  "BNET\" keys hidden cross-account).
- diff_userdata.py: write profile\sex/age/location, read them back (+ an unset
  key) -> ["m","99","NY",""], matching the oracle. bncs_client.py helpers
  write_userdata / read_userdata. Unit suite green (the new BnetUseCaseContext
  field was added to the fsm_test / fsm_channel_test designated-init blocks).

## RESOLVED (wave 46): SID_PROFILE (0x35) profile view

on(ProfileRequest) was a stub (state-check only, no reply). Implemented against
_client_profilereq: resolve the requested account (account_repo); for a
nonexistent account send nothing (matches the original); otherwise reply
ProfileReply{cookie, fail=0, description, location, clan_tag=0} where
description/location come from the wave-45 profile store (profile\description /
profile\location, i.e. the original's account_get_desc / account_get_loc). The
reply has no timestamps, so it byte-diffs exactly. diff_profile.py: write a
description + location, request the profile, get them back — matches the oracle.
bncs_client.py helper request_profile.

## Wave 55: READUSERDATA fallback + BNET\ system fields
Verified differentially (tests/diff/diff_userdata_edges.py): (1) reading a
nonexistent account name returns the caller's own profile (oracle's reqacc=myacc
fallback); (2) BNET\acct\username and BNET\acct\userid are served on a self-read
(seeded at account creation in the oracle) and stay hidden cross-account. Fixed in
on(UserDataReadRequest): name->account resolution with self-fallback, system
fields computed from the account aggregate. Happy-path write/read + cross-account
read already matched (diff_userdata.py). Dynamic BNET fields (ctime) are not
served — wall-clock, not differentially stable.
