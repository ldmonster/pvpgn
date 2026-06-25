# Bug Hunt — Authentication Crypto Flow (OLS double-hash, NLS/SRP, verifier storage)

Subsystem: auth crypto FLOW correctness (beyond packet layout).
ORIGINAL: `/home/cnupt/work/pvpgn-server`  CURRENT v3: `/home/cnupt/work/pvpgn`

Scope note: per the brief, the SRP-3-vs-SRP-6a decision and the hash-constants
work are treated as SEPARATE and are NOT re-reported here. Findings below are
FLOW / LOGIC divergences in the login + change-password + verifier paths.

---

## FINDING 1 — OLS login feeds the client's double-hash (hash2) into a plain compare against the stored single-hash (hash1). Real OLS clients can NEVER log in. [CRIT / BUG]

**Severity:** CRITICAL (login-breaking for every OLS client: SC/BW, D1, W2BNE, D2, etc.)
**Classification:** BUG

### Original behaviour (the canonical OLS double-hash)
`src/bnetd/handle_bnet.cpp` `_client_loginreq2` (SID_LOGONRESPONSE2 / CLIENT_LOGINREQ2 = 0x3A), lines 1784-1817:

```c
struct { bn_int ticks; bn_int sessionkey; bn_int passhash1[5]; } temp;
...
bn_int_set(&temp.ticks,      bn_int_get(packet->u.client_loginreq2.ticks));
bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_loginreq2.sessionkey));
hash_set_str(&oldpasshash1, oldstrhash1);              // stored hash1 (40-hex)
hash_to_bnhash(&oldpasshash1, temp.passhash1);
bnet_hash(&oldpasshash2, sizeof(temp), &temp);         // hash2 = bnet_hash(ticks||sessionkey||hash1)
bnhash_to_hash(packet->u.client_loginreq2.password_hash2, &trypasshash2);  // client-sent hash2
if (hash_eq(trypasshash2, oldpasshash2) == 1) { /* OK */ }
```

The wire packet carries the **double-hash**, confirmed by the protocol struct
`src/common/bnet_protocol.h:1549-1557`:

```c
#define CLIENT_LOGINREQ2 0x3aff
typedef struct { ...; bn_int ticks; bn_int sessionkey; bn_int password_hash2[5]; ... } t_client_loginreq2;
```

So the server NEVER receives hash1 on login. It recomputes the expected hash2
from `(ticks, sessionkey, stored-hash1)` and compares against the client's hash2.

### v3 behaviour
The decoder correctly reads the three fields (codec_auth.cpp:68-76):
```cpp
RD_U32(m.client_token);   // = ticks
RD_U32(m.server_token);   // = sessionkey
for (auto& word : m.password_hash) RD_U32(word);   // = the client's hash2
```

But the FSM handler `BnetFsm::on(const LogonResponse2&)`
(`src/protocol/bnet/src/fsm/fsm_auth.cpp:72-162`) **discards client_token /
server_token** and feeds `password_hash` (which is hash2) straight into the
plain-compare login overload:

```cpp
std::string password_hash = pack_hash1_le(m.password_hash);          // mislabelled: this is hash2
auto password_hash_result = domain::BNHash::from_bytes(password_hash);
application::auth::LoginRequest login_req{
    .name = ..., .password_candidate = password_hash_result.value(), ... };
auto login_result = use_cases_.login_user->execute(login_req);        // LoginRequest overload
```

`LoginUser::execute(LoginRequest)` (login_user.cpp:22) calls
`account.login(req.password_candidate, ...)`, whose check is
(`account.hpp:154`) `if (!(password_ == candidate))` where `password_` is the
**stored hash1** (set verbatim from create-account, see Finding 4).

### Divergence
v3 compares `client_hash2 == stored_hash1`. These are different values by
construction (hash2 = bnet_hash(ticks||sessionkey||hash1)). The compare can
never succeed for a real client → **every OLS login fails** (`InvalidCredentials`).
Conversely the `ticks`/`sessionkey` the server handed out are never validated,
so the entire session-token binding is dropped.

