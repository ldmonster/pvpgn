// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file lua_api_v2.hpp
 * @brief Lua API v2 surface registration (R349).
 *
 * Registers the `pvpgn.*` Lua namespace into a `sol::state`.  The functions
 * delegate to C++ handlers stored in the provided handler map, which is
 * typically populated by the composition root.
 *
 * ### Lua API v2 surface
 * ```lua
 * pvpgn.log(level, message)           -- level: "debug"|"info"|"warn"|"error"
 * pvpgn.send_chat(username, message)  -- send chat message to user
 * pvpgn.get_account(username)         -- returns table {name, flags, wins, losses}
 * pvpgn.ban_account(username, reason)
 * pvpgn.kick_user(username, reason)
 * pvpgn.broadcast(channel, message)
 * ```
 *
 * ### Usage
 * @code
 *   sol::state lua;
 *   std::unordered_map<std::string, ScriptEventHandler> handlers;
 *   handlers["pvpgn.send_chat"] = my_send_chat_handler;
 *   pvpgn::infra::scripting::register_v2_api(lua, handlers);
 * @endcode
 */

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <string>
#include <unordered_map>

#include "application/ports/script_host.hpp"

namespace pvpgn::infra::scripting {

/**
 * @brief Register the `pvpgn.*` Lua API v2 surface into @p lua.
 *
 * Creates a `pvpgn` table in the Lua state and populates it with functions
 * that delegate to the C++ handlers in @p handlers.  If a handler for a
 * given function is not present in the map, the Lua function is still
 * registered but returns `nil` and logs a warning.
 *
 * @param lua       The Lua state to register into.
 * @param handlers  Map of handler name → C++ handler.  Keys use the form
 *                  `"pvpgn.<function_name>"` (e.g. `"pvpgn.send_chat"`).
 */
void register_v2_api(
    sol::state& lua,
    std::unordered_map<std::string, application::ports::ScriptEventHandler>& handlers);

} // namespace pvpgn::infra::scripting
