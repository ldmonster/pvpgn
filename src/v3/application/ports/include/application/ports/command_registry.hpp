// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file command_registry.hpp
/// Port for command dispatch — abstract interface that protocol
/// handlers (telnet admin console, IRC, BNet chat) depend on.
///
/// A concrete implementation lives in `application/chat/` and adapts
/// the chat command system to this interface. Keeping the port in
/// `application/ports/` avoids coupling the protocol layer to the
/// chat-specific concrete `CommandRegistry` type.

#include <string>
#include <string_view>
#include <vector>

#include "application/ports/permission_checker.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Abstract command registry.
///
/// Implementations dispatch a command line to a registered handler
/// after checking the caller's permission via `IPermissionChecker`.
class ICommandRegistry {
public:
    virtual ~ICommandRegistry() = default;

    /// Dispatch a command line, returning the handler's textual
    /// response on success or an error.
    virtual core::Result<std::string, core::Error>
    dispatch(domain::AccountId         caller,
             std::string_view          command_line,
             const IPermissionChecker& checker) const = 0;

    /// List command names available to the given account.
    virtual std::vector<std::string>
    list_available(domain::AccountId         caller,
                   const IPermissionChecker& checker) const = 0;
};

}  // namespace pvpgn::application::ports
