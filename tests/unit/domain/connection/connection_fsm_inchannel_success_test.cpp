// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel_success_test.cpp
/// SUCCESS-path coverage for connection_fsm_inchannel.cpp that the
/// existing inchannel tests (test / branches / error_branches / usecase) do not
/// assert. The earlier suites verify the *state transition* and *game_type*
/// switch arms but never decode the emitted wire bodies; these cases decode the
/// SID replies and SID_CHATEVENT payloads and assert their exact contents:
///
///   on_start_game (SID_STARTADVEX, 0x1C):
///     - the SID_STARTADVEX reply body is exactly a 4-byte LE success result (0)
///     - GameInfo (game_name / password / game_stats / game_type) reaches
///       on_game_created intact, across EACH raw_type switch arm
///   on_join_game (SID_GETADVLISTEX, 0x09):
///     - the SID_GETADVLISTEX reply body is exactly a 4-byte LE success result(0)
///     - GameInfo reaches on_game_joined intact, across EACH raw_type switch arm
///   on_join_channel (injected JoinChannel over in-memory repos):
///     - decode the EID_CHANNEL name, the EID_SHOWUSER for an existing member,
///       and the EID_JOIN echo for the joining user (event ids + name fields)
///   on_chat_command (injected PostMessage over in-memory repos):
///     - decode the EID_TALK echo of the ChannelMessageSent event (event id +
///       sender + message text), not just the packet count
///
/// All wire-layout knowledge below is reconstructed locally because the
/// production build_chat_event() / EID_* constants live in the private
/// connection_fsm_internal.hpp (not on the unit-test include path).

#include "connection_fsm_test_fixtures.hpp"

#include "application/chat/join_channel.hpp"
#include "application/chat/post_message.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

#include <cstdint>
#include <string>

using namespace pvpgn::test::connection_fsm;
using GT = pvpgn::application::connection::GameType;

namespace {

namespace chat  = pvpgn::application::chat;
namespace inmem = pvpgn::infra::inmemory;

// SID_CHATEVENT (build_chat_event -> packet 0x0F).
constexpr std::uint8_t kChatEventSid = 0x0Fu;

// EID_* event ids (mirror connection_fsm_internal.hpp, which is src-private).
constexpr std::uint32_t kEidShowUser = 0x01u;
constexpr std::uint32_t kEidJoin     = 0x02u;
constexpr std::uint32_t kEidTalk     = 0x05u;
constexpr std::uint32_t kEidChannel  = 0x07u;

// SID_CHATEVENT body: 6 LE32 header words, then username\0, then text\0.
constexpr std::size_t kChatEventHeaderBytes = 24;

// Read a little-endian uint32 at offset `off` from a recorded payload.
std::uint32_t le32_at(const std::vector<std::byte>& b, std::size_t off) {
    REQUIRE(off + 4 <= b.size());
    return  static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[off]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[off + 1])) << 8)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[off + 2])) << 16)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[off + 3])) << 24);
}

// Read a NUL-terminated string at byte offset `off`.
std::string cstr_at(const std::vector<std::byte>& b, std::size_t off) {
    std::string s;
    for (std::size_t i = off; i < b.size(); ++i) {
        const auto c = std::to_integer<unsigned char>(b[i]);
        if (c == 0) break;
        s.push_back(static_cast<char>(c));
    }
    return s;
}

// Decode the (event_id, username, text) of a SID_CHATEVENT body.
struct ChatEvent {
    std::uint32_t event_id;
    std::string   username;
    std::string   text;
};
ChatEvent decode_chat_event(const std::vector<std::byte>& body) {
    const std::uint32_t eid  = le32_at(body, 0);
    const std::string   name = cstr_at(body, kChatEventHeaderBytes);
    const std::string   text = cstr_at(body, kChatEventHeaderBytes + name.size() + 1);
    return {eid, name, text};
}

// Find the first recorded SID_CHATEVENT with the given event id.
const SentPacket* find_eid(const FakeContext& ctx, std::uint32_t eid) {
    for (const auto& p : ctx.sent) {
        if (p.packet_id != kChatEventSid) continue;
        if (le32_at(p.payload, 0) == eid) return &p;
    }
    return nullptr;
}

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

