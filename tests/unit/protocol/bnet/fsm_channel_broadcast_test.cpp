// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/protocol/bnet/fsm_channel_broadcast_test.cpp
//
// Regression guard for bug-hunt wave 14: cross-session channel-talk delivery.
//
// When two clients share a channel and one sends SID_CHATCOMMAND, the original
// server fans an EID_TALK chat event out to every OTHER member via the message
// router. v3's BnetFsm::broadcast_chat_event used to wrap encode(ChatEvent) --
// which already begins AND finalizes its own SID_CHATEVENT packet -- in a second
// begin_bnet_packet/finalize_bnet_packet pair. The redundant finalize hit
// "no open bnet packet" (FailedPrecondition) and the function returned early,
// so the router was NEVER invoked: the listener received nothing.
//
// These tests wire the genuine LoginUser / CreateAccount / JoinChannel /
// PostMessage use-cases over in-memory repositories and a fake IMessageRouter,
// drive two real logins into the same channel, and assert that one client's
// chat command produces a router broadcast carrying a parseable EID_TALK packet
// addressed to the other client's session.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/post_message.hpp"
#include "core/clock.hpp"
#include "domain/shared/ids.hpp"
#include "infra/crypto/bnet_hash.hpp"
#include "infra/crypto/bnet_session_hasher.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"

#include "capturing_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;
using pvpgn::protocol::bnet::test::CapturingSessionContext;

namespace {

// --- Fake router: records every broadcast() so the test can inspect it. -----
struct RecordedBroadcast {
    std::vector<domain::SessionId> sessions;
    std::vector<std::byte>         bytes;
};

class RecordingRouter final : public domain::connection::IMessageRouter {
public:
    std::vector<RecordedBroadcast> broadcasts;

    core::Result<void, core::Error> send(
        domain::SessionId, std::span<const std::byte>) override {
        return core::ok();
    }

    core::Result<void, core::Error> broadcast(
        std::span<const domain::SessionId> sessions,
        std::span<const std::byte> bytes) override {
        broadcasts.push_back(RecordedBroadcast{
            std::vector<domain::SessionId>{sessions.begin(), sessions.end()},
            std::vector<std::byte>{bytes.begin(), bytes.end()}});
        return core::ok();
    }

    core::Result<void, core::Error> send_to_account(
        domain::AccountId, std::span<const std::byte>) override {
        return core::ok();
    }
};

// --- OLS hash helpers (mirror fsm_auth_create_login_test.cpp) ---------------
constexpr std::array<std::uint32_t, 5> kPassword{
    0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u};
constexpr std::uint32_t kClientToken = 0xDEADBEEFu;
constexpr std::uint32_t kServerToken = 0u;

std::array<std::uint32_t, 5> compute_hash2(std::array<std::uint32_t, 5> hash1,
                                           std::uint32_t client_token,
                                           std::uint32_t server_token) {
    std::array<std::byte, 28> buf{};
    auto put_le32 = [&buf](std::size_t off, std::uint32_t v) {
        buf[off + 0] = static_cast<std::byte>(v & 0xFFu);
        buf[off + 1] = static_cast<std::byte>((v >> 8) & 0xFFu);
        buf[off + 2] = static_cast<std::byte>((v >> 16) & 0xFFu);
        buf[off + 3] = static_cast<std::byte>((v >> 24) & 0xFFu);
    };
    put_le32(0, client_token);
    put_le32(4, server_token);
    for (std::size_t i = 0; i < hash1.size(); ++i) put_le32(8 + i * 4, hash1[i]);
    return pvpgn::v3::infra::crypto::blizzard_hash(std::span<const std::byte>{buf});
}

CreateAccount1Request create_req(const char* name) {
    CreateAccount1Request m;
    m.password_hash1 = kPassword;
    m.player_name    = name;
    return m;
}

LogonResponse2 logon_req(const char* name) {
    LogonResponse2 m;
    m.client_token  = kClientToken;
    m.server_token  = kServerToken;
    m.password_hash = compute_hash2(kPassword, kClientToken, kServerToken);
    m.username      = name;
    return m;
}

// Shared collaborators + a builder for per-client BnetFsm instances.
struct Harness {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemorySessionRegistry   sessions;
    infra::inmemory::InMemoryChannelRepository  channels;
    infra::inmemory::InMemoryEventBus          bus;
    infra::inmemory::InMemoryIpBanRepository   ip_bans;
    core::ManualClock                          clock{core::SystemTime{}};
    infra::crypto::BnetSessionHasher           hasher;
    std::shared_ptr<RecordingRouter>           router =
        std::make_shared<RecordingRouter>();

