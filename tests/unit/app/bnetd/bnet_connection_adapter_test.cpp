// SPDX-License-Identifier: GPL-2.0-or-later
/// @file bnet_connection_adapter_test.cpp
/// Unit tests for `BnetConnectionAdapter`.
///
/// Verifies that `BnetConnectionAdapter` correctly bridges the protocol layer
/// (`BnetFsm`) and the domain layer (`ConnectionFsm`) by forwarding raw BNCS
/// packets through `dispatch_to_domain()` and tracking `ConnectionFsm` state
/// transitions.
///
/// Test count: 10 TEST_CASEs, 50+ CHECK/REQUIRE assertions.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/bnetd/bnet_connection_adapter.hpp"
#include "domain/connection/connection_context.hpp"
#include "domain/connection/connection_fsm.hpp"

using namespace pvpgn::app::bnetd;
using namespace pvpgn::domain::connection;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

namespace {

struct SentPacket {
    std::uint8_t           packet_id;
    std::vector<std::byte> payload;
};

/// Mock IConnectionContext that records all I/O calls and game-lifecycle events.
class MockConnectionContext final : public IConnectionContext {
public:
    std::vector<SentPacket> sent;
    bool                    closed{false};
    std::string             remote_addr{"127.0.0.1"};
    std::uint32_t           session_id_val{99};

    struct GameEvent {
        enum class Kind { Created, Joined, Left } kind;
        std::uint32_t game_id{0};
        GameInfo      info;
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

    [[nodiscard]] const SentPacket* last() const {
        if (sent.empty()) return nullptr;
        return &sent.back();
    }

    [[nodiscard]] const GameEvent* last_game_event() const {
        if (game_events.empty()) return nullptr;
        return &game_events.back();
    }
};

// ---------------------------------------------------------------------------
// Packet builders — minimal BNCS payloads (body only, no 4-byte header)
// ---------------------------------------------------------------------------

void push_le32(std::vector<std::byte>& v, std::uint32_t x) {
    v.push_back(std::byte{static_cast<std::uint8_t>( x        & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >>  8) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 16) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 24) & 0xFFu)});
}

void push_cstr(std::vector<std::byte>& v, std::string_view s) {
    for (char c : s) v.push_back(std::byte{static_cast<std::uint8_t>(c)});
    v.push_back(std::byte{0});
}

/// SID_AUTH_INFO (0x50) payload.
std::vector<std::byte> make_auth_info(
    std::uint32_t product_id = 0x52415453u /* 'STAR' */) {
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
    push_cstr(p, "USA");
    push_cstr(p, "United States");
    return p;
}

/// SID_AUTH_CHECK (0x51) payload.
std::vector<std::byte> make_auth_check() {
    std::vector<std::byte> p;
    push_le32(p, 0xABCDu);  // client_token
    push_le32(p, 0u);       // exe_version
    push_le32(p, 0u);       // exe_hash
    push_le32(p, 0u);       // num_keys
    push_le32(p, 0u);       // spawn
    push_cstr(p, "game.exe 01/01/2000 00:00:00 12345");
    push_cstr(p, "Owner");
    return p;
}

/// SID_AUTH_ACCOUNTLOGON (0x53) payload.
std::vector<std::byte> make_accountlogon(std::string_view username = "alice") {
    std::vector<std::byte> p;
    for (int i = 0; i < 32; ++i) p.push_back(std::byte{0});  // client_key
    push_cstr(p, username);
    return p;
}

/// SID_AUTH_ACCOUNTLOGONPROOF (0x54) payload.
std::vector<std::byte> make_accountlogonproof() {
    std::vector<std::byte> p;
    for (int i = 0; i < 20; ++i) p.push_back(std::byte{0});  // client_proof
    return p;
}

/// SID_LOGON_REQUEST (0x29) payload — legacy OLS single-step login.
std::vector<std::byte> make_logon_request(std::string_view username = "bob") {
    std::vector<std::byte> p;
    push_le32(p, 0xDEADu);  // client_token
    push_le32(p, 0xBEEFu);  // server_token
    for (int i = 0; i < 5; ++i) push_le32(p, 0u);  // password_hash
    push_cstr(p, username);
    return p;
}

/// SID_ENTERCHAT (0x0A) payload.
std::vector<std::byte> make_enter_chat(std::string_view username = "alice",
                                        std::string_view statstr  = "PXES") {
    std::vector<std::byte> p;
    push_cstr(p, username);
    push_cstr(p, statstr);
    return p;
}

