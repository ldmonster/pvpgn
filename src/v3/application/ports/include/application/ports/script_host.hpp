// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file script_host.hpp
/// Script host port.
///
/// Abstracts the runtime that loads and executes script modules (e.g. Lua).
/// Implementations live in `infra/` and are wired by the composition root.
/// The interface is intentionally string-in / string-out to keep the port
/// free of any scripting-engine types.

#include "core/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::application::ports {

/// Opaque handle to a loaded script module.
using ScriptModuleId = std::uint32_t;

/// Port: script host interface.
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    /// Load a script file; returns a module handle or error.
    virtual core::Status<ScriptModuleId> load_file(std::string_view path) = 0;

    /// Unload a previously loaded module.
    virtual core::Status<void> unload(ScriptModuleId module_id) = 0;

    /// Call a named function in a module with string arguments.
    /// Returns the function's string result, or an error.
    virtual core::Status<std::string>
    call(ScriptModuleId             module_id,
         std::string_view           function_name,
         std::span<const std::string> args) = 0;

    /// Check if a named function exists in a module.
    virtual bool has_function(ScriptModuleId   module_id,
                               std::string_view function_name) const = 0;
};

} // namespace pvpgn::application::ports
