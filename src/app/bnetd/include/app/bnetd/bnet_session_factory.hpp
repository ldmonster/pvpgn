// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_session_factory.hpp
/// SessionFactory for Battle.net protocol.
/// Creates TcpSession + BnetSessionContextImpl + BnetFsm wired together.
/// Handles registration/unregistration with MessageRouter and SessionRegistry.
/// Injects use-case context for domain operation dispatch.
///
/// R284: The factory now also instantiates a `BnetConnectionAdapter` per
/// session, wiring `ConnectionFsm` (domain layer) into the session.  An
/// optional `LoginUserNls*` enables the NLS (WAR3/W3XP) auth path; pass
/// `nullptr` to fall back to OLS-only mode.

#include <atomic>
#include <cstdint>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "app/bnetd/bnet_connection_adapter.hpp"
#include "application/auth/login_user_nls.hpp"

namespace pvpgn::application::auth { class LoginUser; }
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "infra/routing/message_router.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/session_context_impl.hpp"
#include "protocol/bnet/use_case_context.hpp"

namespace pvpgn::app::bnetd {

/// Forward declare MessageRouterImpl.
class MessageRouterImpl;

/// Factory that creates a complete BNet session from an accepted socket.
/// Wires: TcpSession → BnetSessionContextImpl → BnetFsm
///                   → BnetConnectionAdapter → ConnectionFsm → MessageRouter.
class BnetSessionFactory {
public:
    /// Create a factory with the given dependencies.
    ///
    /// @param router      Message router (weak ref); must outlive the factory.
    /// @param registry    Session registry (weak ref); must outlive the factory.
    /// @param use_cases   Use-case context for BnetFsm handlers.
    /// @param login_ols   Optional OLS use-case for STAR/SEXP/D2DV/D2XP clients.
    ///                    Pass nullptr to disable OLS credential checking.
    ///                    Non-owning pointer; must outlive the factory.
    /// @param login_nls   Optional NLS use-case for WAR3/W3XP clients.
    ///                    Pass nullptr to disable NLS (OLS-only mode).
    ///                    Non-owning pointer; must outlive the factory.
    BnetSessionFactory(
        std::shared_ptr<infra::routing::MessageRouterImpl>    router,
        std::shared_ptr<domain::identity::ISessionRegistry> registry,
        const protocol::bnet::BnetUseCaseContext&             use_cases,
        application::auth::LoginUser*                         login_ols = nullptr,
        application::auth::LoginUserNls*                      login_nls = nullptr)
        : router_(router)
        , registry_(registry)
        , use_cases_(use_cases)
        , login_ols_(login_ols)
        , login_nls_(login_nls) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Creates and starts a complete BNet session.
    ///
    /// Wiring per session (R284):
    ///   TcpSession
    ///     └─ TcpSessionEgress (IConnectionEgress)
    ///         └─ BnetSessionContextImpl (ISessionContext)
    ///             └─ BnetFsm
    ///   BnetConnectionAdapter (IConnectionContext)
    ///     └─ ConnectionFsm  ← NLS use-case injected if login_nls_ != nullptr
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId (monotonically incrementing)
        const auto raw_id = next_session_id_.fetch_add(1);
        domain::SessionId session_id{raw_id};

        // Create egress adapter (TcpSession → IConnectionEgress)
        auto egress = std::make_shared<TcpSessionEgress>(tcp_session);

        // Create BNet session context
        auto context = std::make_shared<protocol::bnet::BnetSessionContextImpl>(
            session_id, egress);

        // Create BNet FSM with use-case context
        auto fsm = std::make_shared<protocol::bnet::BnetFsm>(context, use_cases_);

        // R284/R293: Create BnetConnectionAdapter (owns ConnectionFsm).
        // Choose the richest constructor available based on injected use-cases.
        std::shared_ptr<app::bnetd::BnetConnectionAdapter> conn_adapter;
        if (login_ols_ && login_nls_) {
            conn_adapter = std::make_shared<app::bnetd::BnetConnectionAdapter>(
                *context, *login_ols_, *login_nls_,
                static_cast<std::uint32_t>(raw_id));
        } else if (login_nls_) {
            conn_adapter = std::make_shared<app::bnetd::BnetConnectionAdapter>(
                *context, *login_nls_, static_cast<std::uint32_t>(raw_id));
        } else {
            conn_adapter = std::make_shared<app::bnetd::BnetConnectionAdapter>(
                *context, static_cast<std::uint32_t>(raw_id));
        }

        // Register with router
        if (auto router_ptr = router_.lock()) {
            router_ptr->register_session(session_id, egress);
        }

        // Set up TCP session callbacks
        tcp_session->set_on_bytes(
            [fsm, conn_adapter](core::ByteView bytes) {
                // Parse incoming bytes and feed to protocol FSM.
                // The domain adapter is captured so it stays alive for the
                // session lifetime; dispatch_to_domain is called after the
                // protocol FSM has processed the packet.
                on_tcp_bytes(fsm, conn_adapter, bytes);
            });

        tcp_session->set_on_close(
            [this, session_id](const boost::system::error_code& /*ec*/) {
                // Unregister from router and registry
                if (auto router_ptr = router_.lock()) {
                    router_ptr->unregister_session(session_id);
                }
                if (auto registry_ptr = registry_.lock()) {
                    registry_ptr->detach(session_id);
                }
            });

        // Start reading
        tcp_session->start();
    }

private:
    /// Adapter: wrap TcpSession in IConnectionEgress interface.
    class TcpSessionEgress : public domain::connection::IConnectionEgress {
    public:
        explicit TcpSessionEgress(std::shared_ptr<infra::net::TcpSession> session)
            : session_(session) {}

        void send(std::vector<std::byte> bytes) override {
            if (auto s = session_.lock()) {
                s->send(std::move(bytes));
            }
        }

        void close() override {
            if (auto s = session_.lock()) {
                s->close();
            }
        }

    private:
        std::weak_ptr<infra::net::TcpSession> session_;
    };

    /// Process incoming bytes: decode and feed to protocol FSM.
    /// The domain adapter is passed so it can be forwarded to
    /// dispatch_to_domain() once the protocol FSM has processed the packet.
    static void on_tcp_bytes(
        std::shared_ptr<protocol::bnet::BnetFsm>          fsm,
        std::shared_ptr<app::bnetd::BnetConnectionAdapter> adapter,
        core::ByteView                                     bytes) {
        if (!fsm) return;

        // TODO: In Phase 3, implement stateful decoding that handles
        // incomplete packets, multiple messages per recv, etc.
        // For now, this is a placeholder.
        (void)adapter;
        (void)bytes;
    }

    std::weak_ptr<infra::routing::MessageRouterImpl>      router_;
    std::weak_ptr<domain::identity::ISessionRegistry>   registry_;
    protocol::bnet::BnetUseCaseContext                    use_cases_;
    /// Non-owning pointer to the OLS use-case; nullptr → no OLS credential check.
    application::auth::LoginUser*                         login_ols_;
    /// Non-owning pointer to the NLS use-case; nullptr → OLS-only mode.
    application::auth::LoginUserNls*                      login_nls_;
    static std::atomic<std::uint64_t>                     next_session_id_;
};

}  // namespace pvpgn::app::bnetd
