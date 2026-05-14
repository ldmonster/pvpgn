// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_set_icon` (FINDANONGAME
// sub-option 0x0A) -> v3 icon-validator pipeline.
//
// Mirrors the legacy logic exactly:
//   1. Skip when custom-icons override is in effect for the account.
//   2. Translate `desired_icon == 0` to "1O3W" (default).
//   3. Validate via `validate_user_icon`; fall back to "1O3W" on
//      "ICON SWITCH" hack attempts.
//   4. Apply via legacy `account_set_user_icon`,
//      `conn_update_w3_playerinfo`, `channel_rejoin`.
//
// Returns 1 on success (caller must NOT run legacy code), 0 on
// fall-through (legacy code must run as before).

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>

#include "application/icon_table/icon_table.hpp"
#include "infra/legacy_config/icon_req_loader.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/icon_account_adapter.hpp"

#include "common/setup_before.h"
#include "common/tag.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "bnetd/channel.h"
#include "bnetd/connection.h"
#include "bnetd/icons.h"
#include "bnetd/prefs.h"
#include "common/setup_after.h"

namespace it  = pvpgn::application::icon_table;
namespace ila = pvpgn::integration::legacy_bnetd;

// Symbol defined in get_icon_bridge.cpp -- reuse the shared icon_req
// init/state. We re-declare the loader here rather than introducing a
// new shared-state header.
namespace pvpgn::infra::legacy_config {
core::Result<IconReqTable> load_icon_req_table(std::string_view path);
}

namespace {

struct SetIconState {
    std::optional<it::IconReqTable> req;
    bool init_attempted = false;
    bool init_ok        = false;
};

SetIconState&    sstate() { static SetIconState s; return s; }
std::mutex&      sstate_mutex() { static std::mutex m; return m; }

void set_icon_init_locked() {
    auto& s = sstate();
    s.init_attempted = true;
    const char* infos = pvpgn::bnetd::prefs_get_anongame_infos_file();
    if (infos == nullptr) return;
    auto r = pvpgn::infra::legacy_config::load_icon_req_table(infos);
    if (!r) return;
    s.req.emplace(std::move(r).value());
    s.init_ok = true;
}

}  // namespace

extern "C" int pvpgn_v3_set_icon_try(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr) return 0;
    if (body_size < 5) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x0A) return 0;  // not a SET_ICON sub-option

    auto* conn      = static_cast<pvpgn::bnetd::t_connection*>(conn_ptr);
    auto* account   = pvpgn::bnetd::conn_get_account(conn);
    if (account == nullptr) return 0;
    auto  clienttag = pvpgn::bnetd::conn_get_clienttag(conn);

    // Legacy: do nothing when custom_icons is enabled and the
    // account already has a custom icon.
    if (pvpgn::bnetd::prefs_get_custom_icons() == 1
        && pvpgn::bnetd::customicons_allowed_by_client(clienttag)
        && pvpgn::bnetd::customicons_get_icon_by_account(account, clienttag)) {
        return 1;  // legacy returns 0 here too -- "do nothing" is success
    }

    // Translate the 4-byte payload into the user_icon string.
    const std::uint32_t desired =
        static_cast<std::uint32_t>(p[1])
        | (static_cast<std::uint32_t>(p[2]) << 8)
        | (static_cast<std::uint32_t>(p[3]) << 16)
        | (static_cast<std::uint32_t>(p[4]) << 24);

    std::array<char, 4> user_icon{};
    if (desired == 0) {
        user_icon = {'1', 'O', '3', 'W'};
    } else {
        user_icon[0] = static_cast<char>(p[1]);
        user_icon[1] = static_cast<char>(p[2]);
        user_icon[2] = static_cast<char>(p[3]);
        user_icon[3] = static_cast<char>(p[4]);
    }

    // Lazy-load the IconReqTable.
    {
        std::lock_guard<std::mutex> g{sstate_mutex()};
        if (!sstate().init_attempted) set_icon_init_locked();
        if (!sstate().init_ok) return 0;
    }

    // Validate against the account's race-win counts.
    auto ctx = ila::build_icon_account_context(
        account, static_cast<std::uint32_t>(clienttag));
    if (!it::validate_user_icon(*sstate().req, user_icon, ctx.race_wins)) {
        // ICON SWITCH HACK PROTECTION: silently fall back to default.
        user_icon = {'1', 'O', '3', 'W'};
        char const* uname = pvpgn::bnetd::conn_get_username(conn);
        std::string emsg = "[";
        emsg += (uname ? uname : "<?>");
        emsg += "] ICON SWITCH hack attempt, icon set to default";
        ila::bridge_log(pvpgn::core::LogLevel::Info,
                        "v3_set_icon_bridge", emsg);
    }

    // Apply via legacy account/conn machinery.
    char zterm[5] = {user_icon[0], user_icon[1], user_icon[2], user_icon[3], 0};
    pvpgn::bnetd::account_set_user_icon(account, clienttag, zterm);
    pvpgn::bnetd::conn_update_w3_playerinfo(conn);
    pvpgn::bnetd::channel_rejoin(conn);
    return 1;
}