/// SID_LEAVECHAT (0x28) payload — empty.
std::vector<std::byte> make_leave_chat() {
    return {};
}

/// SID_STARTADVEX (0x1C) payload.
std::vector<std::byte> make_start_game(
    std::string_view game_name = "TestGame",
    std::string_view password  = "",
    std::string_view stats     = "PXES",
    std::uint32_t    game_type = 0u) {
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

/// SID_GETADVLISTEX (0x09) payload — join game.
std::vector<std::byte> make_join_game_pkt(
    std::string_view game_name = "TestGame",
    std::string_view password  = "",
    std::string_view stats     = "",
    std::uint32_t    game_type = 0u) {
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

/// SID_STOPADV (0x07) payload — leave/close game.
std::vector<std::byte> make_close_game() {
    return {};
}

// ---------------------------------------------------------------------------
// Helper: drive adapter to a given state via dispatch_to_domain()
// ---------------------------------------------------------------------------

/// Drive adapter from Connecting → Authenticating.
void reach_authenticating(BnetConnectionAdapter& adapter) {
    auto ai = make_auth_info();
    REQUIRE(adapter.dispatch_to_domain(
        sid::kAuthInfo,
        std::span<const std::byte>{ai}).has_value());
    REQUIRE(adapter.connection_fsm().state() == ConnectionState::Authenticating);
}

/// Drive adapter from Connecting → LoggedIn via NLS path.
void reach_logged_in_nls(BnetConnectionAdapter& adapter,
                          std::string_view username = "alice") {
    reach_authenticating(adapter);
    auto al = make_accountlogon(username);
    REQUIRE(adapter.dispatch_to_domain(
        sid::kAuthAccountLogon,
        std::span<const std::byte>{al}).has_value());
    auto proof = make_accountlogonproof();
    REQUIRE(adapter.dispatch_to_domain(
        sid::kAuthAccountLogonProof,
        std::span<const std::byte>{proof}).has_value());
    REQUIRE(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
}

/// Drive adapter from Connecting → LoggedIn via legacy OLS path.
[[maybe_unused]] void reach_logged_in_ols(BnetConnectionAdapter& adapter,
                                           std::string_view username = "bob") {
    auto lr = make_logon_request(username);
    REQUIRE(adapter.dispatch_to_domain(
        sid::kLogonRequest,
        std::span<const std::byte>{lr}).has_value());
    REQUIRE(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
}

/// Drive adapter from LoggedIn → InChannel.
void reach_in_channel(BnetConnectionAdapter& adapter) {
    auto ec = make_enter_chat();
    REQUIRE(adapter.dispatch_to_domain(
        sid::kEnterChat,
        std::span<const std::byte>{ec}).has_value());
    REQUIRE(adapter.connection_fsm().state() == ConnectionState::InChannel);
}

/// Drive adapter from InChannel → InGame.
void reach_in_game(BnetConnectionAdapter& adapter) {
    auto sg = make_start_game();
    REQUIRE(adapter.dispatch_to_domain(
        sid::kStartGame1,
        std::span<const std::byte>{sg}).has_value());
    REQUIRE(adapter.connection_fsm().state() == ConnectionState::InGame);
}

}  // namespace

// ===========================================================================
// TEST CASES
// ===========================================================================

// ---------------------------------------------------------------------------
// TC-1: Construction — initial state is Connecting
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: initial state is Connecting", "[adapter]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 1u};

    CHECK(adapter.connection_fsm().state() == ConnectionState::Connecting);
    CHECK(adapter.connection_fsm().session_id() == 1u);
    CHECK(!ctx.closed);
    CHECK(ctx.sent.empty());
}

// ---------------------------------------------------------------------------
// TC-2: Auth flow — on_auth_info → ConnectionFsm transitions to Authenticating
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: AUTH_INFO transitions to Authenticating",
          "[adapter][auth]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 2u};

    auto ai = make_auth_info();
    auto st = adapter.dispatch_to_domain(
        sid::kAuthInfo, std::span<const std::byte>{ai});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::Authenticating);
    // ConnectionFsm sends SID_AUTH_INFO reply (0x50)
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthInfo);
}

// ---------------------------------------------------------------------------
// TC-3: Logon flow — on_logon_request → ConnectionFsm transitions to LoggedIn
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: LOGON_REQUEST (OLS) transitions to LoggedIn",
          "[adapter][auth]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 3u};

    auto lr = make_logon_request("bob");
    auto st = adapter.dispatch_to_domain(
        sid::kLogonRequest, std::span<const std::byte>{lr});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
}

