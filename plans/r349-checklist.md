# R349 Checklist — Lua API v2 Surface + Migration from Legacy Bindings

## Goal
Define the Lua API v2 surface that plugins see, and provide a migration shim
from the legacy `luainterface` bindings.

## Files Created / Modified

- [x] `src/v3/infra/scripting/include/infra/scripting/lua_api_v2.hpp`
  - `register_v2_api(sol::state&, HandlerMap&)` declaration
  - Documents all 6 `pvpgn.*` functions with Lua signatures
  - Handler key convention: `"pvpgn.<function_name>"`

- [x] `src/v3/infra/scripting/src/lua_api_v2.cpp`
  - Creates `pvpgn` table via `lua.create_named_table("pvpgn")`
  - Registers 6 functions:
    - `pvpgn.log(level, message)` — delegates to `"pvpgn.log"` handler
    - `pvpgn.send_chat(username, message)` — delegates to `"pvpgn.send_chat"` handler
    - `pvpgn.get_account(username)` — delegates to `"pvpgn.get_account"` handler; returns Lua table or nil
    - `pvpgn.ban_account(username, reason)` — delegates to `"pvpgn.ban_account"` handler
    - `pvpgn.kick_user(username, reason)` — delegates to `"pvpgn.kick_user"` handler
    - `pvpgn.broadcast(channel, message)` — delegates to `"pvpgn.broadcast"` handler
  - JSON payload construction via `std::format`
  - Missing handler → stderr warning + return nil (no crash)

- [x] `src/v3/infra/scripting/include/infra/scripting/legacy_shim.hpp`
  - `install_legacy_shim(sol::state&)` declaration
  - Migration table documented in header

- [x] `src/v3/infra/scripting/src/legacy_shim.cpp`
  - `constexpr const char* k_legacy_shim_code` — Lua source string
  - Maps: `bnetd_send_message` → `pvpgn.send_chat`
  - Maps: `bnetd_get_account_info` → `pvpgn.get_account`
  - Maps: `bnetd_ban_user` → `pvpgn.ban_account`
  - Maps: `bnetd_kick_user` → `pvpgn.kick_user`
  - Maps: `bnetd_broadcast` → `pvpgn.broadcast`
  - Maps: `bnetd_log` → `pvpgn.log`
  - Guard: shim is no-op if `pvpgn` table not present
  - Installed via `lua.script(k_legacy_shim_code)`

- [x] `plugins/example-quiz/main.lua` (updated)
  - Added `-- Lua API v2 (pvpgn.* namespace)` comment at top
  - Replaced all `pvpgn.chat.send_whisper(...)` → `pvpgn.send_chat(...)`
  - Replaced all `print(...)` → `pvpgn.log("info", ...)`
  - Added `pvpgn.get_account()` usage in leaderboard
  - Added `pvpgn.broadcast()` usage for high score announcement
  - Removed legacy `bnetd_*` calls

- [x] `docs/lua-api-v2.md`
  - All 6 `pvpgn.*` functions with parameter tables and examples
  - Migration table: `bnetd_*` → `pvpgn.*`
  - How to load the legacy shim
  - Step-by-step migration guide with before/after code
  - Complete plugin example
  - Error handling section
  - See Also links

## Acceptance Criteria

- [x] All 6 `pvpgn.*` functions registered in `register_v2_api()`
- [x] Handler map key convention documented and consistent
- [x] Missing handler → warning + nil return (no crash)
- [x] Legacy shim maps all 6 `bnetd_*` functions
- [x] Shim is guarded (no-op if pvpgn table absent)
- [x] `plugins/example-quiz/main.lua` uses only `pvpgn.*` API
- [x] `docs/lua-api-v2.md` covers all functions + migration table
