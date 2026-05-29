# R284 + R285 Checklist: Wire BnetConnectionAdapter + Use-Cases

## Summary

R284 and R285 are implemented together because they form a single wiring chain:

```
BnetdService
  └─ owns LoginUserNls (constructed from INlsCredentialStore)
      └─ passed to BnetSessionFactory (as LoginUserNls*)
          └─ per session: BnetConnectionAdapter (shared_ptr)
              └─ owns ConnectionFsm (constructed with LoginUserNls& if NLS client)
```

---

## Files Modified

### R284 — Wire BnetConnectionAdapter into BnetSessionFactory

#### `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp`
- Added `#include "application/auth/login_user_nls.hpp"`
- Added second constructor overload:
  ```cpp
  BnetConnectionAdapter(
      domain::connection::IConnectionContext&  ctx,
      application::auth::LoginUserNls&         login_user_nls,
      std::uint32_t                            session_id = 0) noexcept;
  ```
  This constructor passes `login_user_nls` to `ConnectionFsm`'s NLS-aware constructor.

#### `src/v3/app/bnetd/src/bnet_connection_adapter.cpp`
- Added `#include "application/auth/login_user_nls.hpp"`
- Implemented the new NLS constructor:
  ```cpp
  BnetConnectionAdapter::BnetConnectionAdapter(
      domain::connection::IConnectionContext&  ctx,
      application::auth::LoginUserNls&         login_user_nls,
      std::uint32_t                            session_id) noexcept
      : ctx_(ctx)
      , fsm_(std::make_unique<domain::connection::ConnectionFsm>(
            *this, login_user_nls, session_id))
  {}
  ```

#### `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp`
- Added includes for `app/bnetd/bnet_connection_adapter.hpp` and
  `application/auth/login_user_nls.hpp`
- Added `login_nls_` constructor parameter (optional `LoginUserNls*`, default `nullptr`)
- Added `login_nls_` member field
- Updated `operator()` to instantiate `BnetConnectionAdapter` per session:
  - Uses NLS constructor when `login_nls_ != nullptr`
  - Uses OLS-only constructor otherwise
  - Captures `conn_adapter` in the `set_on_bytes` lambda (keeps it alive for session lifetime)
- Updated `on_tcp_bytes` static helper signature to accept the adapter

### R285 — Wire Use-Cases into BnetdService

#### `src/v3/services/bnetd/include/services/bnetd/bnetd_service.hpp`
- Added `#include "application/auth/login_user_nls.hpp"`
- Added `INlsCredentialStore& nls_store` as third constructor parameter
- Added `login_user_nls()` accessor returning `LoginUserNls&`
- Added `std::unique_ptr<application::auth::LoginUserNls> login_user_nls_` member
- Removed the `TODO(Phase3)` comment about use-case wiring

#### `src/v3/services/bnetd/src/bnetd_service.cpp`
- Updated constructor to accept `INlsCredentialStore& nls_store`
- Constructs `login_user_nls_` via `std::make_unique<LoginUserNls>(nls_store)`
- Added `#include "application/auth/login_user_nls.hpp"`

---

## CMakeLists Changes

### `src/v3/app/bnetd/CMakeLists.txt`
- Added `application_auth` and `domain_connection` to `app_bnetd_legacy_bridge` PUBLIC_DEPS
  (exposes `LoginUserNls` and `ConnectionFsm` headers to consumers of the bridge)
- Added `application_auth` to `pvpgn_v3_bnetd` PRIVATE link deps

### `src/v3/services/bnetd/CMakeLists.txt`
- Added `application_auth` to `services_bnetd` PUBLIC_DEPS
  (needed for `LoginUserNls` and `INlsCredentialStore` in the public header)

### `src/v3/infra/session/CMakeLists.txt`
- Added `application_auth` and `domain_connection` to `infra_session` INTERFACE deps
- **Note**: This file is the standalone subdirectory CMakeLists. The main build uses
  the inline `infra_session` definition in `src/v3/CMakeLists.txt` (line 1088).
  That inline definition also needs `application_auth` and `domain_connection` added,
  but `src/v3/CMakeLists.txt` is outside the R284/R285 task scope.

---

## Follow-Up Required (outside R284/R285 scope)

### `src/v3/CMakeLists.txt` (NOT modified — outside task scope)
Two inline target definitions need updating in a follow-up:

1. **`application_auth` target (line 680)**: Add `login_user_nls.cpp` to SOURCES and
   `infra_crypto_nls` to PRIVATE_DEPS so `LoginUserNls` is compiled into the main build.

2. **`infra_session` target (line 1088)**: Add `application_auth` and `domain_connection`
   to PUBLIC_DEPS so `BnetSessionFactory` can include `bnet_connection_adapter.hpp`.

---

## Wiring Chain

```
main()
  │
  ├─ INlsCredentialStore& nls_store  (e.g. InMemoryNlsCredentialStore)
  │
  └─ BnetdService(uow_factory, event_loop, nls_store)
       │
       ├─ owns: LoginUserNls login_user_nls_(nls_store)
       │
       └─ creates: BnetSessionFactory(router, registry, use_cases, &login_user_nls_)
            │
            └─ per accepted TCP connection:
                 ├─ BnetSessionContextImpl (ISessionContext)
                 ├─ BnetFsm (protocol FSM)
                 └─ BnetConnectionAdapter(*context, login_user_nls_, session_id)
                      └─ owns: ConnectionFsm(*this, login_user_nls_, session_id)
                           └─ NLS path: LoginUserNls::challenge() / verify()
                           └─ OLS path: direct hash comparison
```

---

## Constructor Parameters Added

| Class | New Parameter | Type | Ownership |
|-------|--------------|------|-----------|
| `BnetConnectionAdapter` | `login_user_nls` | `LoginUserNls&` | non-owning ref |
| `BnetSessionFactory` | `login_nls` | `LoginUserNls*` | non-owning ptr (nullable) |
| `BnetdService` | `nls_store` | `INlsCredentialStore&` | non-owning ref |

`BnetdService` OWNS `LoginUserNls` (via `unique_ptr`).
`BnetSessionFactory` holds a non-owning pointer to it.
`BnetConnectionAdapter` holds a non-owning reference to it (passed to `ConnectionFsm`).

---

## Coding Standards Compliance

- C++20, `#pragma once`, SPDX header on all modified files ✓
- No raw `new`/`delete`; `std::make_unique` / `std::make_shared` used ✓
- `[[nodiscard]]` on `login_user_nls()` accessor ✓
- Constructor injection for all dependencies ✓
- `login_nls_` defaults to `nullptr` for backward compatibility ✓
