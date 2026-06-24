// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel_error_branches_test.cpp
/// Error / edge branches of connection_fsm_inchannel.cpp that the existing
/// inchannel tests (stub + happy-path use-case) do not reach:
///   on_chat_command:
///     - empty-text early return (no use-case call, no packet)
///     - ChatMessage::create failure (message > 223 bytes) -> EID_ERROR
///     - injected PostMessage failure (posting without a joined channel) -> EID_ERROR
///   on_join_channel:
///     - injected success path with a *valid* ClientTag parsed from a STAR
///       AUTH_INFO (the usecase test leaves the tag at 0 -> default ClientTag{}),
///       plus an existing-member EID_SHOWUSER + EID_JOIN drain
///   on_leave_channel:
///     - injected LeaveChannel present but channel_id_ == 0 (leave without join):
///       the `channel_id_ != 0u` guard skips the use-case call.
///
/// Wires real application::chat use-cases over in-memory repos, mirroring
/// connection_fsm_inchannel_usecase_test.cpp.

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

namespace chat  = pvpgn::application::chat;
namespace inmem = pvpgn::infra::inmemory;

constexpr std::uint8_t kChatEventSid = 0x0Fu;

// STAR product tag (big-endian 'STAR'); a non-zero tag that ClientTag can parse.
constexpr std::uint32_t kStarTag = 0x53544152u;

void seed_account(inmem::InMemoryAccountRepository& repo, std::uint32_t id,
                  std::string_view name) {
    auto uname = pvpgn::domain::UserName::parse(name).value();
    auto acc = pvpgn::domain::identity::Account::rehydrate(
        pvpgn::domain::AccountId{id}, uname, pvpgn::domain::BNHash{},
        pvpgn::domain::Locale::parse_or_default("enUS"),
        pvpgn::domain::identity::CommandGroupMask{}, std::nullopt, false);
    REQUIRE(repo.save(acc));
}

}  // namespace

// --- on_chat_command: empty text is a no-op ok (no packet emitted) ----------

TEST_CASE("ConnectionFsm: CHATCOMMAND with empty text is a no-op ok",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_chat_command("");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(ctx.sent.empty());  // early return before any send
    CHECK_FALSE(ctx.closed);
}

// --- on_chat_command: over-long message fails ChatMessage::create -----------
// Exercises the injected-use-case path's `if (!msg_result)` EID_ERROR branch.

TEST_CASE("ConnectionFsm: CHATCOMMAND over-long message emits EID_ERROR",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemorySessionRegistry   sessions;
    chat::PostMessage post_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_post_message(&post_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    // ChatMessage::kMaxBytes == 223; build a 300-char message to force a
    // create() failure before PostMessage is ever called.
    const std::string too_long(300, 'x');
    auto payload = make_chat_command(too_long);
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());                 // swallowed into EID_ERROR
    CHECK(ctx.count_sid(kChatEventSid) == 1);    // single error event
    CHECK(fsm.state() == ConnectionState::InChannel);
}

// --- on_chat_command: PostMessage failure (not in channel) -> EID_ERROR -----
// channel_id_ is still 0 (never joined), so PostMessage::execute returns
// ChannelNotFound, driving the `if (!result)` EID_ERROR branch.

TEST_CASE("ConnectionFsm: CHATCOMMAND with no joined channel emits EID_ERROR",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemorySessionRegistry   sessions;
    chat::PostMessage post_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_post_message(&post_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);     // InChannel, but channel_id_ == 0 (no JOINCHANNEL)
    ctx.sent.clear();

    auto payload = make_chat_command("hello");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(ctx.count_sid(kChatEventSid) == 1);    // EID_ERROR ("Cannot send message.")
    CHECK(fsm.state() == ConnectionState::InChannel);
}

// --- on_join_channel: injected success with a parsed STAR ClientTag ----------
// Drives AUTH_INFO with the STAR product so client_product_tag_ is non-zero and
// ClientTag::from_packed_be() succeeds (the usecase test leaves it at 0).

TEST_CASE("ConnectionFsm: injected JoinChannel uses a parsed STAR ClientTag",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1, "alice");

    chat::JoinChannel join_uc{channels, accounts, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);

    // AUTH_INFO with STAR -> client_product_tag_ = kStarTag (parseable).
    auto ai = make_auth_info(kStarTag);
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    REQUIRE(fsm.client_product_tag() == kStarTag);
    REQUIRE_FALSE(fsm.is_nls_client());

    // Finish login + enter chat (NLS stub path still works regardless of tag).
    auto al = make_accountlogon("alice");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(ctx.count_sid(kChatEventSid) >= 2);  // EID_CHANNEL + showuser/join
    CHECK(channels.find_by_name("Lobby").has_value());
}

// --- on_join_channel: second joiner sees an existing-member EID_SHOWUSER -----
// One FSM joins "Lobby" first (seeding a member); a second FSM then joins the
// same channel, exercising the member_ids() EID_SHOWUSER loop and the
// ChannelJoined drain_events() branch for a non-empty channel.

TEST_CASE("ConnectionFsm: injected JoinChannel emits EID_SHOWUSER for an existing member",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1, "alice");
    seed_account(accounts, 2, "bob");

    chat::JoinChannel join_uc{channels, accounts, sessions};

    // First member joins via use-case directly so the channel has a member.
    {
        auto tag = pvpgn::domain::ClientTag{};
        REQUIRE(join_uc.execute(pvpgn::domain::AccountId{2}, "Lobby", tag)
                    .has_value());
    }

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    reach_logged_in_nls(fsm);   // account_id_ == 1 (alice placeholder)
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    // EID_CHANNEL + EID_SHOWUSER(existing bob) + EID_SHOWUSER(self) + EID_JOIN:
    // more than the 2 a single-member join would emit.
    CHECK(ctx.count_sid(kChatEventSid) >= 3);
}

// --- on_leave_channel: injected use-case skipped when channel_id_ == 0 -------
// LeaveChannel is wired, but the client never joined a channel, so the
// `channel_id_ != 0u` guard means the use-case is NOT invoked; the FSM still
// transitions InChannel -> LoggedIn.

TEST_CASE("ConnectionFsm: LEAVECHAT without a joined channel skips the use-case",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemorySessionRegistry   sessions;
    chat::LeaveChannel leave_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_leave_channel(&leave_uc);
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);     // channel_id_ == 0
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kLeaveChannel, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// --- on_leave_channel: out-of-order LEAVECHAT (not InChannel) is ignored -----

TEST_CASE("ConnectionFsm: LEAVECHAT outside InChannel is a silent no-op ok",
          "[connection_fsm][inchannel]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);   // LoggedIn, not InChannel
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kLeaveChannel, std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);  // unchanged
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.sent.empty());
}
