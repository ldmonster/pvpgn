// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_message_broadcaster.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryMessageBroadcaster: LastMessagesEmptyInitially", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    REQUIRE(bc.last_messages().empty());
}

TEST_CASE("InMemoryMessageBroadcaster: BroadcastToChannelCapturesMessage", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.broadcast_to_channel(domain::ChannelId{1}, "hello channel");

    auto msgs = bc.last_messages();
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].find("hello channel") != std::string::npos);
    REQUIRE(msgs[0].find("channel:1") != std::string::npos);
}

TEST_CASE("InMemoryMessageBroadcaster: SendInfoCapturesMessage", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.send_info(domain::ConnectionId{42}, "info text");

    auto msgs = bc.last_messages();
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].find("info text") != std::string::npos);
    REQUIRE(msgs[0].find("info:42") != std::string::npos);
}

TEST_CASE("InMemoryMessageBroadcaster: SendErrorCapturesMessage", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.send_error(domain::ConnectionId{7}, "error text");

    auto msgs = bc.last_messages();
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].find("error text") != std::string::npos);
    REQUIRE(msgs[0].find("error:7") != std::string::npos);
}

TEST_CASE("InMemoryMessageBroadcaster: MultipleMessagesAccumulate", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.broadcast_to_channel(domain::ChannelId{1}, "msg1");
    bc.send_info(domain::ConnectionId{2}, "msg2");
    bc.send_error(domain::ConnectionId{3}, "msg3");

    auto msgs = bc.last_messages();
    REQUIRE(msgs.size() == 3);
}

TEST_CASE("InMemoryMessageBroadcaster: ClearEmptiesLog", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.broadcast_to_channel(domain::ChannelId{1}, "some message");
    bc.send_info(domain::ConnectionId{1}, "another message");

    REQUIRE(bc.last_messages().size() == 2);

    bc.clear();
    REQUIRE(bc.last_messages().empty());
}

TEST_CASE("InMemoryMessageBroadcaster: ClearThenAddNewMessages", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.broadcast_to_channel(domain::ChannelId{10}, "old");
    bc.clear();
    bc.send_info(domain::ConnectionId{5}, "new");

    auto msgs = bc.last_messages();
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0].find("new") != std::string::npos);
}

TEST_CASE("InMemoryMessageBroadcaster: ChannelIdIsEmbeddedInMessage", "[infra][inmemory]") {
    InMemoryMessageBroadcaster bc;
    bc.broadcast_to_channel(domain::ChannelId{999}, "test");

    auto msgs = bc.last_messages();
    REQUIRE_FALSE(msgs.empty());
    REQUIRE(msgs[0].find("999") != std::string::npos);
}

}  // namespace pvpgn::infra::inmemory
