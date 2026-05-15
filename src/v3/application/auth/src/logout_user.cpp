// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/logout_user.hpp"

namespace pvpgn::application::auth {

LogoutUser::Result LogoutUser::execute(const LogoutRequest& req) {
    // 1. Detach the session from the registry.
    sessions_.detach(req.session_id);

    // 2. Note: Channel and Game cleanup would happen here via forEach()
    // and calling leave() on each aggregate. This is deferred to a full
    // implementation that has access to the complete domain types.
    // For now, session detachment is the primary cleanup.
    //
    // Future: channels_.forEach(...channel.leave...)
    //         games_.forEach(...game.leave...)

    return core::ok();
}

}  // namespace pvpgn::application::auth
