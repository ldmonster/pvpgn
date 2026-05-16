// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "integration/telnet/telnet_session_factory.hpp"

namespace pvpgn::integration::telnet::test {

class TelnetSessionTest : public ::testing::Test {
protected:
    TelnetSessionTest() {
        output_buffer_.clear();
        auto callback = [this](std::string_view text) {
            output_buffer_ += text;
        };
        session_ = TelnetSessionFactory::create("test_session_telnet", callback);
    }
    
    std::unique_ptr<TelnetSession> session_;
    std::string output_buffer_;
};

TEST_F(TelnetSessionTest, InitialState) {
    EXPECT_EQ(session_->state(), TelnetSession::State::connected);
    EXPECT_EQ(session_->session_id(), "test_session_telnet");
    // Should have sent welcome message
    EXPECT_FALSE(output_buffer_.empty());
}

TEST_F(TelnetSessionTest, FeedEmptyData) {
    output_buffer_.clear();
    std::vector<uint8_t> empty_data;
    auto result = session_->feed(std::span<const uint8_t>(empty_data));
    EXPECT_TRUE(result.has_value());
}

TEST_F(TelnetSessionTest, SendUsername) {
    output_buffer_.clear();
    std::string username = "testuser\r\n";
    std::vector<uint8_t> data(username.begin(), username.end());
    
    auto result = session_->feed(std::span<const uint8_t>(data));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), TelnetSession::State::authenticating);
    // Should ask for password
    EXPECT_TRUE(output_buffer_.find("Password") != std::string::npos);
}

TEST_F(TelnetSessionTest, SendPassword) {
    // First send username
    std::string username = "testuser\r\n";
    std::vector<uint8_t> user_data(username.begin(), username.end());
    session_->feed(std::span<const uint8_t>(user_data));
    
    output_buffer_.clear();
    
    // Then send password
    std::string password = "password123\r\n";
    std::vector<uint8_t> pass_data(password.begin(), password.end());
    auto result = session_->feed(std::span<const uint8_t>(pass_data));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), TelnetSession::State::authenticated);
    EXPECT_TRUE(output_buffer_.find("Authenticated") != std::string::npos);
}

TEST_F(TelnetSessionTest, HelpCommand) {
    // Authenticate first
    std::string username = "testuser\r\n";
    std::vector<uint8_t> user_data(username.begin(), username.end());
    session_->feed(std::span<const uint8_t>(user_data));
    
    std::string password = "password123\r\n";
    std::vector<uint8_t> pass_data(password.begin(), password.end());
    session_->feed(std::span<const uint8_t>(pass_data));
    
    output_buffer_.clear();
    
    // Send help command
    std::string help_cmd = "help\r\n";
    std::vector<uint8_t> help_data(help_cmd.begin(), help_cmd.end());
    auto result = session_->feed(std::span<const uint8_t>(help_data));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(output_buffer_.find("help") != std::string::npos);
}

TEST_F(TelnetSessionTest, QuitCommand) {
    // Authenticate first
    std::string username = "testuser\r\n";
    std::vector<uint8_t> user_data(username.begin(), username.end());
    session_->feed(std::span<const uint8_t>(user_data));
    
    std::string password = "password123\r\n";
    std::vector<uint8_t> pass_data(password.begin(), password.end());
    session_->feed(std::span<const uint8_t>(pass_data));
    
    output_buffer_.clear();
    
    // Send quit command
    std::string quit_cmd = "quit\r\n";
    std::vector<uint8_t> quit_data(quit_cmd.begin(), quit_cmd.end());
    auto result = session_->feed(std::span<const uint8_t>(quit_data));
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(session_->state(), TelnetSession::State::disconnected);
    EXPECT_TRUE(output_buffer_.find("Goodbye") != std::string::npos);
}

}  // namespace pvpgn::integration::telnet::test
