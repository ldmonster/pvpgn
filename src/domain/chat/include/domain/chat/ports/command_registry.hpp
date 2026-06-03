// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file command_registry.hpp
/// Application-layer port for the command dispatch registry.
///
/// Defines the `ICommandRegistry` interface that application use cases and
/// integration adapters depend on.
///
/// Concrete implementation: `application::chat::CommandRegistry`.

#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/permission.hpp"

namespace pvpgn::application::ports {

/// Port: dispatch a command string and list available commands.
class ICommandRegistry {
public:
    virtual ~ICommandRegistry() = default;

    /// Dispatch a command line (e.g. `"kick bob"`) on behalf of `caller`.
    ///
    /// @returns The handler's string result on success.
    /// @returns `InvalidArgument` if `command_line` is empty.
    /// @returns `NotFound`        if no command with that name is registered.
    /// @returns `PermissionDenied` if `caller` lacks the required permission.
    [[nodiscard]] virtual core::Result<std::string, core::Error>
    dispatch(domain::AccountId caller, std::string_view command_line,
             const domain::IPermissionChecker& checker) const = 0;

    /// Return a sorted list of command names available to `caller`.
    [[nodiscard]] virtual std::vector<std::string>
    list_available(domain::AccountId caller,
                   const domain::IPermissionChecker& checker) const = 0;
};

}  // namespace pvpgn::application::ports
