// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/kick_connection.hpp"

#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"

namespace pvpgn::application::moderation {

core::Result<void, KickConnectionError>
KickConnection::execute(domain::SessionId session_id, std::string_view reason) {
    // 1. Find the session
    auto session_result = registry_->find_session_by_id(session_id);
    if (!session_result) {
        return core::fail(KickConnectionError::SessionNotFound);
    }

    // 2. Send disconnect message to session
    // (Would route a disconnect/kick message to the session)
    // auto route_result = router_->route_to_session(session_id, kick_message);
    // if (!route_result) {
    //     return core::fail(KickConnectionError::RoutingFailed);
    // }

    // 3. Unregister the session
    auto unregister_result = registry_->remove_session(session_id);
    if (!unregister_result) {
        return core::fail(KickConnectionError::RoutingFailed);
    }

    return core::ok();
}

}  // namespace pvpgn::application::moderation
