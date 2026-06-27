// SPDX-License-Identifier: GPL-2.0-or-later

/// @file bnetd_service.cpp
/// Implementation of BnetdService — composition root for the bnetd server.
///
/// Auth wiring
/// -----------
/// The constructor accepts `INlsCredentialStore&` and constructs the
/// owned `LoginUserNls` use-case from it.  `LoginUserNls` is stateless so
/// a single instance is shared (by non-owning reference) across all sessions
/// via `BnetSessionFactory`.
///
/// Chat wiring
/// -----------
/// The constructor now also accepts `IChannelRepository&`, `IAccountRepository&`,
/// and `ISessionRegistry&` and constructs the owned chat use-cases:
///   - JoinChannel   (channel_repo, account_repo, session_reg)
///   - PostMessage   (channel_repo, session_reg)
///   - LeaveChannel  (channel_repo, session_reg)
///   - ListChannels  (shared_ptr wrapping channel_repo — no-op deleter adapter)
///
/// After constructing the use-cases, the constructor seeds `IChannelRepository`
/// with the default permanent channels.  Seeding is skipped when the repository
/// already contains channels (e.g. after a warm restart against a persistent
/// SQL backend).
///
/// The actual wiring of protocol FSMs, TCP listeners, session managers, and
/// use-case contexts still lives in `app/bnetd/src/main.cpp` (the legacy
/// composition root).

#include "services/bnetd/bnetd_service.hpp"
#include "application/persistence/unit_of_work_factory.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/shared/ports/event_loop.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "application/auth/login_user_nls.hpp"
#include "infra/crypto/nls_crypto_adapter.hpp"
#include "application/auth/logout_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::services::bnetd {

// ---------------------------------------------------------------------------
// Non-owning shared_ptr adapter
// ---------------------------------------------------------------------------
// `ListChannels` requires a `shared_ptr<IChannelRepository>` but
// `BnetdService` holds a non-owning reference.  We use a no-op deleter so
// the shared_ptr never actually deletes the repository.
namespace {

struct NoDelete {
    void operator()(domain::chat::IChannelRepository*) const noexcept {}
};

/// Default permanent channels seeded at startup.
/// Mirrors the most common entries from conf/channel.conf.in.
struct DefaultChannel {
    const char*   name;
    std::uint32_t max_members;  // 0 = unlimited
};

constexpr DefaultChannel kDefaultChannels[] = {
    {"The Void",        0},
    {"Starcraft USA-1", 0},
    {"Diablo II",       0},
    {"Warcraft 3",      0},
    {"Chat",            0},
};

}  // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BnetdService::BnetdService(
    application::ports::IUnitOfWorkFactory& uow_factory,
    application::ports::IEventLoop&         event_loop,
    application::auth::INlsCredentialStore& nls_store,
    domain::chat::IChannelRepository& channel_repo,
    domain::identity::IAccountRepository& account_repo,
    domain::identity::ISessionRegistry&   session_reg,
    domain::gameplay::IGameRepository&    game_repo,
    application::ports::IEventBus&          event_bus)
    : uow_factory_(uow_factory)
    , event_loop_(event_loop)
    , channel_repo_(channel_repo)
    , account_repo_(account_repo)
    , session_reg_(session_reg)
    , game_repo_(game_repo)
    , event_bus_(event_bus)
    // NLS crypto port (constructed before use-case that holds a ref to it)
    , nls_crypto_(std::make_unique<infra::crypto::NlsCryptoAdapter>())
    // Auth use-case
    , login_user_nls_(std::make_unique<application::auth::LoginUserNls>(
          nls_store, *nls_crypto_))
    // Chat use-cases
    , join_channel_(std::make_unique<application::chat::JoinChannel>(
          channel_repo_, account_repo_, session_reg_))
    , post_message_(std::make_unique<application::chat::PostMessage>(
          channel_repo_, session_reg_))
    , leave_channel_(std::make_unique<application::chat::LeaveChannel>(
          channel_repo_, session_reg_))
    , list_channels_(std::make_unique<application::chat::ListChannels>(
          std::shared_ptr<domain::chat::IChannelRepository>(
              &channel_repo_, NoDelete{})))
    // Logout use-case (wired with LeaveChannel for channel cleanup on disconnect)
    , logout_user_(std::make_unique<application::auth::LogoutUser>(
          session_reg_, channel_repo_, game_repo_, event_bus_,
          leave_channel_.get()))
{
    // Seed the channel repository with default permanent channels.
    // Only seed if the repository is currently empty to avoid duplicates
    // on restart (e.g. when backed by a persistent SQL store).
    if (channel_repo_.size() == 0) {
        std::uint32_t next_id = 1;
        for (const auto& def : kDefaultChannels) {
            domain::chat::ChannelPolicy policy;
            policy.flags.set(domain::chat::ChannelFlag::Permanent);
            policy.flags.set(domain::chat::ChannelFlag::AllowBots);
            // The original sets channel_flags_public whenever a channel is
            // permanent (channel.cpp channel_create), so the EID_CHANNEL flags
            // carry CF_PUBLIC (0x01) for these predefined channels.
            policy.flags.set(domain::chat::ChannelFlag::Public);
            // "The Void" is the legacy kicked/banned limbo channel: it exists
            // but the original server never advertises it in the channel list
            // (channel_flags_thevoid). Flag it so ListChannels hides it.
            if (std::string_view{def.name} == "The Void") {
                policy.flags.set(domain::chat::ChannelFlag::TheVoid);
            }
            policy.max_members = def.max_members;

            auto channel = domain::chat::Channel::create(
                domain::ChannelId{next_id++},
                std::string{def.name},
                policy);

            (void)channel_repo_.save(channel);
        }
    }
}

BnetdService::~BnetdService() = default;

void BnetdService::run() {
    // TODO: start TCP listeners before entering the event loop.
    event_loop_.run();
}

void BnetdService::stop() noexcept {
    event_loop_.stop();
}

// ---------------------------------------------------------------------------
// make_use_case_context
// ---------------------------------------------------------------------------
// Wraps the owned chat use-cases in shared_ptrs with no-op deleters so they
// can be passed to BnetSessionFactory / BnetFsm without transferring ownership.
// Other fields (login_user, game use-cases, etc.) are left null; the caller
// (main.cpp) fills them in as needed.

protocol::bnet::BnetUseCaseContext BnetdService::make_use_case_context() noexcept {
    protocol::bnet::BnetUseCaseContext ctx;

    ctx.join_channel  = std::shared_ptr<application::chat::JoinChannel>(
        join_channel_.get(),  [](application::chat::JoinChannel*)  noexcept {});
    ctx.post_message  = std::shared_ptr<application::chat::PostMessage>(
        post_message_.get(),  [](application::chat::PostMessage*)  noexcept {});
    ctx.leave_channel = std::shared_ptr<application::chat::LeaveChannel>(
        leave_channel_.get(), [](application::chat::LeaveChannel*) noexcept {});
    ctx.list_channels = std::shared_ptr<application::chat::ListChannels>(
        list_channels_.get(), [](application::chat::ListChannels*) noexcept {});
    // Channel reader for /who and /whois (the shared channel repository).
    ctx.channel_reader = std::shared_ptr<domain::chat::IChannelReader>(
        &channel_repo_, [](domain::chat::IChannelReader*) noexcept {});

    return ctx;
}

}  // namespace pvpgn::services::bnetd
