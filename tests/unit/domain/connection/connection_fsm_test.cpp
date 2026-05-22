// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_test.cpp
/// Unit tests for domain::connection::ConnectionFsm.
///
/// Test count: 43 TEST_CASEs, 200+ CHECK/REQUIRE assertions.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/connection/connection_context.hpp"
#include "domain/connection/connection_fsm.hpp"

using namespace pvpgn::domain::connection;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

namespace {

struct SentPacket {
    std::uint8_t          packet_id;
    std::vector<std::byte> payload;
};

/// Fake IConnectionContext that records all sent packets and close() calls.
class FakeContext : public IConnectionContext {
public:
    std::vector<SentPacket> sent;
    bool                    closed{false};
    std::string             remote_addr{"127.0.0.1"};
    std::uint32_t           session_id_val{42};

    // Game lifecycle tracking
    struct GameEvent {
        enum class Kind { Created, Joined, Left } kind;
        std::uint32_t game_id{0};
        GameInfo      info;  // only valid for Created/Joined
    };
    std::vector<GameEvent> game_events;

    pvpgn::core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) override {
        sent.push_back({packet_id,
                        std::vector<std::byte>{payload.begin(), payload.end()}});
        return pvpgn::core::ok();
    }

    void close() override { closed = true; }

    [[nodiscard]] std::string get_remote_address() const override {
        return remote_addr;
    }

    [[nodiscard]] std::uint32_t get_session_id() const override {
        return session_id_val;
    }

    void on_game_created(std::uint32_t game_id,
                         const GameInfo& info) override {
        game_events.push_back({GameEvent::Kind::Created, game_id, info});
    }

    void on_game_joined(std::uint32_t game_id,
                        const GameInfo& info) override {
        game_events.push_back({GameEvent::Kind::Joined, game_id, info});
    }

    void on_game_left(std::uint32_t game_id) override {
        game_events.push_back({GameEvent::Kind::Left, game_id, {}});
    }

    /// Return the last sent packet, or nullptr if none.
    [[nodiscard]] const SentPacket* last() const {
        if (sent.empty()) return nullptr;
        return &sent.back();
    }

    /// Return the number of packets sent with the given SID.
    [[nodiscard]] std::size_t count_sid(std::uint8_t sid) const {
        std::size_t n = 0;
        for (const auto& p : sent) if (p.packet_id == sid) ++n;
        return n;
    }

    /// Return the last game event, or nullptr if none.
    [[nodiscard]] const GameEvent* last_game_event() const {
        if (game_events.empty()) return nullptr;
        return &game_events.back();
    }
};

// ---------------------------------------------------------------------------
// Packet builders — minimal BNCS payloads (body only, no 4-byte header)
// ---------------------------------------------------------------------------

/// Write a little-endian uint32 into a byte vector.
void push_le32(std::vector<std::byte>& v, std::uint32_t x) {
    v.push_back(std::byte{static_cast<std::uint8_t>( x        & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >>  8) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 16) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 24) & 0xFFu)});
}

/// Append a NUL-terminated string.
void push_cstr(std::vector<std::byte>& v, std::string_view s) {
    for (char c : s) v.push_back(std::byte{static_cast<std::uint8_t>(c)});
    v.push_back(std::byte{0});
}

/// Build a minimal SID_AUTH_INFO (0x50) payload.
std::vector<std::byte> make_auth_info(std::uint32_t product_id = 0x52415453u /* 'STAR' */) {
    std::vector<std::byte> p;
    push_le32(p, 0u);           // protocol_id
    push_le32(p, 0x36385849u);  // platform_id 'IX86'
    push_le32(p, product_id);   // product_id
    push_le32(p, 0u);           // version_byte
    push_le32(p, 0u);           // language_id
    push_le32(p, 0u);           // local_ip
    push_le32(p, 0u);           // time_zone_bias
    push_le32(p, 0u);           // mpq_locale_id
    push_le32(p, 0u);           // user_language
    push_cstr(p, "USA");        // country_abbrev
    push_cstr(p, "United States"); // country
    return p;
}

