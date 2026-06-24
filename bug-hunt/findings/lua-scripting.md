# Lua Scripting Interface — Original vs v3 Bug Hunt

Subsystem: Lua scripting interface (the `api.*` functions exposed to scripts and the
`lua_handle_*` / `handle_*` event hooks the server fires).

ORIGINAL: `/home/cnupt/work/pvpgn-server/src/bnetd/{luafunctions,luainterface,luaobjects,luawrapper}.cpp`
CURRENT v3: two parallel, largely-disconnected Lua stacks:
  1. **sol2 "Lua API v2"** — `src/infra/scripting/src/{sol2_script_host,lua_api_v2}.cpp` (+ `script_host.hpp` port). Registers a `pvpgn.*` namespace. **Never instantiated outside its own unit tests** — no app wiring.
  2. **`LuaRuntime` + `LuaConnectionContext`** — `src/infra/lua/src/lua_runtime.cpp` + `src/app/bnetd/src/lua_connection_context.cpp`. **This is the live path**: `main.cpp` creates a global `LuaRuntime`, loads `lua/main.lua`, and `bnet_bnftp_dispatch.cpp` wraps every BNet connection in a `LuaConnectionContext` that fires `handle_*` hooks.

The v3 "Lua API v2" (`pvpgn.*`) is an explicitly intentional redesign (the prompt says so) and is NOT-IMPLEMENTED/not wired — see F1. The **bugs** are in the live `LuaRuntime`/`LuaConnectionContext` path, because v3 *ships the original 2014 Lua scripts verbatim* in `scripts/lua/` and tries to drive them from an incompatible new calling convention.

---

## Context: v3 ships the ORIGINAL scripts but a NEW host

`/home/cnupt/work/pvpgn/scripts/lua/` is a byte-for-byte copy of the original
`/home/cnupt/work/pvpgn-server/lua/` (same HarpyWar 2014 headers, same `handle_*.lua`,
`config.lua`, `include/`, `extend/`, ghost/antihack subsystems). Those scripts were written
against the original C++ ABI (account *objects*, `api.*` helpers, all-files-preloaded). The live
v3 host (`LuaConnectionContext`) calls them with a completely different convention. So this is not
a clean greenfield API — it's a new host pointed at old scripts, which makes the divergences
genuine runtime bugs, not intentional redesign.

---

## F1 — `pvpgn.*` "Lua API v2" surface is dead code: never registered, never used
**Severity:** Medium (it is the documented "API" but does nothing)
**Classification:** NOT-IMPLEMENTED

