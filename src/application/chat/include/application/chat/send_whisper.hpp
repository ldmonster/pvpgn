// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_whisper.hpp
/// SEND_WHISPER use-case — deliver a private message to an online account.
///
/// Port-injected class that validates the message, resolves the target
/// account, confirms the target has an active session, and routes the
/// whisper via the message router.

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"

namespace pvpgn::application::chat {

/// Errors that can occur when sending a whisper.
enum class SendWhisperError : std::uint8_t {
    InvalidArgument,  ///< Message body is empty.
    NotFound,         ///< Target account not found or not online.
};

/// Command to send a private whisper message.
struct SendWhisperCommand {
    domain::AccountId sender_id;
    std::string       sender_name;
    std::string       target_name;  ///< Recipient account name.
    std::string       message;
};

class SendWhisper {
public:
    explicit SendWhisper(
        std::shared_ptr<domain::identity::IAccountRepository> accounts,
        std::shared_ptr<domain::identity::ISessionRegistry>   sessions,
        std::shared_ptr<domain::connection::IMessageRouter>     router)
        : accounts_(accounts), sessions_(sessions), router_(router) {}

    /// Execute: validate, resolve target, and route the whisper.
    /// Returns SendWhisperError::InvalidArgument if message is empty.
    /// Returns SendWhisperError::NotFound if target is not online.
    [[nodiscard]] core::Result<void, SendWhisperError>
    execute(SendWhisperCommand cmd) const;

private:
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<domain::identity::ISessionRegistry>   sessions_;
    std::shared_ptr<domain::connection::IMessageRouter>     router_;
};

}  // namespace pvpgn::application::chat