/// Build a minimal SID_AUTH_CHECK (0x51) payload.
std::vector<std::byte> make_auth_check() {
    std::vector<std::byte> p;
    push_le32(p, 0xABCDu);  // client_token
    push_le32(p, 0u);       // exe_version
    push_le32(p, 0u);       // exe_hash
    push_le32(p, 0u);       // num_keys
    push_le32(p, 0u);       // spawn
    push_cstr(p, "game.exe 01/01/2000 00:00:00 12345"); // exe_info
    push_cstr(p, "Owner");  // key_owner
    return p;
}

/// Build a minimal SID_AUTH_ACCOUNTLOGON (0x53) payload.
std::vector<std::byte> make_accountlogon(std::string_view username = "alice") {
    std::vector<std::byte> p;
    // client_key: 32 bytes (NLS SRP A value, zeros for test)
    for (int i = 0; i < 32; ++i) p.push_back(std::byte{0});
    push_cstr(p, username);
    return p;
}

/// Build a minimal SID_AUTH_ACCOUNTLOGONPROOF (0x54) payload.
std::vector<std::byte> make_accountlogonproof() {
    std::vector<std::byte> p;
    // client_proof: 20 bytes (NLS SRP M1, zeros for test)
    for (int i = 0; i < 20; ++i) p.push_back(std::byte{0});
    return p;
}

/// Build a minimal SID_LOGON_REQUEST (0x29) payload.
std::vector<std::byte> make_logon_request(std::string_view username = "bob") {
    std::vector<std::byte> p;
    push_le32(p, 0xDEADu);  // client_token
    push_le32(p, 0xBEEFu);  // server_token
    // password_hash: 5 × uint32 (20 bytes)
    for (int i = 0; i < 5; ++i) push_le32(p, 0u);
    push_cstr(p, username);
    return p;
}

/// Build a minimal SID_ENTERCHAT (0x0A) payload.
std::vector<std::byte> make_enter_chat(std::string_view username = "alice",
                                        std::string_view statstr  = "PXES") {
    std::vector<std::byte> p;
    push_cstr(p, username);
    push_cstr(p, statstr);
    return p;
}

/// Build a minimal SID_JOINCHANNEL (0x0C) payload.
std::vector<std::byte> make_join_channel(std::string_view channel = "Lobby") {
    std::vector<std::byte> p;
    push_le32(p, 0u);  // flags = first join
    push_cstr(p, channel);
    return p;
}

/// Build a minimal SID_CHATCOMMAND (0x0E) payload.
std::vector<std::byte> make_chat_command(std::string_view text = "hello") {
    std::vector<std::byte> p;
    push_cstr(p, text);
    return p;
}

/// Build a minimal SID_PING (0x25) payload.
std::vector<std::byte> make_ping(std::uint32_t cookie = 0x12345678u) {
    std::vector<std::byte> p;
    push_le32(p, cookie);
    return p;
}

/// Build a SID_STARTADVEX (0x1C) payload.
/// Layout: [0..3] game_state, [4..7] game_type, [8..11] unk, [12..15] ladder,
///         [16..] game_name\0, password\0, stats\0
std::vector<std::byte> make_start_game(
    std::string_view game_name = "TestGame",
    std::string_view password  = "",
    std::string_view stats     = "PXES",
    std::uint32_t    game_type = 0u  /* Melee */)
{
    std::vector<std::byte> p;
    push_le32(p, 1u);         // game_state = public
    push_le32(p, game_type);  // game_type
    push_le32(p, 0u);         // unknown
    push_le32(p, 0u);         // ladder_type
    push_cstr(p, game_name);
    push_cstr(p, password);
    push_cstr(p, stats);
    return p;
}

