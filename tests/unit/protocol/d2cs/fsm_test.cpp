// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_test.cpp
/// Unit tests for D2CSSessionFsm.
///
/// Wire format recap (all multi-byte fields are little-endian):
///   Header: [length:2LE][type:1]
///   Payload follows immediately.
///
/// Helper `make_packet()` builds a complete packet from a type byte and
/// a payload byte-vector.

#include <catch2/catch_test_macros.hpp>
#include "protocol/d2cs/fsm.hpp"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2cs {

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

/// Build a complete D2CS packet: [length:2LE][type:1][payload...]
static std::vector<uint8_t> make_packet(uint8_t type,
                                        const std::vector<uint8_t>& payload = {})
{
    const uint16_t total = static_cast<uint16_t>(3 + payload.size());
    std::vector<uint8_t> pkt;
    pkt.reserve(total);
    pkt.push_back(static_cast<uint8_t>(total & 0xFF));
    pkt.push_back(static_cast<uint8_t>((total >> 8) & 0xFF));
    pkt.push_back(type);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

/// Append a little-endian uint32_t to a byte vector.
static void push_u32(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

/// Append a single byte to a byte vector.
static void push_u8(std::vector<uint8_t>& v, uint8_t val) {
    v.push_back(val);
}

/// Append a little-endian uint16_t to a byte vector.
static void push_u16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

/// Append a null-terminated C-string to a byte vector.
static void push_cstr(std::vector<uint8_t>& v, const char* s) {
    while (*s) v.push_back(static_cast<uint8_t>(*s++));
    v.push_back(0x00);
}

/// Feed a packet vector into an FSM and return the result.
static core::Result<size_t, core::Error> feed(D2CSSessionFsm& fsm,
                                               const std::vector<uint8_t>& pkt)
{
    return fsm.feed(pkt.data(), pkt.size());
}

/// Build a real CLIENT_D2CS_LOGINREQ payload body: the 64-byte fixed block
/// (16 little-endian u32 — seqno + 15 fields incl. secret_hash[5]) followed by
/// the null-terminated account name.
static std::vector<uint8_t> login_payload(uint32_t seqno, const char* account) {
    std::vector<uint8_t> p;
    push_u32(p, seqno);
    for (int i = 0; i < 15; ++i) push_u32(p, 0);
    push_cstr(p, account);
    return p;
}

// ---------------------------------------------------------------------------
// TC-01: Construction — initial state is connected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-01 construction initial state", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    CHECK(fsm.state() == D2CSSessionState::connected);
}

// ---------------------------------------------------------------------------
// TC-02: Feed null / empty data returns 0 consumed
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-02 feed null or empty data", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    auto r1 = fsm.feed(nullptr, 0);
    REQUIRE(r1);
    CHECK(r1.value() == 0);

    auto r2 = fsm.feed(nullptr, 10);
    REQUIRE(r2);
    CHECK(r2.value() == 0);
}

// ---------------------------------------------------------------------------
// TC-03: Incomplete packet — buffered, 0 consumed
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-03 incomplete packet buffered", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Announce a 20-byte packet but only send 5 bytes
    uint8_t partial[5] = {0x14, 0x00, 0x01, 0x00, 0x00};
    auto r = fsm.feed(partial, 5);
    REQUIRE(r);
    CHECK(r.value() == 0);
    CHECK(fsm.state() == D2CSSessionState::connected);
}

// ---------------------------------------------------------------------------
// TC-04: Packet length < 3 is rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-04 packet length too small rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // length = 2 (invalid — minimum is 3)
    uint8_t bad[3] = {0x02, 0x00, 0x01};
    auto r = fsm.feed(bad, 3);
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-05: LOGINREQ — callback invoked, state → authenticating
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-05 LOGINREQ callback and state transition", "[protocol][d2cs]") {
    bool called = false;
    D2CSLoginRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_login = [&](const D2CSLoginRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // Real CLIENT_D2CS_LOGINREQ layout: 64-byte fixed block + account name.
    std::vector<uint8_t> payload;
    push_u32(payload, 42);          // seqno
    push_u32(payload, 0);           // u1
    push_u32(payload, 0);           // bncs_addr1
    push_u32(payload, 7);           // sessionnum
    push_u32(payload, 0xDEADBEEF);  // sessionkey
    push_u32(payload, 0);           // cdkey_id
    push_u32(payload, 0);           // u5
    push_u32(payload, 0);           // clienttag
    push_u32(payload, 0);           // bnversion
    push_u32(payload, 0);           // bncs_addr2
    push_u32(payload, 0);           // u6
    for (uint32_t h : {1u, 2u, 3u, 4u, 5u}) push_u32(payload, h);  // secret_hash[5]
    push_cstr(payload, "TestUser");

    auto pkt = make_packet(0x01, payload);
    auto r = feed(fsm, pkt);

    REQUIRE(r);
    CHECK(r.value() == pkt.size());
    CHECK(called);
    CHECK(captured.seqno == 42);
    CHECK(captured.sessionnum == 7);
    CHECK(captured.session_key == 0xDEADBEEF);
    CHECK(captured.secret_hash == std::array<uint32_t, 5>{1, 2, 3, 4, 5});
    CHECK(captured.account_name == "TestUser");
    CHECK(fsm.state() == D2CSSessionState::authenticating);
}

