<!--
SPDX-License-Identifier: GPL-2.0-or-later
-->
# R292 + R293 + R294 — Phase F Close-Out Checklist

## R292 — Fix `src/v3/CMakeLists.txt` inline target drift

Two inline targets in `src/v3/CMakeLists.txt` were out of sync with their
subdirectory `CMakeLists.txt` counterparts.

### `application_auth` inline target

| Item | File | Status |
|------|------|--------|
| Add `application/auth/src/login_user_nls.cpp` to SOURCES | `src/v3/CMakeLists.txt` | ✅ Done |
| Add `PRIVATE_DEPS infra_crypto_nls` | `src/v3/CMakeLists.txt` | ✅ Done |

### `infra_session` inline target

| Item | File | Status |
|------|------|--------|
| Add `application_auth` to PUBLIC_DEPS | `src/v3/CMakeLists.txt` | ✅ Done |
| Add `domain_connection` to PUBLIC_DEPS | `src/v3/CMakeLists.txt` | ✅ Done |

---

## R293 — Wire `LoginUser::execute()` for OLS path in `ConnectionFsm`

### `ConnectionFsm` header

| Item | File | Status |
|------|------|--------|
| Forward-declare `LoginUser` class | `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | ✅ Done |
| Add new constructor accepting both `LoginUser&` and `LoginUserNls&` | `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | ✅ Done |
| Update existing constructors to initialize `login_user_ols_(nullptr)` | `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | ✅ Done |
| Add `application::auth::LoginUser* login_user_ols_` private member | `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | ✅ Done |

### `ConnectionFsm` implementation

| Item | File | Status |
|------|------|--------|
| Add `#include "application/auth/login_user.hpp"` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Wire `LoginUser::execute()` in `on_logon_request()` OLS path | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Extract 20-byte password hash from bytes `[8..27]` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Call `UserName::parse()` — reject on failure | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Call `ClientTag::from_packed_be()` with fallback to default | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Construct `LoginRequest` and call `login_user_ols_->execute()` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| On success: set `username_`, `account_id_ = login_result.value().id.value()`, transition to LoggedIn | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |

### `BnetConnectionAdapter`

| Item | File | Status |
|------|------|--------|
| Forward-declare `LoginUser` in header | `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp` | ✅ Done |
| Add OLS+NLS combined constructor declaration | `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp` | ✅ Done |
| Add `#include "application/auth/login_user.hpp"` | `src/v3/app/bnetd/src/bnet_connection_adapter.cpp` | ✅ Done |
| Implement OLS+NLS combined constructor | `src/v3/app/bnetd/src/bnet_connection_adapter.cpp` | ✅ Done |

### `BnetSessionFactory`

| Item | File | Status |
|------|------|--------|
| Forward-declare `LoginUser` | `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` | ✅ Done |
| Add `login_ols` parameter to constructor | `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` | ✅ Done |
| Add `login_ols_` private member | `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` | ✅ Done |
| Update `operator()` to use OLS+NLS constructor when both available | `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` | ✅ Done |

---

## R294 — Add `account_id` to `NlsProofResult` + thread real ID through both auth paths

### `login_user_nls.hpp`

| Item | File | Status |
|------|------|--------|
| Add `#include "domain/shared/ids.hpp"` | `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | ✅ Done |
| Add `domain::AccountId account_id{0}` to `NlsCredentials` | `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | ✅ Done |
| Add `domain::AccountId account_id{0}` to `NlsChallengeResult` | `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | ✅ Done |
| Add `domain::AccountId account_id{0}` to `NlsProofResult` | `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | ✅ Done |
| Update `verify()` signature to accept `domain::AccountId account_id` | `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | ✅ Done |

### `login_user_nls.cpp`

| Item | File | Status |
|------|------|--------|
| Set `result.account_id = creds->account_id` in `challenge()` | `src/v3/application/auth/src/login_user_nls.cpp` | ✅ Done |
| Update `verify()` signature to accept `domain::AccountId account_id` | `src/v3/application/auth/src/login_user_nls.cpp` | ✅ Done |
| Set `result.account_id = account_id` in `verify()` | `src/v3/application/auth/src/login_user_nls.cpp` | ✅ Done |

### `ConnectionFsm` — NLS path

| Item | File | Status |
|------|------|--------|
| Add `std::optional<domain::AccountId> pending_nls_account_id_` member | `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | ✅ Done |
| Store `pending_nls_account_id_ = challenge_result.account_id` in `on_auth_accountlogon()` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Capture `username` and `account_id` BEFORE `clear_pending_nls()` in `on_auth_accountlogonproof()` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Pass `captured_account_id` as 5th arg to `verify()` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Use `result.value().account_id.value()` instead of hardcoded `1u` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |
| Add `pending_nls_account_id_.reset()` to `clear_pending_nls()` | `src/v3/domain/connection/src/connection_fsm.cpp` | ✅ Done |

---

## Files Changed

| File | Change |
|------|--------|
| `src/v3/CMakeLists.txt` | R292: added `login_user_nls.cpp` source + `infra_crypto_nls` dep to `application_auth`; added `application_auth` + `domain_connection` deps to `infra_session` |
| `src/v3/application/auth/include/application/auth/login_user_nls.hpp` | R294: `account_id` field in `NlsCredentials`, `NlsChallengeResult`, `NlsProofResult`; updated `verify()` signature |
| `src/v3/application/auth/src/login_user_nls.cpp` | R294: populate `account_id` in `challenge()` and `verify()` |
| `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | R293: `LoginUser` fwd-decl, OLS+NLS constructor, `login_user_ols_` member; R294: `pending_nls_account_id_` member |
| `src/v3/domain/connection/src/connection_fsm.cpp` | R293: wire `LoginUser::execute()` in OLS path; R294: capture before clear, pass `account_id` to `verify()`, use `result.account_id`, fix `clear_pending_nls()` |
| `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp` | R293: `LoginUser` fwd-decl, OLS+NLS constructor declaration |
| `src/v3/app/bnetd/src/bnet_connection_adapter.cpp` | R293: `login_user.hpp` include, OLS+NLS constructor implementation |
| `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` | R293: `LoginUser` fwd-decl, `login_ols` param + `login_ols_` member, updated `operator()` wiring |