/// Build a SID_GETADVLISTEX (0x09) payload.
/// Layout: [0..3] game_type, [4..7] sub_type, [8..11] lang, [12..15] ladder,
///         [16..19] num_results, [20..] game_name\0, password\0, stats\0
std::vector<std::byte> make_join_game_pkt(
    std::string_view game_name = "TestGame",
    std::string_view password  = "",
    std::string_view stats     = "",
    std::uint32_t    game_type = 0u  /* Melee */)
{
    std::vector<std::byte> p;
    push_le32(p, game_type);  // game_type
    push_le32(p, 0u);         // sub_game_type
    push_le32(p, 0u);         // language_id
    push_le32(p, 0u);         // ladder_type
    push_le32(p, 1u);         // num_results
    push_cstr(p, game_name);
    push_cstr(p, password);
    push_cstr(p, stats);
    return p;
}

// ---------------------------------------------------------------------------
// Helper: drive FSM to a given state
// ---------------------------------------------------------------------------

/// Drive FSM from Connecting → Authenticating (sends AUTH_INFO).
void reach_authenticating(ConnectionFsm& fsm) {
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    REQUIRE(fsm.state() == ConnectionState::Authenticating);
}

/// Drive FSM from Connecting → LoggedIn via NLS path.
void reach_logged_in_nls(ConnectionFsm& fsm,
                          std::string_view username = "alice") {
    reach_authenticating(fsm);
    auto al = make_accountlogon(username);
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    REQUIRE(fsm.state() == ConnectionState::LoggedIn);
}

/// Drive FSM from Connecting → LoggedIn via legacy OLS path.
void reach_logged_in_ols(ConnectionFsm& fsm,
                          std::string_view username = "bob") {
    auto lr = make_logon_request(username);
    REQUIRE(fsm.dispatch(sid::kLogonRequest,
                         std::span<const std::byte>{lr}).has_value());
    REQUIRE(fsm.state() == ConnectionState::LoggedIn);
}

/// Drive FSM from LoggedIn → InChannel.
void reach_in_channel(ConnectionFsm& fsm,
                       std::string_view username = "alice") {
    auto ec = make_enter_chat(username);
    REQUIRE(fsm.dispatch(sid::kEnterChat,
                         std::span<const std::byte>{ec}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InChannel);
}

/// Drive FSM from InChannel → InGame via StartGame.
void reach_in_game_via_start(ConnectionFsm& fsm,
                              std::string_view game_name = "TestGame") {
    auto sg = make_start_game(game_name);
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{sg}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InGame);
}

/// Drive FSM from InChannel → InGame via JoinGame.
[[maybe_unused]] void reach_in_game_via_join(ConnectionFsm& fsm,
                             std::string_view game_name = "TestGame") {
    auto jg = make_join_game_pkt(game_name);
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{jg}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InGame);
}

}  // namespace

// ===========================================================================
// TEST CASES
// ===========================================================================

// ---------------------------------------------------------------------------
// 1. Initial state
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: initial state is Connecting", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx, 1u};

    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK(fsm.session_id() == 1u);
    CHECK(fsm.account_id() == 0u);
    CHECK(fsm.username().empty());
    CHECK(fsm.game_id() == 0u);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.sent.empty());
}