Note: v3 *has* the correct machinery — `LoginUser::execute(LoginWithSessionHashRequest)`
(login_user.cpp:76-151) re-derives the expected hash2 via
`IPasswordHasher::derive_session_hash(hash1, ticks, sessionkey)` and constant-time
compares it to the client's hash2. The FSM simply calls the WRONG overload.

### Proposed fix
In `BnetFsm::on(const LogonResponse2&)`, build a `LoginWithSessionHashRequest`
(not `LoginRequest`):
- `password_hash2` = `BNHash::from_bytes(pack_hash1_le(m.password_hash))`
- `ticks`      = `m.client_token`
- `sessionkey` = `m.server_token`
and dispatch `use_cases_.login_user->execute(LoginWithSessionHashRequest{...})`.
This also requires wiring a concrete `IPasswordHasher` (see Finding 2).

---

## FINDING 2 — No production `IPasswordHasher` adapter exists; `derive_session_hash` is implemented only by test fakes. The OLS hash2 path is unwired. [CRIT / NOT-IMPLEMENTED]

**Severity:** CRITICAL (prerequisite for any correct OLS login / password-change)
**Classification:** NOT-IMPLEMENTED

### Evidence
`derive_session_hash` is declared on the port
(`domain/identity/ports.hpp:88-98`) with the correct contract:
`hash2 = bnet_hash(client_token || server_token || hash1)`.

Grep for any *implementation* of `derive_session_hash` across the whole tree
returns ONLY test fakes — no production class derives from `IPasswordHasher`:
- `tests/unit/application/auth/login_user_session_hash_test.cpp` (`FakeHasher`)
- `tests/unit/application/auth/change_password_session_hash_test.cpp`
- ... (other auth unit tests)

There is no adapter in `src/infra/crypto/` that bridges the existing
`blizzard_hash()` (bnet_hash.cpp:194) to the port. The `blizzard_hash` primitive
itself is faithfully ported and correct; it is simply never wired to the login
flow.

### Divergence
Even if Finding 1 is fixed to call the right overload, `LoginUser` is constructed
with `hasher_ == nullptr` (no adapter to inject), so the hash2 overload returns
`LoginError::Internal` immediately (login_user.cpp:77). The OLS session-hash
path is effectively dead.

### Proposed fix
Add a concrete adapter in `infra/crypto` implementing `derive_session_hash` as:
pack `client_token` (LE u32) || `server_token` (LE u32) || `hash1` (20 LE bytes)
into a 28-byte buffer, run `blizzard_hash` over it, and return the digest in
wire-LE form as a `BNHash`. Mirror the original `temp{ticks,sessionkey,passhash1[5]}`
byte layout exactly (4 + 4 + 20 = 28 bytes, little-endian). Inject it into
`LoginUser` and `ChangePasswordUseCase` at service construction.

---

## FINDING 3 — SID_CHANGEPASSWORD proves the OLD password via the same double-hash, but the v3 FSM handler is a no-op stub; and create/login already can't round-trip. [HIGH / NOT-IMPLEMENTED]

**Severity:** HIGH (password change unimplemented for OLS clients)
**Classification:** NOT-IMPLEMENTED (logic is present in the use-case; not wired)

### Original
`_client_loginreq*` analogue for password change, `handle_bnet.cpp:930-957`:
```c
struct { bn_int ticks; bn_int sessionkey; bn_int passhash1[5]; } temp;
...
bnet_hash(&oldpasshash2, sizeof(temp), &temp);                       // expected hash2
bnhash_to_hash(packet->u.client_changepassreq.oldpassword_hash2, &trypasshash2);
if (hash_eq(trypasshash2, oldpasshash2) == 1) {
    bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
    account_set_pass(account, hash_get_str(newpasshash1));          // new password = hash1 (single)
}
```
i.e. OLD password proven via double-hash; NEW password stored as the raw hash1.
Wire struct confirms it carries `oldpassword_hash2` + `newpassword_hash1`
(codec/codec_legacy_ols.cpp:208-217 in v3 decodes exactly this).

