#include <catch2/catch_test_macros.hpp>
#include "protocol/d2dbs/fsm.hpp"

namespace pvpgn::protocol::d2dbs::test {

D2DBSFsmCallbacks create_callbacks() {
    D2DBSFsmCallbacks callbacks;
    callbacks.on_char_login = [](const D2DBSCharLoginRequest&) {
        return core::Result<std::vector<uint8_t>, core::Error>(std::vector<uint8_t>());
    };
    callbacks.on_char_save = [](const D2DBSCharSaveRequest&) {
        return core::Result<void, core::Error>();
    };
    callbacks.on_char_list = [](const D2DBSCharListRequest&) {
        return core::Result<std::vector<std::string>, core::Error>(std::vector<std::string>());
    };
    callbacks.on_char_create = [](const D2DBSCharCreateRequest&) {
        return core::Result<void, core::Error>();
    };
    callbacks.on_char_delete = [](const D2DBSCharDeleteRequest&) {
        return core::Result<void, core::Error>();
    };
    callbacks.on_disconnect = []() {};
    return callbacks;
}

TEST_CASE("D2DBSFsm: InitialState", "[protocol][d2dbs]") {
    D2DBSSessionFsm fsm(create_callbacks());
    REQUIRE(fsm.state() == D2DBSSessionState::connected);
}

TEST_CASE("D2DBSFsm: MakeCharLoginReply", "[protocol][d2dbs]") {
    std::vector<uint8_t> save_data = {0x01, 0x02, 0x03};
    auto reply = D2DBSSessionFsm::make_char_login_reply(0, save_data);
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::CHARLOGINREPLY));
}

TEST_CASE("D2DBSFsm: MakeCharSaveReply", "[protocol][d2dbs]") {
    auto reply = D2DBSSessionFsm::make_char_save_reply(0);
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::CHARSAVEREPLY));
    REQUIRE(reply[1] == 3);  // Size
    REQUIRE(reply[2] == 0);  // Result code
}

TEST_CASE("D2DBSFsm: MakeCharListReply", "[protocol][d2dbs]") {
    std::vector<std::string> chars = {"Char1", "Char2"};
    auto reply = D2DBSSessionFsm::make_char_list_reply(chars);
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::CHARLISTREPLY));
}

TEST_CASE("D2DBSFsm: MakeCharCreateReply", "[protocol][d2dbs]") {
    auto reply = D2DBSSessionFsm::make_char_create_reply(0);
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::CHARCREATEREPLY));
    REQUIRE(reply[1] == 3);  // Size
    REQUIRE(reply[2] == 0);  // Result code
}

TEST_CASE("D2DBSFsm: MakeCharDeleteReply", "[protocol][d2dbs]") {
    auto reply = D2DBSSessionFsm::make_char_delete_reply(0);
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::CHARDELREPLY));
    REQUIRE(reply[1] == 3);  // Size
    REQUIRE(reply[2] == 0);  // Result code
}

TEST_CASE("D2DBSFsm: MakeKeepalive", "[protocol][d2dbs]") {
    auto reply = D2DBSSessionFsm::make_keepalive();
    
    REQUIRE_FALSE(reply.empty());
    REQUIRE(reply[0] == static_cast<uint8_t>(D2DBSPacketType::KEEPALIVE));
    REQUIRE(reply[1] == 2);  // Size
}

TEST_CASE("D2DBSFsm: FeedEmptyData", "[protocol][d2dbs]") {
    D2DBSSessionFsm fsm(create_callbacks());
    std::vector<uint8_t> data;
    auto result = fsm.feed(data);
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 0);
}

TEST_CASE("D2DBSFsm: FeedInvalidPacketSize", "[protocol][d2dbs]") {
    D2DBSSessionFsm fsm(create_callbacks());
    std::vector<uint8_t> data = {0x01, 0x01};  // Size too small
    auto result = fsm.feed(data);
    
    REQUIRE_FALSE(result.has_value());
}

} // namespace pvpgn::protocol::d2dbs::test