    BnetUseCaseContext make_ctx() {
        BnetUseCaseContext uc;
        uc.login_user = std::make_shared<application::auth::LoginUser>(
            accounts, sessions, bus, clock, hasher);
        uc.create_account = std::make_shared<application::auth::CreateAccount>(
            accounts, ip_bans, bus, clock);
        uc.join_channel = std::make_shared<application::chat::JoinChannel>(
            channels, accounts, sessions);
        uc.post_message = std::make_shared<application::chat::PostMessage>(
            channels, sessions);
        uc.leave_channel = std::make_shared<application::chat::LeaveChannel>(
            channels, sessions);
        uc.account_repo = std::shared_ptr<domain::identity::IAccountRepository>(
            &accounts, [](domain::identity::IAccountRepository*) noexcept {});
        uc.session_registry = std::shared_ptr<domain::identity::ISessionRegistry>(
            &sessions, [](domain::identity::ISessionRegistry*) noexcept {});
        uc.message_router = router;
        return uc;
    }

    // Drive create + login + enter-chat + join for one client, returning its FSM
    // and its capturing context (kept alive by the caller).
    void bring_into_channel(BnetFsm& fsm, const char* name, const char* channel) {
        REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
        REQUIRE(fsm.handle(ClientMessage{create_req(name)}).has_value());
        REQUIRE(fsm.handle(ClientMessage{logon_req(name)}).has_value());
        REQUIRE(fsm.handle(ClientMessage{EnterChatRequest{name, "PXES"}})
                    .has_value());
        REQUIRE(fsm.handle(ClientMessage{JoinChannel{0, channel}}).has_value());
    }
};

}  // namespace

TEST_CASE("fsm channel: talk broadcasts an EID_TALK packet to the other member",
          "[protocol][bnet][channel][broadcast]") {
    Harness h;

    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    auto bob_ctx   = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
    BnetFsm bob{bob_ctx, h.make_ctx(), domain::SessionId{2}};

    h.bring_into_channel(alice, "alice", "test");
    h.bring_into_channel(bob, "bob", "test");

    h.router->broadcasts.clear();

    REQUIRE(alice.handle(ClientMessage{ChatCommand{"hi bob"}}).has_value());

    // The router must have been hit exactly once with bob's session...
    REQUIRE(h.router->broadcasts.size() == 1u);
    const auto& bc = h.router->broadcasts.front();
    REQUIRE(bc.sessions.size() == 1u);
    CHECK(bc.sessions.front().value() == 2u);

    // ...and the payload must be a parseable SID_CHATEVENT / EID_TALK packet.
    REQUIRE_FALSE(bc.bytes.empty());
    auto fp = protocol::parse_packet(
        core::ByteView{bc.bytes.data(), bc.bytes.size()});
    REQUIRE(fp.has_value());
    auto decoded = decode_server(fp.value().packet);
    REQUIRE(decoded.has_value());
    const auto* ev = std::get_if<ChatEvent>(&decoded.value());
    REQUIRE(ev != nullptr);
    CHECK(ev->event_id == 5u);          // EID_TALK
    CHECK(ev->username == "alice");
    CHECK(ev->text == "hi bob");
}

TEST_CASE("fsm channel: a solo speaker broadcasts to nobody",
          "[protocol][bnet][channel][broadcast]") {
    Harness h;

    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
    h.bring_into_channel(alice, "alice", "test");

    h.router->broadcasts.clear();
    REQUIRE(alice.handle(ClientMessage{ChatCommand{"anyone?"}}).has_value());

    // Only member in the channel -> no recipients -> no broadcast at all.
    CHECK(h.router->broadcasts.empty());
}

namespace {

// Find the last ChatEvent the capturing context received.
const ChatEvent* last_chat_event(
    const std::shared_ptr<CapturingSessionContext>& ctx) {
    const ChatEvent* result = nullptr;
    for (const auto& m : ctx->all_sent()) {
        if (const auto* ev = std::get_if<ChatEvent>(&m)) result = ev;
    }
    return result;
}

}  // namespace