### v3
The use-case `ChangePasswordUseCase::execute(ChangePasswordWithSessionHashRequest)`
(change_password.cpp:55-93) reproduces this correctly:
```cpp
domain::BNHash expected = hasher_->derive_session_hash(account.password_hash1(), req.ticks, req.sessionkey);
if (!(expected == req.current_password_hash2)) return InvalidCurrentPassword;
...
account.change_password(req.new_password);   // new_password = the new hash1
```
This matches the original (prove old via hash2, store new as hash1). **However**:
1. The FSM dispatcher does NOT call it — `BnetFsm::on(const ChangePasswordRequest&)`
   (fsm_auth.cpp:254) is `{ return core::ok(); }`, a silent no-op. The legacy
   `kSidChangePassword` packet is decoded but never acted on.
2. Same missing `IPasswordHasher` blocker as Finding 2 (the 3-arg ctor leaves
   `hasher_ == nullptr` → returns `Internal`).

### Proposed fix
Wire `on(ChangePasswordRequest)` to build a `ChangePasswordWithSessionHashRequest`
(`current_password_hash2` from the packet's `oldpassword_hash2`,
`new_password` = `pack_hash1_le(newpassword_hash1)`, `ticks`, `sessionkey`) and
dispatch the use-case. Requires Finding 2's adapter.

---

## FINDING 4 — Stored passhash IS the single-hash (hash1), and create/login store/read it identically. [MATCHES — no bug here]

**Classification:** MATCHES (this part is correct).

- Original stores `account_set_pass(account, hash_get_str(newpasshash1))` — the
  client-supplied hash1 verbatim (createacctreq1: handle_bnet.cpp:819-820;
  createacctreq2: 873-874).
- v3 create paths (`on(CreateAccount1Request)` fsm_auth.cpp:234,
  `on(CreateAccount2Request)` is a stub but the use-case path
  `create_account.cpp`) store `pack_hash1_le(password_hash1)` as the account's
  `BNHash` password verbatim. The SAME `pack_hash1_le` packing is used by both
  create and the (broken) login path (fsm_auth.cpp:42-52), so the stored value
  and what login reads are byte-identical.

Conclusion: the **storage** of hash1 is consistent and correct. The break is
purely that login compares the client's *hash2* against this stored *hash1*
(Finding 1), not a create/verify packing mismatch. Once Finding 1+2 are fixed,
the round-trip is sound.

---

## FINDING 5 — Constant-time comparison: BNHash `==` is constant-time (good); NLS M1 compare uses `memcmp` (minor). [LOW / mixed]

**Classification:** MATCHES (BNHash) / BUG-minor (NLS M1).

- `domain::BNHash::operator==` is implemented as `equals_constant_time`
  (bn_hash.hpp:42-54) — XOR-accumulate, no early exit. All password / hash2
  proof compares in `account.login()`, `verify_password()`, and the hash2
  overloads go through this. GOOD — timing-safe, and the original used a
  non-constant-time `hash_eq` here, so v3 is actually stronger.
- The NLS path (`nls.cpp:541`) compares the SRP M1 proof with
  `std::memcmp(M1_expected, client_proof_M1, 20) != 0` — NOT constant-time.
  Low severity (M1 is a one-shot proof, not a repeatable oracle on a stored
  secret), but for consistency it should use a constant-time compare
  (`CRYPTO_memcmp`). Note this is in SRP-6a code which is itself a separate
  decision; flagging only the timing aspect.

---

## FINDING 6 — "Account has no password / empty hash" edge: original logs the user IN; v3 rejects. [MEDIUM / BUG-divergence]

**Severity:** MEDIUM (behavioural divergence on passwordless accounts)
**Classification:** BUG (divergence) — likely INTENTIONAL hardening, but undocumented.

### Original
Both OLS login paths special-case an account with no stored password and grant
access:
- loginreq1: `else { conn_login(c, account, username); "... logged in (no password)"; SUCCESS; }` (handle_bnet.cpp:1654-1657)
- loginreq2: same at 1820-1824.
The W3/NLS path similarly passes the account check when there is no salt
(1911-1915) or no verifier (1916-1921).

### v3
There is no "empty password ⇒ allow" branch anywhere. `account.login()`
(account.hpp:154) always runs `password_ == candidate`. A default-constructed
`BNHash` (all-zero) only matches an all-zero candidate, so a passwordless account
in v3 is effectively *unloggable* via the normal path (and, via the broken
Finding-1 path, nothing logs in regardless).

