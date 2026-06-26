// SPDX-License-Identifier: GPL-2.0-or-later
// main/wol_session.cpp — WOL session factory implementation.

#include "main/wol_session.hpp"

#include <memory>
#include <span>

#include <boost/system/error_code.hpp>

#include "core/bytes.hpp"
#include "infra/routing/message_router.hpp"
#include "protocol/wol/wol_fsm.hpp"
#include "app/bnetd/tcp_session.hpp"

namespace pvpgn::app::bnetd {

void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg,
    domain::SessionId                        session_id,
    std::shared_ptr<infra::routing::MessageRouterImpl> router,
    domain::chat::IChannelReader*            channel_reader,
    application::game::IWolGameStore*        wol_game_store,
    protocol::wol::WolAuthDeps               auth,
    std::shared_ptr<application::chat::ListChannels> list_channels,
    std::shared_ptr<application::chat::JoinChannel>   join_channel,
    std::shared_ptr<application::chat::PostMessage>   post_message,
    application::social::AddFriend*           add_friend,
    application::social::RemoveFriend*        remove_friend,
    application::social::ListFriends*         list_friends,
    domain::connection::IPeerAddressStore*   peer_store,
    application::game::IWolUserFlagsStore*    user_flags_store,
    std::shared_ptr<application::chat::LeaveChannel> leave_channel) {

    auto egress = std::make_shared<TcpSessionEgress>(tcp);
    auto ctx    = std::make_shared<WolEgressContext>(egress, cfg.server_name);
    auto fsm    = std::make_shared<protocol::wol::WolFsm>(
        ctx, auth, std::move(list_channels), std::move(join_channel),
        std::move(post_message));

    // Give the FSM its cross-session identity and register the egress with the
    // router so channel chat from other members is delivered to this client.
    // The router holds a weak_ptr to the egress; `egress` is kept alive by the
    // WolEgressContext owned by the FSM (captured in the callbacks below).
    fsm->set_routing(session_id, router.get(), channel_reader);
    fsm->set_game_store(wol_game_store);
    fsm->set_social(add_friend, remove_friend, list_friends);
    // Capture this connection's peer IP so USERIP/STARTG can report it. The
    // endpoint is available now (socket open); guard against a transport race.
    {
        std::string peer_ip;
        try {
            peer_ip = tcp->remote_endpoint().address().to_string();
        } catch (...) {
            peer_ip.clear();
        }
        fsm->set_peer(std::move(peer_ip), peer_store);
    }
    fsm->set_user_flags_store(user_flags_store);
    fsm->set_leave_channel(leave_channel.get());
    if (router) {
        router->register_session(session_id, egress);
    }

    tcp->set_on_bytes([fsm](core::ByteView bv) {
        auto sp = std::span<const std::byte>(bv.data(), bv.size());
        (void)fsm->on_bytes(sp);
    });

    tcp->set_on_close([fsm, router, session_id](const boost::system::error_code&) {
        fsm->on_close();
        if (router) {
            router->unregister_session(session_id);
        }
    });

    tcp->start();
}

} // namespace pvpgn::app::bnetd
