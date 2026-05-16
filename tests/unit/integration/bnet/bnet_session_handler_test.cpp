// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "integration/bnet/bnet_session_handler.hpp"

namespace pvpgn::integration::bnet::test {

class BnetSessionHandlerTest : public ::testing::Test {
protected:
    BnetSessionHandlerTest() {
        // Create mock use cases (nullptr for now, as they're not fully implemented)
        deps_.join_channel = nullptr;
        deps_.post_message = nullptr;
        deps_.login_user = nullptr;
        deps_.logout_user = nullptr;
        
        handler_ = std::make_unique<BnetSessionHandler>(deps_, "test_session_123");
    }
    
    BnetSessionHandler::Dependencies deps_;
    std::unique_ptr<BnetSessionHandler> handler_;
};

TEST_F(BnetSessionHandlerTest, InitialState) {
    EXPECT_EQ(handler_->session_id(), "test_session_123");
    EXPECT_TRUE(handler_->account_name().empty());
    EXPECT_TRUE(handler_->current_channel().empty());
}

TEST_F(BnetSessionHandlerTest, LoginSuccess) {
    auto result = handler_->on_login("testuser", "hash123");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(handler_->account_name(), "testuser");
}

TEST_F(BnetSessionHandlerTest, JoinChannelBeforeLogin) {
    auto result = handler_->on_join_channel("General", 0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), core::StatusCode::Unauthenticated);
}

TEST_F(BnetSessionHandlerTest, JoinChannelAfterLogin) {
    handler_->on_login("testuser", "hash123");
    auto result = handler_->on_join_channel("General", 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(handler_->current_channel(), "General");
}

TEST_F(BnetSessionHandlerTest, ChatCommandBeforeLogin) {
    auto result = handler_->on_chat_command("hello");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), core::StatusCode::Unauthenticated);
}

TEST_F(BnetSessionHandlerTest, ChatCommandWithoutChannel) {
    handler_->on_login("testuser", "hash123");
    auto result = handler_->on_chat_command("hello");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), core::StatusCode::FailedPrecondition);
}

TEST_F(BnetSessionHandlerTest, ChatCommandInChannel) {
    handler_->on_login("testuser", "hash123");
    handler_->on_join_channel("General", 0);
    auto result = handler_->on_chat_command("hello world");
    EXPECT_TRUE(result.has_value());
}

TEST_F(BnetSessionHandlerTest, Disconnect) {
    handler_->on_login("testuser", "hash123");
    handler_->on_join_channel("General", 0);
    
    auto result = handler_->on_disconnect();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(handler_->account_name().empty());
    EXPECT_TRUE(handler_->current_channel().empty());
}

}  // namespace pvpgn::integration::bnet::test