// ---------------------------------------------------------------------------
// 2. SID_AUTH_INFO in Connecting → Authenticating
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_INFO transitions Connecting→Authenticating",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_auth_info(0x52415453u /* STAR */);
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);
    // FSM must send a SID_AUTH_INFO reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthInfo);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 3. SID_AUTH_INFO in wrong state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_INFO in Authenticating state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_authenticating(fsm);
    ctx.sent.clear();

    // Second AUTH_INFO while already Authenticating → reject
    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 4. SID_AUTH_INFO in LoggedIn state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_INFO in LoggedIn state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 5. NLS auth path: AUTH_INFO → ACCOUNTLOGON → ACCOUNTLOGONPROOF → LoggedIn
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: NLS auth path reaches LoggedIn", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // Step 1: AUTH_INFO
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // Step 2: ACCOUNTLOGON
    auto al = make_accountlogon("alice");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating); // still waiting for proof

    // Step 3: ACCOUNTLOGONPROOF
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "alice");
    CHECK(fsm.account_id() != 0u);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 6. SID_LOGON_REQUEST (legacy OLS) in Connecting → LoggedIn
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_LOGON_REQUEST transitions Connecting→LoggedIn",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "bob");
    // FSM must send a reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kLogonRequest);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 7. SID_LOGON_REQUEST in Authenticating state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_LOGON_REQUEST in Authenticating state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_authenticating(fsm);
    ctx.sent.clear();

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 8. SID_ENTERCHAT in LoggedIn → InChannel + reply sent
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_ENTERCHAT transitions LoggedIn→InChannel",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "alice");
    ctx.sent.clear();

    auto payload = make_enter_chat("alice", "PXES");
    auto result  = fsm.dispatch(sid::kEnterChat,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    // FSM must send SID_ENTERCHAT reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kEnterChat);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 9. SID_ENTERCHAT in Connecting state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_ENTERCHAT in Connecting state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_enter_chat("alice");
    auto result  = fsm.dispatch(sid::kEnterChat,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 10. SID_JOINCHANNEL in InChannel → accepted (no reply required)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_JOINCHANNEL in InChannel is accepted",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 11. SID_JOINCHANNEL in LoggedIn state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_JOINCHANNEL in LoggedIn state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_join_channel("Lobby");
    auto result  = fsm.dispatch(sid::kJoinChannel,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 12. SID_CHATCOMMAND in InChannel → accepted
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_CHATCOMMAND in InChannel is accepted",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto payload = make_chat_command("hello world");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 13. SID_CHATCOMMAND in Connecting state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_CHATCOMMAND in Connecting state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_chat_command("hello");
    auto result  = fsm.dispatch(sid::kChatCommand,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 14. Unknown packet in any state → silently ignored (not disconnected)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: unknown packet in Connecting is silently ignored",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // SID 0xAB is not handled
    std::vector<std::byte> payload{std::byte{0xDE}, std::byte{0xAD}};
    auto result = fsm.dispatch(0xABu, std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: unknown packet in LoggedIn is silently ignored",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    std::vector<std::byte> payload{std::byte{0x00}};
    auto result = fsm.dispatch(0xFFu, std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 15. close() from Connecting → Disconnecting
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: close() from Connecting transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 16. close() from LoggedIn → Disconnecting
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: close() from LoggedIn transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 17. close() from InChannel → Disconnecting
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: close() from InChannel transitions to Disconnecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 18. Packets in Disconnecting state are silently dropped
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: packets in Disconnecting state are silently dropped",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    fsm.close();
    ctx.sent.clear();

    auto payload = make_auth_info();
    auto result  = fsm.dispatch(sid::kAuthInfo,
                                std::span<const std::byte>{payload});

    // Must succeed (not fail) — just silently dropped
    REQUIRE(result.has_value());
    CHECK(ctx.sent.empty());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
}

// ---------------------------------------------------------------------------
// 19. SID_NULL (keepalive) is accepted in every state
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in Connecting",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Connecting);
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in LoggedIn",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 20. SID_PING echo — cookie is reflected verbatim
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_PING echoes cookie verbatim", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_ping(0xCAFEBABEu);
    auto result  = fsm.dispatch(sid::kPing,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kPing);
    // The reply body must contain the same 4-byte cookie
    REQUIRE(ctx.last()->payload.size() >= 4);
    const std::uint32_t echoed =
        static_cast<std::uint32_t>(ctx.last()->payload[0])
      | (static_cast<std::uint32_t>(ctx.last()->payload[1]) <<  8)
      | (static_cast<std::uint32_t>(ctx.last()->payload[2]) << 16)
      | (static_cast<std::uint32_t>(ctx.last()->payload[3]) << 24);
    CHECK(echoed == 0xCAFEBABEu);
    CHECK(fsm.state() == ConnectionState::Connecting);
}

// ---------------------------------------------------------------------------
// 21. SID_LEAVECHAT transitions InChannel → LoggedIn
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_LEAVECHAT transitions InChannel→LoggedIn",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kLeaveChannel,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 22. SID_AUTH_ACCOUNTLOGON in Connecting state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_ACCOUNTLOGON in Connecting state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_accountlogon("alice");
    auto result  = fsm.dispatch(sid::kAuthAccountLogon,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 23. SID_AUTH_ACCOUNTLOGONPROOF in Connecting state → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_ACCOUNTLOGONPROOF in Connecting state is rejected",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_accountlogonproof();
    auto result  = fsm.dispatch(sid::kAuthAccountLogonProof,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 24. AUTH_INFO reply contains logon_type field
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_AUTH_INFO reply has logon_type in body",
          "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthInfo);
    // First 4 bytes of reply body = logon_type (LE uint32)
    REQUIRE(ctx.last()->payload.size() >= 4);
    const std::uint32_t logon_type =
        static_cast<std::uint32_t>(ctx.last()->payload[0])
      | (static_cast<std::uint32_t>(ctx.last()->payload[1]) <<  8)
      | (static_cast<std::uint32_t>(ctx.last()->payload[2]) << 16)
      | (static_cast<std::uint32_t>(ctx.last()->payload[3]) << 24);
    // logon_type 2 = NLS (SRP)
    CHECK(logon_type == 2u);
}

// ---------------------------------------------------------------------------
// 25. Full NLS flow: AUTH_INFO → AUTH_CHECK → ACCOUNTLOGON → PROOF → ENTERCHAT
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: full NLS flow reaches InChannel", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // AUTH_INFO
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // AUTH_CHECK (optional but common)
    auto ac = make_auth_check();
    REQUIRE(fsm.dispatch(sid::kAuthCheck,
                         std::span<const std::byte>{ac}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // ACCOUNTLOGON
    auto al = make_accountlogon("charlie");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{al}).has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);

    // ACCOUNTLOGONPROOF
    auto proof = make_accountlogonproof();
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogonProof,
                         std::span<const std::byte>{proof}).has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username() == "charlie");

    // ENTERCHAT
    auto ec = make_enter_chat("charlie", "PXES");
    REQUIRE(fsm.dispatch(sid::kEnterChat,
                         std::span<const std::byte>{ec}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);

    CHECK_FALSE(ctx.closed);
    // Verify we got replies for AUTH_INFO, AUTH_CHECK, ACCOUNTLOGON, PROOF, ENTERCHAT
    CHECK(ctx.count_sid(sid::kAuthInfo)              >= 1);
    CHECK(ctx.count_sid(sid::kAuthCheck)             >= 1);
    CHECK(ctx.count_sid(sid::kAuthAccountLogon)      >= 1);
    CHECK(ctx.count_sid(sid::kAuthAccountLogonProof) >= 1);
    CHECK(ctx.count_sid(sid::kEnterChat)             >= 1);
}

// ---------------------------------------------------------------------------
// 26. Full OLS flow: LOGON_REQUEST → ENTERCHAT → InChannel
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: full OLS flow reaches InChannel", "[connection_fsm]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    // LOGON_REQUEST (legacy OLS single-step auth)
    reach_logged_in_ols(fsm, "bob");
    CHECK(fsm.username() == "bob");
    ctx.sent.clear();

    // ENTERCHAT
    reach_in_channel(fsm, "bob");

    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.count_sid(sid::kEnterChat) >= 1);
}

// ===========================================================================
// NEW TEST CASES — Round 137: InGame state and game lifecycle
// ===========================================================================

// ---------------------------------------------------------------------------
// 27. SID_STARTADVEX in InChannel → InGame (happy path)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX transitions InChannel→InGame",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("MyGame", "", "PXES");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);
    CHECK_FALSE(ctx.closed);

    // FSM must send a SID_STARTADVEX reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kStartGame1);

    // on_game_created must have been called exactly once
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].game_id == fsm.game_id());
    CHECK(ctx.game_events[0].info.game_name == "MyGame");
}

