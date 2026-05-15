// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/telnet/admin_fsm.hpp"

#include <sstream>

#include "application/ports/command_registry.hpp"
#include "application/ports/permission_checker.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::protocol::telnet {

core::Status<> TelnetAdminFsm::on_connected() {
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "session context unavailable"));
    }

    // Send welcome banner
    std::string welcome = "\r\n=== pvpgn admin console ===\r\nlogin: ";
    // For now, we don't implement actual login — just show prompt
    
    // Stub: would send via ctx_->send()
    return core::ok();
}

core::Status<> TelnetAdminFsm::on_line(std::string_view line) {
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "session context unavailable"));
    }

    // Trim \r and \n
    std::string trimmed{line};
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }

    // Handle quit/exit
    if (trimmed == "quit" || trimmed == "exit") {
        ctx_->close();
        return core::ok();
    }

    // Execute command
    auto result = execute_command(trimmed);

    // Send prompt back
    // Stub: would send "> " via ctx_->send()

    return result;
}

void TelnetAdminFsm::on_close() {
    // Cleanup if needed
}

core::Status<> TelnetAdminFsm::execute_command(std::string_view line) {
    // Stub: would parse command, check permissions, execute
    // For now, return ok
    return core::ok();
}

}  // namespace pvpgn::protocol::telnet
