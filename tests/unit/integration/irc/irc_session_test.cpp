// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "core/bytes.hpp"
#include "integration/irc/irc_session_factory.hpp"

namespace pvpgn::integration::irc::test {

class IrcSessionTest : public ::testing::Test {
protected:
    IrcSessionTest() {
        output_buffer_.clear();
        auto callback = [this](std::string_view text) {
            output_buffer_ += text;
        };
        session_ = IrcSessionFactory::create("test_session_irc", callback);
    }
    
    std::unique_ptr<IrcSession> session_;
    std::string output_buffer_;
};

TEST_F(IrcSessionTest, InitialState) {
    EXPECT_EQ(session_->state(), IrcSession::State::connected);
    EXPECT_EQ(session_->session_id(), "test_session_irc");
    EXPECT_TRUE(session_->nickname().empty());
}

TEST_F(IrcSessionTest, FeedEmptyData) {
    output_buffer_.clear();
    std::vector<uint8_t> empty_data;
    auto result = session_->feed(core::as_byte_view(empty_data.data(), empty_data.size()));
    EXPECT_TRUE(result.has_value());
}

TEST_F(IrcSessionTest, SendNick) {
    output_buffer_.clear();
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> data(nick_cmd.begin(), nick_cmd.end());
    
    auto result = session_->feed(core::as_byte_view(data.data(), data.size()));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->nickname(), "testuser");
}

TEST_F(IrcSessionTest, SendUser) {
    // First send NICK
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> nick_data(nick_cmd.begin(), nick_cmd.end());
    session_->feed(core::as_byte_view(nick_data.data(), nick_data.size()));
    
    output_buffer_.clear();
    
    // Then send USER
    std::string user_cmd = "USER testuser 0 * :Test User\r\n";
    std::vector<uint8_t> user_data(user_cmd.begin(), user_cmd.end());
    auto result = session_->feed(core::as_byte_view(user_data.data(), user_data.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), IrcSession::State::registered);
    // Should have sent welcome messages
    EXPECT_TRUE(output_buffer_.find("Welcome") != std::string::npos);
}

TEST_F(IrcSessionTest, JoinChannel) {
    // First authenticate
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> nick_data(nick_cmd.begin(), nick_cmd.end());
    session_->feed(core::as_byte_view(nick_data.data(), nick_data.size()));
    
    std::string user_cmd = "USER testuser 0 * :Test User\r\n";
    std::vector<uint8_t> user_data(user_cmd.begin(), user_cmd.end());
    session_->feed(core::as_byte_view(user_data.data(), user_data.size()));
    
    output_buffer_.clear();
    
    // Send JOIN command
    std::string join_cmd = "JOIN #general\r\n";
    std::vector<uint8_t> join_data(join_cmd.begin(), join_cmd.end());
    auto result = session_->feed(core::as_byte_view(join_data.data(), join_data.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(output_buffer_.find("JOIN") != std::string::npos);
}

TEST_F(IrcSessionTest, ListChannels) {
    // First authenticate
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> nick_data(nick_cmd.begin(), nick_cmd.end());
    session_->feed(core::as_byte_view(nick_data.data(), nick_data.size()));
    
    std::string user_cmd = "USER testuser 0 * :Test User\r\n";
    std::vector<uint8_t> user_data(user_cmd.begin(), user_cmd.end());
    session_->feed(core::as_byte_view(user_data.data(), user_data.size()));
    
    output_buffer_.clear();
    
    // Send LIST command
    std::string list_cmd = "LIST\r\n";
    std::vector<uint8_t> list_data(list_cmd.begin(), list_cmd.end());
    auto result = session_->feed(core::as_byte_view(list_data.data(), list_data.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(output_buffer_.find("General") != std::string::npos);
}

TEST_F(IrcSessionTest, PrivMsg) {
    // First authenticate
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> nick_data(nick_cmd.begin(), nick_cmd.end());
    session_->feed(core::as_byte_view(nick_data.data(), nick_data.size()));
    
    std::string user_cmd = "USER testuser 0 * :Test User\r\n";
    std::vector<uint8_t> user_data(user_cmd.begin(), user_cmd.end());
    session_->feed(core::as_byte_view(user_data.data(), user_data.size()));
    
    output_buffer_.clear();
    
    // Send PRIVMSG command
    std::string msg_cmd = "PRIVMSG #general :Hello world\r\n";
    std::vector<uint8_t> msg_data(msg_cmd.begin(), msg_cmd.end());
    auto result = session_->feed(core::as_byte_view(msg_data.data(), msg_data.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(output_buffer_.find("Hello world") != std::string::npos);
}

TEST_F(IrcSessionTest, Ping) {
    output_buffer_.clear();
    std::string ping_cmd = "PING :server\r\n";
    std::vector<uint8_t> data(ping_cmd.begin(), ping_cmd.end());
    
    auto result = session_->feed(core::as_byte_view(data.data(), data.size()));
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(output_buffer_.find("PONG") != std::string::npos);
}

TEST_F(IrcSessionTest, Quit) {
    // First authenticate
    std::string nick_cmd = "NICK testuser\r\n";
    std::vector<uint8_t> nick_data(nick_cmd.begin(), nick_cmd.end());
    session_->feed(core::as_byte_view(nick_data.data(), nick_data.size()));
    
    std::string user_cmd = "USER testuser 0 * :Test User\r\n";
    std::vector<uint8_t> user_data(user_cmd.begin(), user_cmd.end());
    session_->feed(core::as_byte_view(user_data.data(), user_data.size()));
    
    output_buffer_.clear();
    
    // Send QUIT command
    std::string quit_cmd = "QUIT :Goodbye\r\n";
    std::vector<uint8_t> quit_data(quit_cmd.begin(), quit_cmd.end());
    auto result = session_->feed(core::as_byte_view(quit_data.data(), quit_data.size()));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), IrcSession::State::disconnected);
}

}  // namespace pvpgn::integration::irc::test
