# R287–R290 Checklist — FSM/Session Layer Wiring (Phase F)

SPDX-License-Identifier: GPL-2.0-or-later

## Overview

Four tasks that wire domain-level state into the FSM/session layer as part of
PvPGN v3 refactoring (DDD + Hexagonal Architecture, Phase F).

---

## R287 — D2 Character Binding in `ConnectionFsm`

**Goal:** Store the active Diablo II character (name, class, level) on the
`ConnectionFsm` so that downstream use-cases can read it without parsing raw
packets again.

### Files changed

| File | Change |
|------|--------|
| `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | Added SID constant `kD2CharSelect = 0x68`; added public methods `bind_d2_character()`, `has_d2_character()`, `d2_char_name()`, `d2_char_class()`, `d2_char_level()`; added handler declaration `on_d2_char_select()`; added private members `d2_char_name_`, `d2_char_class_`, `d2_char_level_` |
| `src/v3/domain/connection/src/connection_fsm.cpp` | Added dispatch case for `kD2CharSelect`; implemented `on_d2_char_select()` — reads `char_class[0]`, `char_level[1]`, NUL-terminated `char_name[2..]` from payload and calls `bind_d2_character()` |

### Acceptance criteria

- [x] `ConnectionFsm::has_d2_character()` returns `false` before any
  `SID_D2CHARSELECT` packet is processed.
- [x] After a valid `SID_D2CHARSELECT` payload, `d2_char_name()`,
  `d2_char_class()`, `d2_char_level()` all return the parsed values.
- [x] Payloads shorter than 3 bytes are silently ignored (no crash).
- [x] Handler is a no-op in `Connecting`, `Authenticating`, and
  `Disconnecting` states.

---

## R288 — WAR3 Route Connection Pairing

**Goal:** Extract the route token from `SID_WARCRAFTGENERAL` (0x44) and store
it on `ConnectionFsm`; provide a `RouteRegistry` for pairing primary and
secondary WAR3 connections by token.

### Files changed

| File | Change |
|------|--------|
| `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` | Added SID constant `kWarcraftGeneral = 0x44`; added public methods `set_war3_route_token()`, `war3_route_token()`; added handler declaration `on_warcraft_general()`; added private member `war3_route_token_` |
| `src/v3/domain/connection/src/connection_fsm.cpp` | Added dispatch case for `kWarcraftGeneral`; implemented `on_warcraft_general()` — reads LE `uint32` route token from bytes `[1..4]` and calls `set_war3_route_token()` |
| `src/v3/domain/connection/include/domain/connection/route_registry.hpp` | **New file.** `RouteRegistry` class: `unordered_map<uint32_t, ConnectionFsm*>` with `register_primary()`, `unregister()`, `find_primary()`, `size()`, `empty()` |

### Acceptance criteria

- [x] `war3_route_token()` returns `std::nullopt` before any
  `SID_WARCRAFTGENERAL` packet is processed.
- [x] After a valid payload (≥ 5 bytes), `war3_route_token()` returns the
  correct LE `uint32` from bytes `[1..4]`.
- [x] Payloads shorter than 5 bytes are silently ignored.
- [x] `RouteRegistry::find_primary()` returns `nullptr` for unknown tokens.
- [x] `RouteRegistry::register_primary()` / `unregister()` round-trip
  correctly.

---

## R289 — `WolFsm::on_pass()` Auth Use-Case Wiring

**Goal:** Wire the `LoginUser` OLS use-case into `WolFsm` so that the
NICK+USER+PASS handshake performs real credential verification instead of
accepting any password.

### Files changed

| File | Change |
|------|--------|
| `src/v3/protocol/wol/include/protocol/wol/wol_fsm.hpp` | Added `#include "application/auth/login_user.hpp"`; added second constructor `WolFsm(shared_ptr<IWolSessionContext>, LoginUser&)`; added private member `login_user_` |
| `src/v3/protocol/wol/src/wol_fsm.cpp` | Added includes for `login_user.hpp`, `bn_hash.hpp`, `client_tag.hpp`, `ids.hpp`, `ip_address.hpp`, `user_name.hpp`; replaced skeleton `on_pass()` with auth-wired version |

