// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_connection_adapter.hpp
/// Adapter that bridges `BnetFsm` (protocol layer) and `ConnectionFsm`
/// (domain layer).
///
/// ## Architecture
///
/// The v3 stack has two FSM layers:
///
///   BnetFsm (protocol/bnet/)
///     - Parses raw BNCS bytes into typed messages
///     - Sends immediate wire-level replies (PING echo, auth acks)
///     - Calls `ISessionContext::send()` for outbound messages
///     - Calls `ISessionContext::close()` on fatal errors
///
///   ConnectionFsm (domain/connection/)
///     - Owns the session lifecycle (Connecting → … → Disconnecting)
///     - Dispatches raw BNCS packets (SID byte + payload) to handlers
///     - Calls `IConnectionContext::send_packet()` for outbound packets
///     - Calls `IConnectionContext::close()` on orderly shutdown
///
/// `BnetConnectionAdapter` sits between them:
///
///   TCP bytes
///     │
///     ▼
///   BnetFramer (in main.cpp)
///     │  decoded ClientMessage
///     ▼
///   BnetFsm::handle()
///     │  ISessionContext::send()  ──────────────────────────────▶ TCP egress
///     │  ISessionContext::close() ──────────────────────────────▶ TCP close
///     │
///     │  (BnetConnectionAdapter::dispatch_to_domain)
///     ▼
///   ConnectionFsm::dispatch(packet_id, payload)
///     │  IConnectionContext::send_packet() ─────────────────────▶ TCP egress
///     │  IConnectionContext::close()       ─────────────────────▶ TCP close
///     │  IConnectionContext::on_game_*()   ─────────────────────▶ domain callbacks
///     ▼
///   IConnectionContext (LoggingConnectionContext / production impl)
///
/// ## Dual-interface design
///
/// `BnetConnectionAdapter` implements two interfaces:
///
///   1. `IConnectionContext` — injected into `ConnectionFsm` so the domain
///      FSM can send packets and fire game-lifecycle callbacks.
///
///   2. Exposes `dispatch_to_domain(packet_id, payload)` — called by the
///      composition root after `BnetFsm::handle()` to feed the same packet
///      into `ConnectionFsm::dispatch()`.
///
/// The adapter does NOT implement `ISessionContext` (BnetFsm's I/O interface)
/// because the BnetFsm already has its own `BnetSessionContextImpl` for that.
/// The two FSMs share the same underlying TCP egress via `IConnectionContext`.
///
/// ## Ownership
///
///   BnetConnectionAdapter owns:
///     - `ConnectionFsm` (by value — non-movable, so stored via unique_ptr)
///     - `IConnectionContext` reference (non-owning — must outlive adapter)
///
/// ## C++20 conventions
///   - No exceptions; errors returned as `core::Status<>`
///   - `[[nodiscard]]` on all status-returning methods
///   - `-std=c++20 -Wall -Wextra -Werror` clean

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "application/auth/login_user_nls.hpp"
#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"
#include "domain/connection/connection_fsm.hpp"

namespace pvpgn::application::auth {
class LoginUser;
}  // namespace pvpgn::application::auth

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// BnetConnectionAdapter
// ---------------------------------------------------------------------------