For NLS, v3 `LoginUserNls::challenge` (login_user_nls.cpp:18-21) returns
`AccountNotFound` if credentials are missing, rather than the original's
"pass the check anyway" behaviour for salt-less / verifier-less accounts.

### Divergence
Accounts created without a password (legacy installs, admin-provisioned shells,
imported data) that the original lets in are locked out by v3. If this is
intentional hardening it is fine, but it is a silent behavioural change and
should be a documented policy decision, not an accident of a missing branch.

### Proposed fix
Decide policy explicitly. If parity is desired, add an
`Account::has_password()` (e.g. `password_` is the default/empty sentinel) and
have `LoginUser` short-circuit to success (subject to lock/ban gating). If
hardening is intended, document it and ensure account creation forbids empty
passwords so no legacy passwordless account silently becomes unreachable.

---

## FINDING 7 — NLS verifier `x = H(salt || H(U:P))`: salt/hash ORDER matches original; hash FUNCTION and case-folding differ (SRP-3 vs SRP-6a territory). [INFO]

**Classification:** UNSURE / mostly out-of-scope (SRP-3-vs-6a is a separate decision).

For completeness, comparing the verifier derivation:
- v3 `NlsVerifier::hash_password` (nls.cpp:587-617):
  `x = SHA1( salt[32] || SHA1( UPPER(user) ":" UPPER(pass) ) )`, using OpenSSL
  true SHA-1, username AND password uppercased.
- Original `BnetSRP3::init` (common/bnetsrp3.cpp:132-166):
  `x = little_endian_sha1_hash( salt[32] || little_endian_sha1_hash(user ":" pass) )`,
  using the Blizzard little-endian SHA-1 variant, and (per the code at
  151-161) the raw `username`/`password` bytes — case-folding is applied by the
  caller, not here.

The **salt-before-innerhash ordering matches** (good). The hash primitive
(OpenSSL SHA-1 vs Blizzard `little_endian_sha1_hash`) and the upper-casing differ.
These belong to the SRP-3→SRP-6a migration the brief excludes, so flagged as
INFO only — but note that if any *existing* salts/verifiers were generated by
the original server, v3 will not validate them (verifier values are not
wire-compatible). A migration/regeneration path is needed for existing DBs.

---

## Summary of severities

| # | Finding | Severity | Class |
|---|---------|----------|-------|
| 1 | OLS login compares client hash2 vs stored hash1 (wrong overload) | CRIT | BUG |
| 2 | No production IPasswordHasher / derive_session_hash adapter | CRIT | NOT-IMPLEMENTED |
| 3 | SID_CHANGEPASSWORD handler is a no-op stub (+hasher missing) | HIGH | NOT-IMPLEMENTED |
| 4 | Stored hash1 + create/login packing | — | MATCHES (correct) |
| 5 | BNHash compare constant-time (good); NLS M1 memcmp (minor) | LOW | mixed |
| 6 | Empty-password account: original allows, v3 rejects | MEDIUM | BUG (divergence) |
| 7 | NLS verifier salt-order matches; hash fn/case differ | INFO | out-of-scope |

**Net:** OLS (old logon) authentication is fundamentally broken in v3 (Findings
1+2): the use-case layer has the correct double-hash logic, but the protocol FSM
calls the wrong login overload and no concrete session-hasher is wired, so no
real OLS client can authenticate and password-change is a silent no-op. The
verifier *storage* of hash1 is correct (Finding 4) and the proof compare is
constant-time (Finding 5). One behavioural divergence on passwordless accounts
(Finding 6).

---

## FIX PLAN (Findings 1 + 2) — DEFERRED, not applied

**Status: DEFERRED.** Verified 2026-06-25. The use-case layer is correct and the
fix is well understood, but applying it now meets BOTH of the brief's "do not
force it" triggers: (a) no production `IPasswordHasher` adapter exists (must be
written + composed), and (b) the change **breaks the passing e2e**
(`tests/e2e/modern_login_journey_test.py`) — see "e2e impact" below. The e2e
currently passes *because of* the bug, so a correct fix cannot land without also
rewriting the test client's crypto, which is a separate, env-gated artifact.

