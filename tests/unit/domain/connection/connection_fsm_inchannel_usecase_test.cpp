// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel_usecase_test.cpp
/// Covers the *injected-use-case* paths of connection_fsm_inchannel.cpp — the
/// `if (join_channel_/post_message_/leave_channel_ != nullptr)` blocks that the
/// other InChannel tests skip (they drive only the stub fallbacks). Wires real
/// application::chat use-cases over in-memory repos and drives a full
/// join -> chat -> leave flow, plus the JoinChannel failure (EID_ERROR) branch.

#include "connection_fsm_test_fixtures.hpp"

#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/post_message.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

using namespace pvpgn::test::connection_fsm;

namespace {

namespace chat = pvpgn::application::chat;
namespace inmem = pvpgn::infra::inmemory;

// Seed the placeholder account id the stub login assigns (account_id_ == 1), so
// JoinChannel's account lookup succeeds.
void seed_account(inmem::InMemoryAccountRepository& repo, std::uint32_t id) {
    auto name = pvpgn::domain::UserName::parse("alice").value();
    auto acc = pvpgn::domain::identity::Account::rehydrate(
        pvpgn::domain::AccountId{id}, name, pvpgn::domain::BNHash{},
        pvpgn::domain::Locale::parse_or_default("enUS"),
        pvpgn::domain::identity::CommandGroupMask{}, std::nullopt, false);
    REQUIRE(repo.save(acc));
}

// SID for chat events the FSM emits (build_chat_event -> packet 0x0F).
constexpr std::uint8_t kChatEventSid = 0x0Fu;

}  // namespace

TEST_CASE("ConnectionFsm: injected JoinChannel runs the real join path",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1);

    chat::JoinChannel join_uc{channels, accounts, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    REQUIRE(fsm.account_id() == 1u);

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    // A successful join emits EID_CHANNEL + EID_SHOWUSER(self) + EID_JOIN, i.e.
    // more than the single packet the failure path would send.
    CHECK(ctx.count_sid(kChatEventSid) >= 2);
    // The channel was created and persisted by the use-case.
    CHECK(channels.find_by_name("Lobby").has_value());
}

TEST_CASE("ConnectionFsm: injected JoinChannel failure emits a single EID_ERROR",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;  // account 1 NOT seeded
    inmem::InMemorySessionRegistry   sessions;

    chat::JoinChannel join_uc{channels, accounts, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    // The FSM swallows the use-case error into an EID_ERROR reply and returns ok.
    REQUIRE(result.has_value());
    CHECK(ctx.count_sid(kChatEventSid) == 1);  // just the error event
}

TEST_CASE("ConnectionFsm: injected PostMessage echoes a chat message",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1);

    chat::JoinChannel join_uc{channels, accounts, sessions};
    chat::PostMessage post_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    fsm.set_post_message(&post_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    // Join first so the FSM has a valid channel_id_ for PostMessage.
    auto join_pl = make_join_channel("Lobby");
    REQUIRE(fsm.dispatch(sid::kJoinChannel,
                         std::span<const std::byte>{join_pl}).has_value());
    ctx.sent.clear();

    auto chat_pl = make_chat_command("hello channel");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{chat_pl});

    REQUIRE(result.has_value());
    CHECK(ctx.count_sid(kChatEventSid) >= 1);  // EID_TALK echo
}

TEST_CASE("ConnectionFsm: injected LeaveChannel transitions InChannel->LoggedIn",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1);

    chat::JoinChannel  join_uc{channels, accounts, sessions};
    chat::LeaveChannel leave_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    fsm.set_leave_channel(&leave_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    auto join_pl = make_join_channel("Lobby");
    REQUIRE(fsm.dispatch(sid::kJoinChannel,
                         std::span<const std::byte>{join_pl}).has_value());

    // SID_LEAVECHAT carries no body.
    auto result = fsm.dispatch(sid::kLeaveChannel, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
}
