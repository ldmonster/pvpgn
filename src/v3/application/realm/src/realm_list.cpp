// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/realm_list.hpp"

namespace pvpgn::application::realm {

RealmListResponse dispatch_realm_list(const IRealmRepository& repo) {
    RealmListResponse out;
    auto all = repo.list_all();
    out.active_entries.reserve(all.size());
    for (auto& r : all) {
        if (r.active) {
            out.active_entries.push_back(std::move(r));
        }
    }
    return out;
}

} // namespace pvpgn::application::realm