### 1. Exact overload-call change (`src/protocol/bnet/src/fsm/fsm_auth.cpp`)

In `BnetFsm::on(const LogonResponse2& m)` (~lines 95-120) replace the
`LoginRequest` construction + dispatch with the session-hash overload. The
20-byte LE packing of `m.password_hash` IS the client's hash2 (the wire field
already carries the double-hash — `password_hash` is mislabelled, not hash1):

```cpp
auto username_result = domain::UserName::parse(m.username);
if (!username_result) { /* reply 0x01 as today */ }

// m.password_hash is the client's hash2 (double-hash), NOT hash1.
auto hash2_result = domain::BNHash::from_bytes(pack_hash1_le(m.password_hash));
if (!hash2_result) { /* reply 0x02 as today */ }

application::auth::LoginWithSessionHashRequest login_req{
    .name           = username_result.value(),
    .password_hash2 = hash2_result.value(),
    .ticks          = m.client_token,    // = original `ticks`
    .sessionkey     = m.server_token,    // = original `sessionkey`
    .tag            = client_tag_,
    .ip             = domain::IpAddress{},
    .session        = session_id_,
};
auto login_result = use_cases_.login_user->execute(login_req);
// error-code mapping + success bookkeeping unchanged (same LoginError enum).
```

The existing `pack_hash1_le` helper is reused verbatim (it just packs 5×u32 →
20 LE bytes; the name is now doubly-misleading but functionally correct). The
`LoginError` switch and the `current_account_id_ / current_username_ / state_`
success path are unchanged. `ChangePasswordRequest` (Finding 3, fsm_auth.cpp:254)
gets the analogous wiring to `ChangePasswordWithSessionHashRequest`.

### 2. Production `IPasswordHasher` adapter (new — `src/infra/crypto/`)

No class implements `domain::identity::IPasswordHasher` outside test fakes. Add
one bridging the existing, correct `blizzard_hash` primitive
(`infra/crypto/bnet_hash.{hpp,cpp}`) to the port. Byte layout must mirror the
original `temp{ ticks; sessionkey; passhash1[5] }` exactly (confirmed against
`pvpgn-server/src/bnetd/handle_bnet.cpp:1784-1817`): a **28-byte** buffer =
`ticks` (LE u32) ‖ `sessionkey` (LE u32) ‖ `hash1` (20 wire-LE bytes, i.e. the
stored `BNHash::bytes()` verbatim — that array already IS wire-LE), then
`blizzard_hash` over all 28 bytes, then emit the digest in **wire-LE** form
(`digest_to_wire`, from `bnet_hash_conv.hpp`) as a `domain::BNHash`.

Header `src/infra/crypto/include/infra/crypto/bnet_session_hasher.hpp`:

```cpp
#pragma once
#include "domain/identity/ports.hpp"
namespace pvpgn::infra::crypto {
class BnetSessionHasher final : public domain::identity::IPasswordHasher {
public:
    domain::BNHash derive_session_hash(const domain::BNHash& hash1,
                                       std::uint32_t ticks,
                                       std::uint32_t sessionkey) const noexcept override;
};
}  // namespace
```

Impl (`src/infra/crypto/src/bnet_session_hasher.cpp`): build the 28-byte buffer
(ticks LE, sessionkey LE, then `hash1.bytes()` copied verbatim), call
`v3::infra::crypto::blizzard_hash(span)`, convert the resulting `BnetDigest`
with `digest_to_wire(...)` to a 20-byte array, return `domain::BNHash{that}`.
A parity unit test should pin one known (ticks,sessionkey,hash1)→hash2 vector
against a value computed by the legacy `bnet_hash` to guarantee bit-exactness.
Add both files to `src/infra/crypto/CMakeLists.txt`.

### 3. Composition-root wiring (`src/app/bnetd/src/main.cpp` ~line 388)

Today: `LoginUser` is built with the **4-arg** ctor (no hasher) →
`hasher_ == nullptr` → the session-hash overload returns `LoginError::Internal`.
Construct a long-lived `BnetSessionHasher` in the run-loop scope and pass it to
the **5-arg** ctors of both `LoginUser` and `ChangePasswordUseCase`:

