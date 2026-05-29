// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/kick_connection.hpp"

#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"

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
    // auto route_result = router_->route_to_session(session_id, kick_message);
    // if (!route_result) {
    //     return core::fail(KickConnectionError::RoutingFailed);
    // }

    // 3. Unregister the session
    registry_->detach(session_id);

    return core::Result<void, KickConnectionError>{};
}

}  // namespace pvpgn::application::moderation
