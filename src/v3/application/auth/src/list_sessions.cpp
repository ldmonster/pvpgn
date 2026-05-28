// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/list_sessions.hpp"

namespace pvpgn::application::auth {

core::Status<std::vector<SessionInfo>> ListSessions::execute() const {
    // 1. Enumerate all active session IDs from the registry.
    const auto session_ids = sessions_.list();

    std::vector<SessionInfo> result;
    result.reserve(session_ids.size());

    // 2. For each session, resolve the account ID and then the account name.
    for (const auto& sid : session_ids) {
        const auto account_id_opt = sessions_.account_for(sid);
        if (!account_id_opt.has_value()) {
            // Session was removed between list() and account_for() — skip.
            continue;
        }
        const domain::AccountId account_id = *account_id_opt;

        auto found = repo_.find_by_id(account_id.value());
        if (!found) {
            // Account disappeared from the repository — skip gracefully.
            continue;
        }
        const auto& account = found.value();

        result.push_back(SessionInfo{
            account_id,
            std::string{account.name().canonical()},
        });
    }

    return result;
}

}  // namespace pvpgn::application::auth
