// SPDX-License-Identifier: GPL-2.0-or-later
// main/bnet_bnftp_dispatch.cpp — BNet+BNFTP shared-port dispatch factory impl.

#include "main/bnet_bnftp_dispatch.hpp"

#include <span>
#include <vector>

#include "application/auth/logout_user.hpp"
#include "application/init/init_conn_dispatch.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::app::bnetd {

void BnetBnftpDispatchFactory::operator()(
    std::shared_ptr<infra::net::TcpSession> tcp) {
    if (!tcp) return;

    // Peek buffer: accumulate bytes until we can identify the protocol.
    auto peek_buf = std::make_shared<std::vector<std::byte>>();

    tcp->set_on_bytes([this, tcp, peek_buf](core::ByteView bv) mutable {
        // Reentrancy guard: the protocol-selection logic below calls
        // `tcp->set_on_bytes(...)` to rewire future reads. That reassigns
        // `on_bytes_` — *this very std::function* — which destroys the
        // currently-executing lambda and frees its captures (`tcp`,
        // `peek_buf`). Any subsequent use of those captures (e.g.
        // `tcp->set_on_close`, `peek_buf->clear()`) would then be a
        // use-after-free → SIGSEGV. Hold stack-local owning copies so the
        // session and peek buffer outlive the rewire for the rest of this
        // call. NOTE this includes the implicit `this` capture: any lambda
        // we build *after* the rewire (e.g. the on_close handler below)
        // would otherwise capture `this` by re-reading it from the freed
        // peek-lambda closure → a dangling factory pointer. Snapshot it too.
        auto  session = tcp;
        auto  pbuf    = peek_buf;
        auto* self    = this;

        pbuf->insert(pbuf->end(), bv.begin(), bv.end());
        if (pbuf->empty()) return;

        const std::byte first = (*pbuf)[0];

        // A real Battle.net client opens with a single protocol-select octet:
        //   0x01 = CLIENT_INITCONN_CLASS_BNET  (the standard BNCS login stream)
        //   0x02 = CLIENT_INITCONN_CLASS_FILE  (BNFTP)
        // The original consumes this byte in handle_init before any packet
        // parsing. So a BNet connection is identified by EITHER a leading 0x01
        // init byte (real clients) OR a bare 0xFF packet marker (a client that
        // already stripped the init byte). Strip the 0x01 before replaying so
        // the framer sees the 0xFF packet stream cleanly.
        const bool   is_bnet   = (first == static_cast<std::byte>(0xFF)) ||
                                 (first == static_cast<std::byte>(0x01));
        const std::size_t bnet_skip =
            (first == static_cast<std::byte>(0x01)) ? 1u : 0u;

        if (is_bnet) {
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

            auto egress = std::make_shared<TcpSessionEgress>(session);
            const domain::SessionId sid = next_session_id();
            const std::uint32_t     sid32 =
                static_cast<std::uint32_t>(sid.value());

            // BnetFsm I/O context (wire-level replies)
            auto bnet_ctx = std::make_shared<
                protocol::bnet::BnetSessionContextImpl>(sid, egress);
            session_mgr_.register_session(sid, bnet_ctx);

            // Register this session's egress with the cross-session router so
            // chat broadcasts (EID_TALK / EID_JOIN / EID_LEAVE / whispers) from
            // OTHER clients are delivered here. Unregistered on close (below).
            if (router_) {
                router_->register_session(sid, egress);
            }

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

            // Replay buffered bytes through both FSMs (past any consumed
            // 0x01 BNET init byte).
            framer->feed(
                core::ByteView{pbuf->data() + bnet_skip, pbuf->size() - bnet_skip},
                [&fsm, &adapter](protocol::bnet::ClientMessage msg) {
                    // Feed to BnetFsm (wire-level)
                    (void)fsm->handle(msg);
                    // Feed to ConnectionFsm (domain-level) via adapter.
                    // We need the raw packet_id + payload; for replay we
                    // use an empty payload since the BnetFsm already
                    // handled the wire dance. The domain FSM will silently
                    // ignore unknown SIDs.
                    // TODO: extract packet_id from ClientMessage
                    // variant and pass the original payload bytes.
                });

            // Rewire for future bytes
            session->set_on_bytes(
                [fsm, framer, adapter, session](core::ByteView bv2) {
                    framer->feed(bv2,
                        [&fsm](protocol::bnet::ClientMessage m) {
                            (void)fsm->handle(m);
                        });
                    // An unrecoverably-corrupt header (declared size < 4)
                    // cannot be resynced; close the session, mirroring the
                    // original server destroying such connections.
                    if (framer->wants_close) session->close();
                });

            // Keep tcp_conn_ctx, lua_ctx, and logging_ctx alive for the
            // session lifetime by capturing them in the close handler
            // alongside the adapter (which holds non-owning refs to all).
            // bnetd_svc_ is a member of BnetBnftpDispatchFactory;
            // captured via `this` for LogoutUser cleanup on disconnect.
            session->set_on_close(
                [self, sid, fsm, adapter, tcp_conn_ctx, lua_ctx, logging_ctx](
                    const boost::system::error_code&) {
                    // A disconnect while in a channel is a channel part: let the
                    // BnetFsm notify the remaining members with EID_LEAVE before
                    // any membership teardown (must run while this session's
                    // peers are still routable and before LogoutUser strips the
                    // membership). Matches the original's conn_destroy ->
                    // channel part broadcast.
                    if (fsm) fsm->on_disconnect();

                    // Call LogoutUser to clean up channel membership
                    // before unregistering the session. Use `self` (the
                    // snapshotted factory) rather than `this`: see the
                    // reentrancy note at the top of the handler.
                    // The OLS/NLS login runs through the BnetFsm (it attaches the
                    // session to the registry), so its account id is authoritative.
                    // Reading only the connection-level FSM here left acct_id == 0
                    // for OLS logins, so LogoutUser was skipped and the session was
                    // never detached — blocking re-login of the same account after
                    // a disconnect (single-session policy). Prefer the BnetFsm's
                    // account id, falling back to the connection FSM's.
                    const auto& conn_fsm = adapter->connection_fsm();
                    std::uint32_t acct_id =
                        fsm ? static_cast<std::uint32_t>(fsm->account_id().value())
                            : 0u;
                    if (acct_id == 0) acct_id = conn_fsm.account_id();
                    if (acct_id != 0) {
                        application::auth::LogoutRequest req{
                            domain::SessionId{sid},
                            domain::AccountId{acct_id}};
                        (void)self->bnetd_svc_.logout_user().execute(req);
                    }
                    self->session_mgr_.unregister_session(sid);
                    if (self->router_) {
                        self->router_->unregister_session(sid);
                    }
                    adapter->connection_fsm().close();
                    (void)tcp_conn_ctx;
                    (void)lua_ctx;
                    (void)logging_ctx;
                });

        } else {
            // Non-BNet stream (first byte was not 0xFF/0x01). Mirror the
            // original's handle_init_packet (src/bnetd/handle_init.cpp),
            // which switches on this single connection-class octet and
            // *destroys* the connection (returns -1) for the ENC (0x04),
            // LOCALMACHINE (0x98), D2CS_BNETD-from-a-non-realm-IP (0x65),
            // and every unknown/default byte. Previously v3 routed every
            // such byte into BnftpFsm, which just waited for more bytes —
            // silently holding unsupported/unknown init classes OPEN (a
            // remote half-open-connection exhaustion surface). Reuse the
            // already-unit-tested decision logic so the live dispatch and
            // the pure decision function agree.
            using application::init::dispatch_init_conn;
            using application::init::InitConnRequest;
            using application::init::InitDecision;
            const InitDecision decision =
                dispatch_init_conn(
                    InitConnRequest{
                        /*cclass=*/static_cast<std::uint8_t>(first),
                        /*conn_count=*/0,
                        /*max_conns_per_ip=*/0,
                        // v3 has no realmlist, so a D2CS_BNETD (0x65) link
                        // from any IP is denied — matching the original
                        // closing such links from a non-realm IP.
                        /*d2cs_ip_allowed=*/false,
                    })
                    .decision;

            // kRejected (ENC/LOCALMACHINE/unknown), kD2csIpDenied (0x65
            // with no realmlist), kRateLimited, and kD2csBnetd (no v3
            // realm-link implementation) all have no live handler: close
            // immediately, matching the original's conn_destroy. Only
            // kFile (0x02 → BNFTP) is accepted below; kBot (0x03) and
            // kTelnet (0x0d) are unimplemented in v3 but the original
            // keeps those sockets OPEN (it emits a prompt), so they fall
            // through and BnftpFsm leaves them waiting — preserving the
            // open/closed observable rather than introducing a new
            // close divergence.
            if (decision == InitDecision::kRejected ||
                decision == InitDecision::kD2csIpDenied ||
                decision == InitDecision::kRateLimited ||
                decision == InitDecision::kD2csBnetd) {
                session->close();
                pbuf->clear();
                return;
            }

            // BNFTP protocol (or an accepted-but-unimplemented bot/telnet
            // class — let BnftpFsm wait, matching the original keeping the
            // socket open).
            auto egress = std::make_shared<TcpSessionEgress>(session);
            auto ctx    = std::make_shared<BnftpEgressContext>(egress);
            auto fsm    = std::make_shared<protocol::file::BnftpFsm>(
                ctx, cfg_.data_dir.string());

            // A BNFTP connection opens with a single init-class octet
            // (CLIENT_INITCONN_CLASS_FILE == 0x02) that selects the file
            // protocol; the CLIENT_FILE_REQ packet (which BnftpFsm parses,
            // starting with its {size,type} header) follows immediately
            // after. The original server consumes this byte in its init
            // handler (handle_init.cpp: conn_set_class(c, conn_class_file))
            // and never forwards it to the file-request parser. We must do
            // the same here: strip the leading 0x02 before replaying, or
            // BnftpFsm would misread it as the low byte of `size`, shifting
            // the whole header by one and silently closing the connection.
            //
            // The init byte only appears once, at the very start of the
            // stream, so only this initial replay needs the skip; the
            // rewired on_bytes handler below sees post-init bytes verbatim.
            std::size_t skip = 0;
            if (first == static_cast<std::byte>(0x02)) {
                skip = 1;  // CLIENT_INITCONN_CLASS_FILE
            }

            // Replay buffered bytes (past the consumed init byte).
            auto sp = std::span<const std::byte>(pbuf->data() + skip,
                                                  pbuf->size() - skip);
            (void)fsm->on_bytes(sp);

            // Rewire for future bytes
            session->set_on_bytes([fsm](core::ByteView bv2) {
                auto sp2 = std::span<const std::byte>(bv2.data(), bv2.size());
                (void)fsm->on_bytes(sp2);
            });
            session->set_on_close([fsm](const boost::system::error_code&) {
                fsm->on_close();
            });
        }

        // Clear peek buffer — it has been replayed. (Use the stack-local
        // owning copy: the capture may have been freed by the rewire above.)
        pbuf->clear();
    });

    tcp->start();
}

} // namespace pvpgn::app::bnetd
