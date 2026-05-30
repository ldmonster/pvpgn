// SPDX-License-Identifier: GPL-2.0-or-later
//
// Typed sub-message parser/serializer for SID_WARCRAFTGENERAL (0x44).
// Operates over the opaque `data` field of WarcraftGeneralRequest /
// WarcraftGeneralReply that the wire codec produces.
//
// Sub-TU layout
// -------------
//   anongame/anongame_client.cpp  — client decoders + encoders
//   anongame/anongame_server.cpp  — server decoders + encoders

#include "protocol/bnet/anongame.hpp"

#include <utility>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "protocol/common/reader.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

namespace {

core::ByteView view_of(const std::vector<std::byte>& v) noexcept {
    return core::ByteView{v.data(), v.size()};
}

std::vector<std::byte> take_bytes(Writer&& w) { return std::move(w).take(); }

core::Error unimplemented_sub(std::uint8_t opt, const char* side) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "FINDANONGAME %s sub-option 0x%02X", side,
                  static_cast<unsigned>(opt));
    return core::make_error(core::StatusCode::Unimplemented, buf);
}

#include "anongame/anongame_client.cpp"   // dec_search … enc_set_icon
#include "anongame/anongame_server.cpp"   // dec_search_reply … enc_tournament_reply

}  // namespace

// ----- public API --------------------------------------------------------

core::Result<AnonGameClient> parse_findanongame_request(
    const WarcraftGeneralRequest& env) {
    Reader r{view_of(env.data)};
    switch (env.sub_option) {
        case kAnonGameClientSearch: {
            auto v = dec_search(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientAtSearch: {
            auto v = dec_at_search(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientAtInviterSearch: {
            auto v = dec_at_inv(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientInfos: {
            auto v = dec_inforeq(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientCancel: {
            auto v = dec_client_cancel(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientProfile: {
            auto v = dec_profile_req(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientTournament: {
            auto v = dec_tournament_req(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientProfileClan: {
            auto v = dec_clan_profile(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientGetIcon: {
            auto v = dec_get_icon(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientSetIcon: {
            auto v = dec_set_icon(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        default:
            return core::fail(unimplemented_sub(env.sub_option, "client"));
    }
}

core::Result<AnonGameServer> parse_findanongame_reply(
    const WarcraftGeneralReply& env) {
    Reader r{view_of(env.data)};
    switch (env.sub_option) {
        case kAnonGameServerSearch: {
            auto v = dec_search_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameServerFound: {
            auto v = dec_found(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameServerCancel: {
            auto v = dec_server_cancel(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientInfos: {  // 0x02 = INFOREPLY (server)
            auto v = dec_inforeply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientProfile: {  // 0x04 = PROFILE2 (server)
            auto v = dec_profile_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientTournament: {  // 0x07 = TOURNAMENT (server)
            auto v = dec_tournament_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientGetIcon: {  // 0x09 = ICONREPLY (server)
            auto v = dec_icon_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientProfileClan: {  // 0x08 = CLAN_PROFILE (server)
            auto v = dec_clan_profile_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        default:
            return core::fail(unimplemented_sub(env.sub_option, "server"));
    }
}

WarcraftGeneralRequest serialize_findanongame_request(const AnonGameClient& a) {
    WarcraftGeneralRequest env;
    Writer w;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, AnonGameSearch>) {
            env.sub_option = kAnonGameClientSearch; enc_search(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameAtSearch>) {
            env.sub_option = kAnonGameClientAtSearch; enc_at_search(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameAtInviterSearch>) {
            env.sub_option = kAnonGameClientAtInviterSearch; enc_at_inv(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameInfoRequest>) {
            env.sub_option = kAnonGameClientInfos; enc_inforeq(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClientCancel>) {
            env.sub_option = kAnonGameClientCancel; enc_client_cancel(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameProfileRequest>) {
            env.sub_option = kAnonGameClientProfile; enc_profile_req(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameTournamentRequest>) {
            env.sub_option = kAnonGameClientTournament; enc_tournament_req(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClanProfileRequest>) {
            env.sub_option = kAnonGameClientProfileClan; enc_clan_profile(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameGetIcon>) {
            env.sub_option = kAnonGameClientGetIcon; enc_get_icon(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameSetIcon>) {
            env.sub_option = kAnonGameClientSetIcon; enc_set_icon(w, m);
        }
    }, a);
    env.data = take_bytes(std::move(w));
    return env;
}

WarcraftGeneralReply serialize_findanongame_reply(const AnonGameServer& a) {
    WarcraftGeneralReply env;
    Writer w;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, AnonGameSearchReply>) {
            env.sub_option = kAnonGameServerSearch; enc_search_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameFound>) {
            env.sub_option = kAnonGameServerFound; enc_found(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameServerCancel>) {
            env.sub_option = kAnonGameServerCancel; enc_server_cancel(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameInfoReply>) {
            env.sub_option = kAnonGameClientInfos; enc_inforeply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameProfileReply>) {
            env.sub_option = kAnonGameClientProfile; enc_profile_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameTournamentReply>) {
            env.sub_option = kAnonGameClientTournament; enc_tournament_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameIconReply>) {
            env.sub_option = kAnonGameClientGetIcon; enc_icon_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClanProfileReply>) {
            env.sub_option = kAnonGameClientProfileClan; enc_clan_profile_reply(w, m);
        }
    }, a);
    env.data = take_bytes(std::move(w));
    return env;
}

}  // namespace pvpgn::protocol::bnet
