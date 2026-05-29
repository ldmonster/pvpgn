// SPDX-License-Identifier: GPL-2.0-or-later
//
// Linked half of the realm_list bridge. Pulls `realmlist()` /
// `realm_get_*` from `bnetd_legacy`, runs the application
// `dispatch_realm_list` and ships the reply through the existing
// `pvpgn_v3_send_realmlistreply` /
// `pvpgn_v3_send_realmlistlegacyreply` bridges. Linked only into
// `integration_legacy_bnetd_linked`; the headers above are pure
// C++ and have no legacy dependency.

#include "integration/legacy_bnetd/realm_list_bridge.hpp"

#include <atomic>
#include <string>
#include <vector>

#include "application/realm/realm_list.hpp"
#include "integration/legacy_bnetd/send_realmlist_bridge.hpp"
#include "integration/legacy_bnetd/send_realmlistlegacy_bridge.hpp"

#include "common/setup_before.h"
#include "bnetd/realm.h"
#include "common/setup_after.h"

// Legacy "unknown" field constants used by the on-wire reply
// structures. Copied verbatim from
// `src/bnetd/handle_bnet.cpp:_client_realmlistreq{,110}` so the
// bridge produces byte-identical bytes to the legacy path.
#ifndef SERVER_REALMLISTREPLY_DATA_UNKNOWN3
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN3      0x00000080u
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN4      0xff000000u
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN5      0xff7700ffu
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN6      0xff000000u
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN7      0xff0000ffu
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN8      0xff0000ffu
#  define SERVER_REALMLISTREPLY_DATA_UNKNOWN9      0xff0000ffu
#endif
#ifndef SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1
#  define SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1  0x00000001u
#endif

namespace pa = pvpgn::application::realm;

namespace pvpgn::integration::legacy_bnetd {

namespace {

/// Adapter exposing the legacy `realmlist()` vector as an
/// `IRealmRepository`. Strings are copied so the caller can keep
/// the `RealmListing` after `realmlist()` mutates.
class LegacyRealmRepository final : public pa::IRealmRepository {
public:
    std::vector<pa::RealmListing> list_all() const override {
        std::vector<pa::RealmListing> out;
        using ::pvpgn::bnetd::realmlist;
        using ::pvpgn::bnetd::realm_get_name;
        using ::pvpgn::bnetd::realm_get_description;
        using ::pvpgn::bnetd::realm_get_active;
        for (auto const* r : realmlist()) {
            if (r == nullptr) continue;
            char const* nm   = realm_get_name(r);
            char const* desc = realm_get_description(r);
            pa::RealmListing e;
            e.id          = 0;  // legacy reply does not carry id
            e.name        = (nm   != nullptr) ? std::string{nm}   : std::string{};
            e.description = (desc != nullptr) ? std::string{desc} : std::string{};
            e.active      = (realm_get_active(r) != 0);
            out.push_back(std::move(e));
        }
        return out;
    }
};

int legacy_realm_list_handler(void* conn_ptr, int legacy_format) {
    LegacyRealmRepository repo;
    auto resp = pa::dispatch_realm_list(repo);

    if (legacy_format != 0) {
        std::vector<pvpgn_v3_realm_legacy_entry> entries;
        entries.reserve(resp.active_entries.size());
        for (auto const& e : resp.active_entries) {
            pvpgn_v3_realm_legacy_entry x;
            x.unknown3    = SERVER_REALMLISTREPLY_DATA_UNKNOWN3;
            x.unknown4    = SERVER_REALMLISTREPLY_DATA_UNKNOWN4;
            x.unknown5    = SERVER_REALMLISTREPLY_DATA_UNKNOWN5;
            x.unknown6    = SERVER_REALMLISTREPLY_DATA_UNKNOWN6;
            x.unknown7    = SERVER_REALMLISTREPLY_DATA_UNKNOWN7;
            x.unknown8    = SERVER_REALMLISTREPLY_DATA_UNKNOWN8;
            x.unknown9    = SERVER_REALMLISTREPLY_DATA_UNKNOWN9;
            x.name        = e.name.c_str();
            x.description = e.description.c_str();
            entries.push_back(x);
        }
        int rc = ::pvpgn_v3_send_realmlistlegacyreply(
            conn_ptr,
            entries.empty() ? nullptr : entries.data(),
            static_cast<unsigned int>(entries.size()));
        return (rc == 1) ? 1 : 0;
    }

    std::vector<pvpgn_v3_realm_entry> entries;
    entries.reserve(resp.active_entries.size());
    for (auto const& e : resp.active_entries) {
        pvpgn_v3_realm_entry x;
        x.unknown     = SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1;
        x.name        = e.name.c_str();
        x.description = e.description.c_str();
        entries.push_back(x);
    }
    int rc = ::pvpgn_v3_send_realmlistreply(
        conn_ptr,
        entries.empty() ? nullptr : entries.data(),
        static_cast<unsigned int>(entries.size()));
    return (rc == 1) ? 1 : 0;
}

}  // namespace

void install_legacy_realm_list_handler() {
    set_realm_list_handler(&legacy_realm_list_handler);
}

}  // namespace pvpgn::integration::legacy_bnetd
