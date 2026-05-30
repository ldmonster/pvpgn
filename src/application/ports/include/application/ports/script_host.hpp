// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file script_host.hpp
/// Application-layer port for an embedded scripting host (Lua API v2, R348/R349).
///
/// Defines the `IScriptHost` interface that application use cases and
/// integration glue depend on without pulling in any scripting-runtime headers
/// (sol2 / Lua).
///
/// Concrete implementation: `pvpgn::infra::scripting::Sol2ScriptHost`.

#include <functional>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

/// Callback invoked by the script host when a script dispatches an event
/// (or when a registered C++ function is called from a script).
///
/// @param event    Event / function name (e.g. `"pvpgn.send_chat"`).
/// @param payload  Opaque JSON-encoded payload.
/// @returns        Opaque JSON-encoded response (may be empty).
using ScriptEventHandler =
    std::function<std::string(std::string_view event, std::string_view payload)>;

/// Port: an embedded script interpreter used to load user scripts and to
/// exchange events between the engine and those scripts.
///
/// Implementations are not required to be thread-safe; callers must
/// synchronise externally if multiple threads share a single host.
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    IScriptHost(const IScriptHost&)            = delete;
    IScriptHost& operator=(const IScriptHost&) = delete;
    IScriptHost(IScriptHost&&)                 = delete;
    IScriptHost& operator=(IScriptHost&&)      = delete;

    /// Load and execute a script file from disk.
    /// @throws std::runtime_error on I/O or script error.
    virtual void load_file(std::string_view path) = 0;

    /// Execute an in-memory script source string.
    /// @throws std::runtime_error on syntax or runtime error.
    virtual void exec(std::string_view code) = 0;

    /// Register a C++ handler under @p name so scripts can call it.
    virtual void register_function(std::string_view name,
                                   ScriptEventHandler handler) = 0;

    /// Dispatch @p event with @p payload to a script-side handler.
    /// Returns the handler's string result, or an empty string if no
    /// handler is registered for @p event.
    [[nodiscard]] virtual std::string dispatch_event(std::string_view event,
                                                     std::string_view payload) = 0;

    /// Whether a callable script-side handler exists for @p event.
    [[nodiscard]] virtual bool has_handler(std::string_view event) const noexcept = 0;

protected:
    IScriptHost() = default;
};

} // namespace pvpgn::application::ports