```cpp
static const infra::crypto::BnetSessionHasher session_hasher;  // stateless
use_cases.login_user = std::make_shared<application::auth::LoginUser>(
    account_repo, session_reg, event_bus, auth_clock, session_hasher);
// + the ChangePassword use-case (currently unwired) with the same hasher.
```

The hasher is stateless and `noexcept`, so a function-static / scope-static is
safe and outlives the use-cases. Link `app/bnetd` against the crypto target if
not already.

### 4. e2e impact — THE BLOCKER (`tests/e2e/modern_login_journey_test.py`)

The test's `build_logon_response2` sends the **raw** `PASSWORD_WORDS` in the
password slot with `client_token=0xDEADBEEF, server_token=0`, and `create_account`
stores those same words as hash1. It passes today only because the buggy FSM
compares `client-words == stored-words`. After the fix the server expects
`hash2 = blizzard_hash(0xDEADBEEF ‖ 0 ‖ words)`, which ≠ raw `PASSWORD_WORDS`, so:
- Journey 1 (accept) would receive `0x02` and FAIL.
- Journey 2 (wrong password → 0x02) would still pass by accident.
- Journey 3 (unknown user → 0x01) unaffected.

To keep the e2e meaningful the Python client must compute the real double-hash:
port Blizzard's broken-SHA-1 (the `ROTL32(1,x)` message-schedule variant) into
the test, then `build_logon_response2` sends
`blizzard_hash(pack_le_u32(client_token) + pack_le_u32(server_token) +
pack_5x_le_u32(words))` instead of the raw words. `WRONG_WORDS` then naturally
produces a non-matching hash2. This is a self-contained but non-trivial crypto
reimplementation in Python and should ship in the SAME change as the FSM fix so
the e2e never regresses.

### 5. Test additions (unit, env-independent)

- A `BnetSessionHasher` parity test (one fixed vector vs legacy `bnet_hash`).
- An FSM-level OLS test driving `on(LogonResponse2)` with a hasher that
  reproduces the real double-hash: assert correct hash2 → accept (0x00) and
  wrong hash2 → reject (0x02). (The use-case-level happy/sad paths are already
  covered by `login_user_session_hash_test.cpp`.)

### Why deferred (summary)

Correct fix = FSM overload swap (small) + new infra hasher adapter + CMake +
composition wiring + a Blizzard-SHA1 reimplementation inside the e2e client.
The last item is required to avoid breaking the currently-green e2e and is an
env-gated, separately-verifiable artifact (no local build of the e2e per the
brief's "do not run any build" constraint). Per the brief — auth path,
correctness over forcing — this is recorded as a plan rather than a rushed,
unbuildable, e2e-breaking change.

---

## F-W19 — v3 never sent the SID_AUTH_INFO (0x50) seed → no real client could auth
**Severity:** CRITICAL (real-client compatibility) — fixed (wave 19) for OLS
**Classification:** REAL BUG — fixed (OLS) / scopes the NLS work

**Symptom:** v3's `on(AuthInfo)` immediately replied with SID_AUTH_CHECK (0x51,
result=0) and NEVER sent the SID_AUTH_INFO reply (0x50). Our mock client had an
adaptive shortcut that masked this. But a REAL Blizzard client follows a fixed
sequence: after sending SID_AUTH_INFO it BLOCKS for the server's 0x50 reply,
which carries (a) the server token it folds into the OLS password double-hash and
(b) the logon-type flag that selects OLS vs WarCraft-3 NLS. Without that packet
no real client — OLS or W3 — could ever authenticate.

**Original ref:** handle_bnet.cpp builds SERVER_AUTHREQ_109 (logontype:
2 for WAR3/W3XP else 0; server_token; udpvalue; checkrevision file timestamp +
filename + equation), THEN the client sends 0x51, THEN the server replies 0x51.

