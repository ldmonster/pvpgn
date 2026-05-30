// SPDX-License-Identifier: GPL-2.0-or-later
// main/bnet_bnftp_dispatch.hpp — BNet+BNFTP shared-port dispatch factory.
//
// Port 6112 is shared by BNet (first byte 0xFF) and BNFTP (first byte 0x01).
// BnetBnftpDispatchFactory peeks at the first byte from each new TCP
// connection and routes it to the appropriate FSM (BnetFsm or BnftpFsm).
#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "core/bytes.hpp"
#include "domain/session_id.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/session_context_impl.hpp"
#include "protocol/bnet/use_case_context.hpp"
#include "protocol/file/bnftp_fsm.hpp"
#include "infra/net/tcp_session.hpp"
#include "application/auth/logout_request.hpp"

#include "app/bnetd/bnet_connection_adapter.hpp"
#include "app/bnetd/bnftp_tcp_session.hpp"
#include "app/bnetd/logging_connection_context.hpp"
#include "app/bnetd/lua_connection_context.hpp"
#include "app/bnetd/server_config.hpp"
#include "app/bnetd/session_manager.hpp"
#include "app/bnetd/tcp_session.hpp"
#include "services/bnetd/bnetd_service.hpp"

#include "main/bnet_framer.hpp"
#include "main/tcp_connection_context.hpp"

namespace pvpgn::app::bnetd {

// Global Lua runtime — defined in main.cpp, shared across all sessions.
extern infra::lua::LuaRuntime g_lua_runtime;

// Session-ID generator — defined in main.cpp.
[[nodiscard]] domain::SessionId next_session_id() noexcept;

/// Callable factory passed to TcpListener for port 6112.
/// On each new connection it buffers the first byte, then routes:
///   0xFF → BnetFsm (BNCS binary protocol)
///   other → BnftpFsm (file-transfer protocol)
class BnetBnftpDispatchFactory {
public:
    BnetBnftpDispatchFactory(const ServerConfig&                       cfg,
                              SessionManager&                           session_mgr,
                              const protocol::bnet::BnetUseCaseContext& use_cases,
                              services::bnetd::BnetdService&            bnetd_svc)
        : cfg_(cfg), session_mgr_(session_mgr), use_cases_(use_cases)
        , bnetd_svc_(bnetd_svc) {}

    void operator()(std::shared_ptr<infra::net::TcpSession> tcp);

private:
    const ServerConfig&                       cfg_;
    SessionManager&                           session_mgr_;
    protocol::bnet::BnetUseCaseContext        use_cases_;
    services::bnetd::BnetdService&            bnetd_svc_;
};

} // namespace pvpgn::app::bnetd
