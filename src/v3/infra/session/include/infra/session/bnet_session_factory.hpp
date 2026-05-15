// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_session_factory.hpp
/// SessionFactory for Battle.net protocol.
/// Creates TcpSession + BnetSessionContextImpl + BnetFsm wired together.
/// Handles registration/unregistration with MessageRouter and SessionRegistry.
/// Injects use-case context for domain operation dispatch.

#include <atomic>
#include <cstdint>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "application/ports/message_router.hpp"
#include "application/ports/session_registry.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "infra/routing/message_router.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/session_context_impl.hpp"
#include "protocol/bnet/use_case_context.hpp"

namespace pvpgn::infra::session {

/// Forward declare MessageRouterImpl.
class MessageRouterImpl;

/// Factory that creates a complete BNet session from an accepted socket.
/// Wires: TcpSession → BnetSessionContextImpl → BnetFsm → MessageRouter.
class BnetSessionFactory {
public:
    /// Create a factory with the given dependencies.
    /// Router must be the concrete MessageRouterImpl type.
    /// use_cases contains all use-case dependencies for FSM handlers.
    BnetSessionFactory(
        std::shared_ptr<infra::routing::MessageRouterImpl> router,
        std::shared_ptr<application::ports::ISessionRegistry> registry,
        const protocol::bnet::BnetUseCaseContext& use_cases)
        : router_(router), registry_(registry), use_cases_(use_cases) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Creates and starts a complete BNet session.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId (monotonically incrementing)
        domain::SessionId session_id{next_session_id_.fetch_add(1)};

        // Create egress adapter (TcpSession → IConnectionEgress)
        auto egress = std::make_shared<TcpSessionEgress>(tcp_session);

        // Create BNet session context
        auto context = std::make_shared<protocol::bnet::BnetSessionContextImpl>(
            session_id, egress);

        // Create BNet FSM with use-case context
        auto fsm = std::make_shared<protocol::bnet::BnetFsm>(context, use_cases_);

        // Register with router
        if (auto router_ptr = router_.lock()) {
            router_ptr->register_session(session_id, egress);
        }

        // Set up TCP session callbacks
        tcp_session->set_on_bytes([fsm, context](core::ByteView bytes) {
            // Parse incoming bytes and feed to FSM
            on_tcp_bytes(fsm, bytes);
        });

        tcp_session->set_on_close([this, session_id, tcp_session](
                                      const boost::system::error_code& ec) {
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
    class TcpSessionEgress : public application::ports::IConnectionEgress {
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

    /// Process incoming bytes: decode and feed to FSM.
    static void on_tcp_bytes(
        std::shared_ptr<protocol::bnet::BnetFsm> fsm,
        core::ByteView bytes) {
        if (!fsm) return;

        // TODO: In Phase 3, implement stateful decoding that handles
        // incomplete packets, multiple messages per recv, etc.
        // For now, this is a placeholder.
        (void)bytes;
    }

    std::weak_ptr<infra::routing::MessageRouterImpl> router_;
    std::weak_ptr<application::ports::ISessionRegistry> registry_;
    protocol::bnet::BnetUseCaseContext use_cases_;
    static std::atomic<std::uint64_t> next_session_id_;
};

}  // namespace pvpgn::infra::session