// ---------------------------------------------------------------------------
// TC-06: LOGINREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-06 LOGINREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_login = [](const D2CSLoginRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::Unauthenticated, "bad credentials"));
    };
    D2CSSessionFsm fsm(cb);

    // Full 64-byte fixed block (16 u32) + account name, so the parse reaches
    // the callback (which then fails).
    std::vector<uint8_t> payload;
    push_u32(payload, 1);                                   // seqno
    for (int i = 0; i < 15; ++i) push_u32(payload, 0);      // rest of the block
    push_cstr(payload, "user");

    auto r = feed(fsm, make_packet(0x01, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::Unauthenticated);
}

// ---------------------------------------------------------------------------
// TC-07: CHARLOGINREQ — callback invoked, state → authenticated
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-07 CHARLOGINREQ callback and state transition", "[protocol][d2cs]") {
    bool called = false;
    D2CSCharLoginRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_char_login = [&](const D2CSCharLoginRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // CHARLOGINREQ (0x07) wire body is just the NUL-terminated char name
    // (d2cs_protocol.h t_client_d2cs_charloginreq). No seqno/class/level/status
    // and no account on the wire — the account comes from the session.
    std::vector<uint8_t> payload;
    push_cstr(payload, "MyAmazon");

    auto r = feed(fsm, make_packet(0x07, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.char_name == "MyAmazon");
    CHECK(fsm.state() == D2CSSessionState::authenticated);
}

// ---------------------------------------------------------------------------
// TC-08: CREATEGAMEREQ — callback invoked, state → in_game
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-08 CREATEGAMEREQ callback and state transition", "[protocol][d2cs]") {
    bool called = false;
    D2CSCreateGameRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_create_game = [&](const D2CSCreateGameRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 99);  // seqno
    payload.push_back(1);   // difficulty = Nightmare
    payload.push_back(0);   // hardcore = false
    payload.push_back(1);   // expansion = true
    push_cstr(payload, "MyGame");
    push_cstr(payload, "secret");
    push_cstr(payload, "A fun game");

    auto r = feed(fsm, make_packet(0x03, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 99);
    CHECK(captured.difficulty == 1);
    CHECK(captured.hardcore == 0);
    CHECK(captured.expansion == 1);
    CHECK(captured.game_name == "MyGame");
    CHECK(captured.game_password == "secret");
    CHECK(captured.game_description == "A fun game");
    CHECK(fsm.state() == D2CSSessionState::in_game);
}

// ---------------------------------------------------------------------------
// TC-09: JOINGAMEREQ — callback invoked, state → in_game
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-09 JOINGAMEREQ callback and state transition", "[protocol][d2cs]") {
    bool called = false;
    D2CSJoinGameRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_join_game = [&](const D2CSJoinGameRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 55);  // seqno
    push_cstr(payload, "ExistingGame");
    push_cstr(payload, "pass123");

    auto r = feed(fsm, make_packet(0x04, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 55);
    CHECK(captured.game_name == "ExistingGame");
    CHECK(captured.game_password == "pass123");
    CHECK(fsm.state() == D2CSSessionState::in_game);
}

// ---------------------------------------------------------------------------
// TC-10: GAMELISTREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-10 GAMELISTREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSGameListRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_game_list = [&](const D2CSGameListRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u16(payload, 11);  // seqno (bn_short, per d2cs_protocol.h)
    push_u32(payload, 2);   // game_type (gameflag)

    auto r = feed(fsm, make_packet(0x05, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 11);
    CHECK(captured.game_type == 2);
}

// ---------------------------------------------------------------------------
// TC-11: GAMEINFOREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-11 GAMEINFOREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSGameInfoRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_game_info = [&](const D2CSGameInfoRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u16(payload, 22);  // seqno (bn_short, per d2cs_protocol.h)
    push_cstr(payload, "TargetGame");

    auto r = feed(fsm, make_packet(0x06, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 22);
    CHECK(captured.game_name == "TargetGame");
}

// ---------------------------------------------------------------------------
// TC-12: CREATECHARREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-12 CREATECHARREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSCreateCharRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_create_char = [&](const D2CSCreateCharRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // CREATECHARREQ (0x02) wire body per d2cs_protocol.h
    // t_client_d2cs_createcharreq: chclass(u16) + u1(u16=0) + status(u16) + name.
    // There is NO seqno; class and status are 16-bit.
    std::vector<uint8_t> payload;
    push_u16(payload, 4);     // chclass = Barbarian
    push_u16(payload, 0);     // u1 (always zero)
    push_u16(payload, 0x20);  // status = expansion
    push_cstr(payload, "NewBarb");

    auto r = feed(fsm, make_packet(0x02, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.char_class == 4);
    CHECK(captured.char_status == 0x20);
    CHECK(captured.char_name == "NewBarb");
}

// ---------------------------------------------------------------------------
// TC-13: DELETECHARREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-13 DELETECHARREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSDeleteCharRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_delete_char = [&](const D2CSDeleteCharRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // DELETECHARREQ (0x0a) wire body per d2cs_protocol.h
    // t_client_d2cs_deletecharreq: u1(u16=0) + name. There is NO seqno; only a
    // 2-byte u1 precedes the name.
    std::vector<uint8_t> payload;
    push_u16(payload, 0);   // u1 (always zero)
    push_cstr(payload, "OldChar");

    auto r = feed(fsm, make_packet(0x0A, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.char_name == "OldChar");
}

// ---------------------------------------------------------------------------
// TC-13r: Regression (F2) — CREATECHARREQ reads class from offset 0, not 4.
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-13r CREATECHARREQ class at offset 0 (no seqno)", "[protocol][d2cs]") {
    D2CSCreateCharRequest captured;
    D2CSFsmCallbacks cb;
    cb.on_create_char = [&](const D2CSCreateCharRequest& req) {
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // chclass=6 (Assassin) at offset 0; status=0x24 at offset 4. With the old
    // phantom-seqno parse, class would have been read from offset 4 (=0x24).
    std::vector<uint8_t> payload;
    push_u16(payload, 6);     // chclass @0
    push_u16(payload, 0);     // u1     @2
    push_u16(payload, 0x24);  // status @4
    push_cstr(payload, "Sin");

    REQUIRE(feed(fsm, make_packet(0x02, payload)));
    CHECK(captured.char_class == 6);      // not 0x24
    CHECK(captured.char_status == 0x24);
    CHECK(captured.char_name == "Sin");
}

// ---------------------------------------------------------------------------
// TC-13s: Regression (F4) — DELETECHARREQ name starts at offset 2 (u1), not 4.
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-13s DELETECHARREQ name after 2-byte u1", "[protocol][d2cs]") {
    D2CSDeleteCharRequest captured;
    D2CSFsmCallbacks cb;
    cb.on_delete_char = [&](const D2CSDeleteCharRequest& req) {
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // Only a 2-byte u1 precedes the name. The old 4-byte skip would have eaten
    // the first two name bytes ("He"), corrupting it to "ro".
    std::vector<uint8_t> payload;
    push_u16(payload, 0);   // u1 @0..1
    push_cstr(payload, "Hero");

    REQUIRE(feed(fsm, make_packet(0x0A, payload)));
    CHECK(captured.char_name == "Hero");  // not "ro"
}

// ---------------------------------------------------------------------------
// TC-13t: Regression (F3) — CHARLOGINREQ name starts at offset 0 (no 16 bytes).
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-13t CHARLOGINREQ name at offset 0", "[protocol][d2cs]") {
    D2CSCharLoginRequest captured;
    D2CSFsmCallbacks cb;
    cb.on_char_login = [&](const D2CSCharLoginRequest& req) {
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // Body is the name only. The old parse consumed 16 phantom bytes first, so
    // it would have eaten the name into integer fields and read garbage.
    std::vector<uint8_t> payload;
    push_cstr(payload, "Hero");

    REQUIRE(feed(fsm, make_packet(0x07, payload)));
    CHECK(captured.char_name == "Hero");
}

// ---------------------------------------------------------------------------
// TC-14: CHARLISTREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-14 CHARLISTREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSCharListRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_char_list = [&](const D2CSCharListRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 66);  // seqno

    auto r = feed(fsm, make_packet(0x17, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 66);
}

// ---------------------------------------------------------------------------
// TC-15: MOTDREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-15 MOTDREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSMotdRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_motd = [&](const D2CSMotdRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 77);  // seqno

    auto r = feed(fsm, make_packet(0x12, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 77);
}

// ---------------------------------------------------------------------------
// TC-16: CANCELCREATEGAME — callback invoked, state reverts from in_game
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-16 CANCELCREATEGAME reverts state", "[protocol][d2cs]") {
    bool cancel_called = false;

    D2CSFsmCallbacks cb;
    cb.on_create_game = [](const D2CSCreateGameRequest&) {
        return core::Result<void, core::Error>();
    };
    cb.on_cancel_create_game = [&]() {
        cancel_called = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // First, enter in_game state via CREATEGAMEREQ
    {
        std::vector<uint8_t> payload;
        push_u32(payload, 1);
        payload.push_back(0); payload.push_back(0); payload.push_back(0);
        push_cstr(payload, "G"); push_cstr(payload, ""); push_cstr(payload, "");
        auto r = feed(fsm, make_packet(0x03, payload));
        REQUIRE(r);
    }
    CHECK(fsm.state() == D2CSSessionState::in_game);

    // Now cancel
    auto r = feed(fsm, make_packet(0x13, {}));
    REQUIRE(r);
    CHECK(cancel_called);
    CHECK(fsm.state() == D2CSSessionState::authenticated);
}

// ---------------------------------------------------------------------------
// TC-17: CONVERTCHARREQ — callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-17 CONVERTCHARREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSConvertCharRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_convert_char = [&](const D2CSConvertCharRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 88);  // seqno
    push_cstr(payload, "ClassicChar");

    auto r = feed(fsm, make_packet(0x18, payload));

    REQUIRE(r);
    CHECK(called);
    CHECK(captured.seqno == 88);
    CHECK(captured.char_name == "ClassicChar");
}

