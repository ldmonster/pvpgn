// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/kick_connection.hpp"


namespace pvpgn::application::moderation {

core::Result<void, KickConnectionError>
KickConnection::execute(domain::SessionId session_id, std::string_view reason) {
    // 1. Check if the session exists
    auto account_opt = registry_->account_for(session_id);
    if (!account_opt) {
        return core::fail(KickConnectionError::SessionNotFound);
    }

    // 2. Send disconnect message to session
    // (Would route a disconnect/kick message to the session)

    // 3. Unregister the session
    registry_->detach(session_id);

    return core::Result<void, KickConnectionError>{};
}

}  // namespace pvpgn::application::moderation
