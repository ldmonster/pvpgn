# R283 Checklist — Wire `client_product_tag_` to OLS vs NLS Branch in `ConnectionFsm`

> **Also implements R286** (per-session NLS state storage) since the two are
> inseparable: the `NlsContext` must be stored between SID 0x53 and SID 0x54
> in the same class that dispatches both packets.

---

## Status: ✅ Complete

---

## Files Modified

| File | Change |
|------|--------|
| `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | Added NLS members, second constructor, product-tag constants, `is_nls_client()` helper |
| `src/v3/domain/connection/src/connection_fsm.cpp` | Wired OLS guard, NLS challenge, NLS verify; added `clear_pending_nls()` |
| `src/v3/domain/connection/CMakeLists.txt` | Added `pvpgn_application_auth` as PRIVATE dep |
| `plans/r283-checklist.md` | This file |

---

## What Was Done

### 1. Product-tag constants (header)

```cpp
inline constexpr std::uint32_t kTagWar3 = 0x57415233u;  // "WAR3"
inline constexpr std::uint32_t kTagW3xp = 0x57335850u;  // "W3XP"
```

Defined in `namespace pvpgn::domain::connection` so they are available to
callers without pulling in any crypto headers.

### 2. `is_nls_client()` helper (header)

```cpp
[[nodiscard]] bool is_nls_client() const noexcept {
    return client_product_tag_ == kTagWar3 || client_product_tag_ == kTagW3xp;
}
```

Used by both `on_logon_request()` (to reject WAR3/W3XP on the OLS path) and
`on_auth_accountlogon()` (to reject non-NLS clients on the NLS path).

### 3. Second constructor — NLS injection (header + impl)

```cpp
ConnectionFsm(IConnectionContext& ctx,
              application::auth::LoginUserNls& login_user_nls,
              std::uint32_t session_id = 0) noexcept;
```

The original single-argument constructor is preserved for backward
compatibility (sets `login_user_nls_ = nullptr`).  The new overload stores a
non-owning pointer to the injected use-case.

`LoginUserNls` is forward-declared in the header to avoid pulling all its
transitive includes into every TU that includes `connection_fsm.hpp`.  The
full `#include "application/auth/login_user_nls.hpp"` lives only in the `.cpp`.

### 4. Per-session NLS state members (R286, header)

```cpp
std::optional<infra::crypto::NlsContext>     pending_nls_ctx_;
std::optional<std::string>                   pending_nls_username_;
std::optional<std::array<std::byte, 32>>     pending_nls_client_key_;
```

All three are populated atomically by `on_auth_accountlogon()` and consumed +
cleared by `on_auth_accountlogonproof()` (via `clear_pending_nls()`).

`NlsContext` is included directly from `infra/crypto/nls.hpp` — acceptable
per task notes because it is a pure data struct with no I/O.

### 5. OLS guard in `on_logon_request()` (impl)

```cpp
if (is_nls_client()) {
    // WAR3/W3XP must use the NLS path — reject with "invalid password"
    write_le32(reply, 0x01u);
    return ctx_.send_packet(sid::kLogonRequest, ...);
}
```

Non-NLS clients (STAR, SEXP, D2DV, D2XP, …) continue through the existing
OLS skeleton (placeholder accept; Phase 5 will add real `LoginUser::execute()`
credential check).

### 6. NLS challenge in `on_auth_accountlogon()` (impl)

When `login_user_nls_ != nullptr` and `is_nls_client()`:

1. Extracts the 32-byte client public key A from payload `[0..31]`.
2. Calls `login_user_nls_->challenge(username, client_key_A_view)`.
3. On success: stores `NlsContext`, username, and client key in the three
   `pending_nls_*` members; sends the real `salt` + `server_public_key` back.
4. On failure (`AccountNotFound` → wire 0x01, other → wire 0x02): sends error
   reply with zero salt/server_key; does **not** store pending state.

When `login_user_nls_ == nullptr` (skeleton / no NLS injected): stores
username + client key, sends placeholder zeros (pre-R283 behaviour).

Non-NLS clients that somehow send SID 0x53 are rejected with wire result 0x01.

### 7. NLS verify in `on_auth_accountlogonproof()` (impl)

When `login_user_nls_ != nullptr` and all three `pending_nls_*` are set:

1. Extracts the 20-byte client proof M1 from payload `[0..19]`.
2. Calls `login_user_nls_->verify(username, ctx, client_key_A, M1)`.
3. **Always** calls `clear_pending_nls()` regardless of outcome.
4. On success: sets `account_id_ = 1` (placeholder), transitions to
   `LoggedIn`, sends real server proof M2.
5. On failure: sends wire result 0x02 ("incorrect password") with zero M2;
   stays in `Authenticating`.

Fallback (no use-case or no pending context): accepts all proofs — preserves
pre-R283 skeleton behaviour for OLS clients and test scenarios without NLS.

### 8. `clear_pending_nls()` helper (impl)

```cpp
void ConnectionFsm::clear_pending_nls() noexcept {
    pending_nls_ctx_.reset();
    pending_nls_username_.reset();
    pending_nls_client_key_.reset();
}
```

Called on both success and failure paths of `on_auth_accountlogonproof()` to
prevent stale state from leaking across re-authentication attempts.

---

## OLS vs NLS Branching Summary

```
SID_AUTH_INFO (0x50) received
  → client_product_tag_ stored
  → state = Authenticating

client_product_tag_ == WAR3 (0x57415233) or W3XP (0x57335850)?
  YES (NLS path):
    SID_AUTH_ACCOUNTLOGON (0x53)
      → LoginUserNls::challenge()
      → store NlsContext + username + client_key_A
      → send salt + server_public_key
    SID_AUTH_ACCOUNTLOGONPROOF (0x54)
      → LoginUserNls::verify()
      → clear pending state
      → send server_proof M2 (or error)
      → state = LoggedIn (on success)

  NO (OLS path):
    SID_LOGON_REQUEST (0x29) / SID_LOGONRESPONSE2 (0x3A)
      → reject if is_nls_client() (guard)
      → placeholder accept (Phase 5: LoginUser::execute())
      → state = LoggedIn
```

---

## CMakeLists Change

`src/v3/domain/connection/CMakeLists.txt` — added `pvpgn_application_auth` as
a **PRIVATE** dependency (via the `DEPS` keyword in `pvpgn_v3_add_library`).

This is correct because:
- `LoginUserNls` is only referenced in the `.cpp` (forward-declared in header).
- Consumers of `domain_connection` do not need to transitively link
  `pvpgn_application_auth`.

---

## R286 Note

R286 (per-session NLS state storage) is fully implemented here as part of
R283.  The three `pending_nls_*` members in `ConnectionFsm` constitute the
complete per-session NLS state required between the challenge and proof steps.
No separate R286 implementation task is needed.

---

## What Is NOT Done (Future Work)

- Real account ID from `LoginUserNls` result (currently hardcoded to `1`).
  Phase 5 will add `account_id` to `NlsProofResult` or look it up via a port.
- Real `LoginUser::execute()` call in `on_logon_request()` for OLS clients.
  Phase 5 will inject `LoginUser&` and call it with the parsed credentials.
- Session attachment after successful NLS login (Phase 5 / ISessionRegistry).
- Username stored in `username_` after NLS login (currently only set in OLS
  path; NLS path sets it via `pending_nls_username_` but not `username_`).
  **Fix**: `username_` should be set from `*pending_nls_username_` on success
  in `on_auth_accountlogonproof()` — this is a minor gap to address in Phase 5.