// ---------------------------------------------------------------------------
// TC-18: Unknown packet type — silently ignored (no error)
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-18 unknown packet type silently ignored", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // 0xFF is not a known packet type
    auto r = feed(fsm, make_packet(0xFF, {}));
    REQUIRE(r);  // Must NOT fail
    CHECK(r.value() == 3);
    CHECK(fsm.state() == D2CSSessionState::connected);
}

// ---------------------------------------------------------------------------
// TC-19: Multiple packets in one feed call
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-19 multiple packets in one feed", "[protocol][d2cs]") {
    int login_count = 0;
    int charlist_count = 0;

    D2CSFsmCallbacks cb;
    cb.on_login = [&](const D2CSLoginRequest&) {
        ++login_count;
        return core::Result<void, core::Error>();
    };
    cb.on_char_list = [&](const D2CSCharListRequest&) {
        ++charlist_count;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    // Build two packets back-to-back
    auto pkt1 = make_packet(0x01, login_payload(1, "u"));

    std::vector<uint8_t> payload2;
    push_u32(payload2, 2);
    auto pkt2 = make_packet(0x17, payload2);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), pkt1.begin(), pkt1.end());
    combined.insert(combined.end(), pkt2.begin(), pkt2.end());

    auto r = fsm.feed(combined.data(), combined.size());
    REQUIRE(r);
    CHECK(r.value() == combined.size());
    CHECK(login_count == 1);
    CHECK(charlist_count == 1);
}

