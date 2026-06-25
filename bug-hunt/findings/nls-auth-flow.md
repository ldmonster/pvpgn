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