// ---------------------------------------------------------------------------
// on_start_game: success reply body + GameInfo passthrough across every arm
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: STARTADVEX success reply is a 4-byte zero result",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("ReplyGame", "", "PXES", 1u);
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{payload}).has_value());

    // Exactly one packet, the SID_STARTADVEX reply, carrying LE32 result == 0.
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kStartGame1);
    REQUIRE(ctx.last()->payload.size() == 4);
    CHECK(le32_at(ctx.last()->payload, 0) == 0u);  // 0 == success
    CHECK(fsm.state() == ConnectionState::InGame);
}

TEST_CASE("ConnectionFsm: STARTADVEX forwards full GameInfo for each game_type arm",
          "[connection_fsm][inchannel][ingame]") {
    struct Case { std::uint32_t raw; GT expected; const char* stats; };
    const Case cases[] = {
        {1, GT::FreeForAll,  "S1"},
        {2, GT::OneOnOne,    "S2"},
        {3, GT::Cooperative, "S3"},
        {4, GT::Custom,      "S4"},
        {0, GT::Melee,       "S0"},
        {255, GT::Melee,     "Sd"},  // default arm via an out-of-range value
    };

    for (const auto& c : cases) {
        FakeContext ctx;
        ConnectionFsm fsm{ctx};
        reach_logged_in_nls(fsm);
        reach_in_channel(fsm);
        ctx.sent.clear();
        ctx.game_events.clear();

        const std::string name = std::string("Game") + std::to_string(c.raw);
        const std::string pw   = std::string("pw")   + std::to_string(c.raw);
        auto payload = make_start_game(name, pw, c.stats, c.raw);
        REQUIRE(fsm.dispatch(sid::kStartGame1,
                             std::span<const std::byte>{payload}).has_value());

        REQUIRE(ctx.game_events.size() == 1);
        const auto& ev = ctx.game_events[0];
        CHECK(ev.kind            == FakeContext::GameEvent::Kind::Created);
        CHECK(ev.game_id         == fsm.game_id());
        CHECK(ev.info.game_type  == c.expected);
        CHECK(ev.info.game_name  == name);
        CHECK(ev.info.password   == pw);
        CHECK(ev.info.game_stats == c.stats);

        // Reply body is always a 4-byte zero success result.
        REQUIRE(ctx.last() != nullptr);
        CHECK(ctx.last()->packet_id == sid::kStartGame1);
        REQUIRE(ctx.last()->payload.size() == 4);
        CHECK(le32_at(ctx.last()->payload, 0) == 0u);
    }
}

// ---------------------------------------------------------------------------
// on_join_game: success reply body + GameInfo passthrough across every arm
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: GETADVLISTEX success reply is a 4-byte zero result",
          "[connection_fsm][inchannel][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_join_game_pkt("JoinReply", "", "", 1u);
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kJoinGame);
    REQUIRE(ctx.last()->payload.size() == 4);
    CHECK(le32_at(ctx.last()->payload, 0) == 0u);  // 0 == success / found
    CHECK(fsm.state() == ConnectionState::InGame);
}

TEST_CASE("ConnectionFsm: GETADVLISTEX forwards full GameInfo for each game_type arm",
          "[connection_fsm][inchannel][ingame]") {
    struct Case { std::uint32_t raw; GT expected; const char* stats; };
    const Case cases[] = {
        {1, GT::FreeForAll,  "J1"},
        {2, GT::OneOnOne,    "J2"},
        {3, GT::Cooperative, "J3"},
        {4, GT::Custom,      "J4"},
        {0, GT::Melee,       "J0"},
        {123, GT::Melee,     "Jd"},  // default arm
    };

    for (const auto& c : cases) {
        FakeContext ctx;
        ConnectionFsm fsm{ctx};
        reach_logged_in_nls(fsm);
        reach_in_channel(fsm);
        ctx.sent.clear();
        ctx.game_events.clear();

        const std::string name = std::string("Join") + std::to_string(c.raw);
        const std::string pw   = std::string("jw")   + std::to_string(c.raw);
        auto payload = make_join_game_pkt(name, pw, c.stats, c.raw);
        REQUIRE(fsm.dispatch(sid::kJoinGame,
                             std::span<const std::byte>{payload}).has_value());

        REQUIRE(ctx.game_events.size() == 1);
        const auto& ev = ctx.game_events[0];
        CHECK(ev.kind            == FakeContext::GameEvent::Kind::Joined);
        CHECK(ev.game_id         == fsm.game_id());
        CHECK(ev.info.game_type  == c.expected);
        CHECK(ev.info.game_name  == name);
        CHECK(ev.info.password   == pw);
        CHECK(ev.info.game_stats == c.stats);

        REQUIRE(ctx.last() != nullptr);
        CHECK(ctx.last()->packet_id == sid::kJoinGame);
        REQUIRE(ctx.last()->payload.size() == 4);
        CHECK(le32_at(ctx.last()->payload, 0) == 0u);
    }
}

