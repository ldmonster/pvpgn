// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_loggedin.cpp
/// ConnectionFsm — LoggedIn-state handlers.
///
/// Handlers in this TU:
///   on_enter_chat() — SID_ENTERCHAT (0x0A): client enters the chat environment

#include "application/connection/connection_fsm.hpp"

#include <span>
#include <vector>

#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

// ---------------------------------------------------------------------------
// LoggedIn state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_enter_chat(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::LoggedIn) {
        return reject("connection_fsm: SID_ENTERCHAT out of order");
    }

    // SID_ENTERCHAT (0x0A) body:
    //   [0..]  username    (NUL-terminated, may differ from login name)
    //   [..]   statstring  (NUL-terminated)

    const std::string chat_name = read_cstring(payload, 0);
    const std::string statstr   = read_cstring(payload,
                                               chat_name.size() + 1);

    // Use the login username if the chat name is empty
    const std::string& effective_name =
        chat_name.empty() ? username_ : chat_name;

    state_ = ConnectionState::InChannel;

    // Reply: SID_ENTERCHAT (0x0A)
    // Body:
    //   [0..]  unique_name  (NUL-terminated)
    //   [..]   statstring   (NUL-terminated)
    //   [..]   account_name (NUL-terminated)
    std::vector<std::byte> reply;
    for (char c : effective_name) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL
    for (char c : statstr) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL
    for (char c : username_) reply.push_back(std::byte{static_cast<std::uint8_t>(c)});
    reply.push_back(std::byte{0}); // NUL

    return ctx_.send_packet(sid::kEnterChat,
                            std::span<const std::byte>{reply});
}

}  // namespace pvpgn::application::connection
