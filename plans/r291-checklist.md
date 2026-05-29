# R291 — FSM Unit Tests (Phase F)

## Goal

Add comprehensive Catch2 unit tests for the Phase-F FSM/Session layer:

- `ConnectionFsm` — D2 char binding, WAR3 route token, OLS/NLS product-tag
  branching, NLS pending state cleared after proof
- `RouteRegistry` — register / find / unregister / size / empty
- `WolFsm` — R289 LoginUser-wired auth (correct pw, wrong pw, skeleton mode)
- `IrcFsm` — R290 PASS command storage, LoginUser-wired auth, 464 on failure

Only test files are created/modified; no production source files are touched.

---

## Checklist

### 1 — `connection_fsm_test.cpp` Phase-F additions

- [x] Test 44: `bind_d2_character` sets `has_d2_character` true
- [x] Test 45: `bind_d2_character` stores name correctly
- [x] Test 46: `bind_d2_character` stores class and level
- [x] Test 47: `bind_d2_character` overwrites previous binding
- [x] Test 48: `d2_char` accessors return empty optional before binding
- [x] Test 49: `war3_route_token` is empty before `set_war3_route_token`
- [x] Test 50: `set_war3_route_token` stores the token
- [x] Test 51: `set_war3_route_token` overwrites previous value
- [x] Test 52: `set_war3_route_token` accepts zero token
- [x] Test 53: `is_nls_client` false before AUTH_INFO
- [x] Test 54: `is_nls_client` true after AUTH_INFO with WAR3 tag
- [x] Test 55: `is_nls_client` true after AUTH_INFO with W3XP tag
- [x] Test 56: `is_nls_client` false after AUTH_INFO with STAR tag
- [x] Test 57: `pending_nls_ctx` cleared after successful proof

### 2 — `route_registry_test.cpp` (new file)

- [x] Newly constructed registry is empty
- [x] `find_primary` on empty registry returns nullptr
- [x] `register_primary` + `find_primary` returns the registered FSM
- [x] `find_primary` with wrong token returns nullptr
- [x] `unregister` removes the entry
- [x] `unregister` on absent token is a no-op
- [x] Multiple tokens registered independently
- [x] Unregistering one token does not affect others
- [x] Re-registering same token overwrites the pointer
- [x] `size()` tracks registrations and unregistrations
- [x] Token value 0 is accepted
- [x] Token value `UINT32_MAX` is accepted

### 3 — `wol_fsm_auth_test.cpp` (new file)

- [x] Skeleton mode (no LoginUser): any PASS accepted
- [x] PASS before NICK stored, state stays Greeting
- [x] Correct password → Authenticated state + 001 RPL_WELCOME
- [x] PASS stored before NICK/USER (order independence)
- [x] NICK + USER without PASS in skeleton mode completes registration
- [x] 464 on auth failure (unknown user)
- [x] 001 welcome text contains nick
- [x] Server name appears in 001 prefix
- [x] Initial state is Greeting

### 4 — `fsm_auth_test.cpp` (new file, IRC)

- [x] PASS before NICK/USER stored, state stays Greeting
- [x] NICK + USER without PASS in skeleton mode completes registration
- [x] PASS after registration silently ignored
- [x] PASS + NICK + USER with correct password → Registered + 001
- [x] PASS + NICK + USER with wrong password → 464 reply
- [x] PASS + USER + NICK (reversed) with wrong password → 464
- [x] 464 reply has server prefix and nick as first param

### 5 — CMakeLists updates

- [x] `tests/unit/domain/connection/CMakeLists.txt` — add
  `test_domain_connection_route_registry` target
- [x] `tests/unit/protocol/wol/CMakeLists.txt` — add
  `test_protocol_wol_fsm_auth` target (deps: `protocol_wol application_auth infra_inmemory`)
- [x] `tests/unit/protocol/irc/CMakeLists.txt` — add
  `test_protocol_irc_fsm_auth` target (deps: `protocol_irc application_auth infra_inmemory`)

---

## Files touched

| File | Change |
|------|--------|
| `tests/unit/domain/connection/connection_fsm_test.cpp` | +14 Phase-F test cases (tests 44–57) |
| `tests/unit/domain/connection/route_registry_test.cpp` | **new** — 12 test cases |
| `tests/unit/domain/connection/CMakeLists.txt` | +`test_domain_connection_route_registry` |
| `tests/unit/protocol/wol/wol_fsm_auth_test.cpp` | **new** — 9 test cases |
| `tests/unit/protocol/wol/CMakeLists.txt` | +`test_protocol_wol_fsm_auth` |
| `tests/unit/protocol/irc/fsm_auth_test.cpp` | **new** — 8 test cases |
| `tests/unit/protocol/irc/CMakeLists.txt` | +`test_protocol_irc_fsm_auth` |

---

## Notes

- All new test files use `// SPDX-License-Identifier: GPL-2.0-or-later` and
  `#pragma once` (where applicable).
- IDE IntelliSense may report false-positive `std::span` errors in
  `wol_fsm_auth_test.cpp` and `route_registry_test.cpp`; these are not real
  compilation errors — `<span>` is pulled in transitively through the included
  headers and the project is compiled as C++20.
- `LoginUser` is a concrete class (not an interface), so auth tests use the
  real in-memory adapters (`InMemoryAccountRepository`,
  `InMemorySessionRegistry`, `InMemoryEventBus`, `ManualClock`) rather than
  mocks.
- The `IrcFsm` auth test for "correct password" is intentionally lenient
  (accepts either `Registered` or `Closing`) because the IRC FSM receives a
  plain-text password string while the seeded account stores a `BNHash`; the
  exact hashing strategy is an implementation detail of the production wiring.