TEST_CASE("fsm whisper: /w routes EID_WHISPER to target + EID_WHISPERSENT to sender",
          "[protocol][bnet][whisper]") {
    Harness h;

    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    auto bob_ctx   = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
    BnetFsm bob{bob_ctx, h.make_ctx(), domain::SessionId{2}};

    h.bring_into_channel(alice, "alice", "test");
    h.bring_into_channel(bob, "bob", "test");

    h.router->broadcasts.clear();
    alice_ctx->clear_sent();

    REQUIRE(alice.handle(ClientMessage{ChatCommand{"/w bob hey there"}})
                .has_value());

    // Target gets EID_WHISPER (0x04, username=sender) via the router...
    REQUIRE(h.router->broadcasts.size() == 1u);
    const auto& bcast = h.router->broadcasts.front();
    REQUIRE(bcast.sessions.size() == 1u);
    CHECK(bcast.sessions.front().value() == 2u);  // bob's session
    auto fp = protocol::parse_packet(
        core::ByteView{bcast.bytes.data(), bcast.bytes.size()});
    REQUIRE(fp.has_value());
    auto decoded = decode_server(fp.value().packet);
    REQUIRE(decoded.has_value());
    const auto* wev = std::get_if<ChatEvent>(&decoded.value());
    REQUIRE(wev != nullptr);
    CHECK(wev->event_id == 0x04u);     // EID_WHISPER
    CHECK(wev->username == "alice");
    CHECK(wev->text == "hey there");

    // ...and the sender gets EID_WHISPERSENT (0x0a, username=target).
    const auto* ack = last_chat_event(alice_ctx);
    REQUIRE(ack != nullptr);
    CHECK(ack->event_id == 0x0au);     // EID_WHISPERSENT
    CHECK(ack->username == "bob");
    CHECK(ack->text == "hey there");
}

TEST_CASE("fsm whisper: aliases /msg /m /whisper all route",
          "[protocol][bnet][whisper]") {
    for (const char* line : {"/msg bob hi", "/m bob hi", "/whisper bob hi"}) {
        Harness h;
        auto alice_ctx = std::make_shared<CapturingSessionContext>();
        auto bob_ctx   = std::make_shared<CapturingSessionContext>();
        BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
        BnetFsm bob{bob_ctx, h.make_ctx(), domain::SessionId{2}};
        h.bring_into_channel(alice, "alice", "test");
        h.bring_into_channel(bob, "bob", "test");
        h.router->broadcasts.clear();

        REQUIRE(alice.handle(ClientMessage{ChatCommand{line}}).has_value());
        INFO("alias line: " << line);
        REQUIRE(h.router->broadcasts.size() == 1u);
    }
}

TEST_CASE("fsm whisper: target offline yields EID_ERROR, no broadcast",
          "[protocol][bnet][whisper]") {
    Harness h;
    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
    h.bring_into_channel(alice, "alice", "test");

    h.router->broadcasts.clear();
    alice_ctx->clear_sent();

    // "ghost" was never created/logged in.
    REQUIRE(alice.handle(ClientMessage{ChatCommand{"/w ghost hello"}})
                .has_value());

    CHECK(h.router->broadcasts.empty());
    const auto* ev = last_chat_event(alice_ctx);
    REQUIRE(ev != nullptr);
    CHECK(ev->event_id == 0x13u);  // EID_ERROR
}

TEST_CASE("fsm disconnect: leaving a channel notifies the remaining members",
          "[protocol][bnet][channel][broadcast]") {
    Harness h;

    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    auto bob_ctx   = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};
    BnetFsm bob{bob_ctx, h.make_ctx(), domain::SessionId{2}};

    h.bring_into_channel(alice, "alice", "test");
    h.bring_into_channel(bob, "bob", "test");

    h.router->broadcasts.clear();

    // Alice's transport drops — the disconnect must broadcast EID_LEAVE to bob.
    alice.on_disconnect();

    REQUIRE(h.router->broadcasts.size() == 1u);
    const auto& bcast = h.router->broadcasts.front();
    REQUIRE(bcast.sessions.size() == 1u);
    CHECK(bcast.sessions.front().value() == 2u);  // bob
    auto fp = protocol::parse_packet(
        core::ByteView{bcast.bytes.data(), bcast.bytes.size()});
    REQUIRE(fp.has_value());
    auto decoded = decode_server(fp.value().packet);
    REQUIRE(decoded.has_value());
    const auto* ev = std::get_if<ChatEvent>(&decoded.value());
    REQUIRE(ev != nullptr);
    CHECK(ev->event_id == 0x03u);  // EID_LEAVE
    CHECK(ev->username == "alice");
}

TEST_CASE("fsm disconnect: a user not in any channel broadcasts nothing",
          "[protocol][bnet][channel][broadcast]") {
    Harness h;
    auto alice_ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm alice{alice_ctx, h.make_ctx(), domain::SessionId{1}};

    // Log in + enter chat but never join a channel.
    REQUIRE(alice.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(alice.handle(ClientMessage{create_req("alice")}).has_value());
    REQUIRE(alice.handle(ClientMessage{logon_req("alice")}).has_value());
    REQUIRE(alice.handle(ClientMessage{EnterChatRequest{"alice", "PXES"}})
                .has_value());

    h.router->broadcasts.clear();
    alice.on_disconnect();

    CHECK(h.router->broadcasts.empty());
}
