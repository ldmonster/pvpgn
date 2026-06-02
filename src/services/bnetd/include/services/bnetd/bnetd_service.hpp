// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_service.hpp
/// Composition root for the bnetd server.
///
/// `BnetdService` owns the dependency graph for the bnetd server and
/// provides a clean `run()` / `stop()` lifecycle interface.  It is
/// constructed by `main()` after CLI parsing and infrastructure adapter
/// creation; `main()` itself becomes a thin bootstrap.
///
/// Dependency injection (R285 / R304)
/// ------------------------------------
/// The constructor accepts the primary port interfaces:
///   - `IUnitOfWorkFactory&`   — repository factory (in-memory, SQL, …)
///   - `IEventLoop&`           — event loop (Asio-backed in production)
///   - `INlsCredentialStore&`  — NLS (SRP-6a) credential lookup for WAR3/W3XP
///   - `IChannelRepository&`   — persistent channel store (shared across sessions)
///   - `IAccountRepository&`   — persistent account store (shared across sessions)
///   - `ISessionRegistry&`     — session ↔ account mapping (shared across sessions)
///
/// All injected references are non-owning; the caller (main) owns the
/// concrete adapter objects and must ensure they outlive `BnetdService`.
///
/// `BnetdService` OWNS:
///   - `LoginUserNls`   — NLS authentication use-case (stateless)
///   - `JoinChannel`    — chat use-case
///   - `PostMessage`    — chat use-case
///   - `LeaveChannel`   — chat use-case
///   - `ListChannels`   — chat use-case
///
/// R304: At construction, `BnetdService` seeds `IChannelRepository` with
/// the default permanent channels via `ChannelConfigLoader::defaults()`.

#include <memory>

#include "application/auth/login_user_nls.hpp"
#include "application/auth/logout_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "domain/identity/ports.hpp"
#include "domain/chat/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ports/event_loop.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "application/persistence/unit_of_work_factory.hpp"
#include "protocol/bnet/use_case_context.hpp"

namespace pvpgn::infra::crypto {
class NlsCryptoAdapter;
}

namespace pvpgn::services::bnetd {

/// Composition root for the bnetd server.
///
/// Non-copyable, non-movable — owns internal state that must not be
/// transferred after construction.
class BnetdService {
public:
    /// Construct the service and wire all owned use-cases.
    ///
    /// @param uow_factory    Repository factory; must outlive this object.
    /// @param event_loop     Event loop implementation; must outlive this object.
    /// @param nls_store      NLS credential store for WAR3/W3XP auth;
    ///                       must outlive this object.
    /// @param channel_repo   Persistent channel repository shared across sessions;
    ///                       must outlive this object.
    /// @param account_repo   Persistent account repository shared across sessions;
    ///                       must outlive this object.
    /// @param session_reg    Session registry shared across sessions;
    ///                       must outlive this object.
    /// @param game_repo      Persistent game repository shared across sessions;
    ///                       must outlive this object.
    /// @param event_bus      Event bus for domain events; must outlive this object.
    BnetdService(application::ports::IUnitOfWorkFactory&      uow_factory,
                 application::ports::IEventLoop&              event_loop,
                 application::auth::INlsCredentialStore&      nls_store,
                 domain::chat::IChannelRepository&      channel_repo,
                 domain::identity::IAccountRepository&      account_repo,
                 domain::identity::ISessionRegistry&        session_reg,
                 domain::gameplay::IGameRepository&         game_repo,
                 application::ports::IEventBus&               event_bus);

    ~BnetdService();

    // Non-copyable, non-movable
    BnetdService(const BnetdService&)            = delete;
    BnetdService& operator=(const BnetdService&) = delete;
    BnetdService(BnetdService&&)                 = delete;
    BnetdService& operator=(BnetdService&&)      = delete;

    /// Start the event loop and block until stop() is called or a fatal
    /// error occurs.  Delegates to IEventLoop::run().
    void run();

    /// Signal the event loop to stop.  Safe to call from any thread.
    /// Delegates to IEventLoop::stop().
    void stop() noexcept;

    /// Access the owned NLS use-case (for wiring into BnetSessionFactory).
    [[nodiscard]] application::auth::LoginUserNls& login_user_nls() noexcept {
        return *login_user_nls_;
    }

    /// Access the owned JoinChannel use-case (for wiring into BnetSessionFactory).
    [[nodiscard]] application::chat::JoinChannel& join_channel() noexcept {
        return *join_channel_;
    }

    /// Access the owned PostMessage use-case (for wiring into BnetSessionFactory).
    [[nodiscard]] application::chat::PostMessage& post_message() noexcept {
        return *post_message_;
    }

    /// Access the owned LeaveChannel use-case (for wiring into BnetSessionFactory).
    [[nodiscard]] application::chat::LeaveChannel& leave_channel() noexcept {
        return *leave_channel_;
    }

    /// Access the owned ListChannels use-case (for wiring into BnetSessionFactory).
    [[nodiscard]] application::chat::ListChannels& list_channels() noexcept {
        return *list_channels_;
    }

    /// Access the owned LogoutUser use-case (for wiring into disconnect handlers).
    [[nodiscard]] application::auth::LogoutUser& logout_user() noexcept {
        return *logout_user_;
    }

    /// Build a BnetUseCaseContext that wraps the owned chat use-cases with
    /// no-op shared_ptr deleters so they can be passed to BnetSessionFactory
    /// without transferring ownership.
    ///
    /// Only the four chat use-cases are populated here; other fields
    /// (login_user, game use-cases, etc.) remain null and must be filled in
    /// by the caller (main.cpp) if needed.
    [[nodiscard]] protocol::bnet::BnetUseCaseContext make_use_case_context() noexcept;

private:
    application::ports::IUnitOfWorkFactory& uow_factory_;
    application::ports::IEventLoop&         event_loop_;
    domain::chat::IChannelRepository& channel_repo_;
    domain::identity::IAccountRepository& account_repo_;
    domain::identity::ISessionRegistry&   session_reg_;
    domain::gameplay::IGameRepository&    game_repo_;
    application::ports::IEventBus&          event_bus_;

    /// Owned NLS crypto adapter (concrete `INlsCryptoService`). Declared
    /// before `login_user_nls_` so it is constructed first and outlives
    /// the use-case that holds a reference to it.
    std::unique_ptr<infra::crypto::NlsCryptoAdapter> nls_crypto_;

    /// Owned NLS authentication use-case (stateless; safe to share across
    /// sessions via non-owning pointer/reference).
    std::unique_ptr<application::auth::LoginUserNls> login_user_nls_;

    /// Owned chat use-cases (stateless; safe to share across sessions).
    std::unique_ptr<application::chat::JoinChannel>  join_channel_;
    std::unique_ptr<application::chat::PostMessage>  post_message_;
    std::unique_ptr<application::chat::LeaveChannel> leave_channel_;
    std::unique_ptr<application::chat::ListChannels> list_channels_;

    /// Owned logout use-case (R305: wired with LeaveChannel for channel cleanup).
    std::unique_ptr<application::auth::LogoutUser>   logout_user_;
};

}  // namespace pvpgn::services::bnetd