**Fix (OLS):** `BnetFsm::on(AuthInfo)` now sends the `AuthInfoReply` (0x50) seed:
logon-type 2 for WAR3/W3XP else 0, a per-session nonzero `server_token` (stored in
`server_token_`), standard MPQ name + a representative CheckRevision equation.
`on(AuthCheckRequest)` now sends the AUTH_CHECK (0x51) result. The mock clients
(tests/diff/bncs_client.py + tests/e2e) were made FAITHFUL — they require the
0x50 seed, read its server_token, send 0x51, then login — so they exercise the
real client sequence and double as a regression guard for the seed.

**Verified:** diff_ols_login now matches on auth_seed_present=True/True,
server_token_nonzero=True/True, logon_type, and all login outcomes. Both e2e
journeys (modern_login_journey, account_persistence) pass with the faithful
handshake. Unit guards in fsm_auth_create_login_test.cpp (0x50 seed nonzero token,
OLS logon-type 0, WAR3/W3XP logon-type 2, AUTH_CHECK ack) + updated fsm_test.cpp.

**Still open (NLS):** the 0x50 reply now advertises logon-type 2 for WAR3/W3XP,
but the SID_AUTH_ACCOUNTLOGON (0x53)/PROOF (0x54) handlers remain stubbed (return
ok(), send nothing) and there is no SRP verifier at account creation. A real
WarCraft-3 client would get logon-type 2 and then stall at 0x53. Wiring the
existing bnet_srp3 + LoginUserNls use-case + a credential store is the next step.

---

## F-W20 — WarCraft III SRP-3 (NLS) login implemented + wired
**Severity:** HIGH (whole client family could not authenticate) — fixed (wave 20)
**Classification:** NOT-IMPLEMENTED → implemented

The 0x53 (SID_AUTH_ACCOUNTLOGON) / 0x54 (..._PROOF) handlers were stubbed
(returned ok(), sent nothing); there was no verifier at account creation. So a
real WAR3/W3XP client got logon-type 2 from the AUTH_INFO seed (wave 19) and then
stalled at 0x53. Now implemented end to end:

- `application::auth::LoginUserW3` (new) — SRP-3 challenge: looks up the account's
  (salt, verifier) from the new `ISrp3CredentialStore`, derives the server public
  key B and pre-computes the expected client proof M1 + server proof M2 from the
  client public key A. Mirrors the original `_client_loginreqw3` exactly, using
  the parity-verified, golden-tested `infra::crypto::BnetSrp3` (32-byte modulus),
  so the bytes match real clients. Wire block-size conventions copied verbatim
  from the original: salt=4, verifier/A=1, B/proofs=4.
- `ISrp3CredentialStore` port + `InMemorySrp3CredentialStore` (salt+verifier per
  account name).
- FSM: `on(LoginW3Request)` (0x53) runs the challenge, sends salt+B, holds
  (M1,M2,account,username); `on(LogonProofW3Request)` (0x54) compares the client
  M1 (20-byte), on match attaches the session (W3 bypasses LoginUser) + returns
  M2 + LoggedIn; `on(CreateAccount2Request)` (0x52) creates the account and stores
  the client-supplied salt+verifier (server never sees the password).
- Wired into live bnetd (main.cpp): srp3_store lives for the run loop;
  use_cases.login_user_w3 + srp3_store set alongside the OLS use-cases.

**Verified:** tests/unit/protocol/bnet/fsm_auth_w3_test.cpp drives the FULL
create→login→proof round-trip through the real message structs + FSM, with a
`BnetSrp3` *client* (the same bit-exact crypto the original server uses): both
sides derive the same K, the server's M2 matches the client's independent M2,
the session attaches, state→LoggedIn. Wrong-proof→BadPass and unknown-account
→failure also covered. Full unit suite: 3141 passed. Since BnetSrp3 is
parity-verified against the original's BnetSRP3, a real WAR3 client that
authenticates against the original authenticates against v3 with identical bytes.

**Still open:** a Python differential mock for 0x52/0x53/0x54 (needs a faithful
SRP-3 + the BigUInt legacy block conversions ported to Python, golden-verified
against bnet_srp3_golden_test vectors). The C++ round-trip already proves interop
at the message level; the Python mock would add running-server differential
parity. Password-change over NLS (0x55/0x56) remains stubbed.