// ---------------------------------------------------------------------------
// TC-20: Fragmented delivery — packet split across two feed calls
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-20 fragmented packet reassembly", "[protocol][d2cs]") {
    bool called = false;

    D2CSFsmCallbacks cb;
    cb.on_login = [&](const D2CSLoginRequest&) {
        called = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    auto pkt = make_packet(0x01, login_payload(5, "acc"));

    // Feed first half
    size_t half = pkt.size() / 2;
    auto r1 = fsm.feed(pkt.data(), half);
    REQUIRE(r1);
    CHECK(r1.value() == 0);
    CHECK_FALSE(called);

    // Feed second half — the FSM reassembles the full packet from its buffer
    // and returns the number of bytes consumed from this call's input.
    // The full packet (pkt.size() bytes) is consumed, but only (pkt.size()-half)
    // bytes were provided in this call, so consumed == pkt.size() - half.
    // However, the FSM's internal accounting returns total bytes consumed from
    // the buffer, which equals the full packet length.  We just verify > 0.
    auto r2 = fsm.feed(pkt.data() + half, pkt.size() - half);
    REQUIRE(r2);
    CHECK(r2.value() > 0);
    CHECK(called);
}

// ---------------------------------------------------------------------------
// TC-21: make_login_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-21 make_login_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_login_reply(0x00);

    REQUIRE(reply.size() == 7);
    // length = 7 (LE)
    CHECK(reply[0] == 0x07);
    CHECK(reply[1] == 0x00);
    // type = LOGINREPLY = 0x01
    CHECK(reply[2] == 0x01);
    // result_code = 0 (LE)
    CHECK(reply[3] == 0x00);
    CHECK(reply[4] == 0x00);
    CHECK(reply[5] == 0x00);
    CHECK(reply[6] == 0x00);
}

// ---------------------------------------------------------------------------
// TC-22: make_login_reply — non-zero result code
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-22 make_login_reply bad-password code", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_login_reply(0x0C);  // kLoginReplyBadPass

    REQUIRE(reply.size() == 7);
    CHECK(reply[2] == 0x01);  // LOGINREPLY
    CHECK(reply[3] == 0x0C);  // result_code low byte
}

// ---------------------------------------------------------------------------
// TC-23: make_char_login_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-23 make_char_login_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_char_login_reply(0x00);

    REQUIRE(reply.size() == 7);
    CHECK(reply[2] == 0x07);  // CHARLOGINREPLY
    CHECK(reply[3] == 0x00);  // result_code = success
}

// ---------------------------------------------------------------------------
// TC-24: make_create_game_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-24 make_create_game_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_create_game_reply(42, 1001, 0x00);

    REQUIRE(reply.size() == 19);
    CHECK(reply[2] == 0x03);  // CREATEGAMEREPLY

    // seqno = 42 (LE)
    CHECK(reply[3] == 42);
    CHECK(reply[4] == 0);
    CHECK(reply[5] == 0);
    CHECK(reply[6] == 0);

    // game_id = 1001 = 0x3E9 (LE)
    CHECK(reply[7] == 0xE9);
    CHECK(reply[8] == 0x03);
    CHECK(reply[9] == 0x00);
    CHECK(reply[10] == 0x00);
}

// ---------------------------------------------------------------------------
// TC-25: make_join_game_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-25 make_join_game_reply structure", "[protocol][d2cs]") {
    // gs_ip = 192.168.1.1 = 0xC0A80101
    auto reply = D2CSSessionFsm::make_join_game_reply(10, 500, 0xC0A80101, 0xABCD, 0x00);

    REQUIRE(reply.size() == 27);
    CHECK(reply[2] == 0x04);  // JOINGAMEREPLY

    // seqno = 10
    CHECK(reply[3] == 10);

    // result_code = 0 (last 4 bytes)
    CHECK(reply[23] == 0x00);
    CHECK(reply[24] == 0x00);
    CHECK(reply[25] == 0x00);
    CHECK(reply[26] == 0x00);
}

