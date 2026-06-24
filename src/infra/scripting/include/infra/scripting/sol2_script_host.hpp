// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/**
 * @file sol2_script_host.hpp
 * @brief sol2 (Lua 5.4) implementation of IScriptHost.
 *
 * `Sol2ScriptHost` is the concrete infrastructure adapter that backs the
 * `pvpgn::application::ports::IScriptHost` port using the sol2 C++ Lua
 * binding library.
 *
 * ### Thread safety
 * Not thread-safe.  External synchronisation is required if multiple threads
 * call methods on the same instance.
 *
 * ### Error handling
 * All sol2 errors (`sol::error`) are caught and rethrown as
 * `std::runtime_error` so that callers do not need to depend on sol2 headers.
 */

#include "infra/scripting/script_host.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace pvpgn::infra::scripting {

/**
 * @brief sol2 / Lua 5.4 implementation of IScriptHost.
 *
 * Owns a `sol::state` (Lua VM) and exposes the IScriptHost interface.
 * The Lua standard libraries are opened on construction.
 */
class Sol2ScriptHost final : public application::ports::IScriptHost {
public:
    /**
     * @brief Construct a Sol2ScriptHost and open Lua standard libraries.
     * @throws std::runtime_error if Lua VM initialisation fails.
     */
    Sol2ScriptHost();

    /**
     * @brief Destructor — closes the Lua VM.
     */
    ~Sol2ScriptHost() override;

    // Non-copyable, non-movable (owns a Lua VM).
    Sol2ScriptHost(const Sol2ScriptHost&)            = delete;
    Sol2ScriptHost& operator=(const Sol2ScriptHost&) = delete;
    Sol2ScriptHost(Sol2ScriptHost&&)                 = delete;
    Sol2ScriptHost& operator=(Sol2ScriptHost&&)      = delete;

    /**
     * @brief Load and execute a Lua script file.
     * @throws std::runtime_error on file-not-found or Lua error.
     */
    void load_file(std::string_view path) override;

    /**
     * @brief Execute a Lua code string.
     * @throws std::runtime_error on Lua syntax / runtime error.
     */
    void exec(std::string_view code) override;

    /**
     * @brief Register a C++ handler as a Lua global function.
     *
     * The handler is stored in the Lua state as a global with the given name.
     * When called from Lua, it receives the event name and a JSON payload
     * string and returns a JSON response string.
     */
    void register_function(std::string_view name,
                           application::ports::ScriptEventHandler handler) override;

    /**
     * @brief Dispatch an event to a Lua global function.
     *
     * Calls `lua[event](payload)` if the global exists and is callable.
     * Returns the result as a string, or an empty string if no handler exists.
     *
     * @throws std::runtime_error if the Lua handler raises an error.
     */
    [[nodiscard]] std::string dispatch_event(std::string_view event,
                                             std::string_view payload) override;

    /**
     * @brief Check whether a callable Lua global exists for the given event.
     */
    [[nodiscard]] bool has_handler(std::string_view event) const noexcept override;

private:
    /// Pimpl — hides sol2 headers from consumers of this header.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pvpgn::infra::scripting
