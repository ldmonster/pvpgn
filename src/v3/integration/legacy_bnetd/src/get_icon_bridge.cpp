// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_get_icon` (FINDANONGAME
// sub-option 0x09) -> v3 icon-table pipeline.
//
// Returns 1 on success (caller must NOT run legacy code), 0 on
// fall-through (legacy code must run as before).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "application/icon_table/icon_table.hpp"
#include "infra/legacy_config/icon_req_loader.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/icon_account_adapter.hpp"
#include "integration/legacy_bnetd/dispatch.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

#include "common/setup_before.h"
#include "common/packet.h"
#include "common/tag.h"
#include "bnetd/connection.h"
#include "common/setup_after.h"

// R194: `bnetd/prefs.h` was deleted in R165. Use the v3 C bridge.
#include "integration/legacy_bnetd/prefs_bridge.hpp"

namespace it  = pvpgn::application::icon_table;
namespace pb  = pvpgn::protocol::bnet;
namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct IconBridgeState {
    std::optional<it::IconReqTable> req;
    bool init_attempted = false;
    bool init_ok        = false;
};

IconBridgeState& istate() { static IconBridgeState s; return s; }
std::mutex&      istate_mutex() { static std::mutex m; return m; }

// Lazy-load IconReqTable from the configured anongame_infos.conf.
// Called under istate_mutex.
void icon_req_init_locked() {
    auto& s = istate();
    s.init_attempted = true;
    const char* infos = pvpgn_v3_prefs_get_anongame_infos_file();
    if (infos == nullptr) {
        ila::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_get_icon_bridge",
                        "anongame_infos_file not configured");
        return;
    }
    auto r = pvpgn::infra::legacy_config::load_icon_req_table(infos);
    if (!r) {
        std::string emsg = "failed to load icon_req table: ";
        emsg += r.error().message();
        ila::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_get_icon_bridge", emsg);
        return;
    }
    s.req.emplace(std::move(r).value());
    s.init_ok = true;
}

// Map a legacy clienttag uint to icon_table::Clienttag. Returns
// nullopt for non-WC3 clients.
std::optional<it::Clienttag> map_clienttag(std::uint32_t tag) {
    constexpr std::uint32_t WAR3 = 0x57415233u;  // 'WAR3'
    constexpr std::uint32_t W3XP = 0x57335850u;  // 'W3XP'
    if (tag == WAR3) return it::Clienttag::War3;
    if (tag == W3XP) return it::Clienttag::W3xp;
    return std::nullopt;
}

bool dispatch_one_frame(void*                          conn_ptr,
                        const std::vector<std::byte>& bytes) {
    if (!pvpgn::integration::legacy_bnetd::dispatch_bnet_frame_v3(
            conn_ptr, bytes.data(), bytes.size())) {
        std::string emsg =
            "dispatch_bnet_frame_v3 rejected serialized frame (bytes ";
        emsg += std::to_string(bytes.size());
        emsg += ")";
        ila::bridge_log(pvpgn::core::LogLevel::Error,
                        "v3_get_icon_bridge", emsg);
        return false;
    }
    return true;
}

}  // namespace

extern "C" int pvpgn_v3_get_icon_try(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr || body_size == 0) return 0;
    auto* conn = static_cast<pvpgn::bnetd::t_connection*>(conn_ptr);

    // Body layout: [option(1)][count(4)].
    if (body_size < 5) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x09) return 0;  // not a GET_ICON sub-option
    const std::uint32_t count =
        static_cast<std::uint32_t>(p[1])
        | (static_cast<std::uint32_t>(p[2]) << 8)
        | (static_cast<std::uint32_t>(p[3]) << 16)
        | (static_cast<std::uint32_t>(p[4]) << 24);

    auto* account = pvpgn::bnetd::conn_get_account(conn);
    if (account == nullptr) return 0;
    auto tag_uint = static_cast<std::uint32_t>(
        pvpgn::bnetd::conn_get_clienttag(conn));
    auto kind = map_clienttag(tag_uint);
    if (!kind) return 0;

    {
        std::lock_guard<std::mutex> g{istate_mutex()};
        if (!istate().init_attempted) icon_req_init_locked();
        if (!istate().init_ok) return 0;
    }

    auto ctx = ila::build_icon_account_context(account, tag_uint);
    ila::PortraitResolverCtx pctx{account, tag_uint};
    auto pure = it::build_icon_reply_table(
        *kind, *istate().req, ctx,
        &ila::portrait_resolver_fn, &pctx);

    pb::AnonGameIconReply msg{};
    msg.count       = count;
    msg.curricon    = pure.curricon;
    msg.table_width = pure.width;
    msg.table_size  = static_cast<std::uint8_t>(pure.entries.size());
    msg.entries.reserve(pure.entries.size());
    for (const auto& e : pure.entries) {
        pb::AnonGameIconReplyEntry w{};
        w.icon_code      = e.icon_code;
        w.portrait_code  = e.portrait_code;
        w.race           = e.race;
        w.required_wins  = e.required_wins;
        w.client_enabled = e.client_enabled;
        msg.entries.push_back(w);
    }

    auto env = pb::serialize_findanongame_reply(pb::AnonGameServer{msg});
    pvpgn::protocol::Writer w;
    if (auto st = pb::encode(w, env); !st) {
        std::string emsg = "encode failed: ";
        emsg += st.error().message();
        ila::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_get_icon_bridge", emsg);
        return 0;
    }
    auto view = w.view();
    std::vector<std::byte> bytes(view.size());
    std::memcpy(bytes.data(), view.data(), view.size());
    if (!dispatch_one_frame(conn_ptr, bytes)) return 0;
    return 1;
}
