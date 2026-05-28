// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "core/bytes.hpp"
#include "integration/wol/wol_session_factory.hpp"

namespace pvpgn::integration::wol::test {

class WolSessionTest : public ::testing::Test {
protected:
    WolSessionTest() {
        session_ = WolSessionFactory::create("test_session_wol");
    }
    
    std::unique_ptr<WolSession> session_;
};

TEST_F(WolSessionTest, InitialState) {
    EXPECT_EQ(session_->state(), WolSession::State::connected);
    EXPECT_EQ(session_->session_id(), "test_session_wol");
}

TEST_F(WolSessionTest, FeedEmptyData) {
    std::vector<uint8_t> empty_data;
    auto result = session_->feed(core::as_byte_view(empty_data.data(), empty_data.size()));
    EXPECT_TRUE(result.has_value());
}

TEST_F(WolSessionTest, HandlePingPacket) {
    // WOL PING packet: [type=0x0A][length=0x02]
    std::vector<uint8_t> ping_packet = {0x0A, 0x02};
    auto result = session_->feed(core::as_byte_view(ping_packet.data(), ping_packet.size()));
    
    EXPECT_TRUE(result.has_value());
    auto reply = std::move(result).value();
    // Should get PONG reply
    EXPECT_FALSE(reply.empty());
    EXPECT_EQ(reply[0], 0x0B);  // PONG type
}

TEST_F(WolSessionTest, HandleLoginRequest) {
    // WOL LOGIN_REQUEST packet: [type=0x01][length=0x02]
    std::vector<uint8_t> login_packet = {0x01, 0x02};
    auto result = session_->feed(core::as_byte_view(login_packet.data(), login_packet.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), WolSession::State::authenticated);
}

TEST_F(WolSessionTest, InvalidPacketLength) {
    // Invalid packet with bad length
    std::vector<uint8_t> bad_packet = {0x01, 0xFF};
    auto result = session_->feed(core::as_byte_view(bad_packet.data(), bad_packet.size()));
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), core::StatusCode::InvalidArgument);
}

TEST_F(WolSessionTest, IncompletePacket) {
    // Incomplete packet (only type, no length)
    std::vector<uint8_t> incomplete = {0x01};
    auto result = session_->feed(core::as_byte_view(incomplete.data(), incomplete.size()));
    
    EXPECT_TRUE(result.has_value());
    // Should wait for more data
    EXPECT_EQ(session_->state(), WolSession::State::connected);
}

}  // namespace pvpgn::integration::wol::test
