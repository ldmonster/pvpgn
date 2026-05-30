// SPDX-License-Identifier: GPL-2.0-or-later
// main/bnet_bnftp_dispatch.cpp — BNet+BNFTP shared-port dispatch factory impl.

#include "main/bnet_bnftp_dispatch.hpp"

#include <span>
#include <vector>

#include "application/auth/logout_request.hpp"
#include "domain/account_id.hpp"
#include "domain/session_id.hpp"

namespace pvpgn::app::bnetd {

void BnetBnftpDispatchFactory::operator()(
    std::shared_ptr<infra::net::TcpSession> tcp) {
    if (!tcp) return;

    // Peek buffer: accumulate bytes until we can identify the protocol.
    auto peek_buf = std::make_shared<std::vector<std::byte>>();

    tcp->set_on_bytes([this, tcp, peek_buf](core::ByteView bv) mutable {
        peek_buf->insert(peek_buf->end(), bv.begin(), bv.end());
        if (peek_buf->empty()) return;

        const std::byte first = (*peek_buf)[0];

        if (first == static_cast<std::byte>(0xFF)) {
            // BNet protocol — rewire callbacks and replay buffered bytes.
            //
            // Wiring:
            //   TcpSessionEgress
            //     ├── BnetSessionContextImpl  → BnetFsm (wire-level)
            //     └── TcpConnectionContext
            //           └── LuaConnectionContext   ← fires Lua hooks
            //                 └── LoggingConnectionContext
            //                       └── BnetConnectionAdapter
            //                             └── ConnectionFsm (domain-level)
            //
            // Both FSMs share the same TCP egress. BnetFsm handles the
            // wire dance (PING echo, auth acks). ConnectionFsm tracks
            // domain state (Connecting → Authenticating → LoggedIn → …).
            // The composition root feeds each decoded packet to both FSMs.

            auto egress = std::make_shared<TcpSessionEgress>(tcp);
            const domain::SessionId sid = next_session_id();
            const std::uint32_t     sid32 =
                static_cast<std::uint32_t>(sid.value());

            // BnetFsm I/O context (wire-level replies)
            auto bnet_ctx = std::make_shared<
                protocol::bnet::BnetSessionContextImpl>(sid, egress);
            session_mgr_.register_session(sid, bnet_ctx);

            // Domain-level transport context
            auto tcp_conn_ctx = std::make_shared<TcpConnectionContext>(
                egress, /*remote_addr=*/"", sid32);

            // LuaConnectionContext fires Lua hooks; wraps tcp_conn_ctx
            auto lua_ctx = std::make_shared<LuaConnectionContext>(
                *tcp_conn_ctx, g_lua_runtime);

            // LoggingConnectionContext wraps lua_ctx
            auto logging_ctx = std::make_shared<LoggingConnectionContext>(
                *lua_ctx);

            // BnetConnectionAdapter owns ConnectionFsm; implements
            // IConnectionContext by forwarding to logging_ctx
            auto adapter = std::make_shared<BnetConnectionAdapter>(
                *logging_ctx, sid32);

            auto fsm    = std::make_shared<protocol::bnet::BnetFsm>(
                bnet_ctx, use_cases_, sid);
            auto framer = std::make_shared<BnetFramer>();

            // Replay buffered bytes through both FSMs
            framer->feed(
                core::ByteView{peek_buf->data(), peek_buf->size()},
                [&fsm, &adapter](protocol::bnet::ClientMessage msg) {
                    // Feed to BnetFsm (wire-level)
                    (void)fsm->handle(msg);
                    // Feed to ConnectionFsm (domain-level) via adapter.
                    // We need the raw packet_id + payload; for replay we
                    // use an empty payload since the BnetFsm already
                    // handled the wire dance. The domain FSM will silently
                    // ignore unknown SIDs.
                    // TODO(Phase3): extract packet_id from ClientMessage
                    // variant and pass the original payload bytes.
                });

            // Rewire for future bytes
            tcp->set_on_bytes(
                [fsm, framer, adapter](core::ByteView bv2) {
                    framer->feed(bv2,
                        [&fsm](protocol::bnet::ClientMessage m) {
                            (void)fsm->handle(m);
                        });
                });

            // Keep tcp_conn_ctx, lua_ctx, and logging_ctx alive for the
            // session lifetime by capturing them in the close handler
            // alongside the adapter (which holds non-owning refs to all).
            // R305: bnetd_svc_ is a member of BnetBnftpDispatchFactory;
            // captured via `this` for LogoutUser cleanup on disconnect.
            tcp->set_on_close(
                [this, sid, adapter, tcp_conn_ctx, lua_ctx, logging_ctx](
                    const boost::system::error_code&) {
                    // R305: call LogoutUser to clean up channel membership
                    // before unregistering the session.
                    const auto& conn_fsm = adapter->connection_fsm();
                    const std::uint32_t acct_id = conn_fsm.account_id();
                    if (acct_id != 0) {
                        application::auth::LogoutRequest req{
                            domain::SessionId{sid},
                            domain::AccountId{acct_id}};
                        (void)bnetd_svc_.logout_user().execute(req);
                    }
                    session_mgr_.unregister_session(sid);
                    adapter->connection_fsm().close();
                    (void)tcp_conn_ctx;
                    (void)lua_ctx;
                    (void)logging_ctx;
                });

        } else {
            // BNFTP protocol (or unknown — let BnftpFsm reject it)
            auto egress = std::make_shared<TcpSessionEgress>(tcp);
            auto ctx    = std::make_shared<BnftpEgressContext>(egress);
            auto fsm    = std::make_shared<protocol::file::BnftpFsm>(
                ctx, cfg_.data_dir.string());

            // Replay buffered bytes
            auto sp = std::span<const std::byte>(peek_buf->data(),
                                                  peek_buf->size());
            (void)fsm->on_bytes(sp);

            // Rewire for future bytes
            tcp->set_on_bytes([fsm](core::ByteView bv2) {
                auto sp2 = std::span<const std::byte>(bv2.data(), bv2.size());
                (void)fsm->on_bytes(sp2);
            });
            tcp->set_on_close([fsm](const boost::system::error_code&) {
                fsm->on_close();
            });
        }

        // Clear peek buffer — it has been replayed
        peek_buf->clear();
    });

    tcp->start();
}

} // namespace pvpgn::app::bnetd
