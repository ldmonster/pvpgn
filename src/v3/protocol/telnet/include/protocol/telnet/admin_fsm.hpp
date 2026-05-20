// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file admin_fsm.hpp
/// Telnet admin console line-oriented finite state machine.
/// Accepts login-less commands (for now) and executes them.

#include <memory>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "protocol/telnet/telnet_session_context.hpp"

// Forward declarations
namespace pvpgn::protocol::bnet {
class ISessionContext;
}

namespace pvpgn::application::ports {
class ICommandRegistry;
class IPermissionChecker;
}

namespace pvpgn::protocol::telnet {

/// Simple line-oriented admin console FSM.
/// Parses incoming lines, looks up commands, checks permissions, executes.
class TelnetAdminFsm {
public:
    TelnetAdminFsm(
        std::shared_ptr<ITelnetSessionContext> ctx,
        std::shared_ptr<pvpgn::application::ports::ICommandRegistry> commands,
        std::shared_ptr<pvpgn::application::ports::IPermissionChecker> permissions)
        : ctx_(ctx), commands_(commands), permissions_(permissions) {}

    /// Called when connection is established.
    /// Sends welcome banner and initial prompt.
    core::Status<> on_connected();

    /// Called when a complete line of text is received.
    /// Handles \r\n and \n line endings.
    core::Status<> on_line(std::string_view line);

    /// Called when connection is closing.
    void on_close();

private:
    /// Parse and execute a command line.
    core::Status<> execute_command(std::string_view line);

    std::shared_ptr<ITelnetSessionContext> ctx_;
    std::shared_ptr<pvpgn::application::ports::ICommandRegistry> commands_;
    std::shared_ptr<pvpgn::application::ports::IPermissionChecker> permissions_;
};

}  // namespace pvpgn::protocol::telnet
