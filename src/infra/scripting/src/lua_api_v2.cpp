// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file lua_api_v2.cpp
 * @brief Lua API v2 surface — registers the `pvpgn.*` namespace into a sol::state.
 *
 * Each function in the `pvpgn` table delegates to a C++ handler stored in the
 * provided handler map.  If no handler is registered for a given function, the
 * Lua call returns nil and prints a warning to stderr (so plugins don't crash
 * silently during development).
 *
 * ### Registered functions
 * ```lua
 * pvpgn.log(level, message)           -- level: "debug"|"info"|"warn"|"error"
 * pvpgn.send_chat(username, message)  -- send chat message to user
 * pvpgn.get_account(username)         -- returns table {name, flags, wins, losses}
 * pvpgn.ban_account(username, reason)
 * pvpgn.kick_user(username, reason)
 * pvpgn.broadcast(channel, message)
 * ```
 */

#include "infra/scripting/lua_api_v2.hpp"

#include <format>
#include <iostream>
#include <string>
#include <unordered_map>

namespace pvpgn::infra::scripting {

namespace {

using HandlerMap = std::unordered_map<std::string, application::ports::ScriptEventHandler>;

/**
 * @brief Look up a handler and call it, or return nil if not registered.
 *
 * @param handlers   Handler map.
 * @param key        Handler key (e.g. "pvpgn.send_chat").
 * @param payload    JSON payload to pass to the handler.
 * @return           JSON response string, or empty string if no handler.
 */
std::string invoke_handler(HandlerMap& handlers,
                           const std::string& key,
                           const std::string& payload) {
    auto it = handlers.find(key);
    if (it == handlers.end()) {
        std::cerr << "[pvpgn lua api v2] WARNING: no handler registered for '"
                  << key << "'\n";
        return {};
    }
    return it->second(key, payload);
}

} // anonymous namespace

// ============================================================================
// register_v2_api
// ============================================================================

void register_v2_api(sol::state& lua, HandlerMap& handlers) {
    // Create (or reuse) the `pvpgn` table.
    sol::table pvpgn = lua.create_named_table("pvpgn");

    // -------------------------------------------------------------------------
    // pvpgn.log(level, message)
    // -- level: "debug" | "info" | "warn" | "error"
    // -------------------------------------------------------------------------
    pvpgn.set_function("log",
        [&handlers](const std::string& level, const std::string& message) {
            const std::string payload =
                std::format(R"({{"level":"{}","message":"{}"}})", level, message);
            invoke_handler(handlers, "pvpgn.log", payload);
        });

    // -------------------------------------------------------------------------
    // pvpgn.send_chat(username, message)
    // -- Send a chat message to the named user.
    // -------------------------------------------------------------------------
    pvpgn.set_function("send_chat",
        [&handlers](const std::string& username, const std::string& message) {
            const std::string payload =
                std::format(R"({{"username":"{}","message":"{}"}})", username, message);
            invoke_handler(handlers, "pvpgn.send_chat", payload);
        });

    // -------------------------------------------------------------------------
    // pvpgn.get_account(username)
    // -- Returns a table {name, flags, wins, losses} or nil on error.
    // -------------------------------------------------------------------------
    pvpgn.set_function("get_account",
        [&handlers, &lua](const std::string& username) -> sol::object {
            const std::string payload =
                std::format(R"({{"username":"{}"}})", username);
            const std::string response =
                invoke_handler(handlers, "pvpgn.get_account", payload);

            if (response.empty()) return sol::nil;

            // The handler returns a JSON string; parse it into a Lua table.
            // For simplicity we execute a small Lua snippet to decode it.
            // A production implementation would use a proper JSON parser.
            try {
                lua.script("_pvpgn_tmp = " + response);
                sol::object result = lua["_pvpgn_tmp"].get<sol::object>();
                lua["_pvpgn_tmp"] = sol::nil;
                return result;
            } catch (...) {
                return sol::nil;
            }
        });

    // -------------------------------------------------------------------------
    // pvpgn.ban_account(username, reason)
    // -------------------------------------------------------------------------
    pvpgn.set_function("ban_account",
        [&handlers](const std::string& username, const std::string& reason) {
            const std::string payload =
                std::format(R"({{"username":"{}","reason":"{}"}})", username, reason);
            invoke_handler(handlers, "pvpgn.ban_account", payload);
        });

    // -------------------------------------------------------------------------
    // pvpgn.kick_user(username, reason)
    // -------------------------------------------------------------------------
    pvpgn.set_function("kick_user",
        [&handlers](const std::string& username, const std::string& reason) {
            const std::string payload =
                std::format(R"({{"username":"{}","reason":"{}"}})", username, reason);
            invoke_handler(handlers, "pvpgn.kick_user", payload);
        });

    // -------------------------------------------------------------------------
    // pvpgn.broadcast(channel, message)
    // -------------------------------------------------------------------------
    pvpgn.set_function("broadcast",
        [&handlers](const std::string& channel, const std::string& message) {
            const std::string payload =
                std::format(R"({{"channel":"{}","message":"{}"}})", channel, message);
            invoke_handler(handlers, "pvpgn.broadcast", payload);
        });
}

} // namespace pvpgn::infra::scripting
