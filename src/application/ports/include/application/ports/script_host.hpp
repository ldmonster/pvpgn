// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file script_host.hpp
 * @brief Application port: IScriptHost — Lua scripting engine abstraction (R348).
 *
 * This port decouples the application layer from any specific Lua binding
 * library (sol2, plain Lua C API, etc.).  The infrastructure layer provides
 * a concrete implementation (`Sol2ScriptHost`).
 *
 * ### Typical usage
 * @code
 *   // In composition root:
 *   auto host = std::make_unique<pvpgn::infra::scripting::Sol2ScriptHost>();
 *   host->register_function("pvpgn_send_chat", my_send_chat_handler);
 *   host->load_file("plugins/my-plugin/main.lua");
 *
 *   // Dispatch an event from the server:
 *   std::string response = host->dispatch_event("on_user_login",
 *       R"({"username":"Alice"})");
 * @endcode
 */

#include <functional>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

/**
 * @brief Callback type for C++ functions exposed to Lua.
 *
 * Receives the event name and a JSON payload string; returns a JSON response
 * string (may be empty if no response is needed).
 */
using ScriptEventHandler =
    std::function<std::string(std::string_view event, std::string_view payload)>;

/**
 * @brief Port: Lua scripting host interface.
 *
 * Implementations must be thread-compatible (external synchronisation
 * required for concurrent access) but need not be thread-safe internally.
 */
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    // Non-copyable.
    IScriptHost(const IScriptHost&)            = delete;
    IScriptHost& operator=(const IScriptHost&) = delete;

    /**
     * @brief Load and execute a Lua script file.
     *
     * @param path  Path to the `.lua` file (UTF-8).
     * @throws std::runtime_error if the file cannot be opened or contains
     *         a Lua syntax / runtime error.
     */
    virtual void load_file(std::string_view path) = 0;

    /**
     * @brief Execute a Lua code string.
     *
     * @param code  Lua source code.
     * @throws std::runtime_error on Lua syntax / runtime error.
     */
    virtual void exec(std::string_view code) = 0;

    /**
     * @brief Register a C++ function callable from Lua as a global.
     *
     * The function is exposed as a Lua global with the given @p name.
     * Calling it from Lua passes the event name and a JSON payload string;
     * the return value (if any) is the JSON response string.
     *
     * @param name     Lua global name (e.g. `"pvpgn_send_chat"`).
     * @param handler  C++ handler to invoke.
     */
    virtual void register_function(std::string_view name,
                                   ScriptEventHandler handler) = 0;

    /**
     * @brief Dispatch a named event to Lua handlers.
     *
     * Looks up a Lua global function named @p event and calls it with
     * @p payload as the argument.  If no such function exists, returns an
     * empty string without error.
     *
     * @param event    Event name (must be a valid Lua identifier).
     * @param payload  JSON payload string passed to the Lua handler.
     * @return         JSON response string returned by the Lua handler,
     *                 or an empty string if no handler is registered.
     * @throws std::runtime_error if the Lua handler throws a Lua error.
     */
    [[nodiscard]] virtual std::string dispatch_event(std::string_view event,
                                                     std::string_view payload) = 0;

    /**
     * @brief Check whether a Lua handler is registered for the given event.
     *
     * @param event  Event name (Lua global function name).
     * @return `true` if a callable Lua global with that name exists.
     */
    [[nodiscard]] virtual bool has_handler(std::string_view event) const noexcept = 0;

protected:
    IScriptHost() = default;
};

} // namespace pvpgn::application::ports