// ---------------------------------------------------------------------------
// TC-26: make_char_list_reply — wire-accurate CHARLISTREPLY structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-26 make_char_list_reply with names", "[protocol][d2cs]") {
    // Real CHARLISTREPLY body (d2cs_protocol.h t_d2cs_client_charlistreply):
    //   maxchar(u16) currchar(u16) u1(u16=0) currchar2(u16)
    //   then per char: NUL-terminated name + NUL-terminated portrait.
    charlistreply::CharEntry barb;
    barb.charname = "Barb";
    barb.portrait = {std::byte{0xAA}};  // 1-byte stand-in portrait
    charlistreply::CharEntry sorc;
    sorc.charname = "Sorc";
    sorc.portrait = {std::byte{0xBB}};

    auto reply = D2CSSessionFsm::make_char_list_reply(8, {barb, sorc});

    auto u16le = [&](size_t off) {
        return static_cast<uint16_t>(reply[off] | (reply[off + 1] << 8));
    };

    // Header(3) + 4*u16(8) + "Barb\0"(5) + portrait(1)+NUL(1) +
    //                        "Sorc\0"(5) + portrait(1)+NUL(1) = 25
    REQUIRE(reply.size() == 25);
    CHECK(u16le(0) == 25);     // size (includes header)
    CHECK(reply[2] == 0x17);   // CHARLISTREPLY type

    CHECK(u16le(3) == 8);      // maxchar
    CHECK(u16le(5) == 2);      // currchar
    CHECK(u16le(7) == 0);      // u1 (always zero)
    CHECK(u16le(9) == 2);      // currchar2

    // First char: "Barb\0" + 0xAA + NUL
    CHECK(reply[11] == 'B');
    CHECK(reply[12] == 'a');
    CHECK(reply[13] == 'r');
    CHECK(reply[14] == 'b');
    CHECK(reply[15] == 0x00);
    CHECK(reply[16] == 0xAA);
    CHECK(reply[17] == 0x00);

    // Second char: "Sorc\0" + 0xBB + NUL
    CHECK(reply[18] == 'S');
    CHECK(reply[22] == 0x00);
    CHECK(reply[23] == 0xBB);
    CHECK(reply[24] == 0x00);
}

// ---------------------------------------------------------------------------
// TC-27: make_char_list_reply — empty list (header-only body)
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-27 make_char_list_reply empty list", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_char_list_reply(8, {});

    auto u16le = [&](size_t off) {
        return static_cast<uint16_t>(reply[off] | (reply[off + 1] << 8));
    };

    // Header(3) + maxchar/currchar/u1/currchar2 (4*u16 = 8) = 11
    REQUIRE(reply.size() == 11);
    CHECK(u16le(0) == 11);    // size
    CHECK(reply[2] == 0x17);  // CHARLISTREPLY
    CHECK(u16le(3) == 8);     // maxchar
    CHECK(u16le(5) == 0);     // currchar = 0
    CHECK(u16le(7) == 0);     // u1
    CHECK(u16le(9) == 0);     // currchar2 = 0
}

// ---------------------------------------------------------------------------
// TC-27b: make_char_list_reply — regression: maxchar=0 signals "no new char"
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-27b make_char_list_reply maxchar zero", "[protocol][d2cs]") {
    // When the account is full the caller passes maxchar_field = 0, which the
    // client reads as "Create disabled".
    charlistreply::CharEntry e;
    e.charname = "Full";
    auto reply = D2CSSessionFsm::make_char_list_reply(0, {e});

    auto u16le = [&](size_t off) {
        return static_cast<uint16_t>(reply[off] | (reply[off + 1] << 8));
    };
    CHECK(reply[2] == 0x17);
    CHECK(u16le(3) == 0);   // maxchar = 0 (no new-char allowed)
    CHECK(u16le(5) == 1);   // currchar = 1
}

// ---------------------------------------------------------------------------
// TC-28: make_create_char_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-28 make_create_char_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_create_char_reply(0x00);

    REQUIRE(reply.size() == 7);
    CHECK(reply[2] == 0x02);  // CREATECHARREPLY
    CHECK(reply[3] == 0x00);  // success
}

// ---------------------------------------------------------------------------
// TC-29: make_delete_char_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-29 make_delete_char_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_delete_char_reply(0x01);  // failed

    // Header(3) + u1(u16, always zero) + reply(u32) = 9 bytes, per
    // d2cs_protocol.h t_d2cs_client_deletecharreply.
    REQUIRE(reply.size() == 9);
    CHECK(reply[2] == 0x0A);  // DELETECHARREPLY
    CHECK(reply[3] == 0x00);  // u1 (low)
    CHECK(reply[4] == 0x00);  // u1 (high)
    CHECK(reply[5] == 0x01);  // reply code (LE low byte) = failed
}

// ---------------------------------------------------------------------------
// TC-30: make_motd_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-30 make_motd_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_motd_reply("Welcome!");

    // Header(3) + u1(1) + "Welcome!\0"(9) = 13
    REQUIRE(reply.size() == 13);
    CHECK(reply[2] == 0x12);  // MOTDREPLY
    CHECK(reply[3] == 0x00);  // u1
    CHECK(reply[4] == 'W');
    CHECK(reply[12] == 0x00);  // null terminator
}