// ---------------------------------------------------------------------------
// 28. SID_STARTADVEX3 also transitions InChannel→InGame
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX3 transitions InChannel→InGame",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_start_game("AdvGame3", "", "WAR3");
    auto result  = fsm.dispatch(sid::kStartGame3,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);

    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].info.game_name == "AdvGame3");
}

// ---------------------------------------------------------------------------
// 29. SID_GETADVLISTEX (JoinGame) in InChannel → InGame (happy path)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX transitions InChannel→InGame",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    auto payload = make_join_game_pkt("FriendGame", "secret");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK(fsm.game_id() != 0u);
    CHECK_FALSE(ctx.closed);

    // FSM must send a SID_GETADVLISTEX reply
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kJoinGame);

    // on_game_joined must have been called exactly once
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Joined);
    CHECK(ctx.game_events[0].game_id == fsm.game_id());
    CHECK(ctx.game_events[0].info.game_name == "FriendGame");
    CHECK(ctx.game_events[0].info.password  == "secret");
}

// ---------------------------------------------------------------------------
// 30. SID_STOPADV (LeaveGame) in InGame → InChannel
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STOPADV transitions InGame→InChannel",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);

    const std::uint32_t gid = fsm.game_id();
    ctx.sent.clear();
    ctx.game_events.clear();

    auto result = fsm.dispatch(sid::kCloseGame,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);
    CHECK_FALSE(ctx.closed);

    // on_game_left must have been called with the correct game_id
    REQUIRE(ctx.game_events.size() == 1);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[0].game_id == gid);
}