// ---------------------------------------------------------------------------
// TC-4: NLS auth flow — AUTH_INFO → AUTH_CHECK → ACCOUNTLOGON → PROOF → LoggedIn
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: NLS auth flow reaches LoggedIn",
          "[adapter][auth]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 4u};

    // Step 1: AUTH_INFO → Authenticating
    {
        auto ai = make_auth_info();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthInfo, std::span<const std::byte>{ai}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::Authenticating);
    }

    // Step 2: AUTH_CHECK — stays Authenticating
    {
        auto ac = make_auth_check();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthCheck, std::span<const std::byte>{ac}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::Authenticating);
    }

    // Step 3: ACCOUNTLOGON — stays Authenticating
    {
        auto al = make_accountlogon("alice");
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthAccountLogon, std::span<const std::byte>{al}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::Authenticating);
    }

    // Step 4: ACCOUNTLOGONPROOF → LoggedIn
    {
        auto proof = make_accountlogonproof();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthAccountLogonProof,
            std::span<const std::byte>{proof}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
    }
}

// ---------------------------------------------------------------------------
// TC-5: Channel flow — on_enter_chat → ConnectionFsm transitions to InChannel
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: ENTERCHAT transitions to InChannel",
          "[adapter][channel]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 5u};

    reach_logged_in_nls(adapter);

    auto ec = make_enter_chat("alice");
    auto st = adapter.dispatch_to_domain(
        sid::kEnterChat, std::span<const std::byte>{ec});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::InChannel);
}

// ---------------------------------------------------------------------------
// TC-6: Leave channel — on_leave_chat → ConnectionFsm transitions back to LoggedIn
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: LEAVECHAT transitions back to LoggedIn",
          "[adapter][channel]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 6u};

    reach_logged_in_nls(adapter);
    reach_in_channel(adapter);

    CHECK(adapter.connection_fsm().state() == ConnectionState::InChannel);

    auto lc = make_leave_chat();
    auto st = adapter.dispatch_to_domain(
        sid::kLeaveChannel, std::span<const std::byte>{lc});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
}

// ---------------------------------------------------------------------------
// TC-7: Game flow — on_start_game → ConnectionFsm transitions to InGame
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: STARTADVEX transitions to InGame",
          "[adapter][game]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 7u};

    reach_logged_in_nls(adapter);
    reach_in_channel(adapter);

    auto sg = make_start_game("MyGame");
    auto st = adapter.dispatch_to_domain(
        sid::kStartGame1, std::span<const std::byte>{sg});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::InGame);

    // on_game_created callback should have fired
    REQUIRE(ctx.last_game_event() != nullptr);
    CHECK(ctx.last_game_event()->kind == MockConnectionContext::GameEvent::Kind::Created);
    CHECK(ctx.last_game_event()->game_id != 0u);
    CHECK(ctx.last_game_event()->info.game_name == "MyGame");
}

// ---------------------------------------------------------------------------
// TC-8: Leave game — on_close_game → ConnectionFsm transitions back to InChannel
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: STOPADV transitions back to InChannel",
          "[adapter][game]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 8u};

    reach_logged_in_nls(adapter);
    reach_in_channel(adapter);
    reach_in_game(adapter);

    CHECK(adapter.connection_fsm().state() == ConnectionState::InGame);

    auto cg = make_close_game();
    auto st = adapter.dispatch_to_domain(
        sid::kCloseGame, std::span<const std::byte>{cg});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::InChannel);

    // on_game_left callback should have fired
    REQUIRE(ctx.last_game_event() != nullptr);
    CHECK(ctx.last_game_event()->kind == MockConnectionContext::GameEvent::Kind::Left);
}

// ---------------------------------------------------------------------------
// TC-9: Disconnect — close() → ConnectionFsm transitions to Disconnecting
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: close() transitions to Disconnecting",
          "[adapter][lifecycle]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 9u};

    reach_logged_in_nls(adapter);
    reach_in_channel(adapter);

    adapter.connection_fsm().close();

    CHECK(adapter.connection_fsm().state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);
}

