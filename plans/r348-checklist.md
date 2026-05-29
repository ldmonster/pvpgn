# R348 Checklist — sol2 Adapter + ScriptHost Port

## Goal
Define the `IScriptHost` application port and implement it with sol2 (Lua 5.4 bindings).

## Files Created / Modified

- [x] `src/v3/application/ports/include/application/ports/script_host.hpp`
  - `ScriptEventHandler` typedef: `std::function<std::string(std::string_view, std::string_view)>`
  - `IScriptHost` abstract interface:
    - `load_file(std::string_view path)` — load + execute Lua file
    - `exec(std::string_view code)` — execute Lua string
    - `register_function(name, handler)` — expose C++ fn to Lua
    - `dispatch_event(event, payload) -> std::string` — call Lua handler
    - `has_handler(event) const noexcept -> bool` — check handler exists
  - Non-copyable base class
  - Full Doxygen documentation

- [x] `src/v3/infra/scripting/include/infra/scripting/sol2_script_host.hpp`
  - `Sol2ScriptHost final : public IScriptHost`
  - Pimpl pattern — hides `sol::state` from public header consumers
  - Non-copyable, non-movable (owns Lua VM)
  - All five IScriptHost methods declared

- [x] `src/v3/infra/scripting/src/sol2_script_host.cpp`
  - `#define SOL_ALL_SAFETIES_ON 1` before sol2 include
  - `Impl` struct holds `sol::state lua` with all standard libraries opened
  - `load_file`: `lua.script_file(path)` wrapped in try/catch → `std::runtime_error`
  - `exec`: `lua.script(code)` wrapped in try/catch → `std::runtime_error`
  - `register_function`: `lua.set_function(name, lambda)` — handler captured by value
  - `dispatch_event`:
    - Checks `lua[event]` is valid and is a function
    - Uses `sol::protected_function` to call safely
    - Returns result as string or empty string
    - Throws `std::runtime_error` on Lua error
  - `has_handler`: checks `lua[event].valid()` and type is function; noexcept

- [x] `src/v3/infra/scripting/CMakeLists.txt` (updated)
  - `option(PVPGN_V3_WITH_LUA "Enable Lua scripting via sol2" ON)`
  - `find_package(Lua 5.4 REQUIRED)`
  - sol2 discovery: local path → FetchContent fallback (v3.3.0)
  - `pvpgn_v3_add_library(pvpgn_infra_scripting STATIC ...)`
  - Sources: `sol2_script_host.cpp`, `lua_api_v2.cpp`, `legacy_shim.cpp`
  - Links: `pvpgn_application_ports` (PUBLIC), `${LUA_LIBRARIES}` (PRIVATE)
  - `pvpgn_v3_target_warnings(pvpgn_infra_scripting)`

## Acceptance Criteria

- [x] `IScriptHost` is in `application/ports/` (no infra dependency)
- [x] `Sol2ScriptHost` is in `infra/scripting/` (depends on sol2 + Lua)
- [x] Public header (`sol2_script_host.hpp`) does NOT include `<sol/sol.hpp>`
- [x] All sol2 errors caught and rethrown as `std::runtime_error`
- [x] `has_handler()` is `noexcept`
- [x] `dispatch_event()` returns empty string (not error) when no handler
- [x] CMake option defaults to `ON`
- [x] FetchContent fallback for sol2 when not installed locally
