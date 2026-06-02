// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_test_fixtures.hpp
/// Shared test doubles, BNCS packet builders, and state-driver helpers for the
/// `application::connection::ConnectionFsm` unit tests.
///
/// The production FSM is split (for the per-TU size cap) into one core file
/// plus one file per state:
///   connection_fsm.cpp                 → dispatch / close / replies (core)
///   connection_fsm_connecting.cpp      → on_auth_info / on_auth_check / on_logon_request
///   connection_fsm_authenticating.cpp  → on_auth_accountlogon / ...proof
///   connection_fsm_loggedin.cpp        → on_enter_chat
///   connection_fsm_inchannel.cpp       → on_join_channel / on_chat_command / ...
///   connection_fsm_ingame.cpp          → on_leave_game / on_d2_char_select / ...
///
/// The unit tests mirror that split (one `*_test.cpp` per production TU) and
/// share the fixtures below. All free functions are `inline` so the header may
/// be included from multiple translation units without ODR violations.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/connection/connection_fsm.hpp"
#include "domain/connection/connection_context.hpp"

namespace pvpgn::test::connection_fsm {

using namespace pvpgn::domain::connection;
using namespace pvpgn::application::connection;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

struct SentPacket {
    std::uint8_t           packet_id;
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
inline void push_le32(std::vector<std::byte>& v, std::uint32_t x) {
    v.push_back(std::byte{static_cast<std::uint8_t>( x        & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >>  8) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 16) & 0xFFu)});
    v.push_back(std::byte{static_cast<std::uint8_t>((x >> 24) & 0xFFu)});
}

/// Append a NUL-terminated string.
inline void push_cstr(std::vector<std::byte>& v, std::string_view s) {
    for (char c : s) v.push_back(std::byte{static_cast<std::uint8_t>(c)});
    v.push_back(std::byte{0});
}

/// Build a minimal SID_AUTH_INFO (0x50) payload.
inline std::vector<std::byte> make_auth_info(std::uint32_t product_id = 0x52415453u /* 'STAR' */) {
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
inline std::vector<std::byte> make_auth_check() {
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
inline std::vector<std::byte> make_accountlogon(std::string_view username = "alice") {
    std::vector<std::byte> p;
    // client_key: 32 bytes (NLS SRP A value, zeros for test)
    for (int i = 0; i < 32; ++i) p.push_back(std::byte{0});
    push_cstr(p, username);
    return p;
}

/// Build a minimal SID_AUTH_ACCOUNTLOGONPROOF (0x54) payload.
inline std::vector<std::byte> make_accountlogonproof() {
    std::vector<std::byte> p;
    // client_proof: 20 bytes (NLS SRP M1, zeros for test)
    for (int i = 0; i < 20; ++i) p.push_back(std::byte{0});
    return p;
}

/// Build a minimal SID_LOGON_REQUEST (0x29) payload.
inline std::vector<std::byte> make_logon_request(std::string_view username = "bob") {
    std::vector<std::byte> p;
    push_le32(p, 0xDEADu);  // client_token
    push_le32(p, 0xBEEFu);  // server_token
    // password_hash: 5 × uint32 (20 bytes)
    for (int i = 0; i < 5; ++i) push_le32(p, 0u);
    push_cstr(p, username);
    return p;
}

/// Build a minimal SID_ENTERCHAT (0x0A) payload.
inline std::vector<std::byte> make_enter_chat(std::string_view username = "alice",
                                              std::string_view statstr  = "PXES") {
    std::vector<std::byte> p;
    push_cstr(p, username);
    push_cstr(p, statstr);
    return p;
}

/// Build a minimal SID_JOINCHANNEL (0x0C) payload.
inline std::vector<std::byte> make_join_channel(std::string_view channel = "Lobby") {
    std::vector<std::byte> p;
    push_le32(p, 0u);  // flags = first join
    push_cstr(p, channel);
    return p;
}

/// Build a minimal SID_CHATCOMMAND (0x0E) payload.
inline std::vector<std::byte> make_chat_command(std::string_view text = "hello") {
    std::vector<std::byte> p;
    push_cstr(p, text);
    return p;
}

/// Build a minimal SID_PING (0x25) payload.
inline std::vector<std::byte> make_ping(std::uint32_t cookie = 0x12345678u) {
    std::vector<std::byte> p;
    push_le32(p, cookie);
    return p;
}

/// Build a SID_STARTADVEX (0x1C) payload.
/// Layout: [0..3] game_state, [4..7] game_type, [8..11] unk, [12..15] ladder,
///         [16..] game_name\0, password\0, stats\0
inline std::vector<std::byte> make_start_game(
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
inline std::vector<std::byte> make_join_game_pkt(
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
// Helpers: drive the FSM to a given state
// ---------------------------------------------------------------------------

/// Drive FSM from Connecting → Authenticating (sends AUTH_INFO).
inline void reach_authenticating(ConnectionFsm& fsm) {
    auto ai = make_auth_info();
    REQUIRE(fsm.dispatch(sid::kAuthInfo,
                         std::span<const std::byte>{ai}).has_value());
    REQUIRE(fsm.state() == ConnectionState::Authenticating);
}

/// Drive FSM from Connecting → LoggedIn via NLS path.
inline void reach_logged_in_nls(ConnectionFsm& fsm,
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
inline void reach_logged_in_ols(ConnectionFsm& fsm,
                                std::string_view username = "bob") {
    auto lr = make_logon_request(username);
    REQUIRE(fsm.dispatch(sid::kLogonRequest,
                         std::span<const std::byte>{lr}).has_value());
    REQUIRE(fsm.state() == ConnectionState::LoggedIn);
}

/// Drive FSM from LoggedIn → InChannel.
inline void reach_in_channel(ConnectionFsm& fsm,
                             std::string_view username = "alice") {
    auto ec = make_enter_chat(username);
    REQUIRE(fsm.dispatch(sid::kEnterChat,
                         std::span<const std::byte>{ec}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InChannel);
}

/// Drive FSM from InChannel → InGame via StartGame.
inline void reach_in_game_via_start(ConnectionFsm& fsm,
                                    std::string_view game_name = "TestGame") {
    auto sg = make_start_game(game_name);
    REQUIRE(fsm.dispatch(sid::kStartGame1,
                         std::span<const std::byte>{sg}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InGame);
}

/// Drive FSM from InChannel → InGame via JoinGame.
inline void reach_in_game_via_join(ConnectionFsm& fsm,
                                   std::string_view game_name = "TestGame") {
    auto jg = make_join_game_pkt(game_name);
    REQUIRE(fsm.dispatch(sid::kJoinGame,
                         std::span<const std::byte>{jg}).has_value());
    REQUIRE(fsm.state() == ConnectionState::InGame);
}

}  // namespace pvpgn::test::connection_fsm