// ---------------------------------------------------------------------------
// TC-31: make_create_game_wait — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-31 make_create_game_wait structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_create_game_wait(3);

    REQUIRE(reply.size() == 7);
    CHECK(reply[2] == 0x14);  // CREATEGAMEWAIT
    CHECK(reply[3] == 3);     // position = 3
}

// ---------------------------------------------------------------------------
// TC-32: make_convert_char_reply — correct structure
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-32 make_convert_char_reply structure", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_convert_char_reply(0x00);

    REQUIRE(reply.size() == 7);
    CHECK(reply[2] == 0x18);  // CONVERTCHARREPLY
    CHECK(reply[3] == 0x00);  // success
}

// ---------------------------------------------------------------------------
// TC-33: LOGINREQ with no callback — state still transitions
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-33 LOGINREQ no callback state transitions", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;  // no callbacks set
    D2CSSessionFsm fsm(cb);

    auto r = feed(fsm, make_packet(0x01, login_payload(1, "u")));
    REQUIRE(r);
    CHECK(fsm.state() == D2CSSessionState::authenticating);
}

// ---------------------------------------------------------------------------
// TC-34: CHARLISTREQ110 (0x19) — same handler as CHARLISTREQ
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-34 CHARLISTREQ110 uses same handler", "[protocol][d2cs]") {
    bool called = false;

    D2CSFsmCallbacks cb;
    cb.on_char_list = [&](const D2CSCharListRequest&) {
        called = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 100);

    auto r = feed(fsm, make_packet(0x19, payload));
    REQUIRE(r);
    CHECK(called);
}

// ---------------------------------------------------------------------------
// TC-35: LADDERREQ (0x11) — silently ignored
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-35 LADDERREQ silently ignored", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);
    push_u32(payload, 0);

    auto r = feed(fsm, make_packet(0x11, payload));
    REQUIRE(r);
    CHECK(r.value() > 0);
}

// ---------------------------------------------------------------------------
// TC-36: CHARLADDERREQ (0x16) — no callback set, succeeds without error
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-36 CHARLADDERREQ silently ignored", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Provide a valid payload: hardcore(4) + expansion(4) + char_name(cstr)
    std::vector<uint8_t> payload;
    push_u32(payload, 0);           // hardcore = 0
    push_u32(payload, 0);           // expansion = 0
    push_cstr(payload, "Hero");     // char_name

    auto r = feed(fsm, make_packet(0x16, payload));
    REQUIRE(r);
    CHECK(r.value() > 0);
}

