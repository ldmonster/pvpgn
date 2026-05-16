#include <catch2/catch_test_macros.hpp>
#include "protocol/d2cs/fsm.hpp"
#include <cstring>

namespace pvpgn::protocol::d2cs {

TEST_CASE("D2CSSessionFsm - construction", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    D2CSSessionFsm fsm(callbacks);
    
    CHECK(fsm.state() == D2CSSessionState::connected);
}

TEST_CASE("D2CSSessionFsm - feed empty data", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    D2CSSessionFsm fsm(callbacks);
    
    auto result = fsm.feed(nullptr, 0);
    REQUIRE(result);
    CHECK(result.value() == 0);
}

TEST_CASE("D2CSSessionFsm - feed invalid packet length", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    D2CSSessionFsm fsm(callbacks);
    
    // Create a packet with invalid length (too small)
    uint8_t data[3] = {0x01, 0x00, 0x01};  // length = 1 (invalid, min is 3)
    
    auto result = fsm.feed(data, 3);
    CHECK_FALSE(result);
}

TEST_CASE("D2CSSessionFsm - feed incomplete packet", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    D2CSSessionFsm fsm(callbacks);
    
    // Create a packet header but not enough data
    uint8_t data[3] = {0x10, 0x00, 0x01};  // length = 16, but only 3 bytes provided
    
    auto result = fsm.feed(data, 3);
    REQUIRE(result);
    CHECK(result.value() == 0);  // No complete packet processed
}

TEST_CASE("D2CSSessionFsm - make_login_reply", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_login_reply(0x00);
    
    CHECK(reply.size() >= 3);
    CHECK(reply[2] == static_cast<uint8_t>(D2CSPacketType::LOGINREPLY));
}

TEST_CASE("D2CSSessionFsm - make_char_login_reply", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_char_login_reply(0x00);
    
    CHECK(reply.size() >= 3);
    CHECK(reply[2] == static_cast<uint8_t>(D2CSPacketType::CHARLOGINREPLY));
}

TEST_CASE("D2CSSessionFsm - make_create_game_reply", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_create_game_reply(0x00, 12345);
    
    CHECK(reply.size() >= 3);
    CHECK(reply[2] == static_cast<uint8_t>(D2CSPacketType::CREATEGAMEREPLY));
}

TEST_CASE("D2CSSessionFsm - make_join_game_reply", "[protocol][d2cs]") {
    auto reply = D2CSSessionFsm::make_join_game_reply(0x00, "gs1.example.com", 4000);
    
    CHECK(reply.size() >= 3);
    CHECK(reply[2] == static_cast<uint8_t>(D2CSPacketType::JOINGAMEREPLY));
}

TEST_CASE("D2CSSessionFsm - make_char_list_reply", "[protocol][d2cs]") {
    std::vector<std::string> chars = {"Barbarian", "Sorceress", "Paladin"};
    auto reply = D2CSSessionFsm::make_char_list_reply(chars);
    
    CHECK(reply.size() >= 3);
    CHECK(reply[2] == static_cast<uint8_t>(D2CSPacketType::CHARLISTREPLY));
}

TEST_CASE("D2CSSessionFsm - login callback invoked", "[protocol][d2cs]") {
    bool login_called = false;
    D2CSFsmCallbacks callbacks;
    callbacks.on_login = [&login_called](const D2CSLoginRequest& req) {
        login_called = true;
        return core::Result<void, core::Error>();
    };
    
    D2CSSessionFsm fsm(callbacks);
    
    // Create a valid login packet
    // Format: length (2 bytes) + type (1 byte) + account_id (4 bytes) + session_key (4 bytes)
    uint8_t data[11];
    data[0] = 0x0B;  // length = 11 (little-endian)
    data[1] = 0x00;
    data[2] = static_cast<uint8_t>(D2CSPacketType::LOGINREQ);
    data[3] = 0x01;  // account_id = 1
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x02;  // session_key = 2
    data[8] = 0x00;
    data[9] = 0x00;
    data[10] = 0x00;
    
    auto result = fsm.feed(data, 11);
    REQUIRE(result);
    CHECK(login_called);
    CHECK(fsm.state() == D2CSSessionState::authenticating);
}

TEST_CASE("D2CSSessionFsm - unknown packet type fails", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    D2CSSessionFsm fsm(callbacks);
    
    // Create a packet with unknown type
    uint8_t data[3];
    data[0] = 0x03;  // length = 3
    data[1] = 0x00;
    data[2] = 0xFF;  // unknown type
    
    auto result = fsm.feed(data, 3);
    CHECK_FALSE(result);
}

TEST_CASE("D2CSSessionFsm - multiple packets in one feed", "[protocol][d2cs]") {
    int login_count = 0;
    D2CSFsmCallbacks callbacks;
    callbacks.on_login = [&login_count](const D2CSLoginRequest& req) {
        login_count++;
        return core::Result<void, core::Error>();
    };
    
    D2CSSessionFsm fsm(callbacks);
    
    // Create two login packets
    uint8_t data[22];
    
    // First packet
    data[0] = 0x0B;  // length = 11
    data[1] = 0x00;
    data[2] = static_cast<uint8_t>(D2CSPacketType::LOGINREQ);
    data[3] = 0x01;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x02;
    data[8] = 0x00;
    data[9] = 0x00;
    data[10] = 0x00;
    
    // Second packet
    data[11] = 0x0B;  // length = 11
    data[12] = 0x00;
    data[13] = static_cast<uint8_t>(D2CSPacketType::LOGINREQ);
    data[14] = 0x03;
    data[15] = 0x00;
    data[16] = 0x00;
    data[17] = 0x00;
    data[18] = 0x04;
    data[19] = 0x00;
    data[20] = 0x00;
    data[21] = 0x00;
    
    auto result = fsm.feed(data, 22);
    REQUIRE(result);
    CHECK(result.value() == 22);
    CHECK(login_count == 2);
}

TEST_CASE("D2CSSessionFsm - login callback failure propagates", "[protocol][d2cs]") {
    D2CSFsmCallbacks callbacks;
    callbacks.on_login = [](const D2CSLoginRequest& req) {
        return core::fail(
            core::make_error(core::StatusCode::Unauthenticated, "Invalid credentials")
        );
    };
    
    D2CSSessionFsm fsm(callbacks);
    
    uint8_t data[11];
    data[0] = 0x0B;
    data[1] = 0x00;
    data[2] = static_cast<uint8_t>(D2CSPacketType::LOGINREQ);
    data[3] = 0x01;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x02;
    data[8] = 0x00;
    data[9] = 0x00;
    data[10] = 0x00;
    
    auto result = fsm.feed(data, 11);
    CHECK_FALSE(result);
}

TEST_CASE("D2CSSessionFsm - packet type enum values", "[protocol][d2cs]") {
    CHECK(static_cast<uint8_t>(D2CSPacketType::LOGINREQ) == 0x01);
    CHECK(static_cast<uint8_t>(D2CSPacketType::CHARLOGINREQ) == 0x07);
    CHECK(static_cast<uint8_t>(D2CSPacketType::CREATEGAMEREQ) == 0x09);
    CHECK(static_cast<uint8_t>(D2CSPacketType::JOINGAMEREQ) == 0x0B);
    CHECK(static_cast<uint8_t>(D2CSPacketType::LOGINREPLY) == 0x02);
    CHECK(static_cast<uint8_t>(D2CSPacketType::CHARLOGINREPLY) == 0x08);
    CHECK(static_cast<uint8_t>(D2CSPacketType::CREATEGAMEREPLY) == 0x0A);
    CHECK(static_cast<uint8_t>(D2CSPacketType::JOINGAMEREPLY) == 0x0C);
}

TEST_CASE("D2CSSessionFsm - session state enum values", "[protocol][d2cs]") {
    CHECK(D2CSSessionState::connected == D2CSSessionState::connected);
    CHECK(D2CSSessionState::authenticating == D2CSSessionState::authenticating);
    CHECK(D2CSSessionState::authenticated == D2CSSessionState::authenticated);
    CHECK(D2CSSessionState::in_game == D2CSSessionState::in_game);
    CHECK(D2CSSessionState::disconnected == D2CSSessionState::disconnected);
}

} // namespace pvpgn::protocol::d2cs