- v3 ref: `src/infra/scripting/src/lua_api_v2.cpp:61` `void register_v2_api(sol::state&, HandlerMap&)`; `src/infra/scripting/src/sol2_script_host.cpp:46` `Sol2ScriptHost`.
- Divergence: `grep` across the entire tree (excluding `build/` and the file's own header/tests) finds **zero callers** of `register_v2_api`, and **zero instantiations** of `Sol2ScriptHost` / `IScriptHost` outside `tests/unit/scripting/lua_api_v2_test.cpp`. The `pvpgn.log/send_chat/get_account/ban_account/kick_user/broadcast` functions exist only in unit tests. The live server never constructs a `Sol2ScriptHost`; it uses `infra::lua::LuaRuntime` instead, into which **no `api`/`pvpgn` functions are ever registered** (grep for `set_function`/`lua_register`/`create_named_table` in `src/infra/lua` + `src/app/bnetd/src` = none).
- Impact: Any script that calls `api.message_send_text(...)`, `api.account_get_attr(...)`, etc. (i.e. every original script that does anything) hits a nil global → runtime error. The new `pvpgn.*` surface is not reachable from the live VM either.
- Proposed fix: Decide on ONE host. Either (a) wire `Sol2ScriptHost` + `register_v2_api` into `main.cpp` and port scripts to `pvpgn.*`, or (b) register the legacy `api.*` table into `LuaRuntime`. Today neither is done, so scripting is effectively non-functional for any non-trivial script.

---

## F2 — Live host loads ONLY `main.lua`; original preloaded the whole script dir
**Severity:** High
**Classification:** BUG

- Original ref: `luainterface.cpp:89-100`
  ```cpp
  std::vector<std::string> files = dir_getfiles(scriptdir, ".lua", true);
  for (int i = 0; i < files.size(); ++i)
      vm.load_file(files[i].c_str());   // load EVERY .lua file
  _register_functions();
  ...
  lua_handle_server(luaevent_server_start);  // then call main()
  ```
  The original loads every `.lua` in the tree (config.lua, include/*, extend/*, ghost/*, antihack/*, and all `handle_*.lua`) **before** calling `main()`. The scripts therefore rely on every global being defined by side effect of file loading — there is **no `require`/`dofile`** anywhere (`grep require/dofile scripts/lua/*.lua` = none).
- v3 ref: `src/app/bnetd/src/main.cpp:248-259`
  ```cpp
  const std::string lua_main = (cfg.data_dir / "lua/main.lua").string();
  if (auto err = g_lua_runtime.load_file(lua_main)) { ... }
  else { (void)g_lua_runtime.call_hook("main"); }
  ```
  v3 loads **only `main.lua`** and immediately calls `main()`. It never loads `config.lua`, `include/`, `extend/`, the `handle_*.lua` files, or ghost/antihack. Since the scripts have no `require` chain, none of those globals exist.
- Impact: `main.lua:12` does `if (config.ah) then` — `config` is nil (defined in `config.lua:11`, never loaded) → `attempt to index a nil value (global 'config')` → `main()` errors and bails. Worse, **none of the `handle_*` hook functions are ever defined**, so every `call_hook("handle_user_login", …)` in F3/F4 silently no-ops (`is_function` returns false). The entire bundled script suite is inert.
- Proposed fix: Replicate the original loader — enumerate `*.lua` under the script dir and `load_file` each (respecting the original ordering where include/ helpers load first), then call `main`. Or add an explicit `require` bootstrap to `main.lua`.

---

## F3 — Hook arguments are bare strings; original/scripts expect account & object TABLES
**Severity:** High
**Classification:** BUG

- Original ref: `luainterface.cpp:505-553` (`lua_handle_user`) builds `o_account = get_account_object(account)` (a `map<string,string>` → Lua table with `.name`, `.flags`, `.uid`, `.bnet`, attrs, etc.) and passes the **table**:
  ```cpp
  lua::transaction(vm) << lua::lookup("handle_user_login") << o_account << lua::invoke ...
  ```
  Bundled script (identical in both repos) `scripts/lua/handle_user.lua:14`:
  ```lua
  function handle_user_login(account)
      if (config.ghost) then gh_handle_user_login(account) end  -- ghost reads account.name, account.uid, ...
  ```
- v3 ref: `src/app/bnetd/src/lua_connection_context.cpp:61-69`
  ```cpp
  void LuaConnectionContext::on_authenticated(std::string_view username) {
      username_ = std::string{username};
      runtime_.call_hook("handle_user_login", username_);  // passes a STRING, not a table
  }
  ```
  `LuaRuntime::call_hook` only ever pushes string args (`lua_pushlstring`, `lua_runtime.cpp:219+`). There is no table/object marshalling at all.
- Impact: Any script that does `account.name` (every real handler, including the ghost subsystem the bundled `handle_user_login` immediately calls) gets `attempt to index a string value`. A script written for the original is silently broken. Even if F2 were fixed so the hooks were defined, they'd crash on first field access.
- Proposed fix: Marshal an account table (name + key fields/attrs) like `get_account_object`, or formally document and migrate scripts to a string-based contract. The current state is the worst of both: ships object-expecting scripts, feeds them strings.

---

## F4 — `handle_channel_*` argument ORDER is swapped vs original
**Severity:** High
**Classification:** BUG

- Original ref: `luainterface.cpp:484-491` passes **channel first, account second**:
  ```cpp
  lua::transaction(vm) << lua::lookup(func_name) << o_channel << o_account << ... << lua::invoke ...
  ```
  Bundled script `scripts/lua/handle_channel.lua:17`:
  ```lua
  function handle_channel_userjoin(channel, account)   -- (channel, account)
  function handle_channel_userleft(channel, account)
  function handle_channel_message(channel, account, text, message_type)
  ```
- v3 ref: `src/app/bnetd/src/lua_connection_context.cpp:71-90` passes **username first, channel second** (and both as strings):
  ```cpp
  runtime_.call_hook("handle_channel_userjoin", username_, channel_name_);  // (username, channel)
  runtime_.call_hook("handle_channel_userleft", username_, channel_name_);
  ```
- Impact: Argument positions are inverted *and* types changed (string vs table). A script's `channel` param receives the username string and `account` receives the channel name. `channel.name` / `account.name` both break, and any logic keyed on channel vs account is inverted. `handle_channel_message` isn't fired at all in v3 (see F6).
- Proposed fix: Match the original `(channel, account, …)` order and object shape, or rewrite the bundled scripts.

---

## F5 — `handle_game_create` arity/shape mismatch
**Severity:** Medium-High
**Classification:** BUG

- Original ref: `luainterface.cpp:357,383-397` — `handle_game_create` receives a single **game object** (`o_game = get_game_object(game)`):
  ```cpp
  case luaevent_game_create: func_name = "handle_game_create"; break;
  ...
  lua::transaction(vm) << lua::lookup(func_name) << o_game << lua::invoke ...
  ```
  Bundled `scripts/lua/handle_game.lua:10` `function handle_game_create(game)`.
- v3 ref: `src/app/bnetd/src/lua_connection_context.cpp:92-105` fires `handle_game_create(username, game_name, game_type)` — three strings, and from the **joining/creating connection** rather than a game-centric event:
  ```cpp
  runtime_.call_hook("handle_game_create", username_, game_name_, game_type_str(info.game_type));
  ```
  Also: original `handle_game_userjoin(game, account)` / `handle_game_userleft(game, account)`
  (`handle_game.lua:20,30`) vs v3 `handle_game_userjoin(username, game_name)` /
  `handle_game_userleft(username, game_name)` (`lua_connection_context.cpp:107-129`) — game object replaced by name string, arg order/shape changed.
- Impact: `game.id`, `game.name`, `game.clienttag`, etc. all unavailable / wrong type. Game-create scripts break.
- Proposed fix: Pass a game object table; align the userjoin/userleft signatures with `(game, account)`.

---

## F6 — Many original hooks are NOT fired by v3 at all
**Severity:** Medium (feature gap that silently disables script behaviour)
**Classification:** BUG (live path) — partially NOT-IMPLEMENTED

Original fires these (firing sites confirmed via grep in pvpgn-server):
| Hook | Original firing site | v3 equivalent |
|---|---|---|
| `handle_user_whisper` | `command.cpp:188` | **none** |
| `handle_command` / `handle_command_before` | `command.cpp:557,578` | **none** |
| `handle_user_disconnect` | `connection.cpp:699` | **none** (`on_disconnected` is documented in the header comment at `lua_connection_context.hpp:18`/`.cpp:14` but **no such method exists** and nothing calls it) |
| `handle_user_icon` | `connection.cpp:2641,3767` (return value rewrites icon) | **none** |
| `handle_channel_message` | `channel.cpp:769` (return value can suppress msg) | **none** (v3 fires join/left only) |
| `handle_game_report/_end/_destroy/_changestatus/_list` | `game.cpp:479,747,1120,1463,2210` | **none** |
| `handle_client_readmemory` / `handle_client_extrawork` | `handle_bnet.cpp:3473,5556` | **none** |
| `handle_server_mainloop` / `handle_server_rehash` | `server.cpp:1502,1520` | **none** |

- v3 ref: the only hooks fired are the 6 in `lua_connection_context.cpp` (login, channel join/left, game create/join/left). The header comment claims `on_disconnected → handle_user_disconnect` but the method is absent.
- Impact: Whisper interception, command interception (`/`-commands implemented in Lua), icon rewriting, channel-message moderation/filtering, game lifecycle logging, antihack memory reads, and the per-second mainloop tick are all silently dead. Several of these have **return-value semantics** in the original (e.g. `lua_handle_command` returning `1` suppresses the built-in command, `lua_handle_channel`/`lua_handle_user` return `1` to swallow the message) — v3 discards all hook return values (`do_call` always calls with `nresults=0`, `lua_runtime.cpp:174`), so even if these hooks were added they couldn't veto an action.
- Proposed fix: Add the missing fire points and a return-value channel (capture an int/bool result from `lua_pcall`) before claiming hook parity.

---

## F7 — `pvpgn.get_account` JSON decode runs untrusted handler output as Lua code
**Severity:** Medium (security/robustness) — only live if F1 is wired up
**Classification:** BUG (latent)

- v3 ref: `src/infra/scripting/src/lua_api_v2.cpp:103-110`
  ```cpp
  lua.script("_pvpgn_tmp = " + response);   // executes the handler's JSON string AS LUA
  sol::object result = lua["_pvpgn_tmp"].get<sol::object>();
  ```
  The handler's response (built with `std::format` and naive `"{}"` interpolation — `lua_api_v2.cpp:71-140`, no escaping of `"`/`\`/newlines in `username`/`message`/`reason`) is concatenated into a Lua chunk and executed. Original has no analogue (it marshals C++ → Lua via typed stack pushes in `luawrapper`, never `eval`s data).
- Impact: (a) Code injection — a username/account field containing Lua (e.g. `"]] os.execute('…')--"`) executes in the VM. (b) The `std::format(R"({{"username":"{}"}})", username)` payloads are not JSON-escaped, so any `"` in a username produces malformed JSON and breaks the handler. Both are real once the API is wired.
- Proposed fix: Use a real JSON parser/serializer for both directions; never `lua.script()` on data. Escape all interpolated string fields.

---

## F8 — `dispatch_event` / sol2 host throws on Lua error; original swallows
**Severity:** Low (latent; host unused)
**Classification:** UNSURE (divergence, low impact while unused)

- Original ref: every `lua_handle_*` wraps the call in `try/catch(...)` and logs, never propagating (e.g. `luainterface.cpp:340-347`). A faulty script can't crash the server.
- v3 ref: `src/infra/scripting/src/sol2_script_host.cpp:104-122` — `dispatch_event` **throws `std::runtime_error`** on a script error. The live `LuaRuntime` path is fine (it returns the error as `optional<string>` and `LuaConnectionContext` logs+ignores, `lua_connection_context.cpp:65-68`), but the sol2 host's throwing contract differs from the original's swallow-all behaviour. If F1 is ever wired without try/catch at the call site, a buggy script becomes a server-killing exception.
- Proposed fix: Decide the error contract; if matching original semantics, catch+log inside the host rather than throwing.

---

## What MATCHES / is correct

- **Hook *names*** in the live path are correct: `handle_user_login`, `handle_channel_userjoin`,
  `handle_channel_userleft`, `handle_game_create`, `handle_game_userjoin`, `handle_game_userleft`,
  and `main` all match the original function names — so the *naming* convention was preserved even
  though arg shape/order/typing (F3–F5) and the load model (F2) were not.
- **Missing-hook = silent no-op**: `LuaRuntime::call_hook` guards with `is_function` and returns
  `nullopt` when the global is absent (`lua_runtime.cpp:198`), matching the original's tolerance of
  scripts that don't define every hook.
- **No-Lua build is a safe no-op**: the whole `LuaRuntime` is `#ifdef PVPGN_HAVE_LUA`-guarded with
  stub returns (`lua_runtime.cpp:12,107,136,…`), mirroring the original `#ifdef WITH_LUA` guard.
- **`username` captured on auth and reused** for subsequent hooks (`on_authenticated` stores
  `username_`) — sensible, and unit-tested (`lua_connection_context_test.cpp:111,249`).
- **sol2 host error wrapping** uses `protected_function` + validity checks (no raw `lua_pcall`
  longjmp into C++), which is technically sound; only the throw-vs-swallow policy (F8) diverges.

---

## Summary of severities
- High: F2 (only main.lua loaded → whole script suite inert), F3 (string vs account-table args), F4 (channel hook arg order swapped)
- Medium-High: F5 (game hook shape/arity)
- Medium: F1 (Lua API v2 dead/unwired), F6 (most hooks never fired; return-value veto lost), F7 (eval-on-data injection, latent)
- Low: F8 (throw vs swallow on script error, latent)

Net: the live v3 Lua path fires only 6 of the original ~20 hooks, with mismatched argument
types/order, while shipping the unmodified original scripts that expect object tables, the full
`api.*` helper surface, and whole-directory preloading — none of which v3 provides. The separate
"Lua API v2" (`pvpgn.*`) is a clean intentional redesign but is entirely unwired.
