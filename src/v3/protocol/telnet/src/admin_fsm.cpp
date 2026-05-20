// SPDX-License-Identifier: GPL-2.0-or-later

#include "protocol/telnet/admin_fsm.hpp"

#include <string>
#include <string_view>

#include "application/ports/command_registry.hpp"
#include "application/ports/permission_checker.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/telnet/telnet_session_context.hpp"

namespace pvpgn::protocol::telnet {

namespace {

constexpr std::string_view kPrompt = "> ";

core::ByteView as_bytes(std::string_view s) noexcept {
    return core::ByteView{reinterpret_cast<const std::byte*>(s.data()),
                          s.size()};
}

}  // namespace

core::Status<> TelnetAdminFsm::on_connected() {
    if (!ctx_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "session context unavailable"));
    }
    auto st = ctx_->send_line("=== pvpgn admin console ===");
    if (!st) return st;
    return ctx_->send(as_bytes(kPrompt));
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
        (void)ctx_->send_line("bye");
        ctx_->close();
        return core::ok();
    }

    // Execute command
    auto result = execute_command(trimmed);

    // Re-prompt so the next line gets a fresh "> ".
    (void)ctx_->send(as_bytes(kPrompt));

    return result;
}

void TelnetAdminFsm::on_close() {
    // Cleanup if needed
}

core::Status<> TelnetAdminFsm::execute_command(std::string_view line) {
    if (line.empty()) {
        return core::ok();
    }
    if (!commands_ || !permissions_) {
        return core::fail(core::make_error(core::StatusCode::Internal,
                                          "command registry unavailable"));
    }

    // No login yet -- dispatch as guest account 0. A real login flow
    // would replace this AccountId with the authenticated caller.
    domain::AccountId caller{0};
    auto result = commands_->dispatch(caller, line, *permissions_);
    if (!result) {
        // Surface error textually but keep the session alive.
        std::string msg = "error: ";
        msg += core::to_string(result.error().code());
        (void)ctx_->send_line(msg);
        return core::ok();
    }
    if (!result.value().empty()) {
        (void)ctx_->send_line(result.value());
    }
    return core::ok();
}

}  // namespace pvpgn::protocol::telnet