// ---------------------------------------------------------------------------
// 31. Disconnect (close()) from InGame → Disconnecting
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: close() from InGame transitions to Disconnecting",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);

    fsm.close();

    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// 32. StartGame while in LoggedIn (not InChannel) → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX in LoggedIn state is rejected",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_start_game("BadGame");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    // No game event should have been fired
    CHECK(ctx.game_events.empty());
}

// ---------------------------------------------------------------------------
// 33. JoinGame while in LoggedIn (not InChannel) → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX in LoggedIn state is rejected",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    ctx.sent.clear();

    auto payload = make_join_game_pkt("BadGame");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// ---------------------------------------------------------------------------
// 34. StartGame while in Connecting → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STARTADVEX in Connecting state is rejected",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_start_game("EarlyGame");
    auto result  = fsm.dispatch(sid::kStartGame1,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// ---------------------------------------------------------------------------
// 35. JoinGame while in Connecting → rejected
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_GETADVLISTEX in Connecting state is rejected",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_join_game_pkt("EarlyJoin");
    auto result  = fsm.dispatch(sid::kJoinGame,
                                std::span<const std::byte>{payload});

    CHECK_FALSE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// ---------------------------------------------------------------------------
// 36. SID_STOPADV while not in a game → silently ignored
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_STOPADV in InChannel is silently ignored",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.sent.clear();
    ctx.game_events.clear();

    // STOPADV while not in a game — should be silently ignored
    auto result = fsm.dispatch(sid::kCloseGame,
                               std::span<const std::byte>{});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.game_events.empty());
}

// ---------------------------------------------------------------------------
// 37. GameInfo metadata is passed through on_game_created
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: StartGame passes GameInfo metadata to context",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // game_type = 3 (Cooperative), with password
    auto payload = make_start_game("CoopMission", "pw123", "STATS", 3u);
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.game_events.size() == 1);
    const auto& ev = ctx.game_events[0];
    CHECK(ev.kind              == FakeContext::GameEvent::Kind::Created);
    CHECK(ev.info.game_name    == "CoopMission");
    CHECK(ev.info.password     == "pw123");
    CHECK(ev.info.game_stats   == "STATS");
    CHECK(ev.info.game_type    == GameType::Cooperative);
}