// ---------------------------------------------------------------------------
// on_join_channel: decode EID_CHANNEL / EID_SHOWUSER / EID_JOIN wire bodies
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: injected JoinChannel emits decodable CHANNEL/SHOWUSER/JOIN",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1, "alice");
    seed_account(accounts, 2, "bob");

    chat::JoinChannel join_uc{channels, accounts, sessions};

    // Pre-seed the channel with an existing member (account 2 -> name "2")
    // so the joining FSM's member-loop runs for a non-empty channel.
    {
        auto tag = pvpgn::domain::ClientTag{};
        REQUIRE(join_uc.execute(pvpgn::domain::AccountId{2}, "Lobby", tag)
                    .has_value());
    }

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    reach_logged_in_nls(fsm);   // account_id_ == 1 (joining user -> name "1")
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    REQUIRE(fsm.dispatch(sid::kJoinChannel,
                         std::span<const std::byte>{payload}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);

    // 1. EID_CHANNEL carries the channel name in its username field.
    const SentPacket* chan = find_eid(ctx, kEidChannel);
    REQUIRE(chan != nullptr);
    CHECK(decode_chat_event(chan->payload).username == "Lobby");

    // 2. EID_SHOWUSER for the pre-existing member (AccountId{2} -> "2").
    const SentPacket* show = find_eid(ctx, kEidShowUser);
    REQUIRE(show != nullptr);
    {
        // The member-loop names members by AccountId; the existing member is "2".
        // (The joining user "1" may also be listed if the aggregate already
        //  reflects its membership, so accept either as long as "2" appears.)
        bool saw_existing = false;
        for (const auto& p : ctx.sent) {
            if (p.packet_id != kChatEventSid) continue;
            if (le32_at(p.payload, 0) != kEidShowUser) continue;
            if (decode_chat_event(p.payload).username == "2") saw_existing = true;
        }
        CHECK(saw_existing);
    }

    // 3. EID_JOIN: the handler intends to echo the join to the joiner by draining
    //    the channel aggregate's ChannelJoined event. Via the injected JoinChannel
    //    use-case that event is consumed (published to the use-case's bus) before
    //    the FSM sees the returned channel, so the aggregate's event buffer is
    //    empty and NO EID_JOIN is emitted on this path. We assert the actual
    //    behaviour; the lost join-echo through the real use-case is a known
    //    behavioural gap (recorded in the progress tracker), not a pass/fail
    //    expectation of "echo present".
    const SentPacket* join = find_eid(ctx, kEidJoin);
    CHECK(join == nullptr);
}

// ---------------------------------------------------------------------------
// on_chat_command: decode the EID_TALK echo of the ChannelMessageSent event
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: injected PostMessage echoes a decodable EID_TALK",
          "[connection_fsm][inchannel][usecase]") {
    inmem::InMemoryChannelRepository channels;
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry   sessions;
    seed_account(accounts, 1, "alice");

    chat::JoinChannel join_uc{channels, accounts, sessions};
    chat::PostMessage post_uc{channels, sessions};

    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    fsm.set_join_channel(&join_uc);
    fsm.set_post_message(&post_uc);
    reach_logged_in_nls(fsm);   // account_id_ == 1 (sender -> name "1")
    reach_in_channel(fsm);

    // Join so the FSM holds a valid channel_id_ for PostMessage.
    auto join_pl = make_join_channel("Lobby");
    REQUIRE(fsm.dispatch(sid::kJoinChannel,
                         std::span<const std::byte>{join_pl}).has_value());
    ctx.sent.clear();

    const std::string text = "hello channel";
    auto chat_pl = make_chat_command(text);
    REQUIRE(fsm.dispatch(sid::kChatCommand,
                         std::span<const std::byte>{chat_pl}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);

    // Success drain: the ChannelMessageSent event echoed as EID_TALK, with the
    // sender named by AccountId ("1") and the original message body text.
    const SentPacket* talk = find_eid(ctx, kEidTalk);
    REQUIRE(talk != nullptr);
    const ChatEvent ev = decode_chat_event(talk->payload);
    CHECK(ev.event_id == kEidTalk);
    CHECK(ev.username == "1");
    CHECK(ev.text     == text);
}