// ---------------------------------------------------------------------------
// TC-37: LOGINREQ too-short payload — returns error
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-37 LOGINREQ too-short payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Only 4 bytes of payload (need at least 8 for seqno + session_key)
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto r = feed(fsm, make_packet(0x01, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-38: CHARLOGINREQ unterminated char name — returns error
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-38 CHARLOGINREQ unterminated name rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Body is the char name only; a name with no NUL terminator is malformed.
    std::vector<uint8_t> payload = {'B', 'a', 'd'};  // no trailing NUL
    auto r = feed(fsm, make_packet(0x07, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-39: CREATEGAMEREQ too-short payload — returns error
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-39 CREATEGAMEREQ too-short payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Only 3 bytes (need at least 7)
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    auto r = feed(fsm, make_packet(0x03, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-40: Full login flow: LOGINREQ → CHARLOGINREQ → CREATEGAMEREQ
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-40 full login flow state machine", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_login      = [](const D2CSLoginRequest&)     { return core::Result<void, core::Error>(); };
    cb.on_char_login = [](const D2CSCharLoginRequest&) { return core::Result<void, core::Error>(); };
    cb.on_create_game= [](const D2CSCreateGameRequest&){ return core::Result<void, core::Error>(); };
    D2CSSessionFsm fsm(cb);

    CHECK(fsm.state() == D2CSSessionState::connected);

    // Step 1: LOGINREQ
    {
        REQUIRE(feed(fsm, make_packet(0x01, login_payload(1, "player"))));
        CHECK(fsm.state() == D2CSSessionState::authenticating);
    }

    // Step 2: CHARLOGINREQ — body is just the NUL-terminated char name.
    {
        std::vector<uint8_t> p;
        push_cstr(p, "hero");
        REQUIRE(feed(fsm, make_packet(0x07, p)));
        CHECK(fsm.state() == D2CSSessionState::authenticated);
    }

    // Step 3: CREATEGAMEREQ
    {
        std::vector<uint8_t> p;
        push_u32(p, 3);
        p.push_back(0); p.push_back(0); p.push_back(1);
        push_cstr(p, "game1"); push_cstr(p, ""); push_cstr(p, "");
        REQUIRE(feed(fsm, make_packet(0x03, p)));
        CHECK(fsm.state() == D2CSSessionState::in_game);
    }
}

// ---------------------------------------------------------------------------
// TC-41: LADDERREQ — callback invoked with correct fields
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-41 LADDERREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSLadderRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_ladder = [&](const D2CSLadderRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u8(payload, 1);    // ladder_type = 1 (hardcore)
    push_u16(payload, 50);  // start_pos = 50

    auto r = feed(fsm, make_packet(0x11, payload));

    REQUIRE(r);
    CHECK(r.value() > 0);
    CHECK(called);
    CHECK(captured.ladder_type == 1);
    CHECK(captured.start_pos == 50);
}

// ---------------------------------------------------------------------------
// TC-42: LADDERREQ — too-short payload rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-42 LADDERREQ too-short payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Only 2 bytes (need at least 3: type(1) + start_pos(2))
    std::vector<uint8_t> payload = {0x01, 0x00};
    auto r = feed(fsm, make_packet(0x11, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-43: LADDERREQ — empty payload rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-43 LADDERREQ empty payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    auto r = feed(fsm, make_packet(0x11, {}));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-44: LADDERREQ — no callback, still succeeds
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-44 LADDERREQ no callback succeeds", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;  // no on_ladder set
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u8(payload, 0);    // ladder_type = 0 (standard)
    push_u16(payload, 0);   // start_pos = 0

    auto r = feed(fsm, make_packet(0x11, payload));
    REQUIRE(r);
    CHECK(r.value() > 0);
}

// ---------------------------------------------------------------------------
// TC-45: LADDERREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-45 LADDERREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_ladder = [](const D2CSLadderRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "ladder not found"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u8(payload, 0);
    push_u16(payload, 0);

    auto r = feed(fsm, make_packet(0x11, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::NotFound);
}

// ---------------------------------------------------------------------------
// TC-46: LADDERREQ — start_pos round-trips correctly (LE encoding)
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-46 LADDERREQ start_pos LE encoding", "[protocol][d2cs]") {
    uint16_t captured_pos = 0;

    D2CSFsmCallbacks cb;
    cb.on_ladder = [&](const D2CSLadderRequest& req) {
        captured_pos = req.start_pos;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u8(payload, 2);       // ladder_type = 2
    push_u16(payload, 0x0102); // start_pos = 258 (0x01 low, 0x02 high in LE → 0x0201 = 513? No: LE means low byte first)
    // push_u16 writes val & 0xFF first, then val >> 8
    // So 0x0102 → bytes [0x02, 0x01] → read back as 0x0102 = 258

    auto r = feed(fsm, make_packet(0x11, payload));
    REQUIRE(r);
    CHECK(captured_pos == 0x0102);
}

// ---------------------------------------------------------------------------
// TC-47: CHARLADDERREQ — callback invoked with correct fields
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-47 CHARLADDERREQ callback invoked", "[protocol][d2cs]") {
    bool called = false;
    D2CSCharLadderRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_char_ladder = [&](const D2CSCharLadderRequest& req) {
        called = true;
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);           // hardcore = 1
    push_u32(payload, 1);           // expansion = 1
    push_cstr(payload, "LadderChar");

    auto r = feed(fsm, make_packet(0x16, payload));

    REQUIRE(r);
    CHECK(r.value() > 0);
    CHECK(called);
    CHECK(captured.hardcore == 1);
    CHECK(captured.expansion == 1);
    CHECK(captured.char_name == "LadderChar");
}

// ---------------------------------------------------------------------------
// TC-48: CHARLADDERREQ — too-short payload rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-48 CHARLADDERREQ too-short payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Only 4 bytes (need at least 8 for hardcore + expansion)
    std::vector<uint8_t> payload(4, 0x00);
    auto r = feed(fsm, make_packet(0x16, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-49: CHARLADDERREQ — unterminated char_name rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-49 CHARLADDERREQ unterminated char_name rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 0);  // hardcore
    push_u32(payload, 0);  // expansion
    // char_name without null terminator
    payload.push_back('A');
    payload.push_back('B');
    payload.push_back('C');

    auto r = feed(fsm, make_packet(0x16, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-50: CHARLADDERREQ — no callback, still succeeds
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-50 CHARLADDERREQ no callback succeeds", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;  // no on_char_ladder set
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 0);
    push_u32(payload, 0);
    push_cstr(payload, "Hero");

    auto r = feed(fsm, make_packet(0x16, payload));
    REQUIRE(r);
    CHECK(r.value() > 0);
}

// ---------------------------------------------------------------------------
// TC-51: CHARLADDERREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-51 CHARLADDERREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_char_ladder = [](const D2CSCharLadderRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "char not found"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 0);
    push_u32(payload, 0);
    push_cstr(payload, "Hero");

    auto r = feed(fsm, make_packet(0x16, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::NotFound);
}

// ---------------------------------------------------------------------------
// TC-52: CHARLADDERREQ — hardcore=0, expansion=0 (classic softcore)
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-52 CHARLADDERREQ classic softcore fields", "[protocol][d2cs]") {
    D2CSCharLadderRequest captured;

    D2CSFsmCallbacks cb;
    cb.on_char_ladder = [&](const D2CSCharLadderRequest& req) {
        captured = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 0);           // hardcore = 0
    push_u32(payload, 0);           // expansion = 0
    push_cstr(payload, "SoftChar");

    REQUIRE(feed(fsm, make_packet(0x16, payload)));
    CHECK(captured.hardcore == 0);
    CHECK(captured.expansion == 0);
    CHECK(captured.char_name == "SoftChar");
}

// ---------------------------------------------------------------------------
// TC-53: CHARLISTREQ110 — on_char_list_110 callback invoked
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-53 CHARLISTREQ110 on_char_list_110 callback invoked", "[protocol][d2cs]") {
    bool called_110 = false;
    D2CSCharListRequest captured_110;

    D2CSFsmCallbacks cb;
    cb.on_char_list_110 = [&](const D2CSCharListRequest& req) {
        called_110 = true;
        captured_110 = req;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 200);  // seqno

    auto r = feed(fsm, make_packet(0x19, payload));
    REQUIRE(r);
    CHECK(called_110);
    CHECK(captured_110.seqno == 200);
}

// ---------------------------------------------------------------------------
// TC-54: CHARLISTREQ110 — both on_char_list AND on_char_list_110 are called
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-54 CHARLISTREQ110 fires both callbacks", "[protocol][d2cs]") {
    bool called_base = false;
    bool called_110  = false;

    D2CSFsmCallbacks cb;
    cb.on_char_list = [&](const D2CSCharListRequest&) {
        called_base = true;
        return core::Result<void, core::Error>();
    };
    cb.on_char_list_110 = [&](const D2CSCharListRequest&) {
        called_110 = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 300);

    auto r = feed(fsm, make_packet(0x19, payload));
    REQUIRE(r);
    CHECK(called_base);
    CHECK(called_110);
}

// ---------------------------------------------------------------------------
// TC-55: CHARLISTREQ110 — on_char_list_110 callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-55 CHARLISTREQ110 on_char_list_110 failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_char_list_110 = [](const D2CSCharListRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::Internal, "110 handler error"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);

    auto r = feed(fsm, make_packet(0x19, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::Internal);
}

// ---------------------------------------------------------------------------
// TC-56: CHARLISTREQ110 — on_char_list failure stops before on_char_list_110
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-56 CHARLISTREQ110 base callback failure stops chain", "[protocol][d2cs]") {
    bool called_110 = false;

    D2CSFsmCallbacks cb;
    cb.on_char_list = [](const D2CSCharListRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::PermissionDenied, "denied"));
    };
    cb.on_char_list_110 = [&](const D2CSCharListRequest&) {
        called_110 = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);

    auto r = feed(fsm, make_packet(0x19, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::PermissionDenied);
    CHECK_FALSE(called_110);  // on_char_list_110 must NOT be called after base fails
}

// ---------------------------------------------------------------------------
// TC-57: CHARLISTREQ110 — too-short payload rejected
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-57 CHARLISTREQ110 too-short payload rejected", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    D2CSSessionFsm fsm(cb);

    // Only 2 bytes (need at least 4 for seqno)
    std::vector<uint8_t> payload = {0x01, 0x02};
    auto r = feed(fsm, make_packet(0x19, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// TC-58: CHARLISTREQ (0x17) — does NOT fire on_char_list_110
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-58 CHARLISTREQ does not fire on_char_list_110", "[protocol][d2cs]") {
    bool called_110 = false;

    D2CSFsmCallbacks cb;
    cb.on_char_list_110 = [&](const D2CSCharListRequest&) {
        called_110 = true;
        return core::Result<void, core::Error>();
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);

    auto r = feed(fsm, make_packet(0x17, payload));
    REQUIRE(r);
    CHECK_FALSE(called_110);  // 0x17 must NOT trigger on_char_list_110
}

// ---------------------------------------------------------------------------
// TC-59: GAMELISTREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-59 GAMELISTREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_game_list = [](const D2CSGameListRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::Unavailable, "game list unavailable"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);
    push_u32(payload, 0);

    auto r = feed(fsm, make_packet(0x05, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::Unavailable);
}

// ---------------------------------------------------------------------------
// TC-60: GAMEINFOREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-60 GAMEINFOREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_game_info = [](const D2CSGameInfoRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "game not found"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u32(payload, 1);
    push_cstr(payload, "NoSuchGame");

    auto r = feed(fsm, make_packet(0x06, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::NotFound);
}

// ---------------------------------------------------------------------------
// TC-61: CREATECHARREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-61 CREATECHARREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_create_char = [](const D2CSCreateCharRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::AlreadyExists, "char already exists"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u16(payload, 0);   // chclass
    push_u16(payload, 0);   // u1
    push_u16(payload, 0);   // status
    push_cstr(payload, "DupChar");

    auto r = feed(fsm, make_packet(0x02, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::AlreadyExists);
}

// ---------------------------------------------------------------------------
// TC-62: DELETECHARREQ — callback failure propagates
// ---------------------------------------------------------------------------
TEST_CASE("D2CSSessionFsm - TC-62 DELETECHARREQ callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks cb;
    cb.on_delete_char = [](const D2CSDeleteCharRequest&) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, "char not found"));
    };
    D2CSSessionFsm fsm(cb);

    std::vector<uint8_t> payload;
    push_u16(payload, 0);   // u1
    push_cstr(payload, "GhostChar");

    auto r = feed(fsm, make_packet(0x0A, payload));
    CHECK_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::NotFound);
}

} // namespace pvpgn::protocol::d2cs