// ---------------------------------------------------------------------------
// 38. GameInfo metadata is passed through on_game_joined
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: JoinGame passes GameInfo metadata to context",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // game_type = 2 (OneOnOne), with password
    auto payload = make_join_game_pkt("DuelArena", "duel", "", 2u);
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{payload}).has_value());

    REQUIRE(ctx.game_events.size() == 1);
    const auto& ev = ctx.game_events[0];
    CHECK(ev.kind           == FakeContext::GameEvent::Kind::Joined);
    CHECK(ev.info.game_name == "DuelArena");
    CHECK(ev.info.password  == "duel");
    CHECK(ev.info.game_type == GameType::OneOnOne);
}

// ---------------------------------------------------------------------------
// 39. Full game lifecycle: InChannel → InGame (start) → InChannel (leave)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: full StartGame→LeaveGame lifecycle",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "alice");
    reach_in_channel(fsm, "alice");
    ctx.game_events.clear();

    // Start a game
    auto sg = make_start_game("AliceGame");
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{sg}).has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    const std::uint32_t gid = fsm.game_id();
    CHECK(gid != 0u);

    // Leave the game
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);

    // Verify event sequence: Created then Left
    REQUIRE(ctx.game_events.size() == 2);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[0].game_id == gid);
    CHECK(ctx.game_events[1].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[1].game_id == gid);

    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 40. Full join lifecycle: InChannel → InGame (join) → InChannel (leave)
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: full JoinGame→LeaveGame lifecycle",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm, "bob");
    reach_in_channel(fsm, "bob");
    ctx.game_events.clear();

    // Join a game
    auto jg = make_join_game_pkt("BobsGame");
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{jg}).has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    const std::uint32_t gid = fsm.game_id();
    CHECK(gid != 0u);

    // Leave the game
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.state() == ConnectionState::InChannel);
    CHECK(fsm.game_id() == 0u);

    // Verify event sequence: Joined then Left
    REQUIRE(ctx.game_events.size() == 2);
    CHECK(ctx.game_events[0].kind    == FakeContext::GameEvent::Kind::Joined);
    CHECK(ctx.game_events[0].game_id == gid);
    CHECK(ctx.game_events[1].kind    == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[1].game_id == gid);

    CHECK_FALSE(ctx.closed);
}

// ---------------------------------------------------------------------------
// 41. game_id is non-zero while InGame and zero after leaving
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: game_id observer reflects InGame state",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);

    CHECK(fsm.game_id() == 0u);  // not in a game yet

    reach_in_game_via_start(fsm);
    CHECK(fsm.game_id() != 0u);  // now in a game

    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());
    CHECK(fsm.game_id() == 0u);  // left the game
}

// ---------------------------------------------------------------------------
// 42. Two consecutive games get distinct game IDs
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: consecutive games receive distinct game IDs",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    ctx.game_events.clear();

    // First game
    reach_in_game_via_start(fsm, "Game1");
    const std::uint32_t gid1 = fsm.game_id();
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());

    // Second game
    reach_in_game_via_start(fsm, "Game2");
    const std::uint32_t gid2 = fsm.game_id();
    REQUIRE(fsm.dispatch(sid::kCloseGame,
                         std::span<const std::byte>{}).has_value());

    CHECK(gid1 != 0u);
    CHECK(gid2 != 0u);
    CHECK(gid1 != gid2);

    // Four events: Created, Left, Created, Left
    REQUIRE(ctx.game_events.size() == 4);
    CHECK(ctx.game_events[0].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[1].kind == FakeContext::GameEvent::Kind::Left);
    CHECK(ctx.game_events[2].kind == FakeContext::GameEvent::Kind::Created);
    CHECK(ctx.game_events[3].kind == FakeContext::GameEvent::Kind::Left);
}

// ---------------------------------------------------------------------------
// 43. SID_NULL keepalive accepted while InGame
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionFsm: SID_NULL keepalive accepted in InGame",
          "[connection_fsm][ingame]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    reach_logged_in_nls(fsm);
    reach_in_channel(fsm);
    reach_in_game_via_start(fsm);
    ctx.sent.clear();

    auto result = fsm.dispatch(sid::kNull, std::span<const std::byte>{});
    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::InGame);
    CHECK_FALSE(ctx.closed);
}
