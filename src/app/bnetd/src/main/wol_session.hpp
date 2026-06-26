// SPDX-License-Identifier: GPL-2.0-or-later
// main/wol_session.hpp — WOL session factory declaration.
//
// Creates a WolFsm-backed session for each new TCP connection on the WOL
// port (default 4000).
#pragma once

#include <memory>

#include "infra/net/tcp_session.hpp"
#include "app/bnetd/server_config.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/wol/wol_fsm.hpp"

namespace pvpgn::application::chat {
class JoinChannel;
class ListChannels;
class PostMessage;
}  // namespace pvpgn::application::chat

namespace pvpgn::infra::routing {
class MessageRouterImpl;
}  // namespace pvpgn::infra::routing

namespace pvpgn::app::bnetd {

/// Wire up a WolFsm for the given TCP session and start it.
/// @param session_id  Unique cross-session identity for this connection.
/// @param router      Cross-session message router. The session's egress is
///   registered under @p session_id so channel chat can be delivered to it,
///   and unregistered on close. May be null (test/standalone mode).
/// @param auth  Native Westwood Online auth collaborators (CVERS/APGAR flow).
///              When incomplete the FSM falls back to the legacy IRC path.
/// @param list_channels/join_channel/post_message  Chat use-cases for the
///   post-login WOL lobby (LIST / JOIN / PRIVMSG); may be null.
void make_wol_session(
    std::shared_ptr<infra::net::TcpSession> tcp,
    const ServerConfig&                      cfg,
    domain::SessionId                        session_id,
    std::shared_ptr<infra::routing::MessageRouterImpl> router,
    protocol::wol::WolAuthDeps               auth,
    std::shared_ptr<application::chat::ListChannels> list_channels,
    std::shared_ptr<application::chat::JoinChannel>   join_channel,
    std::shared_ptr<application::chat::PostMessage>   post_message);

} // namespace pvpgn::app::bnetd