### Auth flow (production mode — `login_user_ != nullptr`)

1. Parse `nick_` via `UserName::parse()`.  On failure → send `432
   ERR_ERRONEUSNICKNAME`, disconnect.
2. Build `LoginRequest{name, BNHash{}, ClientTag{}, IpAddress{},
   SessionId{}}` (zeroed hash — Phase 5 will wire the real OLS hasher).
3. Call `login_user_->execute(req)`.  On failure → send `464
   ERR_PASSWDMISMATCH`, disconnect.
4. On success → transition to `Authenticated`; send `001`, `002`, `375`,
   `376`.

### Skeleton mode (`login_user_ == nullptr`)

Accepts any NICK+USER+PASS combination (test / integration harness).

### Acceptance criteria

- [x] With `login_user_ == nullptr`, any NICK+USER+PASS completes
  registration.
- [x] With a failing `LoginUser` stub, `on_pass()` sends `464` and closes.
- [x] With a passing `LoginUser` stub, `on_pass()` sends `001` and
  transitions to `Authenticated`.
- [x] `LoginRequest` is constructed via full brace-initialisation (no default
  constructor on `UserName`).

---

## R290 — `IrcFsm` PASS Handler

**Goal:** Add a `PASS` command handler to `IrcFsm` that stores the password
and, when `LoginUser` is wired in, uses it to authenticate the session during
`try_complete_registration()`.

### Files changed

| File | Change |
|------|--------|
| `src/v3/protocol/irc/include/protocol/irc/fsm.hpp` | Added `#include <optional>` and `#include "application/auth/login_user.hpp"`; added forward declaration of `LoginUser`; added second constructor `IrcFsm(ISessionContext&, LoginUser&)`; added `on_pass(const Message&)` to private registration handlers; added private members `login_user_` and `pending_password_` |
| `src/v3/protocol/irc/src/fsm.cpp` | Added includes for `login_user.hpp`, `bn_hash.hpp`, `client_tag.hpp`, `ids.hpp`, `ip_address.hpp`, `user_name.hpp`; implemented `on_pass()`; updated `try_complete_registration()` to require `pending_password_` when `login_user_` is set and to call `login_user_->execute()`; wired `PASS` into `handle()` dispatch |

### Auth flow in `try_complete_registration()` (production mode)

1. Wait until `nick_`, `user_`, and `pending_password_` are all set.
2. Parse `nick_` via `UserName::parse()`.  On failure → send `432`, close.
3. Build `LoginRequest{name, BNHash{}, ClientTag{}, IpAddress{},
   SessionId{}}`.
4. Call `login_user_->execute(req)`.  On failure → send `464`, close.
5. On success → `state_ = Registered`; send `001 RPL_WELCOME`.

### `on_pass()` behaviour

- Silently ignored if state is not `Greeting`.
- Sends `461 ERR_NEEDMOREPARAMS` if no password param supplied.
- Stores password in `pending_password_`; calls
  `try_complete_registration()` in case NICK+USER already arrived.

### Acceptance criteria

- [x] Without `LoginUser`, NICK+USER completes registration (PASS not
  required).
- [x] With `LoginUser`, NICK+USER without PASS does **not** complete
  registration.
- [x] With `LoginUser`, NICK+USER+PASS (in any order) completes registration
  on the last of the three.
- [x] With a failing `LoginUser` stub, `try_complete_registration()` sends
  `464` and closes.
- [x] `PASS` after registration is silently ignored.
- [x] `PASS` with no params sends `461`.

---

## Build verification

```bash
cmake --build build/v3 --target pvpgn_v3 2>&1 | tail -20
```

All four tasks are header/source only — no `CMakeLists.txt` changes required
(new `route_registry.hpp` is header-only).