// ---------------------------------------------------------------------------
// TC-10: Full lifecycle — auth → channel → game → leave game → disconnect
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: full lifecycle auth→channel→game→leave→disconnect",
          "[adapter][lifecycle]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 10u};

    // 1. Start in Connecting
    CHECK(adapter.connection_fsm().state() == ConnectionState::Connecting);

    // 2. AUTH_INFO → Authenticating
    {
        auto ai = make_auth_info();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthInfo, std::span<const std::byte>{ai}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::Authenticating);
    }

    // 3. ACCOUNTLOGON + PROOF → LoggedIn
    {
        auto al = make_accountlogon("charlie");
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthAccountLogon, std::span<const std::byte>{al}).has_value());
        auto proof = make_accountlogonproof();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kAuthAccountLogonProof,
            std::span<const std::byte>{proof}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::LoggedIn);
    }

    // 4. ENTERCHAT → InChannel
    {
        auto ec = make_enter_chat("charlie");
        REQUIRE(adapter.dispatch_to_domain(
            sid::kEnterChat, std::span<const std::byte>{ec}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::InChannel);
    }

    // 5. STARTADVEX → InGame
    {
        auto sg = make_start_game("FullLifecycleGame");
        REQUIRE(adapter.dispatch_to_domain(
            sid::kStartGame1, std::span<const std::byte>{sg}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::InGame);
        REQUIRE(ctx.last_game_event() != nullptr);
        CHECK(ctx.last_game_event()->kind ==
              MockConnectionContext::GameEvent::Kind::Created);
        CHECK(ctx.last_game_event()->info.game_name == "FullLifecycleGame");
    }

    // 6. STOPADV → InChannel
    {
        auto cg = make_close_game();
        REQUIRE(adapter.dispatch_to_domain(
            sid::kCloseGame, std::span<const std::byte>{cg}).has_value());
        CHECK(adapter.connection_fsm().state() == ConnectionState::InChannel);
        REQUIRE(ctx.last_game_event() != nullptr);
        CHECK(ctx.last_game_event()->kind ==
              MockConnectionContext::GameEvent::Kind::Left);
    }

    // 7. close() → Disconnecting
    adapter.connection_fsm().close();
    CHECK(adapter.connection_fsm().state() == ConnectionState::Disconnecting);
    CHECK(ctx.closed);

    // 8. Packets after Disconnecting are silently dropped
    {
        auto ai = make_auth_info();
        auto st = adapter.dispatch_to_domain(
            sid::kAuthInfo, std::span<const std::byte>{ai});
        REQUIRE(st.has_value());  // silently ignored, not an error
        CHECK(adapter.connection_fsm().state() == ConnectionState::Disconnecting);
    }
}

// ---------------------------------------------------------------------------
// TC-11: IConnectionContext forwarding — send_packet forwarded to mock
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: send_packet forwarded to IConnectionContext",
          "[adapter][io]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 11u};

    // Trigger AUTH_INFO which causes ConnectionFsm to send a reply
    auto ai = make_auth_info();
    REQUIRE(adapter.dispatch_to_domain(
        sid::kAuthInfo, std::span<const std::byte>{ai}).has_value());

    // ConnectionFsm should have sent SID_AUTH_INFO reply (0x50) via ctx
    CHECK(!ctx.sent.empty());
    bool found_auth_info_reply = false;
    for (const auto& pkt : ctx.sent) {
        if (pkt.packet_id == sid::kAuthInfo) {
            found_auth_info_reply = true;
            break;
        }
    }
    CHECK(found_auth_info_reply);
}

// ---------------------------------------------------------------------------
// TC-12: IConnectionContext forwarding — get_remote_address / get_session_id
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: get_remote_address and get_session_id forwarded",
          "[adapter][io]") {
    MockConnectionContext ctx;
    ctx.remote_addr    = "10.0.0.1";
    ctx.session_id_val = 42u;

    BnetConnectionAdapter adapter{ctx, 42u};

    CHECK(adapter.get_remote_address() == "10.0.0.1");
    CHECK(adapter.get_session_id() == 42u);
}

// ---------------------------------------------------------------------------
// TC-13: Join game flow — GETADVLISTEX → InGame
// ---------------------------------------------------------------------------
TEST_CASE("BnetConnectionAdapter: GETADVLISTEX (join game) transitions to InGame",
          "[adapter][game]") {
    MockConnectionContext ctx;
    BnetConnectionAdapter adapter{ctx, 13u};

    reach_logged_in_nls(adapter);
    reach_in_channel(adapter);

    auto jg = make_join_game_pkt("ExistingGame");
    auto st = adapter.dispatch_to_domain(
        sid::kJoinGame, std::span<const std::byte>{jg});

    REQUIRE(st.has_value());
    CHECK(adapter.connection_fsm().state() == ConnectionState::InGame);

    // on_game_joined callback should have fired
    REQUIRE(ctx.last_game_event() != nullptr);
    CHECK(ctx.last_game_event()->kind ==
          MockConnectionContext::GameEvent::Kind::Joined);
}
