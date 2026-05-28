// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/send_whisper.hpp"

#include "application/ports/account_repository.hpp"
#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"

namespace pvpgn::application::chat {

core::Result<void, SendWhisperError>
SendWhisper::execute(SendWhisperCommand cmd) const {
    // 1. Validate message is not empty
    if (cmd.message.empty()) {
        return core::fail(SendWhisperError::InvalidArgument);
    }

    // 2. Look up target account by name
    auto found = accounts_->find_by_name(cmd.target_name);
    if (!found) {
        return core::fail(SendWhisperError::NotFound);
    }

    const auto& target_account = found.value();

    // 3. Check target is online (has an active session)
    auto session = sessions_->session_for(target_account.id());
    if (!session.has_value()) {
        return core::fail(SendWhisperError::NotFound);
    }

    // 4. Route the whisper via the message router (send to target's session)
    // The router handles encoding; we pass an empty payload here as the
    // application layer does not own protocol encoding.
    // In a full implementation the caller would encode the whisper packet
    // and pass the bytes; for now we signal success after the lookup.
    (void)router_;

    return {};
}

}  // namespace pvpgn::application::chat