/// Bridges `BnetFsm` (protocol layer) and `ConnectionFsm` (domain layer).
///
/// Implements `IConnectionContext` so it can be injected into `ConnectionFsm`.
/// Exposes `dispatch_to_domain()` so the composition root can feed decoded
/// BNCS packets into `ConnectionFsm::dispatch()` after `BnetFsm::handle()`.
///
/// Lifetime: the `IConnectionContext` reference passed to the constructor
/// MUST outlive this adapter.
class BnetConnectionAdapter final
    : public domain::connection::IConnectionContext {
public:
    /// Construct the adapter (OLS-only, no NLS use-case).
    ///
    /// @param ctx        Domain-level I/O context (send_packet / close /
    ///                   game-lifecycle callbacks). Non-owning reference;
    ///                   MUST outlive this adapter.
    /// @param session_id Opaque session identity for logging / registry.
    explicit BnetConnectionAdapter(
        domain::connection::IConnectionContext& ctx,
        std::uint32_t session_id = 0) noexcept;

    /// Construct the adapter with NLS support for WAR3/W3XP clients.
    ///
    /// @param ctx            Domain-level I/O context. Non-owning reference;
    ///                       MUST outlive this adapter.
    /// @param login_user_nls NLS authentication use-case. Non-owning reference;
    ///                       MUST outlive this adapter.
    /// @param session_id     Opaque session identity for logging / registry.
    BnetConnectionAdapter(
        domain::connection::IConnectionContext&  ctx,
        application::auth::LoginUserNls&         login_user_nls,
        std::uint32_t                            session_id = 0) noexcept;

    /// Construct the adapter with both OLS and NLS use-cases.
    ///
    /// @param ctx            Domain-level I/O context. Non-owning reference;
    ///                       MUST outlive this adapter.
    /// @param login_user_ols OLS authentication use-case. Non-owning reference;
    ///                       MUST outlive this adapter.
    /// @param login_user_nls NLS authentication use-case. Non-owning reference;
    ///                       MUST outlive this adapter.
    /// @param session_id     Opaque session identity for logging / registry.
    BnetConnectionAdapter(
        domain::connection::IConnectionContext&  ctx,
        application::auth::LoginUser&            login_user_ols,
        application::auth::LoginUserNls&         login_user_nls,
        std::uint32_t                            session_id = 0) noexcept;

    // Non-copyable, non-movable (holds a reference to ctx_).
    BnetConnectionAdapter(const BnetConnectionAdapter&)            = delete;
    BnetConnectionAdapter& operator=(const BnetConnectionAdapter&) = delete;
    BnetConnectionAdapter(BnetConnectionAdapter&&)                 = delete;
    BnetConnectionAdapter& operator=(BnetConnectionAdapter&&)      = delete;

    ~BnetConnectionAdapter() override = default;

    // -----------------------------------------------------------------------
    // Domain dispatch entry point
    // -----------------------------------------------------------------------

    /// Feed a decoded BNCS packet into `ConnectionFsm::dispatch()`.
    ///
    /// Call this from the composition root after `BnetFsm::handle()` returns
    /// for the same packet. The adapter forwards the raw SID byte + payload
    /// to `ConnectionFsm::dispatch()` so the domain FSM can track state.
    ///
    /// @param packet_id  The SID byte from the 4-byte BNCS header.
    /// @param payload    The packet body (bytes after the 4-byte header).
    /// @return           ok() on success; fail() on fatal protocol error.
    [[nodiscard]] core::Status<> dispatch_to_domain(
        std::uint8_t packet_id,
        std::span<const std::byte> payload);

    // -----------------------------------------------------------------------
    // Observers
    // -----------------------------------------------------------------------

    /// Access the underlying `ConnectionFsm` (read-only).
    [[nodiscard]] const domain::connection::ConnectionFsm& connection_fsm()
        const noexcept {
        return *fsm_;
    }

    /// Access the underlying `ConnectionFsm` (mutable — for testing).
    [[nodiscard]] domain::connection::ConnectionFsm& connection_fsm()
        noexcept {
        return *fsm_;
    }

    // -----------------------------------------------------------------------
    // IConnectionContext — forwarded to the injected ctx_
    // -----------------------------------------------------------------------

    /// Forward `send_packet` to the injected `IConnectionContext`.
    [[nodiscard]] core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) override;

    /// Forward `close` to the injected `IConnectionContext`.
    void close() override;

    /// Forward `get_remote_address` to the injected `IConnectionContext`.
    [[nodiscard]] std::string get_remote_address() const override;

    /// Forward `get_session_id` to the injected `IConnectionContext`.
    [[nodiscard]] std::uint32_t get_session_id() const override;

    /// Forward `on_game_created` to the injected `IConnectionContext`.
    void on_game_created(std::uint32_t game_id,
                         const domain::connection::GameInfo& info) override;

    /// Forward `on_game_joined` to the injected `IConnectionContext`.
    void on_game_joined(std::uint32_t game_id,
                        const domain::connection::GameInfo& info) override;

    /// Forward `on_game_left` to the injected `IConnectionContext`.
    void on_game_left(std::uint32_t game_id) override;

private:
    /// Injected domain-level I/O context (non-owning reference).
    domain::connection::IConnectionContext& ctx_;

    /// Owned `ConnectionFsm` instance.
    /// Stored via `unique_ptr` because `ConnectionFsm` is non-movable
    /// (it holds a reference to `ctx_` which is `*this`).
    std::unique_ptr<domain::connection::ConnectionFsm> fsm_;
};

}  // namespace pvpgn::app::bnetd
